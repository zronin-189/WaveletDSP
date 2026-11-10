#include "audio_stream.h"
#include "audio_config.h"

#include <math.h>
#include <string.h>

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
