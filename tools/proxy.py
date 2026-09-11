#!/usr/bin/env python3
"""
Transparent proxy + traffic logger for ESP32 Serial Bridge.

Creates a virtual serial port (PTY) that TunerStudio connects to
and acts as man-in-the-middle between TS and the real upstream,
logging each packet to stdout and to a .txt file.

TCP MODE  — first argument is an IP/hostname:
    python proxy.py 192.168.1.100
    python proxy.py 192.168.1.100 8881 --log session.txt

SERIAL USB MODE  — first argument starts with /dev/:
    python proxy.py /dev/cu.usbserial-1410 --baud 115200
    python proxy.py /dev/cu.usbmodem101 --baud 57600 --log session.txt
"""

import pty
import os
import sys
import tty
import select
import argparse
from datetime import datetime


# ── helpers ─────────────────────────────────────────────────────────────────

def hex_dump(data: bytes) -> str:
    return ' '.join(f'{b:02X}' for b in data)


def log(f, direction: str, data: bytes) -> None:
    ts = datetime.now().strftime('%H:%M:%S.%f')[:-3]
    line = f"[{ts}] {direction:14s} [{len(data):4d}b]  {hex_dump(data)}"
    print(line, flush=True)
    f.write(line + '\n')
    f.flush()


def write_session_header(f, upstream: str, pty_name: str) -> None:
    sep = '=' * 72
    f.write(f"\n{sep}\n")
    f.write(f"Session  : {datetime.now()}\n")
    f.write(f"Upstream : {upstream}\n")
    f.write(f"PTY      : {pty_name}\n")
    f.write(f"{sep}\n")
    f.flush()


def make_pty() -> tuple[int, str]:
    """Return (master_fd, slave_device_name) with raw mode set on master."""
    master_fd, slave_fd = pty.openpty()
    tty.setraw(master_fd)          # no line-discipline mangling of binary data
    slave_name = os.ttyname(slave_fd)
    os.close(slave_fd)             # kernel keeps slave alive via master
    return master_fd, slave_name


def proxy_loop(master_fd: int, upstream_fd: int, logfile,
               label_up: str, label_down: str) -> None:
    """Bidirectional proxy between PTY master and upstream fd until disconnect."""
    while True:
        try:
            rlist, _, _ = select.select([master_fd, upstream_fd], [], [], 1.0)
        except (ValueError, OSError):
            break

        for fd in rlist:
            if fd == master_fd:
                try:
                    data = os.read(master_fd, 4096)
                except OSError:
                    return
                if data:
                    log(logfile, label_up, data)
                    try:
                        os.write(upstream_fd, data)
                    except OSError:
                        return

            elif fd == upstream_fd:
                try:
                    data = os.read(upstream_fd, 4096)
                except OSError:
                    data = b''
                if not data:
                    print('\n[upstream disconnected]')
                    return
                log(logfile, label_down, data)
                try:
                    os.write(master_fd, data)
                except OSError:
                    return


# ── TCP mode ────────────────────────────────────────────────────────────────

def mode_tcp(args) -> None:
    import socket

    host = args.target
    port = args.port or 8881

    master_fd, slave_name = make_pty()

    print(f"\n  Virtual port  : {slave_name}  ← connect TunerStudio here")
    print(f"  Bridge TCP    : {host}:{port}")
    print(f"  Log           : {args.log}")
    print("  Ctrl+C to stop\n")

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.connect((host, port))
    except OSError as e:
        print(f"[error] {e}")
        os.close(master_fd)
        sys.exit(1)

    print(f"[connected] {host}:{port}\n")

    with open(args.log, 'a') as f:
        write_session_header(f, f"TCP {host}:{port}", slave_name)
        try:
            proxy_loop(master_fd, sock.fileno(), f, "TS → Bridge", "Bridge → TS")
        except KeyboardInterrupt:
            print('\n[stopped]')
        f.write(f"Session ended: {datetime.now()}\n")

    sock.close()
    os.close(master_fd)


# ── Serial USB mode ──────────────────────────────────────────────────────────

def mode_serial(args) -> None:
    try:
        import serial
    except ImportError:
        print("[error] pyserial is not installed. Run: pip install pyserial")
        sys.exit(1)

    device = args.target
    baud   = args.baud or 19200

    master_fd, slave_name = make_pty()

    print(f"\n  Virtual port  : {slave_name}  ← connect TunerStudio here")
    print(f"  Real device   : {device}  @ {baud} baud")
    print(f"  Log           : {args.log}")
    print("  Ctrl+C to stop\n")

    try:
        ser = serial.Serial(device, baudrate=baud, timeout=0)
    except Exception as e:
        print(f"[error] {e}")
        os.close(master_fd)
        sys.exit(1)

    print(f"[opened] {device}\n")

    with open(args.log, 'a') as f:
        write_session_header(f, f"Serial {device} @ {baud}", slave_name)
        try:
            proxy_loop(master_fd, ser.fd, f, "TS → ECU", "ECU → TS")
        except KeyboardInterrupt:
            print('\n[stopped]')
        f.write(f"Session ended: {datetime.now()}\n")

    ser.close()
    os.close(master_fd)


# ── entry point ──────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description='Transparent proxy + traffic logger',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument('target',
        help='Bridge IP (TCP mode) or /dev/cu... (serial USB mode)')
    parser.add_argument('port', nargs='?', type=int,
        help='TCP port (TCP mode only, default: 8881)')
    parser.add_argument('--baud', type=int,
        help='Baud rate (serial mode only, default: 19200)')
    parser.add_argument('--log', default='traffic.txt',
        help='Log file (default: traffic.txt)')
    args = parser.parse_args()

    if args.target.startswith('/dev/'):
        mode_serial(args)
    else:
        mode_tcp(args)


if __name__ == '__main__':
    main()
