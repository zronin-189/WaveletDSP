#include "audio_config.h"
#include "audio_stream.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float source_sample(int timestamp) {
    return timestamp % 997 == 0 ? 0.5f : (float)(timestamp % 2000 - 1000) / 4000.0f;
}

int main(void) {
    float *frame = calloc(DSP_BLOCK, sizeof(float));
    float *output = calloc(DSP_BLOCK, sizeof(float));
    float *tail = calloc(DSP_OVERLAP, sizeof(float));
    DspWorkspace *workspace = calloc(1, sizeof(DspWorkspace));
    float powers[DSP_NBAND] = {0};
    bool has_tail = false;
    bool clipped;
    float max_error = 0.0f;
    assert(frame && output && tail && workspace);

    assert(audio_decode_mic(INT32_MIN) == -1.0f);
    assert(fabsf(audio_decode_mic(INT32_MAX) - 1.0f) < 2.0e-7f);
    assert(audio_decode_mic(0x40000000) == 0.5f);
    assert(audio_decode_mic(-1073741824) == -0.5f);
    assert(audio_encode_pcm(1.0f, 1.0f, &clipped) == 32767 && !clipped);
    assert(audio_encode_pcm(-1.0f, 1.0f, &clipped) == -32768 && !clipped);
    assert(audio_encode_pcm(2.0f, 1.0f, &clipped) == 32767 && clipped);
    assert(audio_encode_pcm(NAN, 1.0f, &clipped) == 0 && clipped);
    assert(audio_encode_pcm(INFINITY, 1.0f, &clipped) == 0 && clipped);
    assert(audio_encode_pcm(0.5f, AUDIO_DEFAULT_VOLUME, &clipped) == 819);
    assert(AUDIO_RAW_PLAYBACK_GAIN == 8.0f);
    assert(AUDIO_DELAY_SAMPLES >= DSP_BLOCK);
    assert(AUDIO_RAW_SETTLE_SAMPLES == DSP_FS);
    assert(AUDIO_SETTLE_SAMPLES == DSP_FS);
    assert(audio_encode_raw_pcm(1.0f / 256.0f, 0.2f, &clipped) == 205 && !clipped);
    assert(audio_encode_raw_pcm(-1.0f / 256.0f, 0.2f, &clipped) == -205 && !clipped);
    assert(audio_encode_raw_pcm(1.0f, 0.2f, &clipped) == 6553 && clipped);
    assert(audio_encode_raw_pcm(-1.0f, 0.2f, &clipped) == -6553 && clipped);
    assert(audio_encode_raw_pcm(1.0f, 0.0f, &clipped) == 0);
    assert(audio_encode_raw_pcm(NAN, 0.2f, &clipped) == 0 && clipped);
    assert(audio_encode_raw_pcm(INFINITY, 0.2f, &clipped) == 0 && clipped);

    assert(dsp_valid_length(DSP_BLOCK));
    assert(dsp_valid_length(DSP_NOISE_SAMPLES));
    assert(!dsp_valid_length(DSP_BLOCK + 32));
    assert(!dsp_valid_length(DSP_BLOCK - 1));
    assert(!dsp_valid_length(0));
    assert(!dsp_valid_length(-32));
    assert(!dsp_valid_length(INT_MAX));

    assert(!audio_noise_valid(frame, DSP_NOISE_SAMPLES, powers));
    for (int index = 0; index < DSP_BLOCK; ++index) {
        frame[index] = source_sample(index) * 0.1f;
    }
    estimate_pn_frozen(frame, DSP_BLOCK, DSP_NOISE_SAMPLES, powers, workspace);
    assert(audio_noise_valid(frame, DSP_NOISE_SAMPLES, powers));
    float frozen_reference[DSP_NBAND];
    memcpy(frozen_reference, powers, sizeof(powers));
    frame[DSP_NOISE_SAMPLES + 10] = NAN;
    estimate_pn_frozen(frame, DSP_BLOCK, DSP_NOISE_SAMPLES, powers, workspace);
    assert(memcmp(frozen_reference, powers, sizeof(powers)) == 0);
    frame[DSP_NOISE_SAMPLES + 10] = source_sample(DSP_NOISE_SAMPLES + 10) * 0.1f;
    frame[0] = 1.0f;
    assert(!audio_noise_valid(frame, DSP_NOISE_SAMPLES, powers));
    frame[0] = NAN;
    assert(!audio_noise_valid(frame, DSP_NOISE_SAMPLES, powers));
    frame[0] = 0.01f;
    powers[0] = INFINITY;
    assert(!audio_noise_valid(frame, DSP_NOISE_SAMPLES, powers));
    estimate_pn_frozen(frame, DSP_BLOCK, 1, powers, workspace);
    assert(isnan(powers[0]));
    memset(output, 0x7f, DSP_BLOCK * sizeof(float));
    denoise_block_l5(frame, DSP_BLOCK, powers, output, workspace);
    for (int index = 0; index < DSP_BLOCK; ++index) {
        assert(output[index] == 0.0f);
    }
    memset(powers, 0, sizeof(powers));
    memset(frame, 0, DSP_BLOCK * sizeof(float));
    denoise_block_l5(frame, DSP_BLOCK, powers, output, workspace);
    for (int index = 0; index < DSP_BLOCK; ++index) {
        assert(output[index] == 0.0f);
    }
    frame[2] = NAN;
    denoise_block_l5(frame, DSP_BLOCK, powers, output, workspace);
    assert(output[2] == 0.0f);
    output[0] = 1.0f;
    denoise_block_l5(frame, 1, powers, output, workspace);
    assert(output[0] == 0.0f);

    int emitted = 0;
    for (int block = 0; block < 12; ++block) {
        for (int index = 0; index < DSP_BLOCK; ++index) {
            frame[index] = source_sample(block * DSP_HOP + index);
        }
        assert(audio_join_frame(tail, &has_tail, frame, DSP_BLOCK));
        for (int index = 0; index < DSP_HOP; ++index) {
            float error = fabsf(frame[index] - source_sample(emitted++));
            max_error = fmaxf(max_error, error);
            assert(error < 1.0e-7f);
        }
    }
    assert(emitted == 12 * DSP_HOP);
    assert(!audio_join_frame(tail, &has_tail, frame, DSP_BLOCK - 1));
    has_tail = true;
    for (int index = 0; index < DSP_OVERLAP; ++index) {
        tail[index] = 1.0f;
    }
    memset(frame, 0, DSP_BLOCK * sizeof(float));
    assert(audio_join_frame(tail, &has_tail, frame, DSP_BLOCK));
    assert(frame[0] == 1.0f && frame[DSP_OVERLAP - 1] == 0.0f);
    for (int index = 1; index < DSP_OVERLAP; ++index) {
        assert(frame[index] <= frame[index - 1]);
    }

    printf("PASS PCM, calibration rejection, DSP bounds/silence, overlap: %d samples maxerr=%.3e\n",
           emitted, max_error);
    printf("sizeof workspace=%zu PSRAM payload=%zu DMA payload=%zu task stacks=%d bytes\n",
           sizeof(DspWorkspace), sizeof(DspWorkspace) + (DSP_OVERLAP + DSP_NOISE_SAMPLES) * sizeof(float) +
           (AUDIO_INPUT_SLOTS + AUDIO_OUTPUT_SLOTS) * DSP_BLOCK * sizeof(float),
           2 * AUDIO_DMA_COUNT * AUDIO_CHUNK * 2 * sizeof(int32_t), 2 * 6144);
    printf("hop=%d samples %.6f s, overlap=%.6f s, nominal delay=%d samples %.6f s, BCLK=%d Hz\n",
           DSP_HOP, (double)DSP_HOP / DSP_FS, (double)DSP_OVERLAP / DSP_FS,
           AUDIO_DELAY_SAMPLES, (double)AUDIO_DELAY_SAMPLES / DSP_FS, DSP_FS * 64);
    free(frame);
    free(output);
    free(tail);
    free(workspace);
    return 0;
}
