#include "audio_diagnostic_signal.h"
#include "audio_stream.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    const int levels[] = {1, 3, 5, 15, 25};
    assert(diagnostic_tone_percent("tone 1") == 1);
    assert(diagnostic_tone_percent("tone 3") == 3);
    assert(diagnostic_tone_percent("tone 5") == 5);
    assert(diagnostic_tone_percent("tone 15") == 15);
    assert(diagnostic_tone_percent("tone 25") == 25);
    assert(diagnostic_tone_percent("tone 26") == 0);
    assert(diagnostic_tone_percent("tone 16") == 0);
    assert(diagnostic_tone_percent("tone 30") == 0);
    assert(diagnostic_tone_percent("tone 1 junk") == 0);
    assert(diagnostic_tone_percent("tone nan") == 0);
    assert(diagnostic_tone_percent("tone -1") == 0);
    assert(diagnostic_tone_percent(NULL) == 0);
    for (unsigned level = 0; level < sizeof(levels) / sizeof(levels[0]); ++level) {
        float amplitude = levels[level] * 0.01f;
        float peak = 0.0f;
        float previous = 0.0f;
        float max_step = 0.0f;
        double sum = 0.0;
        int crossings = 0;
        for (int sample_index = 0; sample_index < DIAGNOSTIC_TONE_SAMPLES; ++sample_index) {
            float sample = diagnostic_tone_sample(sample_index, levels[level]);
            bool clipped;
            int16_t pcm = audio_encode_pcm(sample, 1.0f, &clipped);
            assert(isfinite(sample) && fabsf(sample) <= amplitude + 1.0e-7f);
            assert(!clipped);
            assert(abs(pcm) <= (int)lroundf(amplitude * 32767.0f));
            peak = fmaxf(peak, fabsf(sample));
            max_step = fmaxf(max_step, fabsf(sample - previous));
            if (previous <= 0.0f && sample > 0.0f) {
                ++crossings;
            }
            previous = sample;
            sum += sample;
        }
        assert(crossings == 2000);
        assert(fabsf(peak - amplitude) < 1.0e-7f);
        assert(max_step < amplitude * 0.4f);
        assert(fabs(sum / DIAGNOSTIC_TONE_SAMPLES) < 1.0e-6);
        assert(diagnostic_tone_sample(0, levels[level]) == 0.0f);
        assert(diagnostic_tone_sample(DIAGNOSTIC_TONE_SAMPLES - 1, levels[level]) == 0.0f);
        printf("tone=%d%% frequency=1000Hz count=%d peak=%.6f max_step=%.6f PASS\n",
               levels[level], DIAGNOSTIC_TONE_SAMPLES, peak, max_step);
    }
    assert(diagnostic_tone_sample(-1, 1) == 0.0f);
    assert(diagnostic_tone_sample(DIAGNOSTIC_TONE_SAMPLES, 1) == 0.0f);
    assert(diagnostic_tone_sample(4, 30) == 0.0f);
    assert(DIAGNOSTIC_TOTAL_SAMPLES % AUDIO_CHUNK == 0);
    printf("PASS diagnostic signal/parser; total_frames=%d duration=%.6fs\n",
           DIAGNOSTIC_TOTAL_SAMPLES, (double)DIAGNOSTIC_TOTAL_SAMPLES / DSP_FS);
    return 0;
}
