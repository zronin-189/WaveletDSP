import argparse
import json
import time
from pathlib import Path
from result_paths import log_path

import serial
from serial.tools import list_ports


PORT = "/dev/serial/by-id/usb-1a86_USB_Single_Serial_5CE5226325-if00"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--percent", type=int, choices=[1, 3, 5, 15, 25], default=1)
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    args.log = log_path(args.log)
    if args.log.exists():
        raise RuntimeError("Refusing to overwrite an existing trial log")
    ports = [port for port in list_ports.comports()
             if port.vid == 0x1A86 and port.pid == 0x55D3
             and port.serial_number == "5CE5226325"]
    if len(ports) != 1 or Path(PORT).resolve() != Path(ports[0].device).resolve():
        raise RuntimeError("Expected board identity not found uniquely")
    lines = []
    pending = ""
    error = None
    done = None
    stopped = False
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.1,
                               write_timeout=1, exclusive=True)
    connection.dtr = False
    connection.rts = False
    connection.port = PORT
    connection.open()

    def send(command):
        lines.append("HOST " + command)
        print("HOST " + command, flush=True)
        connection.write((command + "\n").encode("ascii"))
        connection.flush()

    def receive(seconds):
        nonlocal pending
        received = []
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            pending += connection.read(4096).decode("utf8", "replace")
            while "\n" in pending:
                line, pending = pending.split("\n", 1)
                line = line.rstrip("\r")
                lines.append(line)
                received.append(line)
                print(line, flush=True)
        return received

    def idle_confirmed(received):
        return any(line.startswith("DIAG state=IDLE ready=1 ") for line in received)

    try:
        boot = receive(3)
        if any(line.startswith(("ACK tone", "DONE tone")) for line in boot):
            raise RuntimeError("Unexpected tone activity before command")
        send("stop")
        receive(0.4)
        send("status")
        if not idle_confirmed(receive(1)):
            raise RuntimeError("Diagnostic firmware not IDLE/ready")
        send("tone 30")
        rejected = receive(0.5)
        if not any(line.startswith("REJECT use:") for line in rejected) or any(
                line.startswith("ACK tone") for line in rejected):
            raise RuntimeError("Out-of-range tone not safely rejected")
        send(f"tone {args.percent}")
        response = receive(7)
        if not any(line.startswith(f"ACK tone percent={args.percent} ") for line in response):
            raise RuntimeError("Tone command not acknowledged")
        done_lines = [line for line in response if line.startswith("DONE tone ")]
        if len(done_lines) != 1:
            raise RuntimeError("Expected exactly one tone completion")
        done = dict(field.split("=", 1) for field in done_lines[0].split()[2:])
        expected = {"percent": str(args.percent), "frames": "64000", "tone_frames": "32000",
                    "error": "0", "stopped": "0", "cleanup_ok": "1", "state": "IDLE"}
        if done != expected:
            raise RuntimeError("Tone count/cleanup/fault check failed")
    except Exception as failure:
        error = str(failure)
    finally:
        try:
            send("stop")
            receive(1)
            send("status")
            stopped = idle_confirmed(receive(1))
        finally:
            connection.close()
            args.log.write_text("\n".join(lines) + "\n", encoding="utf8")
    summary = {"percent": args.percent, "done": done, "idle_confirmed": stopped,
               "error": error, "audible_speaker_result": "unverified_user_confirmation_needed"}
    args.log.with_suffix(".json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    return 1 if error or not stopped else 0


if __name__ == "__main__":
    raise SystemExit(main())
