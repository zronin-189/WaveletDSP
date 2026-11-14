"""Record and download in one serial session, or download a retained capture."""
import argparse
import json
from pathlib import Path
import struct
import subprocess
import shutil
import time
import wave
import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='Exact port selected in VS Code')
    parser.add_argument('--record', action='store_true', help='Start capture and download without reopening Serial')
    parser.add_argument('--name', default='mic_music10')
    parser.add_argument('--notify', action='store_true', help='Show local desktop notification when RUNNING')
    args = parser.parse_args()
    if args.notify and not shutil.which('notify-send'):
        parser.error('--notify requires notify-send')
    if not args.name or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in args.name):
        parser.error('Name must be a filename stem with letters/digits/underscore/hyphen')
    root = Path(__file__).resolve().parent.parent
    results = root / 'results'
    data = results / 'du_lieu_kiem_thu'
    outputs = [results / (args.name + '_' + suffix + '.wav') for suffix in ('truoc', 'sau')]
    metadata = data / (args.name + '.json')
    if any(p.exists() for p in outputs + [metadata]):
        raise RuntimeError('Output exists; choose a new --name')
    data.mkdir(parents=True, exist_ok=True)
    con = serial.Serial(port=None, baudrate=115200, timeout=.1, write_timeout=2, exclusive=True)
    con.dtr = con.rts = False
    con.port = args.port
    con.open()
    lines = []
    running_announced = False

    def until(prefix, seconds):
        nonlocal running_announced
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            line = con.readline().decode('utf8', 'replace').strip()
            if line:
                lines.append(line)
            if line.startswith(prefix):
                return line
            if line.startswith('Rejected:'):
                raise RuntimeError(line + ' No complete capture is available. Close Serial Monitor and rerun with --record.')
            if line.startswith('state=FAULT '):
                raise RuntimeError('Device fault: ' + line)
            if line.startswith('state=RUNNING ') and args.record and not running_announced:
                running_announced = True
                if args.notify:
                    subprocess.run(['notify-send', '-u', 'critical', '-t', '10000', 'BẬT NHẠC VÀ NHIỄU', 'ESP32 đang thu 10 giây — bật mức nhỏ ngay.'], timeout=2, check=False)
                print('RUNNING: bật nhạc ngay; đang thu 10 giây...', flush=True)
        raise RuntimeError('Timeout waiting for ' + prefix)

    try:
        if args.record:
            # An open/close may reset this board. Complete acquisition on this handle.
            print('Chờ board khởi động; giữ im lặng để lấy nền.', flush=True)
            until('state=', 8)
            con.write(b'stop\n')
            until('state=IDLE ', 8)
            print('Bắt đầu lấy nền. Chỉ bật nhạc khi thấy RUNNING.', flush=True)
            con.write(b'volume 20\nstart\n')
            until('CAPTURE_READY seconds=10', 35)
            print('Đã thu đủ; đang tải WAV, giữ nguyên kết nối...', flush=True)
        con.write(b'status\n')
        status = until('state=', 4)
        if not status.startswith('state=IDLE ') or 'fault=none ' not in status:
            raise RuntimeError('Capture must finish in IDLE without fault: ' + status)
        con.write(b'filter_dump\n')
        header = until('FILTER_DATA ', 4)
        fields = dict(v.split('=', 1) for v in header.split()[1:])
        frames, size = int(fields['frames']), int(fields['bytes'])
        assert int(fields['fs']) == 16000 and frames == 160000 and size == frames * 4
        payload = bytearray()
        deadline = time.monotonic() + 58
        while len(payload) < size and time.monotonic() < deadline:
            payload.extend(con.read(min(4096, size - len(payload))))
        assert len(payload) == size, 'Incomplete download'
        checksum = 2166136261
        for byte in payload:
            checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
        assert checksum == int(fields['fnv']), 'Checksum mismatch'
        until('FILTER_END state=IDLE', 4)
        words = struct.unpack('<' + 'h' * (frames * 2), payload)
        for slot, path in enumerate(outputs):
            with wave.open(str(path), 'wb') as wav:
                wav.setnchannels(1)
                wav.setsampwidth(2)
                wav.setframerate(16000)
                wav.writeframes(struct.pack('<' + 'h' * frames, *words[slot::2]))
        metadata.write_text(json.dumps({'header': fields, 'frames': frames, 'seconds': frames / 16000,
                                       'checksum_verified': True, 'source': 'Paired device PCM; not acoustic capture',
                                       'before_format': fields.get('before', 'legacy_gain_limiter_volume'),
                                       'after_format': fields.get('after', 'filtered_gain_limiter_volume')}, indent=2) + '\n')
        for path in outputs:
            print(path)
    finally:
        con.close()
        (data / (args.name + '_download.log')).write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
