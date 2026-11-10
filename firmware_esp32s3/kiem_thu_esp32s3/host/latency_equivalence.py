"""Check identical DSP/PCM for old/new scheduling on exactly the same input."""
import ctypes
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

from result_paths import RESULTS, DATA


def main():
    root = RESULTS.parent
    include = root / "firmware_esp32s3/include"
    scratch = root / ".agent-tmp/latency-equivalence"
    scratch.mkdir(parents=True, exist_ok=True)
    samples = [float(v) for v in (root / "firmware_esp32s3/kiem_thu_esp32s3/vectors/block0_input.csv").read_text().split()]
    assert len(samples) == 16000
    hashes = []
    for delay in (32000, 20000):
        folder = scratch / str(delay)
        folder.mkdir(exist_ok=True)
        config = (include / "audio_config.h").read_text().replace("#define AUDIO_DELAY_SAMPLES 20000", f"#define AUDIO_DELAY_SAMPLES {delay}")
        (folder / "audio_config.h").write_text(config)
        for file in ("audio_stream.c", "wavelet_dsp.c"):
            shutil.copyfile(root / "firmware_esp32s3/src" / file, folder / file)
        (folder / "compare.c").write_text('''#include "audio_config.h"
#include "audio_stream.h"
#include <stdlib.h>
int compare(const float *x, short *pcm) {
 DspWorkspace *w=calloc(1,sizeof(*w)); float *y=calloc(DSP_BLOCK,sizeof(float));
 float pn[DSP_NBAND]; if(!w||!y){free(w);free(y);return -1;}
 estimate_pn_frozen(x,DSP_BLOCK,DSP_NOISE_SAMPLES,pn,w);
 denoise_block_l5(x,DSP_BLOCK,pn,y,w);
 for(int i=0;i<DSP_BLOCK;i++){bool limited;pcm[i]=audio_encode_raw_pcm(y[i],0.2f,&limited);}
 free(w);free(y);return AUDIO_DELAY_SAMPLES;
}
''')
        target = folder / "compare.so"
        subprocess.run(["gcc", "-shared", "-fPIC", "-O2", "-std=c11", "-I", str(folder), "-I", str(include),
                        str(folder / "compare.c"), str(folder / "wavelet_dsp.c"), str(folder / "audio_stream.c"),
                        "-lm", "-o", str(target)], check=True)
        lib = ctypes.CDLL(str(target))
        lib.compare.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_int16)]
        source = (ctypes.c_float * 16000)(*samples)
        output = (ctypes.c_int16 * 16000)()
        assert lib.compare(source, output) == delay
        hashes.append(hashlib.sha256(bytes(output)).hexdigest())
    assert hashes[0] == hashes[1]
    report = {"old_delay_samples": 32000, "new_delay_samples": 20000, "pcm_samples": 16000,
              "identical_pcm": True, "sha256": hashes[0],
              "scope": "Identical input and frozen Pn DSP+PCM on host; no live scheduling or acoustic latency claim"}
    (DATA / "latency_pcm_equivalence.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
