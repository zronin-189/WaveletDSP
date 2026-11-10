"""Capture raw stereo32 after RX stops; local files only, no speaker output."""
import argparse
import array
import json
import math
from pathlib import Path
import struct
import sys
import time
import wave
from result_paths import capture_paths

FS = 16000
FRAMES = 4 * FS
SIZE = FRAMES * 8


def fnv(data):
    value = 2166136261
    for byte in data:
        value = ((value ^ byte) * 16777619) & 0xffffffff
    return value


def decode(data):
    words = array.array("i")
    words.frombytes(data)
    assert words.itemsize == 4
    if sys.byteorder != "little":
        words.byteswap()
    return [[word >> 8 for word in words[slot::2]] for slot in (0, 1)]


def stats(values):
    dc = sum(values) / len(values)
    rms = math.sqrt(sum(v * v for v in values) / len(values))
    return dict(count=len(values), minimum=min(values), maximum=max(values),
                dc=dc, rms=rms, ac_rms=math.sqrt(max(0, rms * rms - dc * dc)),
                negative=sum(v < 0 for v in values), zeros=values.count(0),
                clipped=sum(v <= -8388608 or v >= 8388607 for v in values))


def wav(path, values):
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(FS)
        output.writeframes(struct.pack("<" + "h" * len(values), *values))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", type=Path)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--wait-for-enter", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        fixture = struct.pack("<iiii", -2147483648, 0, 2147483392, -256)
        assert decode(fixture) == [[-8388608, 8388607], [0, -1]]
        assert fnv(b"hello") == 0x4f9f2cab
        assert stats([-2, 2])["dc"] == 0
        assert stats([-2, 2])["ac_rms"] == 2
        print("PASS signed stereo24 decode, FNV fixture, DC/AC statistics")
        return
    if not args.prefix:
        parser.error("--prefix required for capture")
    paths = capture_paths(args.prefix)
    if any(path.exists() for path in paths.values()):
        raise RuntimeError("Refusing to overwrite capture files")
    import serial
    from serial.tools import list_ports
    ports = [p for p in list_ports.comports()
             if (p.vid, p.pid, p.serial_number) == (0x1a86, 0x55d3, "5CE5226325")]
    if len(ports) != 1:
        raise RuntimeError("Expected board not uniquely identified")
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.2,
                               write_timeout=1, exclusive=True)
    connection.dtr = connection.rts = False
    connection.port = ports[0].device
    connection.open()
    lines = []

    def send(command):
        lines.append("HOST " + command)
        print(lines[-1], flush=True)
        connection.write((command + "\n").encode("ascii"))

    def until(prefix, seconds):
        deadline = time.monotonic() + seconds
        pending = bytearray()
        while time.monotonic() < deadline:
            pending.extend(connection.read(1))
            if len(pending) > 2048:
                raise RuntimeError("Unexpected text framing")
            if pending.endswith(b"\n"):
                line = pending.decode("utf8", "replace").strip()
                pending.clear()
                lines.append(line)
                print(line, flush=True)
                if line.startswith(prefix):
                    return line
                if line.startswith("ERROR"):
                    raise RuntimeError(line)
        raise RuntimeError("Timeout waiting for " + prefix)

    try:
        # Opening this USB UART may reset the board. Boot must remain IDLE.
        time.sleep(2)
        send("status")
        until("MIC_STANDALONE state=IDLE", 3)
        if args.wait_for_enter:
            input("READY_FOR_CAPTURE: press Enter to begin\n")
        capture_started = time.monotonic()
        send("capture")
        ready = until("CAPTURE_READY ", 8)
        capture_elapsed = time.monotonic() - capture_started
        fields = dict(field.split("=", 1) for field in ready.split()[1:])
        assert int(fields["frames"]) == FRAMES and int(fields["bytes"]) == SIZE
        if not any(line == "DONE mic reason=complete frames=64000 error=0 cleanup=0 state=IDLE" for line in lines):
            raise RuntimeError("RX did not complete cleanly")
        send("dump")
        header = until("DATA ", 2)
        expected_header = f"DATA bytes={SIZE} fnv={fields['fnv']}"
        if header != expected_header:
            raise RuntimeError("Unexpected data header")
        data = bytearray()
        deadline = time.monotonic() + 55
        while len(data) < SIZE and time.monotonic() < deadline:
            data.extend(connection.read(min(4096, SIZE - len(data))))
        if len(data) != SIZE or fnv(data) != int(fields["fnv"]):
            raise RuntimeError("Capture length/checksum mismatch")
        until("END state=IDLE", 2)
        paths[".raw"].write_bytes(data)
        channels = decode(data)
        report = {"fs": FS, "frames": FRAMES, "bytes": len(data), "fnv": fnv(data),
                  "command_to_ready_seconds": capture_elapsed}
        for slot, values in enumerate(channels):
            report[f"slot{slot}"] = stats(values)
            report[f"slot{slot}"]["first_second"] = stats(values[:FS])
            report[f"slot{slot}"]["last_three_seconds"] = stats(values[FS:])
            wav(paths[f"_slot{slot}_pcm.wav"], [v >> 8 for v in values])
        dc = report["slot0"]["dc"]
        peak = max(abs(v - dc) for v in channels[0])
        gain = 0.8 * 32767 / peak if peak else 0
        wav(paths["_slot0_listen.wav"], [max(-32768, min(32767, round((v - dc) * gain))) for v in channels[0]])
        report["listen_file_processing"] = dict(dc_removed_pcm24=dc, gain_pcm24_to_pcm16=gain,
                                                 note="Diagnostic normalization only; PCM originals preserved")
        paths[".json"].write_text(json.dumps(report, indent=2) + "\n")
        send("status")
        until("MIC_STANDALONE state=IDLE", 2)
        print("PASS byte count/checksum/IDLE; listening and waveform validity remain to verify", flush=True)
    finally:
        try:
            send("stop")
            until("DONE mic ", 2)
        finally:
            connection.close()
            paths[".log"].write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
