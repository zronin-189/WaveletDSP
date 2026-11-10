import argparse
import json
import re
import time
from pathlib import Path
from result_paths import log_path

import serial
from serial.tools import list_ports


PORT = "/dev/serial/by-id/usb-1a86_USB_Single_Serial_5CE5226325-if00"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=["raw", "start"])
    parser.add_argument("--seconds", type=int, default=20)
    parser.add_argument("--volume", type=int, default=0)
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    args.log = log_path(args.log)
    if args.log.exists():
        raise RuntimeError("Refusing to overwrite an existing trial log")
    if not 5 <= args.seconds <= 300 or not 0 <= args.volume <= 20:
        parser.error("seconds must be 5..300 and volume must be 0..20")
    matches = [port for port in list_ports.comports()
               if port.vid == 0x1A86 and port.pid == 0x55D3
               and port.serial_number == "5CE5226325"]
    if len(matches) != 1 or Path(PORT).resolve() != Path(matches[0].device).resolve():
        raise RuntimeError("Expected board serial identity not found uniquely")
    states = []
    lines = []
    pending = ""
    started = None
    stopped = False
    active_seen = False
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.2,
                               write_timeout=1, exclusive=True)
    connection.dtr = False
    connection.rts = False
    connection.port = PORT
    connection.open()

    def send(command):
        connection.write((command + "\n").encode("ascii"))
        connection.flush()
        lines.append(f"HOST command={command}")

    def receive(seconds):
        nonlocal pending
        until = time.monotonic() + seconds
        received = []
        while time.monotonic() < until:
            pending += connection.read(4096).decode("utf8", "replace")
            while "\n" in pending:
                line, pending = pending.split("\n", 1)
                line = line.rstrip("\r")
                lines.append(f"{time.monotonic():.3f} {line}")
                if line.startswith("state="):
                    fields = dict(re.findall(r"(\w+)=([^ ]+)", line))
                    states.append(fields)
                    received.append(fields)
                    print(line, flush=True)
                elif "Pn frozen" in line or "Error" in line or "assert" in line:
                    print(line, flush=True)
        return received

    failure = None
    try:
        receive(3)
        send("stop")
        send("volume 0")
        receive(1)
        send("status")
        baseline = receive(1)
        if not baseline or baseline[-1].get("state") != "IDLE" or baseline[-1].get("volume") != "0%":
            raise RuntimeError("Board did not confirm IDLE at volume0")
        if baseline[-1].get("fault") != "none":
            raise RuntimeError("Pre-existing fault requires investigation")
        send(f"volume {args.volume}")
        send("status")
        volume_status = receive(1)
        if not volume_status or volume_status[-1].get("volume") != f"{args.volume}%":
            raise RuntimeError("Volume command not acknowledged")
        started = time.monotonic()
        send(args.mode)
        while time.monotonic() - started < args.seconds:
            recent = receive(1)
            if any(status.get("state") in ("CALIBRATING", "BUFFERING", "RUNNING")
                   and status.get("mode") == ("raw" if args.mode == "raw" else "coif5_L5")
                   for status in recent):
                active_seen = True
            if any(status.get("fault") != "none" for status in recent):
                raise RuntimeError("Firmware reported fault; stopping immediately")
            if recent and recent[-1].get("state") == "IDLE" and (
                    active_seen or time.monotonic() - started > 4):
                raise RuntimeError("Unexpected return to IDLE during test")
        send("status")
        receive(1)
    except Exception as error:
        failure = str(error)
    finally:
        try:
            send("stop")
            send("volume 0")
            receive(3)
            send("status")
            final = receive(1)
            stopped = bool(final and final[-1].get("state") in ("IDLE", "FAULT")
                           and final[-1].get("volume") == "0%")
        finally:
            connection.close()
            args.log.write_text("\n".join(lines) + "\n", encoding="utf8")
    active = [status for status in states if status.get("state") == "RUNNING"]
    summary = {"mode": args.mode, "requested_seconds": args.seconds,
               "volume_percent": args.volume, "running_snapshots": len(active),
               "safe_stop_confirmed": stopped, "error": failure,
               "last_status": states[-1] if states else None}
    args.log.with_suffix(".json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    return 1 if failure or not active or not stopped else 0


if __name__ == "__main__":
    raise SystemExit(main())
