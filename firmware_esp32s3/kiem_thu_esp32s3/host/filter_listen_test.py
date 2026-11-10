"""One bounded filtered trial and paired digital PCM download after stopping."""
import argparse
import json
import math
import struct
import time
import serial
from serial.tools import list_ports
from mic_capture import fnv, wav
from result_paths import RESULTS, DATA


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", default="loc_buoc3", help="Unique output filename stem")
    name = parser.parse_args().name
    if not name or any(c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-" for c in name):
        raise ValueError("Name must contain only ASCII letters, digits, underscores or hyphens")
    log = DATA / (name + ".log")
    metadata = DATA / (name + ".json")
    binary = DATA / (name + ".pcm")
    before = RESULTS / (name + "_truoc.wav")
    after = RESULTS / (name + "_sau.wav")
    if any(p.exists() for p in (log, metadata, binary, before, after)):
        raise RuntimeError("Refusing to overwrite trial")
    DATA.mkdir(parents=True, exist_ok=True)
    ports = [p for p in list_ports.comports()
             if (p.vid, p.pid, p.serial_number) == (0x1a86, 0x55d3, "5CE5226325")]
    if len(ports) != 1:
        raise RuntimeError("Expected USB board not uniquely identified")
    con = serial.Serial(port=None, baudrate=115200, timeout=0.1, write_timeout=1, exclusive=True)
    con.dtr = con.rts = False
    con.port = ports[0].device
    con.open()
    lines, states = [], []
    summary = {"error": None, "listening": "pending_user"}
    pending = bytearray()

    def send(command):
        lines.append("HOST " + command)
        print(lines[-1], flush=True)
        con.write((command + "\n").encode())

    def read_line(deadline):
        while time.monotonic() < deadline:
            pending.extend(con.read(1))
            if len(pending) > 2048:
                raise RuntimeError("Bad serial framing")
            if pending.endswith(b"\n"):
                line = pending.decode("utf8", "replace").strip()
                pending.clear()
                lines.append(line)
                print(line, flush=True)
                if line.startswith("state="):
                    states.append(dict(v.split("=", 1) for v in line.split() if "=" in v))
                return line
        return None

    def receive(seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            read_line(deadline)

    def until(prefix, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            line = read_line(deadline)
            if line and line.startswith(prefix):
                return line
        raise RuntimeError("Timeout " + prefix)

    def idle():
        send("status")
        receive(.5)
        if not states or any(states[-1].get(k) != v for k, v in
                             {"state": "IDLE", "fault": "none", "volume": "0%"}.items()):
            raise RuntimeError("Not clean IDLE/volume0")

    try:
        receive(3)
        send("stop")
        send("volume 0")
        receive(.5)
        idle()
        send("filter_dump")
        until("Rejected:", 2)
        input("READY_FILTER: Enter to begin20s; quiet first3s then speak\n")
        send("volume 20")
        send("status")
        receive(.5)
        if states[-1].get("volume") != "20%":
            raise RuntimeError("Volume not confirmed")
        marker = len(lines)
        send("start")
        began = time.monotonic()
        while time.monotonic() - began < 20:
            receive(.25)
            if states[-1].get("fault") != "none":
                raise RuntimeError("Firmware fault")
        send("stop")
        send("volume 0")
        receive(1)
        idle()
        frozen = [line for line in lines[marker:] if line.startswith("Pn frozen:")]
        if len(frozen) != 1:
            raise RuntimeError("Expected one frozen profile")
        powers = [float(v) for v in frozen[0].split(":")[1].split()]
        assert len(powers) == 6 and all(math.isfinite(v) and v >= 0 for v in powers)
        if not any(s.get("state") == "RUNNING" for s in states):
            raise RuntimeError("Never RUNNING")
        status = states[-1]
        if any(status.get(k) != "0" for k in ("dma", "underrun", "overrun")):
            raise RuntimeError("IO faults")
        summary.update(pn=powers, runtime_status=status, requested_volume=20)
        send("filter_dump")
        header = until("FILTER_DATA ", 2)
        fields = dict(v.split("=", 1) for v in header.split()[1:])
        frames = 8 * 13952
        assert all(int(fields[k]) == v for k, v in
                   {"fs": 16000, "frames": frames, "bytes": frames * 4, "volume": 20, "gain": 8}.items())
        data = bytearray()
        deadline = time.monotonic() + 55
        while len(data) < frames * 4 and time.monotonic() < deadline:
            data.extend(con.read(min(4096, frames * 4 - len(data))))
        if len(data) != frames * 4 or fnv(data) != int(fields["fnv"]):
            raise RuntimeError("Trace length/checksum failed")
        until("FILTER_END state=IDLE", 3)
        binary.write_bytes(data)
        words = struct.unpack("<" + str(frames * 2) + "h", data)
        for slot, path in enumerate((before, after)):
            values = words[slot::2]
            assert max(abs(v) for v in values) <= 6553
            wav(path, values)
            summary[path.stem] = {"frames": len(values), "peak_pcm16": max(abs(v) for v in values),
                                  "rms_pcm16": math.sqrt(sum(v*v for v in values)/len(values)),
                                  "at_limiter_cap": sum(abs(v) == 6553 for v in values)}
        summary.update(fs=16000, frames=frames, seconds=frames/16000, fnv=fnv(data),
                       source="Actual paired raw/filtered PCM rendered on ESP32 from same mic frames",
                       processing="Both gain8/volume20, no normalization; filtered after overlap join; initial playback fade and scheduling delay omitted; not acoustic recording")
        idle()
        print("PASS paired trace/checksum/WAV; listening pending", flush=True)
    except Exception as error:
        summary["error"] = str(error)
        raise
    finally:
        try:
            send("stop")
            send("volume 0")
            receive(.5)
            summary["last_status"] = states[-1] if states else None
        finally:
            con.close()
            log.write_text("\n".join(lines) + "\n")
            metadata.write_text(json.dumps(summary, indent=2) + "\n")


if __name__ == "__main__":
    main()
