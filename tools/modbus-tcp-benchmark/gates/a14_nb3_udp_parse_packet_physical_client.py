import argparse
import re
import socket
import sys
import threading
import time

import serial


READY_RE = re.compile(
    r"^NB3_E2_PROBE_READY=YES IP=([0-9]{1,3}(?:\.[0-9]{1,3}){3})$"
)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout-s", type=float, default=8.0)
    parser.add_argument("--udp-port", type=int, default=5003)
    return parser.parse_args()


def main():
    args = parse_args()

    lines = []
    ready_ip = {"value": None}
    ready_event = threading.Event()
    drain_event = threading.Event()
    result_event = threading.Event()
    stop_event = threading.Event()

    ser = serial.Serial(
        args.serial,
        args.baud,
        timeout=0.1,
    )

    def reader():
        while not stop_event.is_set():
            raw = ser.readline()
            if not raw:
                continue

            line = raw.decode(
                "utf-8",
                errors="replace",
            ).strip()

            if not line:
                continue

            lines.append(line)
            print(line, flush=True)

            match = READY_RE.match(line)
            if match is not None:
                ready_ip["value"] = match.group(1)
                ready_event.set()

            if line == "NB3_E2_DRAIN_DONE=YES":
                drain_event.set()

            if line == "NB3_E2_RESULT=END":
                result_event.set()

    thread = threading.Thread(
        target=reader,
        daemon=True,
    )
    thread.start()

    sock = socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM,
    )

    try:
        deadline = time.monotonic() + args.timeout_s

        if not ready_event.wait(
            timeout=max(0.1, deadline - time.monotonic())
        ):
            print("NB3_E2_READY_TIMEOUT=YES", flush=True)
            return 2

        dut_ip = ready_ip["value"]
        if dut_ip is None:
            print("NB3_E2_DUT_IP_MISSING=YES", flush=True)
            return 3

        print(f"DUT_IP_EFFECTIVE={dut_ip}", flush=True)

        payload = bytearray(256)
        prefix = b"PARTIAL"

        payload[: len(prefix)] = prefix

        for index in range(len(prefix), len(payload)):
            payload[index] = index & 0xFF

        sent = sock.sendto(
            payload,
            (dut_ip, args.udp_port),
        )

        print(f"PARTIAL_SENT_BYTES={sent}", flush=True)

        if sent != len(payload):
            print("NB3_E2_PARTIAL_SHORT_SEND=YES", flush=True)
            return 4

        if not drain_event.wait(
            timeout=max(0.1, deadline - time.monotonic())
        ):
            print("NB3_E2_DRAIN_TIMEOUT=YES", flush=True)
            return 5

        sent_next = sock.sendto(
            b"NEXT",
            (dut_ip, args.udp_port),
        )

        print(f"NEXT_SENT_BYTES={sent_next}", flush=True)

        if sent_next != 4:
            print("NB3_E2_NEXT_SHORT_SEND=YES", flush=True)
            return 6

        if not result_event.wait(
            timeout=max(0.1, deadline - time.monotonic())
        ):
            print("NB3_E2_RESULT_TIMEOUT=YES", flush=True)
            return 7

        time.sleep(0.1)
        print("NB3_E2_CLIENT_PASS=YES", flush=True)
        return 0
    finally:
        stop_event.set()
        thread.join(timeout=1.0)
        ser.close()
        sock.close()


if __name__ == "__main__":
    sys.exit(main())
