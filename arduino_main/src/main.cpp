// WaveletDSP: standalone Arduino ESP32-S3 firmware.
// Paste this entire file into an Arduino IDE sketch, or build with PlatformIO.
// Required: Arduino-ESP32 2.0.17 (ESP-IDF 4.4.7), ESP32-S3 with 16 MB flash / 8 MB OPI PSRAM.
// Mic WS4/SCK5/SD6; amp LRC7/BCLK8/DIN9, SD->3V3, Vin->5V; GPIO10 unused.
#include <Arduino.h>

// Match the tested DSP optimization even when the Arduino IDE defaults to -Os.
#pragma GCC optimize ("O2")


// ==================== wavelet_dsp.h ====================
#ifndef WAVELET_DSP_H
#define WAVELET_DSP_H

#ifdef __cplusplus
extern "C" {
#endif

#define DSP_FS 16000
#define DSP_BLOCK 16000
#define DSP_LEVEL 5
#define DSP_OVERLAP 2048
#define DSP_HOP (DSP_BLOCK - DSP_OVERLAP)
#define DSP_NOISE_SAMPLES 4000
#define DSP_GAIN_FLOOR 0.05f
#define DSP_WIN_POW 0.064f
#define DSP_WIN_SMOOTH 0.040f
#define DSP_NBAND (DSP_LEVEL + 1)
#define DSP_MAX_C 17024

typedef struct {
    float ca[DSP_BLOCK];
    float cd[DSP_BLOCK];
    float C[DSP_MAX_C];
    float rec_a[DSP_BLOCK];
    float rec_b[DSP_BLOCK];
    int L[DSP_LEVEL + 2];
} DspWorkspace;

int dwt_len(int n);
int dsp_valid_length(int n);
void dwt_sym_coif5(const float *x, int n, float *ca, float *cd);
void idwt_sym_coif5(const float *ca, const float *cd, int n_coeff, int lx, float *y);
void wavedec_coif5(const float *x, int n, int level, DspWorkspace *ws);
void waverec_coif5(DspWorkspace *ws, int level, float *y);
void estimate_pn_frozen(const float *x, int n, int noise_n, float *Pn, DspWorkspace *ws);
void denoise_block_l5(const float *x, int n, const float *Pn, float *y, DspWorkspace *ws);
void overlap_fade(float *left_tail, const float *right_head, int ov);

#ifdef __cplusplus
}
#endif
#endif


// ==================== audio_config.h ====================
#ifndef AUDIO_CONFIG_H
#define AUDIO_CONFIG_H


#define AUDIO_PIN_MIC_WS 4
#define AUDIO_PIN_MIC_SCK 5
#define AUDIO_PIN_MIC_SD 6
#define AUDIO_PIN_AMP_LRC 7
#define AUDIO_PIN_AMP_BCLK 8
#define AUDIO_PIN_AMP_DIN 9
#define AUDIO_PIN_AMP_MUTE 10
#define AUDIO_EXTERNAL_NPN_MUTE 0
#define AUDIO_MIC_SLOT 0
#define AUDIO_CHUNK 128
#define AUDIO_DMA_COUNT 8
#define AUDIO_INPUT_SLOTS 3
#define AUDIO_OUTPUT_SLOTS 3
#define AUDIO_DELAY_SAMPLES 20000
#define AUDIO_SETTLE_SAMPLES DSP_FS
#define AUDIO_RAW_SETTLE_SAMPLES DSP_FS
#define AUDIO_DEFAULT_VOLUME 0.05f
#define AUDIO_MAX_VOLUME 0.25f
#define AUDIO_TEST_CAPTURE 0
#define AUDIO_AUTOSTART 1
#define AUDIO_BOOT_VOLUME_PERCENT 20
#ifndef AUDIO_RAW_PLAYBACK_GAIN
#define AUDIO_RAW_PLAYBACK_GAIN 8.0f
#endif
#define AUDIO_IO_TIMEOUT_MS 100

#endif


// ==================== coif5_filters.h ====================
#ifndef COIF5_FILTERS_H
#define COIF5_FILTERS_H

#define COIF5_LEN 30

/* MATLAB wfilters('coif5') dumped 2026-11-09. */
static const float kCoif5LoD[COIF5_LEN] = {
    -9.517657273819165e-08,
    -1.6744288576823017e-07,
    2.0637618513646814e-06,
    3.7346551751414047e-06,
    -2.1315026809955787e-05,
    -4.1340432272512511e-05,
    0.00014054114970203437,
    0.00030225958181306315,
    -0.00063813134304511142,
    -0.0016628637020130838,
    0.0024333732126576722,
    0.0067641854480530832,
    -0.0091642311624818458,
    -0.019761778942572639,
    0.032683574267111833,
    0.041289208750181702,
    -0.10557420870333893,
    -0.062035963962903569,
    0.43799162617183712,
    0.77428960365295618,
    0.42156620669085149,
    -0.052043163176243773,
    -0.091920010559696244,
    0.02816802897093635,
    0.023408156785839195,
    -0.010131117519849788,
    -0.004159358781386048,
    0.0021782363581090178,
    0.00035858968789573785,
    -0.00021208083980379827
};

