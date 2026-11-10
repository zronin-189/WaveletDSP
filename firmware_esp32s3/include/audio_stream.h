#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include <stdbool.h>
#include <stdint.h>
#include "wavelet_dsp.h"

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
