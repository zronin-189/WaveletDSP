"""Verify the actual frozen noise snapshot; never play a tone or raise volume."""
import argparse
import ctypes
import json
import math
from pathlib import Path
import re
import struct
import subprocess
import time

import serial
from serial.tools import list_ports
from mic_capture import fnv, wav
from result_paths import RESULTS, DATA


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--name", default="nhieu_buoc2")
    parser.add_argument("--wait-for-enter", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_-]+", args.name):
        parser.error("name must be a simple filename stem")
    DATA.mkdir(parents=True, exist_ok=True)
    paths = {suffix: DATA / (args.name + suffix) for suffix in (".log", ".json", ".f32")}
    paths["wav"] = RESULTS / (args.name + "_goc_025s.wav")
    paths["listen"] = RESULTS / (args.name + "_phong_dai_lap_2s.wav")
    paths["nonrepeat"] = RESULTS / (args.name + "_nghe_khong_lap.wav")
    if any(path.exists() for path in paths.values()):
        raise RuntimeError("Refusing to overwrite profile results")
    root = RESULTS.parent
    scratch = root / ".agent-tmp/noise-profile"
    scratch.mkdir(parents=True, exist_ok=True)
    library = scratch / "noise_reference.so"
    subprocess.run(["gcc", "-shared", "-fPIC", "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root / "firmware_esp32s3/include"),
                    str(Path(__file__).with_name("noise_reference.c")),
                    str(root / "firmware_esp32s3/src/wavelet_dsp.c"),
                    str(root / "firmware_esp32s3/src/audio_stream.c"), "-lm", "-o", str(library)], check=True)
    reference = ctypes.CDLL(str(library))
    reference.noise_reference.argtypes = [ctypes.POINTER(ctypes.c_float), ctypes.POINTER(ctypes.c_float)]
    reference.audio_encode_pcm.argtypes = [ctypes.c_float, ctypes.c_float, ctypes.POINTER(ctypes.c_bool)]
    reference.audio_encode_pcm.restype = ctypes.c_int16
    ports = [p for p in list_ports.comports()
             if (p.vid, p.pid, p.serial_number) == (0x1a86, 0x55d3, "5CE5226325")]
    if len(ports) != 1:
        raise RuntimeError("Expected USB board not uniquely identified")
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.1,
                               write_timeout=1, exclusive=True)
    connection.dtr = connection.rts = False
    connection.port = ports[0].device
    connection.open()
    lines, states = [], []
    pending = bytearray()
    summary = {"error": None, "listening_noise_only": "pending_user_confirmation"}

    def send(command):
        lines.append("HOST " + command)
        print(lines[-1], flush=True)
        connection.write((command + "\n").encode("ascii"))

    def line_until(deadline):
        while time.monotonic() < deadline:
            pending.extend(connection.read(1))
            if len(pending) > 2048:
                raise RuntimeError("Unexpected serial framing")
            if pending.endswith(b"\n"):
                line = pending.decode("utf8", "replace").strip()
                pending.clear()
                lines.append(line)
                print(line, flush=True)
                if line.startswith("state="):
                    states.append(dict(re.findall(r"(\w+)=([^ ]+)", line)))
                return line
        return None

    def receive(seconds):
        deadline = time.monotonic() + seconds
        result = []
        while time.monotonic() < deadline:
            line = line_until(deadline)
            if line is not None:
                result.append(line)
        return result

    def until(prefix, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            line = line_until(deadline)
            if line is not None and line.startswith(prefix):
                return line
        raise RuntimeError("Timeout waiting for " + prefix)

    def require_idle():
        send("status")
        receive(0.5)
        if not states or any(states[-1].get(k) != v for k, v in
                             {"state": "IDLE", "fault": "none", "volume": "0%"}.items()):
            raise RuntimeError("Not clean IDLE/volume0")

    try:
        receive(3)
        send("stop")
        send("volume 0")
        receive(0.5)
        require_idle()
        send("noise_dump")
        rejected = receive(0.5)
        if not any(line.startswith("Rejected:") for line in rejected):
            raise RuntimeError("Stale/uninitialized profile was not rejected")
        if args.wait_for_enter:
            input("READY_FOR_NOISE: press Enter for 8s muted filtered run; keep quiet\n")
        first = len(lines)
        send("start")
        began = time.monotonic()
        while time.monotonic() - began < 8:
            receive(0.25)
            if states[-1].get("fault") != "none":
                raise RuntimeError("Firmware fault during profile capture")
        frozen = [line for line in lines[first:] if line.startswith("Pn frozen:")]
        if len(frozen) != 1:
            raise RuntimeError("Expected exactly one frozen profile")
        running = [s for s in states if s.get("state") == "RUNNING" and s.get("mode") == "coif5_L5"]
        if not running or any(s.get(k) != "0" for s in running for k in ("dma", "underrun", "overrun", "clip")):
            raise RuntimeError("Filtered run did not stay clean")
        send("stop")
        send("volume 0")
        receive(1)
        require_idle()
        send("noise_dump")
        header = until("NOISE_DATA ", 2)
        fields = dict(field.split("=", 1) for field in header.split()[1:])
        for key, value in {"fs": 16000, "samples": 4000, "bytes": 16000, "settle_samples": 16000}.items():
            if int(fields[key]) != value:
                raise RuntimeError("Unexpected noise snapshot " + key)
        powers = [float(v) for v in until("NOISE_PN ", 2).split()[1:]]
        if len(powers) != 6 or not all(math.isfinite(v) and v >= 0 for v in powers):
            raise RuntimeError("Invalid band powers")
        payload = bytearray()
        deadline = time.monotonic() + 6
        while len(payload) < 16000 and time.monotonic() < deadline:
            payload.extend(connection.read(16000 - len(payload)))
        if len(payload) != 16000 or fnv(payload) != int(fields["fnv"]):
            raise RuntimeError("Snapshot length/checksum mismatch")
        until("NOISE_END state=IDLE", 2)
        paths[".f32"].write_bytes(payload)
        samples = struct.unpack("<4000f", payload)
        if not all(math.isfinite(v) and abs(v) < 0.99 for v in samples):
            raise RuntimeError("Noise input has invalid/clipped samples")
        input_array = (ctypes.c_float * 4000)(*samples)
        host_powers = (ctypes.c_float * 6)()
        if reference.noise_reference(input_array, host_powers) != 0:
            raise RuntimeError("Host rejected noise snapshot")
        errors = [abs(a - b) for a, b in zip(powers, host_powers)]
        if any(error > max(1e-12, abs(expected) * 5e-5) for error, expected in zip(errors, host_powers)):
            raise RuntimeError("Device/host Pn mismatch")
        displayed = [float(v) for v in frozen[0].split(":", 1)[1].split()]
        if len(displayed) != 6 or any(abs(a - b) > max(1e-12, abs(b) * 1e-5) for a, b in zip(displayed, powers)):
            raise RuntimeError("Profile changed before dump")
        limited = ctypes.c_bool()
        pcm = [reference.audio_encode_pcm(v, 1, ctypes.byref(limited)) for v in samples]
        wav(paths["wav"], pcm)
        peak = max(abs(v) for v in samples)
        gain = 0.5 / peak if peak else 0
        listen = [reference.audio_encode_pcm(v * gain, 1, ctypes.byref(limited)) for v in samples]
        wav(paths["listen"], listen * 8)
        dc = sum(samples) / len(samples)
        rms = math.sqrt(sum(v * v for v in samples) / len(samples))
        ac = [v - dc for v in samples]
        ac_peak = max(abs(v) for v in ac)
        ac_gain = 0.5 / ac_peak if ac_peak else 0
        nonrepeat = [reference.audio_encode_pcm(v * ac_gain * min(1, i / 160, (len(ac) - 1 - i) / 160),
                                                1, ctypes.byref(limited)) for i, v in enumerate(ac)]
        wav(paths["nonrepeat"], nonrepeat)
        summary.update(fs=16000, samples=4000, noise_seconds=0.25, settling_seconds=1,
                       bytes=len(payload), fnv=fnv(payload), source="Actual frozen noise snapshot from ESP32-S3",
                       band_order=["A5", "D5", "D4", "D3", "D2", "D1"], pn_device=powers,
                       pn_host=list(host_powers), pn_abs_error=errors, peak=peak, dc=dc, rms=rms,
                       ac_rms=math.sqrt(max(0, rms * rms - dc * dc)), frozen_profile_count=1,
                       first_running=running[0], last_running=running[-1],
                       listen_processing={"gain": gain, "repetitions": 8,
                                          "note": "Repeated preview can click every0.25s at loop boundaries; not evidence of periodic microphone noise"},
                       nonrepeated_listen={"dc_removed": dc, "gain": ac_gain, "fade_samples": 160,
                                           "note": "Nonrepeated0.25s listening aid only; original snapshot and Pn untouched"})
        require_idle()
        print("PASS actual noise snapshot, frozen Pn and host agreement; user noise-only confirmation pending", flush=True)
    except Exception as error:
        summary["error"] = str(error)
        raise
    finally:
        try:
            send("stop")
            send("volume 0")
            receive(0.5)
            summary["last_status"] = states[-1] if states else None
        finally:
            connection.close()
            paths[".log"].write_text("\n".join(lines) + "\n")
            paths[".json"].write_text(json.dumps(summary, indent=2) + "\n")


if __name__ == "__main__":
    main()
