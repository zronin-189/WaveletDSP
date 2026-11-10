#include "wavelet_dsp.h"
#include "coif5_filters.h"

#include <math.h>
#include <string.h>

int dwt_len(int n) {
    return ((n + COIF5_LEN - 3) / 2) + 1;
}

int dsp_valid_length(int n) {
    int level;
    int total = 0;
    if (n < DSP_NOISE_SAMPLES || n > DSP_BLOCK || n % (1 << DSP_LEVEL) != 0) {
        return 0;
    }
    for (level = 0; level < DSP_LEVEL; ++level) {
        n = dwt_len(n);
        total += n;
    }
    return total + n <= DSP_MAX_C;
}

static float at_ext(const float *x, int n, int p, int i) {
    if (i < p) {
        return x[p - 1 - i];
    }
    if (i < p + n) {
        return x[i - p];
    }
    return x[n - 1 - (i - (p + n))];
}

static void conv_valid_down2(const float *x, int n, const float *h, float *out) {
    const int lf = COIF5_LEN;
    const int p = lf - 1;
    const int last = n + lf - 1;
    const int first = 2;
    int count = 0;
    int idx;
    for (idx = first; idx <= last; idx += 2) {
        const int j = idx - 1;
        int m;
        float acc = 0.0f;
        for (m = 0; m < lf; ++m) {
            acc += at_ext(x, n, p, j + m) * h[lf - 1 - m];
        }
        out[count++] = acc;
    }
}

void dwt_sym_coif5(const float *x, int n, float *ca, float *cd) {
    conv_valid_down2(x, n, kCoif5LoD, ca);
    conv_valid_down2(x, n, kCoif5HiD, cd);
}

void idwt_sym_coif5(const float *ca, const float *cd, int n_coeff, int lx, float *y) {
    const int lf = COIF5_LEN;
    const int up_len = 2 * n_coeff - 1;
    const int conv_len = up_len + lf - 1;
    const float d = (conv_len - lx) / 2.0f;
    const int first = (int)floorf(d);
    int t;
    for (t = 0; t < lx; ++t) {
        const int n = first + t;
        float acc = 0.0f;
        int k;
        for (k = 0; k < lf; ++k) {
            const int j = n - k;
            float ua = 0.0f;
            float ud = 0.0f;
            if (j >= 0 && j < up_len && (j % 2) == 0) {
                const int q = j / 2;
                ua = ca[q];
                ud = cd[q];
            }
            acc += ua * kCoif5LoR[k] + ud * kCoif5HiR[k];
        }
        y[t] = acc;
    }
}

void wavedec_coif5(const float *x, int n, int level, DspWorkspace *ws) {
    int n_cur = n;
    int lev;
    int off[DSP_NBAND];
    int i;
    ws->L[level + 1] = n;
    n_cur = n;
    for (lev = 0; lev < level; ++lev) {
        n_cur = dwt_len(n_cur);
        ws->L[level - lev] = n_cur;
    }
    ws->L[0] = n_cur;
    off[0] = 0;
    for (i = 1; i < DSP_NBAND; ++i) {
        off[i] = off[i - 1] + ws->L[i - 1];
    }
    memcpy(ws->rec_a, x, (size_t)n * sizeof(float));
    n_cur = n;
    for (lev = 0; lev < level; ++lev) {
        const int dest = off[level - lev];
        dwt_sym_coif5(ws->rec_a, n_cur, ws->ca, ws->C + dest);
        n_cur = ws->L[level - lev];
        memcpy(ws->rec_a, ws->ca, (size_t)n_cur * sizeof(float));
    }
    memcpy(ws->C, ws->rec_a, (size_t)ws->L[0] * sizeof(float));
}

void waverec_coif5(DspWorkspace *ws, int level, float *y) {
    int pos = ws->L[0];
    int i;
    memcpy(ws->rec_a, ws->C, (size_t)ws->L[0] * sizeof(float));
    for (i = 1; i <= level; ++i) {
        const int lx = ws->L[i + 1];
        idwt_sym_coif5(ws->rec_a, ws->C + pos, ws->L[i], lx, ws->rec_b);
        memcpy(ws->rec_a, ws->rec_b, (size_t)lx * sizeof(float));
        pos += ws->L[i];
    }
    memcpy(y, ws->rec_a, (size_t)ws->L[level + 1] * sizeof(float));
}

