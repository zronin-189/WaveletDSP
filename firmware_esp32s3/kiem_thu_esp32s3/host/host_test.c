#include "wavelet_dsp.h"
#include "golden_impulse64.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int load_col(const char *path, float *dst, int maxn) {
    FILE *fp = fopen(path, "r");
    int n = 0;
    if (!fp) {
        fprintf(stderr, "cannot open %s\n", path);
        return -1;
    }
    while (n < maxn) {
        double v;
        int r = fscanf(fp, "%lf", &v);
        if (r != 1) {
            break;
        }
        dst[n++] = (float)v;
        /* skip rest of line */
        int c;
        while ((c = fgetc(fp)) != EOF && c != '\n') {
        }
    }
    fclose(fp);
    return n;
}

static float maxabs_diff(const float *a, const float *b, int n) {
    float m = 0.0f;
    int i;
    for (i = 0; i < n; ++i) {
        float d = fabsf(a[i] - b[i]);
        if (!isfinite(d)) {
            return INFINITY;
        }
        if (d > m) {
            m = d;
        }
    }
    return m;
}

int main(int argc, char **argv) {
    const char *vec = argc > 1 ? argv[1] : "firmware_esp32s3/kiem_thu_esp32s3/vectors";
    char path[512];
    float x[64];
    float ca[64];
    float cd[64];
    float rec[64];
    float ramp[64];
    int i;
    float e;
    DspWorkspace *ws;
    int fail = 0;

    memset(x, 0, sizeof(x));
    x[0] = 1.0f;
    dwt_sym_coif5(x, 64, ca, cd);
    e = maxabs_diff(ca, kImpulse64Approx, 46);
    printf("impulse dwt approx maxerr=%.3e (n=%d)\n", e, dwt_len(64));
    if (e > 1e-6f) {
        fail = 1;
    }
    e = maxabs_diff(cd, kImpulse64Detail, 46);
    printf("impulse dwt detail maxerr=%.3e\n", e);
    if (e > 1e-6f) {
        fail = 1;
    }
    idwt_sym_coif5(ca, cd, 46, 64, rec);
    e = maxabs_diff(rec, x, 64);
    printf("impulse idwt maxerr=%.3e\n", e);
    if (e > 1e-6f) {
        fail = 1;
    }
    for (i = 0; i < 64; ++i) {
        ramp[i] = (float)(i + 1);
    }
    dwt_sym_coif5(ramp, 64, ca, cd);
    idwt_sym_coif5(ca, cd, 46, 64, rec);
    e = maxabs_diff(rec, ramp, 64);
    printf("ramp idwt maxerr=%.3e\n", e);
    if (e > 2e-5f) {
        fail = 1;
    }

    ws = (DspWorkspace *)calloc(1, sizeof(DspWorkspace));
    if (!ws) {
        fprintf(stderr, "oom workspace\n");
        return 2;
    }

    snprintf(path, sizeof(path), "%s/block0_input.csv", vec);
    {
        float *xin = (float *)malloc((size_t)DSP_BLOCK * sizeof(float));
        float *xgold = (float *)malloc((size_t)DSP_MAX_C * sizeof(float));
        float *yout = (float *)malloc((size_t)DSP_BLOCK * sizeof(float));
        float *ygold = (float *)malloc((size_t)DSP_BLOCK * sizeof(float));
        float Pn[DSP_NBAND];
        float Pn_gold[DSP_NBAND];
        int nx;
        int nc;
        int ny;
        int np;
        if (!xin || !xgold || !yout || !ygold) {
            fprintf(stderr, "oom vectors\n");
            return 2;
        }
        nx = load_col(path, xin, DSP_BLOCK);
        snprintf(path, sizeof(path), "%s/block0_wavedec.csv", vec);
        nc = load_col(path, xgold, DSP_MAX_C);
        snprintf(path, sizeof(path), "%s/block0_output.csv", vec);
        ny = load_col(path, ygold, DSP_BLOCK);
        snprintf(path, sizeof(path), "%s/pn_white10_speech001.csv", vec);
        np = load_col(path, Pn_gold, DSP_NBAND);
        printf("loaded input=%d wavedec=%d out=%d pn=%d\n", nx, nc, ny, np);
        if (nx != DSP_BLOCK || nc < 16000 || ny != DSP_BLOCK || np != DSP_NBAND) {
            fprintf(stderr, "unexpected vector sizes\n");
            return 2;
        }
        wavedec_coif5(xin, DSP_BLOCK, DSP_LEVEL, ws);
        e = maxabs_diff(ws->C, xgold, nc);
        printf("block0 wavedec maxerr=%.3e  L=", e);
        for (i = 0; i <= DSP_LEVEL + 1; ++i) {
            printf("%d%s", ws->L[i], i == DSP_LEVEL + 1 ? "\n" : ",");
        }
        if (e > 5e-4f) {
            fail = 1;
        }
        estimate_pn_frozen(xin, DSP_BLOCK, DSP_NOISE_SAMPLES, Pn, ws);
        printf("Pn C:");
        for (i = 0; i < DSP_NBAND; ++i) {
            printf(" %.6e", Pn[i]);
        }
        printf("\nPn M:");
        for (i = 0; i < DSP_NBAND; ++i) {
            printf(" %.6e", Pn_gold[i]);
        }
        printf("\nPn maxerr=%.3e\n", maxabs_diff(Pn, Pn_gold, DSP_NBAND));
        if (maxabs_diff(Pn, Pn_gold, DSP_NBAND) > 1e-6f) {
            fail = 1;
        }
        denoise_block_l5(xin, DSP_BLOCK, Pn_gold, yout, ws);
        e = maxabs_diff(yout, ygold, DSP_BLOCK);
        {
            double num = 0.0;
            double den = 0.0;
            for (i = 0; i < DSP_BLOCK; ++i) {
                double d = (double)yout[i] - (double)ygold[i];
                num += d * d;
                den += (double)ygold[i] * (double)ygold[i];
            }
            printf("block0 denoise maxerr=%.3e rms=%.3e rel=%.3e\n", e,
                   sqrt(num / DSP_BLOCK), sqrt(num / (den + 1e-30)));
        }
        if (e > 5e-4f) {
            fail = 1;
        }
        free(xin);
        free(xgold);
        free(yout);
        free(ygold);
    }
    free(ws);
    printf(fail ? "FAIL\n" : "PASS\n");
    return fail;
}
