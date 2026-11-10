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