static void movmean_left_heavy_even(const float *x, int n, int k, float *y) {
    int i;
    int nleft;
    int nright;
    if (k <= 1) {
        memcpy(y, x, (size_t)n * sizeof(float));
        return;
    }
    if (k % 2 == 1) {
        nleft = k / 2;
        nright = k / 2;
    } else {
        nleft = k / 2;
        nright = k / 2 - 1;
    }
    int begin = 0;
    int end = 0;
    float sum = 0.0f;
    for (i = 0; i < n; ++i) {
        int a = i - nleft;
        int b = i + nright + 1;
        if (a < 0) {
            a = 0;
        }
        if (b > n) {
            b = n;
        }
        while (end < b) {
            sum += x[end++];
        }
        while (begin < a) {
            sum -= x[begin++];
        }
        y[i] = fmaxf(0.0f, sum / (float)(b - a));
    }
}

void estimate_pn_frozen(const float *x, int n, int noise_n, float *Pn, DspWorkspace *ws) {
    int k;
    int s = 0;
    int use_n = noise_n;
    if (use_n > n) {
        use_n = n;
    }
    if (!x || !Pn || !ws || !dsp_valid_length(n) || !dsp_valid_length(use_n)) {
        if (Pn) {
            for (k = 0; k < DSP_NBAND; ++k) {
                Pn[k] = NAN;
            }
        }
        return;
    }
    wavedec_coif5(x, use_n, DSP_LEVEL, ws);
    for (k = 0; k < DSP_NBAND; ++k) {
        const int len = ws->L[k];
        const int e = s + len;
        int i;
        float mean = 0.0f;
        float variance = 0.0f;
        for (i = s; i < e; ++i) {
            float delta = ws->C[i] - mean;
            mean += delta / (float)(i - s + 1);
            variance += delta * (ws->C[i] - mean);
        }
        Pn[k] = fmaxf(0.0f, variance / (float)len);
        s = e;
    }
}

void denoise_block_l5(const float *x, int n, const float *Pn, float *y, DspWorkspace *ws) {
    int k;
    int s = 0;
    if (!x || !Pn || !y || !ws || !dsp_valid_length(n)) {
        if (y && n > 0 && n <= DSP_BLOCK) {
            memset(y, 0, (size_t)n * sizeof(float));
        }
        return;
    }
    for (k = 0; k < DSP_NBAND; ++k) {
        if (!isfinite(Pn[k]) || Pn[k] < 0.0f) {
            memset(y, 0, (size_t)n * sizeof(float));
            return;
        }
    }
    for (k = 0; k < n; ++k) {
        if (!isfinite(x[k])) {
            memset(y, 0, (size_t)n * sizeof(float));
            return;
        }
    }
    wavedec_coif5(x, n, DSP_LEVEL, ws);
    for (k = 0; k < DSP_NBAND; ++k) {
        const int len = ws->L[k];
        int sub_level = DSP_LEVEL + 2 - (k + 1);
        float fs_band;
        int wp;
        int wsm;
        int i;
        if (sub_level > DSP_LEVEL) {
            sub_level = DSP_LEVEL;
        }
        fs_band = (float)DSP_FS / (float)(1 << sub_level);
        wp = (int)lroundf(DSP_WIN_POW * fs_band);
        wsm = (int)lroundf(DSP_WIN_SMOOTH * fs_band);
        if (wp < 1) {
            wp = 1;
        }
        if (wsm < 1) {
            wsm = 1;
        }
        for (i = 0; i < len; ++i) {
            const float b = ws->C[s + i];
            ws->ca[i] = b * b;
        }
        movmean_left_heavy_even(ws->ca, len, wp, ws->cd);
        for (i = 0; i < len; ++i) {
            float p = ws->cd[i];
            float g;
            if (p < 1.0e-30f) {
                p = 1.0e-30f;
            }
            g = 1.0f - Pn[k] / p;
            if (g < DSP_GAIN_FLOOR) {
                g = DSP_GAIN_FLOOR;
            }
            ws->ca[i] = g;
        }
        movmean_left_heavy_even(ws->ca, len, wsm, ws->cd);
        for (i = 0; i < len; ++i) {
            ws->C[s + i] *= ws->cd[i];
        }
        s += len;
    }
    waverec_coif5(ws, DSP_LEVEL, y);
    for (k = 0; k < n; ++k) {
        float v = y[k];
        if (v > 1.0f) {
            v = 1.0f;
        } else if (v < -1.0f) {
            v = -1.0f;
        }
        y[k] = v;
    }
}

void overlap_fade(float *left_tail, const float *right_head, int ov) {
    int i;
    if (!left_tail || !right_head || ov < 2 || ov > DSP_OVERLAP) {
        return;
    }
    for (i = 0; i < ov; ++i) {
        const float fade = (float)i / (float)(ov - 1);
        left_tail[i] = left_tail[i] * (1.0f - fade) + right_head[i] * fade;
    }
}
