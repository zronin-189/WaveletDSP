#include "audio_stream.h"
#include <stdlib.h>

int noise_reference(const float *samples, float *powers) {
    DspWorkspace *workspace = calloc(1, sizeof(*workspace));
    if (!workspace) return -1;
    estimate_pn_frozen(samples, DSP_NOISE_SAMPLES, DSP_NOISE_SAMPLES, powers, workspace);
    int valid = audio_noise_valid(samples, DSP_NOISE_SAMPLES, powers);
    free(workspace);
    return valid ? 0 : -2;
}
