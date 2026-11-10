"""Bounded standalone tests; identity checked before opening the serial port."""
import argparse
import time
from pathlib import Path
from result_paths import log_path

import serial
from serial.tools import list_ports


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=["mic-status", "mic", "speaker"])
    parser.add_argument("--log", type=Path, required=True)
    args = parser.parse_args()
    args.log = log_path(args.log)
    if args.log.exists():
        raise RuntimeError("Refusing to overwrite an existing trial log")
    ports = [p for p in list_ports.comports()
             if (p.vid, p.pid, p.serial_number) == (0x1A86, 0x55D3, "5CE5226325")]
    if len(ports) != 1:
        raise RuntimeError("Expected USB board not uniquely identified")
    prefix = "SPEAKER_STANDALONE" if args.mode == "speaker" else "MIC_STANDALONE"
    lines = []
    pending = ""
    connection = serial.Serial(port=None, baudrate=115200, timeout=0.1,
                               write_timeout=1, exclusive=True)
    connection.dtr = connection.rts = False
    connection.port = ports[0].device
    connection.open()

    def send(command):
        lines.append("HOST " + command)
        print("HOST " + command, flush=True)
        connection.write((command + "\n").encode("ascii"))

    def receive(seconds):
        nonlocal pending
        result = []
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            pending += connection.read(4096).decode("utf8", "replace")
            while "\n" in pending:
                line, pending = pending.split("\n", 1)
                line = line.rstrip("\r")
                lines.append(line)
                result.append(line)
                print(line, flush=True)
        return result

    def idle(result):
        return any(line.startswith(prefix + " state=IDLE") for line in result)

    try:
        boot = receive(3)
        if any(line.startswith("ACK ") for line in boot):
            raise RuntimeError("Unexpected automatic activity at boot")
        send("status")
        if not idle(receive(1)):
            raise RuntimeError("Wrong firmware or not IDLE")
        send("invalid")
        if not any(line.startswith("REJECT ") for line in receive(0.3)):
            raise RuntimeError("Invalid command not rejected")
        if args.mode != "mic-status":
            print("Starting one bounded trial", flush=True)
            send("tone" if args.mode == "speaker" else "start")
            result = receive(7 if args.mode == "speaker" else 13)
            expected = ("DONE speaker reason=complete frames=176400 error=0 cleanup=0 state=IDLE"
                        if args.mode == "speaker" else
                        "DONE mic reason=complete frames=160000 error=0 cleanup=0 state=IDLE")
            if result.count(expected) != 1:
                raise RuntimeError("Trial did not complete with expected frame count and cleanup")
            if args.mode == "speaker":
                ack = "ACK speaker fs=44100 bits=16 hz=1000 percent=25 tone_frames=88200 total_frames=176400 WS=7 BCLK=8 DIN=9"
                if result.count(ack) != 1:
                    raise RuntimeError("Unexpected speaker configuration")
            if args.mode == "mic":
                for slot in (0, 1):
                    reports = [line for line in result if line.startswith("MIC frame=") and
                               f"slot={slot} " in line]
                    if len(reports) != 100:
                        raise RuntimeError("Missing microphone meter windows")
        send("status")
        if not idle(receive(0.5)):
            raise RuntimeError("Not IDLE after trial")
        lines.append("PASS protocol only; physical audio result needs user confirmation")
    finally:
        try:
            send("stop")
            receive(0.5)
        finally:
            connection.close()
            args.log.write_text("\n".join(lines) + "\n", encoding="utf8")


if __name__ == "__main__":
    main()
