#!/usr/bin/env python3
"""Capture a finite ESP32 serial run (use ESP-IDF Python with pyserial).

Example: python tools/serial_capture.py --port COM4 --seconds 60 --reset
         --until OPUS_BENCH_DONE --output .private/windows-bench/opus.log
"""
import argparse
import math
from pathlib import Path
import sys
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--seconds', type=float, default=60)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--until', help='Exit successfully when this text appears; timeout fails')
    parser.add_argument('--reset', action='store_true', help='Reset through ESP-IDF monitor before capturing')
    args = parser.parse_args()
    if not math.isfinite(args.seconds) or args.seconds <= 0:
        parser.error('--seconds must be finite and positive')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    found = False
    tail = ''
    # Deassert DTR/RTS before opening; only reset when explicitly requested.
    port = serial.Serial(port=None, baudrate=args.baud, timeout=0.2)
    port.dtr = False
    port.rts = False
    port.port = args.port
    try:
        with port, args.output.open('wb') as output:
            if args.reset:
                from esp_idf_monitor.base.reset import Reset
                Reset(port, 'esp32s3').hard()
            deadline = time.monotonic() + args.seconds
            while time.monotonic() < deadline:
                data = port.read(max(1, port.in_waiting))
                if not data:
                    continue
                output.write(data)
                output.flush()
                decoded = data.decode('utf-8', errors='replace')
                print(decoded, end='', flush=True)
                tail += decoded
                if args.until and args.until in tail:
                    found = True
                    break
                tail = tail[-max(4096, len(args.until or '')):]
    except serial.SerialException as error:
        print(f'Serial capture failed: {error}', file=sys.stderr)
        return 1
    if args.until and not found:
        print(f'Timed out waiting for {args.until!r}; log: {args.output}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
