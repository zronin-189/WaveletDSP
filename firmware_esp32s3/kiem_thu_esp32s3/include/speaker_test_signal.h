#ifndef SPEAKER_TEST_SIGNAL_H
#define SPEAKER_TEST_SIGNAL_H

#include <math.h>
#include <stdint.h>

enum
{
    SPEAKER_FS = 44100,
    SPEAKER_TONE_FRAMES = 2 * SPEAKER_FS,
    SPEAKER_TOTAL_FRAMES = 4 * SPEAKER_FS,
    SPEAKER_FADE_FRAMES = SPEAKER_FS / 100
};

static inline int16_t speaker_test_sample(uint32_t index)
{
    if (index >= SPEAKER_TONE_FRAMES) return 0;
    uint32_t edge = index;
    uint32_t remaining = SPEAKER_TONE_FRAMES - 1 - index;
    if (remaining < edge) edge = remaining;
    float envelope = edge < SPEAKER_FADE_FRAMES ? (float)edge / SPEAKER_FADE_FRAMES : 1.0f;
    // Ten complete 1kHz cycles repeat every 441 samples at 44.1kHz.
    float phase = 6.283185307179586f * 10.0f * (index % 441) / 441.0f;
    return (int16_t)lroundf(32767.0f * 0.25f * envelope * sinf(phase));
}

#endif
