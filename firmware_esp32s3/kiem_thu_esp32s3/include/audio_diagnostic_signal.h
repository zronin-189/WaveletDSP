#ifndef AUDIO_DIAGNOSTIC_SIGNAL_H
#define AUDIO_DIAGNOSTIC_SIGNAL_H

#include <math.h>
#include <string.h>
#include "audio_config.h"

#define DIAGNOSTIC_TONE_SAMPLES (2 * DSP_FS)
#define DIAGNOSTIC_FADE_SAMPLES (DSP_FS / 100)
#define DIAGNOSTIC_TOTAL_SAMPLES (4 * DSP_FS)

static inline int diagnostic_tone_percent(const char *command) {
    if (!command) {
        return 0;
    }
    if (!strcmp(command, "tone 1")) {
        return 1;
    }
    if (!strcmp(command, "tone 3")) {
        return 3;
    }
    if (!strcmp(command, "tone 5")) {
        return 5;
    }
    if (!strcmp(command, "tone 15")) {
        return 15;
    }
    if (!strcmp(command, "tone 25")) {
        return 25;
    }
    return 0;
}

static inline float diagnostic_tone_sample(int sample_index, int percent) {
    static const float sine[16] = {
        0.0f, 0.382683432f, 0.707106781f, 0.923879533f,
        1.0f, 0.923879533f, 0.707106781f, 0.382683432f,
        0.0f, -0.382683432f, -0.707106781f, -0.923879533f,
        -1.0f, -0.923879533f, -0.707106781f, -0.382683432f
    };
    if (sample_index < 0 || sample_index >= DIAGNOSTIC_TONE_SAMPLES ||
        (percent != 1 && percent != 3 && percent != 5 && percent != 15 && percent != 25)) {
        return 0.0f;
    }
    int edge = sample_index;
    int remaining = DIAGNOSTIC_TONE_SAMPLES - 1 - sample_index;
    if (remaining < edge) {
        edge = remaining;
    }
    float envelope = edge < DIAGNOSTIC_FADE_SAMPLES ?
        (float)edge / DIAGNOSTIC_FADE_SAMPLES : 1.0f;
    return percent * 0.01f * envelope * sine[sample_index % 16];
}

#endif