static const float kCoif5HiD[COIF5_LEN] = {
    0.00021208083980379827,
    0.00035858968789573785,
    -0.0021782363581090178,
    -0.004159358781386048,
    0.010131117519849788,
    0.023408156785839195,
    -0.02816802897093635,
    -0.091920010559696244,
    0.052043163176243773,
    0.42156620669085149,
    -0.77428960365295618,
    0.43799162617183712,
    0.062035963962903569,
    -0.10557420870333893,
    -0.041289208750181702,
    0.032683574267111833,
    0.019761778942572639,
    -0.0091642311624818458,
    -0.0067641854480530832,
    0.0024333732126576722,
    0.0016628637020130838,
    -0.00063813134304511142,
    -0.00030225958181306315,
    0.00014054114970203437,
    4.1340432272512511e-05,
    -2.1315026809955787e-05,
    -3.7346551751414047e-06,
    2.0637618513646814e-06,
    1.6744288576823017e-07,
    -9.517657273819165e-08
};

static const float kCoif5LoR[COIF5_LEN] = {
    -0.00021208083980379827,
    0.00035858968789573785,
    0.0021782363581090178,
    -0.004159358781386048,
    -0.010131117519849788,
    0.023408156785839195,
    0.02816802897093635,
    -0.091920010559696244,
    -0.052043163176243773,
    0.42156620669085149,
    0.77428960365295618,
    0.43799162617183712,
    -0.062035963962903569,
    -0.10557420870333893,
    0.041289208750181702,
    0.032683574267111833,
    -0.019761778942572639,
    -0.0091642311624818458,
    0.0067641854480530832,
    0.0024333732126576722,
    -0.0016628637020130838,
    -0.00063813134304511142,
    0.00030225958181306315,
    0.00014054114970203437,
    -4.1340432272512511e-05,
    -2.1315026809955787e-05,
    3.7346551751414047e-06,
    2.0637618513646814e-06,
    -1.6744288576823017e-07,
    -9.517657273819165e-08
};

static const float kCoif5HiR[COIF5_LEN] = {
    -9.517657273819165e-08,
    1.6744288576823017e-07,
    2.0637618513646814e-06,
    -3.7346551751414047e-06,
    -2.1315026809955787e-05,
    4.1340432272512511e-05,
    0.00014054114970203437,
    -0.00030225958181306315,
    -0.00063813134304511142,
    0.0016628637020130838,
    0.0024333732126576722,
    -0.0067641854480530832,
    -0.0091642311624818458,
    0.019761778942572639,
    0.032683574267111833,
    -0.041289208750181702,
    -0.10557420870333893,
    0.062035963962903569,
    0.43799162617183712,
    -0.77428960365295618,
    0.42156620669085149,
    0.052043163176243773,
    -0.091920010559696244,
    -0.02816802897093635,
    0.023408156785839195,
    0.010131117519849788,
    -0.004159358781386048,
    -0.0021782363581090178,
    0.00035858968789573785,
    0.00021208083980379827
};

#endif


// ==================== audio_stream.h ====================
#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

float audio_decode_mic(int32_t word);
int16_t audio_encode_pcm(float sample, float volume, bool *clipped);
int16_t audio_encode_raw_pcm(float sample, float volume, bool *limited);
bool audio_noise_valid(const float *samples, int count, const float *powers);
bool audio_join_frame(float *tail, bool *has_tail, float *frame, int count);

#ifdef __cplusplus
}
#endif
#endif


// ==================== audio_i2s.h ====================
#ifndef AUDIO_I2S_H
#define AUDIO_I2S_H

#include <stdint.h>
#include "esp_err.h"

esp_err_t audio_i2s_begin();
void audio_i2s_end();
esp_err_t audio_i2s_read(int32_t *stereo);
esp_err_t audio_i2s_write(const int32_t *stereo);
bool audio_i2s_events_ok();

#endif


// ==================== audio_pipeline.h ====================
#ifndef AUDIO_PIPELINE_H
#define AUDIO_PIPELINE_H

void audio_pipeline_init();
void audio_pipeline_command(const char *command);
void audio_pipeline_poll();

#endif


// ==================== wavelet_dsp.c ====================

#include <math.h>
#include <string.h>

int dwt_len(int n);
int dsp_valid_length(int n);
static float at_ext(const float *x, int n, int p, int i);
static void conv_valid_down2(const float *x, int n, const float *h, float *out);
void dwt_sym_coif5(const float *x, int n, float *ca, float *cd);
void idwt_sym_coif5(const float *ca, const float *cd, int n_coeff, int lx, float *y);
void wavedec_coif5(const float *x, int n, int level, DspWorkspace *ws);
void waverec_coif5(DspWorkspace *ws, int level, float *y);
static void movmean_left_heavy_even(const float *x, int n, int k, float *y);
void estimate_pn_frozen(const float *x, int n, int noise_n, float *Pn, DspWorkspace *ws);
void denoise_block_l5(const float *x, int n, const float *Pn, float *y, DspWorkspace *ws);
void overlap_fade(float *left_tail, const float *right_head, int ov);


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


// ==================== audio_stream.c ====================

#include <math.h>
#include <string.h>

float audio_decode_mic(int32_t word);
int16_t audio_encode_pcm(float sample, float volume, bool *clipped);
int16_t audio_encode_raw_pcm(float sample, float volume, bool *limited);
bool audio_noise_valid(const float *samples, int count, const float *powers);
bool audio_join_frame(float *tail, bool *has_tail, float *frame, int count);


float audio_decode_mic(int32_t word) {
    return (float)(word / 256) * (1.0f / 8388608.0f);
}

int16_t audio_encode_pcm(float sample, float volume, bool *clipped) {
    float scaled = sample * volume;
    *clipped = !isfinite(scaled) || scaled > 1.0f || scaled < -1.0f;
    if (!isfinite(scaled)) {
        return 0;
    }
    if (scaled >= 1.0f) {
        return 32767;
    }
    if (scaled <= -1.0f) {
        return -32768;
    }
    return (int16_t)lroundf(scaled * 32767.0f);
}

