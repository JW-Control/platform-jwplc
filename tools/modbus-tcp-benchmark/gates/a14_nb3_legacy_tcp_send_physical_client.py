import argparse
import re
import socket
import sys
import threading
import time

import serial


READY_RE = re.compile(
    r"^NB3_F2_PROBE_READY=YES IP=([0-9]{1,3}(?:\.[0-9]{1,3}){3})$"
)

NORMAL_PORT = 5004
BACKPRESSURE_PORT = 5005
NORMAL_EXPECTED_BYTES = 4096


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout-s", type=float, default=12.0)
    return parser.parse_args()


def local_ip_for_dut(dut):
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect((dut, 5001))
        return probe.getsockname()[0]
    finally:
        probe.close()


def expected_normal_payload():
    block = bytes(index & 0xFF for index in range(1024))
    return block * 4


class NormalServer:
    def __init__(self, bind_ip):
        self.bind_ip = bind_ip
        self.accepted = False
        self.bytes_received = 0
        self.pattern_ok = False
        self.error = None
        self.ready = threading.Event()
        self.done = threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self.thread.start()

    def stop(self):
        self.thread.join(timeout=1.0)

    def _run(self):
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        conn = None
        try:
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listener.bind((self.bind_ip, NORMAL_PORT))
            listener.listen(1)
            listener.settimeout(6.0)
            self.ready.set()

            conn, _ = listener.accept()
            self.accepted = True
            conn.settimeout(2.0)

            chunks = []
            total = 0

            while total < NORMAL_EXPECTED_BYTES:
                data = conn.recv(NORMAL_EXPECTED_BYTES - total)
                if not data:
                    break
                chunks.append(data)
                total += len(data)

            payload = b"".join(chunks)
            self.bytes_received = len(payload)
            self.pattern_ok = payload == expected_normal_payload()
        except Exception as exc:
            self.error = repr(exc)
        finally:
            if conn is not None:
                try:
                    conn.close()
                except OSError:
                    pass
            listener.close()
            self.done.set()


class BackpressureServer:
    def __init__(self, bind_ip):
        self.bind_ip = bind_ip
        self.accepted = False
        self.error = None
        self.actual_rcvbuf = 0
        self.ready = threading.Event()
        self.accepted_event = threading.Event()
        self.release = threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self.thread.start()

    def stop(self):
        self.release.set()
        self.thread.join(timeout=1.0)

    def _run(self):
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        conn = None
        try:
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            listener.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
            listener.bind((self.bind_ip, BACKPRESSURE_PORT))
            listener.listen(1)
            listener.settimeout(6.0)
            self.ready.set()

            conn, _ = listener.accept()
            self.accepted = True
            conn.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
            self.actual_rcvbuf = conn.getsockopt(
                socket.SOL_SOCKET,
                socket.SO_RCVBUF,
            )
            self.accepted_event.set()

            # Intentionally never recv(): the peer must eventually observe
            # TCP backpressure / zero-window while the connection stays open.
            self.release.wait(timeout=10.0)
        except Exception as exc:
            self.error = repr(exc)
            self.ready.set()
            self.accepted_event.set()
        finally:
            if conn is not None:
                try:
                    conn.close()
                except OSError:
                    pass
            listener.close()


def main():
    args = parse_args()

    ready_event = threading.Event()
    result_event = threading.Event()
    stop_serial = threading.Event()
    ready_ip = {"value": None}

    ser = serial.Serial(args.serial, args.baud, timeout=0.1)

    def reader():
        while not stop_serial.is_set():
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue

            print(line, flush=True)

            match = READY_RE.match(line)
            if match is not None and ready_ip["value"] is None:
                ready_ip["value"] = match.group(1)
                ready_event.set()

            if line == "NB3_F2_RESULT=END":
                result_event.set()

    reader_thread = threading.Thread(target=reader, daemon=True)
    reader_thread.start()

    normal = None
    backpressure = None

    try:
        deadline = time.monotonic() + args.timeout_s

        if not ready_event.wait(
            timeout=max(0.1, deadline - time.monotonic())
        ):
            print("NB3_F2_READY_TIMEOUT=YES", flush=True)
            return 2

        dut_ip = ready_ip["value"]
        if dut_ip is None:
            print("NB3_F2_DUT_IP_MISSING=YES", flush=True)
            return 3

        print(f"DUT_IP_EFFECTIVE={dut_ip}", flush=True)

        host_ip = local_ip_for_dut(dut_ip)
        print(f"PC_SERVER_IP={host_ip}", flush=True)

        normal = NormalServer(host_ip)
        backpressure = BackpressureServer(host_ip)
        normal.start()
        backpressure.start()

        if not normal.ready.wait(timeout=2.0):
            print("NORMAL_SERVER_READY_TIMEOUT=YES", flush=True)
            return 4

        if not backpressure.ready.wait(timeout=2.0):
            print("BACKPRESSURE_SERVER_READY_TIMEOUT=YES", flush=True)
            return 5

        if normal.error is not None:
            print(f"NORMAL_SERVER_ERROR={normal.error}", flush=True)
            return 6

        if backpressure.error is not None:
            print(
                f"BACKPRESSURE_SERVER_ERROR={backpressure.error}",
                flush=True,
            )
            return 7

        ser.write(f"RUN {host_ip}\n".encode("ascii"))
        ser.flush()
        print("RUN_COMMAND_SENT=YES", flush=True)

        if not result_event.wait(
            timeout=max(0.1, deadline - time.monotonic())
        ):
            print("NB3_F2_RESULT_TIMEOUT=YES", flush=True)
            return 8

        normal.done.wait(timeout=1.0)
        time.sleep(0.1)

        print(
            f"PC_NORMAL_ACCEPTED={'YES' if normal.accepted else 'NO'}",
            flush=True,
        )
        print(
            f"PC_NORMAL_BYTES={normal.bytes_received}",
            flush=True,
        )
        print(
            f"PC_NORMAL_PATTERN={'PASS' if normal.pattern_ok else 'FAIL'}",
            flush=True,
        )
        print(
            "PC_BACKPRESSURE_ACCEPTED="
            + ("YES" if backpressure.accepted else "NO"),
            flush=True,
        )
        print(
            f"PC_BACKPRESSURE_RCVBUF={backpressure.actual_rcvbuf}",
            flush=True,
        )

        if normal.error is not None:
            print(f"NORMAL_SERVER_ERROR={normal.error}", flush=True)
            return 9

        if backpressure.error is not None:
            print(
                f"BACKPRESSURE_SERVER_ERROR={backpressure.error}",
                flush=True,
            )
            return 10

        print("NB3_F2_CLIENT_PASS=YES", flush=True)
        return 0
    finally:
        if backpressure is not None:
            backpressure.stop()
        if normal is not None:
            normal.stop()

        stop_serial.set()
        reader_thread.join(timeout=1.0)

        if ser.is_open:
            ser.close()


if __name__ == "__main__":
    sys.exit(main())
