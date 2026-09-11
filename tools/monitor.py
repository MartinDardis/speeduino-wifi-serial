#!/usr/bin/env python3
import socket
import sys
import threading

HOST = sys.argv[1] if len(sys.argv) > 1 else "192.168.4.1"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8880

def receive(sock):
    while True:
        try:
            data = sock.recv(1024)
            if not data:
                print("\n[disconnected]")
                break
            print(data.decode("utf-8", errors="replace"), end="", flush=True)
        except Exception as e:
            print(f"\n[error] {e}")
            break

def send(sock):
    while True:
        try:
            line = input()
            sock.sendall((line + "\r\n").encode())
        except (EOFError, KeyboardInterrupt):
            break
        except Exception as e:
            print(f"\n[send error] {e}")
            break

print(f"Connecting to {HOST}:{PORT} ...")
try:
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((HOST, PORT))
    print(f"Connected. Ctrl+C to exit.\n{'-'*40}")
except Exception as e:
    print(f"[error] {e}")
    sys.exit(1)

t = threading.Thread(target=receive, args=(sock,), daemon=True)
t.start()

try:
    send(sock)
except KeyboardInterrupt:
    pass
finally:
    print("\n[closing]")
    sock.close()