int16_t audio_encode_raw_pcm(float sample, float volume, bool *limited) {
    float scaled = sample * AUDIO_RAW_PLAYBACK_GAIN;
    *limited = !isfinite(scaled) || scaled > 1.0f || scaled < -1.0f;
    if (!isfinite(scaled)) {
        return 0;
    }
    // Limit before volume so microphone gain cannot bypass the output setting.
    scaled = fmaxf(-1.0f, fminf(1.0f, scaled));
    bool clipped;
    int16_t pcm = audio_encode_pcm(scaled, volume, &clipped);
    *limited = *limited || clipped;
    return pcm;
}

bool audio_noise_valid(const float *samples, int count, const float *powers) {
    float energy = 0.0f;
    float total_power = 0.0f;
    int index;
    if (!samples || !powers || count < DSP_NOISE_SAMPLES) {
        return false;
    }
    for (index = 0; index < count; ++index) {
        if (!isfinite(samples[index]) || fabsf(samples[index]) >= 0.99f) {
            return false;
        }
        energy += samples[index] * samples[index];
    }
    for (index = 0; index < DSP_NBAND; ++index) {
        if (!isfinite(powers[index]) || powers[index] < 0.0f) {
            return false;
        }
        total_power += powers[index];
    }
    return isfinite(energy) && energy > 1.0e-12f &&
           isfinite(total_power) && total_power > 1.0e-15f;
}

bool audio_join_frame(float *tail, bool *has_tail, float *frame, int count) {
    int index;
    if (!tail || !has_tail || !frame || count != DSP_BLOCK) {
        return false;
    }
    if (*has_tail) {
        for (index = 0; index < DSP_OVERLAP; ++index) {
            float weight = (float)index / (float)(DSP_OVERLAP - 1);
            frame[index] = tail[index] * (1.0f - weight) + frame[index] * weight;
        }
    }
    memcpy(tail, frame + DSP_HOP, DSP_OVERLAP * sizeof(float));
    *has_tail = true;
    return true;
}


// ==================== audio_i2s.cpp ====================

#include <Arduino.h>
#include "driver/i2s.h"
#include "esp32-hal-matrix.h"
#include "esp_idf_version.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_struct.h"

esp_err_t audio_i2s_begin();
void audio_i2s_end();
esp_err_t audio_i2s_read(int32_t *stereo);
esp_err_t audio_i2s_write(const int32_t *stereo);
bool audio_i2s_events_ok();


#if !defined(CONFIG_IDF_TARGET_ESP32S3) || ESP_IDF_VERSION != ESP_IDF_VERSION_VAL(4, 4, 7)
#error Review the stereo RX workaround before changing target or ESP-IDF version
#endif

static QueueHandle_t events;
static bool installed;

esp_err_t audio_i2s_begin() {
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX);
    config.sample_rate = DSP_FS;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = AUDIO_DMA_COUNT;
    config.dma_buf_len = AUDIO_CHUNK;
    config.tx_desc_auto_clear = true;
    config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    config.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;
    esp_err_t result = i2s_driver_install(I2S_NUM_0, &config, 64, &events);
    if (result != ESP_OK) {
        return result;
    }
    installed = true;
    result = i2s_stop(I2S_NUM_0);
    if (result != ESP_OK) {
        audio_i2s_end();
        return result;
    }
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.ws_io_num = AUDIO_PIN_MIC_WS;
    pins.bck_io_num = AUDIO_PIN_MIC_SCK;
    pins.data_in_num = AUDIO_PIN_MIC_SD;
    pins.data_out_num = AUDIO_PIN_AMP_DIN;
    result = i2s_set_pin(I2S_NUM_0, &pins);
    if (result == ESP_OK) {
        pinMode(AUDIO_PIN_AMP_LRC, OUTPUT);
        pinMode(AUDIO_PIN_AMP_BCLK, OUTPUT);
        pinMatrixOutAttach(AUDIO_PIN_AMP_LRC, I2S0O_WS_OUT_IDX, false, false);
        pinMatrixOutAttach(AUDIO_PIN_AMP_BCLK, I2S0O_BCK_OUT_IDX, false, false);
        I2S0.rx_conf.rx_mono = 0;
        I2S0.rx_conf.rx_mono_fst_vld = 0;
        I2S0.tx_conf.tx_ws_idle_pol = 0;
        I2S0.rx_conf.rx_ws_idle_pol = 0;
        xQueueReset(events);
        result = i2s_start(I2S_NUM_0);
    }
    if (result != ESP_OK) {
        audio_i2s_end();
    }
    return result;
}

void audio_i2s_end() {
    if (installed) {
        i2s_driver_uninstall(I2S_NUM_0);
        installed = false;
        events = nullptr;
        pinMatrixOutDetach(AUDIO_PIN_AMP_LRC, false, false);
        pinMatrixOutDetach(AUDIO_PIN_AMP_BCLK, false, false);
        digitalWrite(AUDIO_PIN_AMP_LRC, LOW);
        digitalWrite(AUDIO_PIN_AMP_BCLK, LOW);
    }
}

esp_err_t audio_i2s_read(int32_t *stereo) {
    size_t received = 0;
    const size_t wanted = AUDIO_CHUNK * 2 * sizeof(int32_t);
    esp_err_t result = i2s_read(I2S_NUM_0, stereo, wanted, &received,
                              pdMS_TO_TICKS(AUDIO_IO_TIMEOUT_MS));
    return result == ESP_OK && received != wanted ? ESP_ERR_TIMEOUT : result;
}

