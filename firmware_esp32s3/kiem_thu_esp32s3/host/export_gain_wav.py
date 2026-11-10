"""Compare old/new raw playback on one saved capture using the C PCM encoders."""
import ctypes
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import wave

from result_paths import RESULTS, DATA


def write_wav(path, samples):
    with wave.open(str(path), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(16000)
        out.writeframes(struct.pack("<" + "h" * len(samples), *samples))


def main():
    root = RESULTS.parent
    scratch = root / ".agent-tmp/gain-wav"
    scratch.mkdir(parents=True, exist_ok=True)
    DATA.mkdir(parents=True, exist_ok=True)
    source = DATA / "mic_capture_baseline.raw"
    data = source.read_bytes()
    assert len(data) == 512000
    words = struct.unpack("<128000i", data)[::2]
    paths = [RESULTS / name for name in ("gain_01_mic_goc.wav", "gain_02_truoc_16x_volume20.wav",
                                        "gain_03_sau_8x_volume20.wav")]
    report_path = DATA / "gain_wav_comparison.json"
    if any(p.exists() for p in paths + [report_path]):
        raise RuntimeError("Refusing to overwrite comparison outputs")
    libraries = {}
    for gain in (16, 8):
        target = scratch / f"pcm_gain{gain}.so"
        subprocess.run(["gcc", "-shared", "-fPIC", "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        f"-DAUDIO_RAW_PLAYBACK_GAIN={gain}.0f", "-I", str(root / "firmware_esp32s3/include"),
                        str(root / "firmware_esp32s3/src/audio_stream.c"), "-lm", "-o", str(target)], check=True)
        lib = ctypes.CDLL(str(target))
        lib.audio_decode_mic.argtypes = [ctypes.c_int32]
        lib.audio_decode_mic.restype = ctypes.c_float
        for name in ("audio_encode_pcm", "audio_encode_raw_pcm"):
            function = getattr(lib, name)
            function.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.POINTER(ctypes.c_bool)]
            function.restype = ctypes.c_int16
        libraries[gain] = lib
    limited = ctypes.c_bool()
    mic = [libraries[8].audio_decode_mic(word) for word in words]
    source_pcm = [libraries[8].audio_encode_pcm(sample, 1.0, ctypes.byref(limited)) for sample in mic]
    write_wav(paths[0], source_pcm)
    report = {"source": str(source.relative_to(root)), "source_sha256": hashlib.sha256(data).hexdigest(),
              "fs": 16000, "frames": len(mic), "volume_percent": 20,
              "origin": "Software rendering of the same previous microphone capture, not a new recording or acoustic speaker measurement",
              "normalization": "None; no DC removal or automatic gain",
              "alignment": "Same original timestamps, settling replaced by zeros; nominal 2s playback buffering omitted in both files",
              "files": [{"path": str(paths[0].relative_to(root)), "kind": "original microphone PCM16"}]}
    for gain, settling, path in ((16, 1600, paths[1]), (8, 16000, paths[2])):
        pcm = []
        count = 0
        for index, sample in enumerate(mic):
            if index < settling:
                pcm.append(0)
                continue
            ramp = min(1.0, (index - settling) / 160)
            value = libraries[gain].audio_encode_raw_pcm(sample, ramp * 0.2, ctypes.byref(limited))
            count += limited.value
            assert abs(value) <= 6553
            pcm.append(value)
        write_wav(path, pcm)
        with wave.open(str(path), "rb") as check:
            assert (check.getnframes(), check.getframerate(), check.getnchannels(), check.getsampwidth()) == (64000, 16000, 1, 2)
        report["files"].append({"path": str(path.relative_to(root)), "kind": "raw playback software comparison",
                                "gain": gain, "settling_seconds": settling / 16000,
                                "limited_samples": count, "peak_pcm16": max(abs(v) for v in pcm),
                                "rms_pcm16": math.sqrt(sum(v * v for v in pcm) / len(pcm))})
    report_path.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
