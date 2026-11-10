#include "speaker_test_signal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int peak = 0, previous = 0, crossings = 0, max_step = 0;
    double sum = 0;
    for (uint32_t i = 0; i < SPEAKER_TOTAL_FRAMES; ++i)
    {
        int sample = speaker_test_sample(i);
        assert(abs(sample) <= 8192);
        if (i >= SPEAKER_TONE_FRAMES) assert(sample == 0);
        if (abs(sample) > peak) peak = abs(sample);
        if (previous <= 0 && sample > 0) ++crossings;
        if (abs(sample - previous) > max_step) max_step = abs(sample - previous);
        previous = sample;
        sum += sample;
    }
    assert(SPEAKER_TONE_FRAMES == 88200 && SPEAKER_TOTAL_FRAMES == 176400);
    assert(speaker_test_sample(0) == 0);
    assert(speaker_test_sample(SPEAKER_TONE_FRAMES - 1) == 0);
    assert(speaker_test_sample(UINT32_MAX) == 0);
    assert(crossings == 2000 && peak >= 8190);
    assert(max_step < 1200);
    assert(fabs(sum / SPEAKER_TONE_FRAMES) < 1);
    printf("PASS fs=%d tone_frames=%d total_frames=%d cycles=%d peak=%d max_step=%d\n",
           SPEAKER_FS, SPEAKER_TONE_FRAMES, SPEAKER_TOTAL_FRAMES, crossings, peak, max_step);
    return 0;
}