esp_err_t audio_i2s_write(const int32_t *stereo) {
    size_t sent = 0;
    const size_t wanted = AUDIO_CHUNK * 2 * sizeof(int32_t);
    esp_err_t result = i2s_write(I2S_NUM_0, stereo, wanted, &sent,
                               pdMS_TO_TICKS(AUDIO_IO_TIMEOUT_MS));
    return result == ESP_OK && sent != wanted ? ESP_ERR_TIMEOUT : result;
}

bool audio_i2s_events_ok() {
    i2s_event_t event;
    bool okay = true;
    for (int index = 0; index < 64 && xQueueReceive(events, &event, 0) == pdTRUE; ++index) {
        if (event.type == I2S_EVENT_DMA_ERROR || event.type == I2S_EVENT_RX_Q_OVF ||
            event.type == I2S_EVENT_TX_Q_OVF) {
            okay = false;
        }
    }
    return okay;
}


// ==================== audio_pipeline.cpp ====================

#include <Arduino.h>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"

enum class State { Idle, Starting, Calibrating, Buffering, Running, Stopping, Fault };
enum class Fault {
    None, Memory, Task, Watchdog, I2sInit, Read, Write, Dma, InputQueue,
    OutputQueue, Underrun, Sequence, Calibration, DspDeadline, NonFinite, Handshake
};
enum class Command { Start, Raw };

struct Frame {
    int slot;
    uint32_t sequence;
};

static const char *state_names[] = {
    "IDLE", "STARTING", "CALIBRATING", "BUFFERING", "RUNNING", "STOPPING", "FAULT"
};
static const char *fault_names[] = {
    "none", "memory", "task", "watchdog", "i2s_init", "rx_timeout", "tx_timeout",
    "dma_overflow", "input_queue", "output_queue", "underrun", "sequence",
    "noise_profile", "dsp_deadline", "nonfinite", "dsp_handshake"
};
static std::atomic<State> state{State::Idle};
static std::atomic<Fault> fault{Fault::None};
static std::atomic<bool> stop_requested{false};
static std::atomic<bool> raw_mode{false};
static std::atomic<uint32_t> volume_percent{AUDIO_BOOT_VOLUME_PERCENT};
static std::atomic<uint32_t> processed{0}, dsp_ms{0}, dsp_max_ms{0};
static std::atomic<uint32_t> underruns{0}, overruns{0}, dma_errors{0}, clipped{0};
static std::atomic<uint32_t> rx_peak_ppm{0}, io_heartbeat{0}, dsp_heartbeat{0};
static QueueHandle_t input_free, input_ready, output_free, output_ready;
static QueueHandle_t commands, dsp_done;
static QueueHandle_t profiles;
static TaskHandle_t io_handle, dsp_handle;
static DspWorkspace *workspace;
static float *inputs[AUDIO_INPUT_SLOTS], *outputs[AUDIO_OUTPUT_SLOTS], *tail;
static float noise_powers[DSP_NBAND];
#if AUDIO_TEST_CAPTURE
static float *noise_snapshot;
static std::atomic<bool> noise_snapshot_ready{false};
static constexpr uint32_t TRACE_FRAMES = 8 * DSP_HOP;
static int16_t *filter_trace;
static std::atomic<uint32_t> trace_frames{0};
static uint32_t trace_volume;
#endif
static int32_t rx_words[AUDIO_CHUNK * 2], tx_words[AUDIO_CHUNK * 2];
static std::atomic<bool> ready{false};
static portMUX_TYPE mute_lock = portMUX_INITIALIZER_UNLOCKED;

static_assert(DSP_HOP % (1 << DSP_LEVEL) == 0, "DWT hop must preserve phase");
static_assert(DSP_HOP > DSP_OVERLAP, "Only adjacent frames may overlap");
static_assert(AUDIO_CHUNK * 2 * sizeof(int32_t) <= 4092, "DMA descriptor too large");
static_assert(AUDIO_BOOT_VOLUME_PERCENT <= AUDIO_MAX_VOLUME * 100.0f, "Boot volume exceeds limit");

// Explicit declarations also support Arduino sketch preprocessing.
static void fail(Fault reason);
static void mute(bool enabled);
static void dsp_task(void *);
static bool reset_pools();
static void run_audio();
static void io_task(void *);
void audio_pipeline_init();
static void dump_noise_snapshot();
static void print_status();
static void dump_filter_trace();
void audio_pipeline_command(const char *command);
void audio_pipeline_poll();

static void fail(Fault reason) {
    portENTER_CRITICAL(&mute_lock);
    Fault expected = Fault::None;
    fault.compare_exchange_strong(expected, reason);
    if (AUDIO_EXTERNAL_NPN_MUTE) {
        digitalWrite(AUDIO_PIN_AMP_MUTE, HIGH);
    }
    portEXIT_CRITICAL(&mute_lock);
}

static void mute(bool enabled) {
    portENTER_CRITICAL(&mute_lock);
    if (AUDIO_EXTERNAL_NPN_MUTE) {
        digitalWrite(AUDIO_PIN_AMP_MUTE,
                     enabled || fault.load() != Fault::None || stop_requested.load() ? HIGH : LOW);
    }
    portEXIT_CRITICAL(&mute_lock);
}

