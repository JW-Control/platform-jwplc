import argparse
import socket
import sys
import threading
import time

import serial


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--host", required=True)
    p.add_argument("--port", type=int, default=5003)
    p.add_argument("--serial", required=True)
    p.add_argument("--baud", type=int, default=115200)
    p.add_argument("--no-read-ms", type=int, default=750)
    p.add_argument("--recv-buffer", type=int, default=4096)
    p.add_argument("--ready-timeout-s", type=float, default=15.0)
    p.add_argument("--result-timeout-s", type=float, default=8.0)
    return p.parse_args()


def main():
    args = parse_args()
    serial_lines = []
    serial_lock = threading.Lock()
    result_end = threading.Event()
    stop_reader = threading.Event()

    ser = serial.Serial(args.serial, args.baud, timeout=0.1)

    def reader():
        while not stop_reader.is_set():
            raw = ser.readline()
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue
            with serial_lock:
                serial_lines.append(line)
            print(line, flush=True)
            if line == "NB1_FLUSH_PROBE_RESULT=END":
                result_end.set()

    thread = threading.Thread(target=reader, daemon=True)
    thread.start()

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, args.recv_buffer)
    sock.settimeout(0.5)

    connected = False
    ready_deadline = time.monotonic() + args.ready_timeout_s
    last_error = None
    while time.monotonic() < ready_deadline and not connected:
        try:
            sock.connect((args.host, args.port))
            connected = True
        except OSError as exc:
            last_error = exc
            time.sleep(0.2)

    if not connected:
        print(f"NB1_FLUSH_CLIENT_CONNECT_ERROR={last_error}")
        stop_reader.set()
        thread.join(timeout=1.0)
        ser.close()
        sock.close()
        return 2

    actual_rcvbuf = sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
    print(f"CLIENT_CONNECTED=YES")
    print(f"CLIENT_SO_RCVBUF_REQUESTED={args.recv_buffer}")
    print(f"CLIENT_SO_RCVBUF_ACTUAL={actual_rcvbuf}")
    print(f"CLIENT_NO_READ_MS={args.no_read_ms}")

    sock.sendall(b"F")
    no_read_started = time.monotonic()
    time.sleep(args.no_read_ms / 1000.0)
    no_read_actual_ms = int((time.monotonic() - no_read_started) * 1000.0)
    print(f"CLIENT_NO_READ_ACTUAL_MS={no_read_actual_ms}")

    total_received = 0
    drain_deadline = time.monotonic() + args.result_timeout_s
    peer_closed = False

    while time.monotonic() < drain_deadline:
        try:
            data = sock.recv(16384)
            if not data:
                peer_closed = True
                break
            total_received += len(data)
        except socket.timeout:
            if result_end.is_set():
                break
        except OSError:
            break

    try:
        sock.close()
    except OSError:
        pass

    if not result_end.wait(timeout=args.result_timeout_s):
        print("NB1_FLUSH_CLIENT_RESULT_TIMEOUT=YES")
        stop_reader.set()
        thread.join(timeout=1.0)
        ser.close()
        return 3

    time.sleep(0.2)
    stop_reader.set()
    thread.join(timeout=1.0)
    ser.close()

    print(f"CLIENT_RX_BYTES={total_received}")
    print(f"CLIENT_PEER_CLOSED={'YES' if peer_closed else 'NO'}")
    print("NB1_FLUSH_CLIENT_PASS=YES")
    return 0


if __name__ == "__main__":
    sys.exit(main())