static void dsp_task(void *) {
    const bool watchdog = esp_task_wdt_add(nullptr) == ESP_OK;
    if (!watchdog) {
        fail(Fault::Watchdog);
        ready.store(false);
    }
    bool has_tail = false;
    for (;;) {
        if (watchdog) {
            esp_task_wdt_reset();
        }
        dsp_heartbeat.store(millis());
        Frame frame;
        if (xQueueReceive(input_ready, &frame, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }
        if (frame.slot < 0) {
            has_tail = false;
            bool done = true;
            if (xQueueSend(dsp_done, &done, 0) != pdTRUE) {
                fail(Fault::Handshake);
            }
            continue;
        }
        const int64_t began = esp_timer_get_time();
        int output_slot = -1;
        if (fault.load() == Fault::None && !stop_requested.load()) {
            if (xQueueReceive(output_free, &output_slot, 0) != pdTRUE) {
                fail(Fault::OutputQueue);
            } else {
                const float *input = inputs[frame.slot];
                float *output = outputs[output_slot];
                if (frame.sequence == 0) {
                    has_tail = false;
                    if (!raw_mode.load()) {
                        estimate_pn_frozen(input, DSP_BLOCK, DSP_NOISE_SAMPLES,
                                           noise_powers, workspace);
                        if (!audio_noise_valid(input, DSP_NOISE_SAMPLES, noise_powers)) {
                            fail(Fault::Calibration);
                        } else {
#if AUDIO_TEST_CAPTURE
                            memcpy(noise_snapshot, input, DSP_NOISE_SAMPLES * sizeof(float));
                            noise_snapshot_ready.store(true);
#endif
                            if (xQueueSend(profiles, noise_powers, 0) != pdTRUE) {
                                fail(Fault::Calibration);
                            }
                        }
                    }
                }
                if (fault.load() == Fault::None) {
                    if (raw_mode.load()) {
                        memcpy(output, input, DSP_BLOCK * sizeof(float));
                    } else {
                        denoise_block_l5(input, DSP_BLOCK, noise_powers, output, workspace);
                    }
                    for (int index = 0; index < DSP_BLOCK; ++index) {
                        if (!std::isfinite(output[index])) {
                            fail(Fault::NonFinite);
                            break;
                        }
                    }
                    audio_join_frame(tail, &has_tail, output, DSP_BLOCK);
#if AUDIO_TEST_CAPTURE
                    if (!raw_mode.load() && frame.sequence < 8 && fault.load() == Fault::None) {
                        const uint32_t base = frame.sequence * DSP_HOP;
                        for (int i = 0; i < DSP_HOP; ++i) {
                            bool limited;
                            filter_trace[2 * (base + i)] = audio_encode_raw_pcm(input[i], trace_volume * 0.01f, &limited);
                            filter_trace[2 * (base + i) + 1] = audio_encode_raw_pcm(output[i], trace_volume * 0.01f, &limited);
                        }
                        trace_frames.store(base + DSP_HOP);
                    }
#endif
                    uint32_t elapsed = static_cast<uint32_t>((esp_timer_get_time() - began) / 1000);
                    dsp_ms.store(elapsed);
                    if (elapsed > dsp_max_ms.load()) {
                        dsp_max_ms.store(elapsed);
                    }
                    if (elapsed >= DSP_HOP * 1000 / DSP_FS) {
                        fail(Fault::DspDeadline);
                    }
                    if (fault.load() == Fault::None && !stop_requested.load()) {
                        Frame result{output_slot, frame.sequence};
                        if (xQueueSend(output_ready, &result, 0) != pdTRUE) {
                            fail(Fault::OutputQueue);
                        } else {
                            output_slot = -1;
                            processed.fetch_add(1);
                            if (frame.sequence == 0) {
                                state.store(State::Buffering);
                            }
                        }
                    }
                }
            }
        }
        if (output_slot >= 0 && xQueueSend(output_free, &output_slot, 0) != pdTRUE) {
            fail(Fault::OutputQueue);
        }
        if (xQueueSend(input_free, &frame.slot, 0) != pdTRUE) {
            fail(Fault::InputQueue);
        }
        vTaskDelay(1);
    }
}

static bool reset_pools() {
    xQueueReset(input_free);
    xQueueReset(input_ready);
    xQueueReset(output_free);
    xQueueReset(output_ready);
    xQueueReset(dsp_done);
    xQueueReset(profiles);
    for (int index = 0; index < AUDIO_INPUT_SLOTS; ++index) {
        if (xQueueSend(input_free, &index, 0) != pdTRUE) {
            return false;
        }
    }
    for (int index = 0; index < AUDIO_OUTPUT_SLOTS; ++index) {
        if (xQueueSend(output_free, &index, 0) != pdTRUE) {
            return false;
        }
    }
    return true;
}

static void run_audio() {
    if (!reset_pools()) {
        fail(Fault::InputQueue);
        return;
    }
    int input_slot = -1;
    if (xQueueReceive(input_free, &input_slot, 0) != pdTRUE) {
        fail(Fault::InputQueue);
        return;
    }
    state.store(raw_mode.load() ? State::Buffering : State::Calibrating);
    if (audio_i2s_begin() != ESP_OK) {
        fail(Fault::I2sInit);
        return;
    }
    uint32_t sequence = 0, output_sequence = 0;
    uint64_t stream_samples = 0;
    int settled = 0, filled = 0, output_slot = -1, output_pos = 0;
    const int settle_samples = raw_mode.load() ? AUDIO_RAW_SETTLE_SAMPLES : AUDIO_SETTLE_SAMPLES;
    bool playing = false;
    while (!stop_requested.load() && fault.load() == Fault::None) {
        esp_task_wdt_reset();
        io_heartbeat.store(millis());
        if (audio_i2s_read(rx_words) != ESP_OK) {
            fail(Fault::Read);
            break;
        }
        memset(tx_words, 0, sizeof(tx_words));
        float peak = 0.0f;
        for (int index = 0; index < AUDIO_CHUNK; ++index) {
            if (settled < settle_samples) {
                ++settled;
                continue;
            }
            float sample = audio_decode_mic(rx_words[2 * index + AUDIO_MIC_SLOT]);
            peak = fmaxf(peak, fabsf(sample));
            inputs[input_slot][filled++] = sample;
            if (filled == DSP_BLOCK) {
                int next_slot;
                if (xQueueReceive(input_free, &next_slot, 0) != pdTRUE) {
                    overruns.fetch_add(1);
                    fail(Fault::InputQueue);
                    break;
                }
                memcpy(inputs[next_slot], inputs[input_slot] + DSP_HOP,
                       DSP_OVERLAP * sizeof(float));
                Frame frame{input_slot, sequence++};
                if (xQueueSend(input_ready, &frame, 0) != pdTRUE) {
                    overruns.fetch_add(1);
                    fail(Fault::InputQueue);
                    break;
                }
                input_slot = next_slot;
                filled = DSP_OVERLAP;
            }
            if (stream_samples >= AUDIO_DELAY_SAMPLES) {
                if (output_slot < 0) {
                    Frame frame;
                    if (xQueueReceive(output_ready, &frame, 0) != pdTRUE) {
                        underruns.fetch_add(1);
                        fail(Fault::Underrun);
                        break;
                    }
                    if (frame.sequence != output_sequence++) {
                        fail(Fault::Sequence);
                        break;
                    }
                    output_slot = frame.slot;
                    output_pos = 0;
                }
                bool saturated;
                float ramp = fminf(1.0f, static_cast<float>(stream_samples - AUDIO_DELAY_SAMPLES) / 160.0f);
                float output_sample = outputs[output_slot][output_pos++];
                float output_volume = ramp * volume_percent.load() * 0.01f;
                int16_t pcm = audio_encode_raw_pcm(output_sample, output_volume, &saturated);
                if (saturated) {
                    clipped.fetch_add(1);
                }
                int32_t word = static_cast<int32_t>(pcm) * 65536;
                tx_words[2 * index] = word;
                tx_words[2 * index + 1] = word;
                playing = true;
                if (output_pos == DSP_HOP) {
                    if (xQueueSend(output_free, &output_slot, 0) != pdTRUE) {
                        fail(Fault::OutputQueue);
                        break;
                    }
                    output_slot = -1;
                }
            }
            ++stream_samples;
        }
        rx_peak_ppm.store(static_cast<uint32_t>(peak * 1000000.0f));
        if (fault.load() != Fault::None || stop_requested.load()) {
            break;
        }
        if (audio_i2s_write(tx_words) != ESP_OK) {
            fail(Fault::Write);
            break;
        }
        if (!audio_i2s_events_ok()) {
            dma_errors.fetch_add(1);
            fail(Fault::Dma);
            break;
        }
        if (playing) {
            mute(false);
            state.store(State::Running);
        }
    }
    mute(true);
    state.store(State::Stopping);
    audio_i2s_end();
    Frame end{-1, 0};
    bool done = false;
    if (xQueueSend(input_ready, &end, pdMS_TO_TICKS(2000)) != pdTRUE ||
        xQueueReceive(dsp_done, &done, pdMS_TO_TICKS(2000)) != pdTRUE) {
        fail(Fault::Handshake);
        ready = false;
    }
}

static void io_task(void *) {
    const bool watchdog = esp_task_wdt_add(nullptr) == ESP_OK;
    if (!watchdog) {
        fail(Fault::Watchdog);
        state.store(State::Fault);
        ready.store(false);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    for (;;) {
        esp_task_wdt_reset();
        io_heartbeat.store(millis());
        Command command;
        if (xQueueReceive(commands, &command, pdMS_TO_TICKS(100)) != pdTRUE) {
            continue;
        }
        if (!ready || fault.load() == Fault::Watchdog) {
            continue;
        }
        raw_mode.store(command == Command::Raw);
#if AUDIO_TEST_CAPTURE
        noise_snapshot_ready.store(false);
        trace_frames.store(0);
        trace_volume = volume_percent.load();
#endif
        fault.store(Fault::None);
        processed.store(0);
        dsp_ms.store(0);
        dsp_max_ms.store(0);
        underruns.store(0);
        overruns.store(0);
        dma_errors.store(0);
        clipped.store(0);
        state.store(State::Starting);
        mute(true);
        run_audio();
        mute(true);
        state.store(fault.load() == Fault::None ? State::Idle : State::Fault);
    }
}

void audio_pipeline_init() {
    if (AUDIO_EXTERNAL_NPN_MUTE) {
        digitalWrite(AUDIO_PIN_AMP_MUTE, HIGH);
        pinMode(AUDIO_PIN_AMP_MUTE, OUTPUT);
    } else {
        pinMode(AUDIO_PIN_AMP_MUTE, INPUT);
    }
    workspace = static_cast<DspWorkspace *>(heap_caps_calloc(1, sizeof(DspWorkspace), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    tail = static_cast<float *>(heap_caps_calloc(DSP_OVERLAP, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    bool allocated = workspace && tail;
#if AUDIO_TEST_CAPTURE
    noise_snapshot = static_cast<float *>(heap_caps_calloc(DSP_NOISE_SAMPLES, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    filter_trace = static_cast<int16_t *>(heap_caps_calloc(TRACE_FRAMES * 2, sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    allocated = allocated && noise_snapshot && filter_trace;
#endif
    for (int index = 0; index < AUDIO_INPUT_SLOTS; ++index) {
        inputs[index] = static_cast<float *>(heap_caps_calloc(DSP_BLOCK, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        allocated = allocated && inputs[index];
    }
    for (int index = 0; index < AUDIO_OUTPUT_SLOTS; ++index) {
        outputs[index] = static_cast<float *>(heap_caps_calloc(DSP_BLOCK, sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        allocated = allocated && outputs[index];
    }
    input_free = xQueueCreate(AUDIO_INPUT_SLOTS, sizeof(int));
    input_ready = xQueueCreate(AUDIO_INPUT_SLOTS + 1, sizeof(Frame));
    output_free = xQueueCreate(AUDIO_OUTPUT_SLOTS, sizeof(int));
    output_ready = xQueueCreate(AUDIO_OUTPUT_SLOTS, sizeof(Frame));
    commands = xQueueCreate(1, sizeof(Command));
    dsp_done = xQueueCreate(1, sizeof(bool));
    profiles = xQueueCreate(1, sizeof(noise_powers));
    if (!allocated || !input_free || !input_ready || !output_free || !output_ready || !commands || !dsp_done || !profiles) {
        fail(Fault::Memory);
        state.store(State::Fault);
        return;
    }
    ready = true;
    if (xTaskCreatePinnedToCore(dsp_task, "audio_dsp", 6144, nullptr, 2, &dsp_handle, 1) != pdPASS ||
        xTaskCreatePinnedToCore(io_task, "audio_io", 6144, nullptr, 4, &io_handle, 0) != pdPASS) {
        ready = false;
        fail(Fault::Task);
        state.store(State::Fault);
    }
    Serial.printf("PSRAM audio=%u bytes; internal_free=%u PSRAM_free=%u\n",
                  static_cast<unsigned>(
#if AUDIO_TEST_CAPTURE
                      TRACE_FRAMES * 2 * sizeof(int16_t) + DSP_NOISE_SAMPLES * sizeof(float) +
#endif
                      sizeof(DspWorkspace) + DSP_OVERLAP * sizeof(float) +
                      (AUDIO_INPUT_SLOTS + AUDIO_OUTPUT_SLOTS) * DSP_BLOCK * sizeof(float)),
                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                  heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    Serial.printf("Autostart=%d test_capture=%d boot_volume=%u%%\n",
                  AUDIO_AUTOSTART, AUDIO_TEST_CAPTURE, AUDIO_BOOT_VOLUME_PERCENT);
#if AUDIO_TEST_CAPTURE
    Serial.println("Commands: start | raw | stop | status | noise_dump | filter_dump | volume 0..25 (percent)");
#else
    Serial.println("Commands: start | raw | stop | status | volume 0..25 (percent)");
#endif
    Serial.println("start: stay quiet for 1.25 s (1 s settling + 0.25 s noise); profile frozen until stop/start.");
}

#if AUDIO_TEST_CAPTURE
static void dump_noise_snapshot() {
    if (state.load() != State::Idle || !noise_snapshot_ready.load()) {
        Serial.println("Rejected: stop a valid filtered session before noise_dump.");
        return;
    }
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(noise_snapshot);
    const size_t size = DSP_NOISE_SAMPLES * sizeof(float);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        hash = (hash ^ bytes[i]) * 16777619u;
    }
    Serial.printf("NOISE_DATA fs=%d samples=%d bytes=%u fnv=%u settle_samples=%d\n",
                  DSP_FS, DSP_NOISE_SAMPLES, static_cast<unsigned>(size), hash, AUDIO_SETTLE_SAMPLES);
    Serial.print("NOISE_PN");
    for (int band = 0; band < DSP_NBAND; ++band) {
        Serial.printf(" %.9e", noise_powers[band]);
    }
    Serial.println();
    size_t offset = 0;
    const uint32_t began = millis();
    while (offset < size && millis() - began < 5000) {
        int available = Serial.availableForWrite();
        if (available > 0) {
            size_t count = size - offset;
            if (count > 64) count = 64;
            if (count > static_cast<size_t>(available)) count = available;
            offset += Serial.write(bytes + offset, count);
        }
        delay(1);
    }
    Serial.println(offset == size ? "NOISE_END state=IDLE" : "NOISE_ERROR dump_timeout");
}
#endif

static void print_status() {
    Serial.printf("state=%s fault=%s mode=%s volume=%u%% blocks=%u dsp=%u max=%u ms "
                  "underrun=%u overrun=%u dma=%u clip=%u peak_ppm=%u\n",
                  state_names[static_cast<int>(state.load())], fault_names[static_cast<int>(fault.load())],
                  raw_mode.load() ? "raw" : "coif5_L5", volume_percent.load(), processed.load(),
                  dsp_ms.load(), dsp_max_ms.load(), underruns.load(), overruns.load(),
                  dma_errors.load(), clipped.load(), rx_peak_ppm.load());
    Serial.printf("stack_free_bytes io=%u dsp=%u; heartbeat_age_ms io=%u dsp=%u\n",
                  io_handle ? uxTaskGetStackHighWaterMark(io_handle) : 0,
                  dsp_handle ? uxTaskGetStackHighWaterMark(dsp_handle) : 0,
                  millis() - io_heartbeat.load(), millis() - dsp_heartbeat.load());
}

#if AUDIO_TEST_CAPTURE
static void dump_filter_trace() {
    const uint32_t count = trace_frames.load();
    if (state.load() != State::Idle || count != TRACE_FRAMES) {
        Serial.println("Rejected: stop a filtered session with eight complete trace hops first.");
        return;
    }
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(filter_trace);
    const uint32_t size = count * 2 * sizeof(int16_t);
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    Serial.printf("FILTER_DATA fs=%d frames=%u bytes=%u fnv=%u volume=%u gain=8\n", DSP_FS, count, size, hash, trace_volume);
    uint32_t offset = 0;
    const uint32_t began = millis();
    while (offset < size && millis() - began < 60000) {
        int available = Serial.availableForWrite();
        if (available > 0) {
            uint32_t n = size - offset;
            if (n > 64) n = 64;
            if (n > static_cast<uint32_t>(available)) n = available;
            offset += Serial.write(bytes + offset, n);
        }
        delay(1);
    }
    Serial.println(offset == size ? "FILTER_END state=IDLE" : "FILTER_ERROR timeout");
}
#endif

void audio_pipeline_command(const char *command) {
    if (!strcmp(command, "stop")) {
        stop_requested.store(true);
        mute(true);
    } else if (!strcmp(command, "status")) {
        print_status();
#if AUDIO_TEST_CAPTURE
    } else if (!strcmp(command, "noise_dump")) {
        dump_noise_snapshot();
    } else if (!strcmp(command, "filter_dump")) {
        dump_filter_trace();
#endif
    } else if (!strncmp(command, "volume ", 7)) {
        char *end;
        unsigned long percent = strtoul(command + 7, &end, 10);
        if (end == command + 7 || *end || percent > static_cast<unsigned>(AUDIO_MAX_VOLUME * 100.0f)) {
            Serial.println("Rejected: volume must be an integer 0..25.");
        } else {
            volume_percent.store(percent);
        }
    } else if (!strcmp(command, "start") || !strcmp(command, "raw")) {
        State current = state.load();
        if (!ready || (current != State::Idle && current != State::Fault)) {
            Serial.println("Rejected: busy or restart required.");
            return;
        }
        stop_requested.store(false);
        state.store(State::Starting);
        Command next = !strcmp(command, "raw") ? Command::Raw : Command::Start;
        if (xQueueSend(commands, &next, 0) != pdTRUE) {
            state.store(current);
            Serial.println("Rejected: command pending.");
        }
    } else {
        Serial.println("Unknown command. Use start/raw/stop/status/volume 0..25.");
    }
}

void audio_pipeline_poll() {
    static uint32_t last_report;
    float profile[DSP_NBAND];
    if (profiles && xQueueReceive(profiles, profile, 0) == pdTRUE) {
        Serial.print("Pn frozen:");
        for (int index = 0; index < DSP_NBAND; ++index) {
            Serial.printf(" %.6e", profile[index]);
        }
        Serial.println();
    }
    State current = state.load();
    if ((current == State::Running || current == State::Calibrating || current == State::Buffering) &&
        millis() - io_heartbeat.load() > 1000) {
        fail(Fault::Read);
    }
    if (fault.load() != Fault::None) {
        mute(true);
    }
    if (millis() - last_report >= 2000) {
        last_report = millis();
        print_status();
    }
}


// ==================== main.cpp ====================
#include <Arduino.h>
#include "WiFi.h"
#include "esp_bt.h"

void setup();
void loop();



void setup() {
    Serial.begin(115200);
    delay(400);
    setCpuFrequencyMhz(240);
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    btStop();

    Serial.println("==== WaveletDSP wired audio ====");
    Serial.printf("DSP config: coif5 L%d, Fs=%d Hz, block=%d, float32\n",
                  DSP_LEVEL, DSP_FS, DSP_BLOCK);
    Serial.printf("GPIO mic WS=%d SCK=%d SD=%d; amp LRC=%d BCLK=%d DIN=%d mute=%d\n",
                  AUDIO_PIN_MIC_WS, AUDIO_PIN_MIC_SCK, AUDIO_PIN_MIC_SD,
                  AUDIO_PIN_AMP_LRC, AUDIO_PIN_AMP_BCLK, AUDIO_PIN_AMP_DIN, AUDIO_PIN_AMP_MUTE);
    Serial.printf("Amp external_NPN_mute=%d; %s\n", AUDIO_EXTERNAL_NPN_MUTE,
                  AUDIO_EXTERNAL_NPN_MUTE ? "GPIO10 active HIGH shutdown" :
                  "SD wired to 3V3; GPIO10 unused; stop uses PCM silence/I2S shutdown");
    audio_pipeline_init();
    Serial.printf("Raw/filtered playback gain=%.0fx, limiter before volume; Wiener algorithm unchanged.\n",
                  AUDIO_RAW_PLAYBACK_GAIN);
#if AUDIO_AUTOSTART
    Serial.println("Autostart: stay quiet during mic settling and noise calibration.");
    audio_pipeline_command("start");
#endif
}

void loop() {
    static char command[48];
    static size_t length;
    static bool overflow;
    for (int index = 0; index < 64 && Serial.available(); ++index) {
        char character = static_cast<char>(Serial.read());
        if (character == '\n' || character == '\r') {
            if (length && !overflow) {
                command[length] = '\0';
                audio_pipeline_command(command);
            }
            length = 0;
            overflow = false;
        } else if (length + 1 < sizeof(command) && !overflow) {
            command[length++] = character;
        } else {
            overflow = true;
        }
    }
    audio_pipeline_poll();
    delay(10);
}
