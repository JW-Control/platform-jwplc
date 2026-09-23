import argparse
import re
import socket
import struct
import sys
import threading
import time

import serial

VALID_HOST = "jwplc.test"
TIMEOUT_HOST = "timeout.jwplc.test"
VALID_IP = bytes((10, 20, 30, 40))
READY_RE = re.compile(
    r"^NB2_DNS_PROBE_READY=YES IP=([0-9]{1,3}(?:\.[0-9]{1,3}){3})$"
)


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--serial", required=True)
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout-s", type=float, default=8.0)
    return parser.parse_args()


def local_ip_for_dut(dut):
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect((dut, 5001))
        return probe.getsockname()[0]
    finally:
        probe.close()


def parse_question(packet):
    if len(packet) < 17:
        return None, None

    offset = 12
    labels = []

    while True:
        if offset >= len(packet):
            return None, None

        length = packet[offset]
        offset += 1

        if length == 0:
            break

        if offset + length > len(packet):
            return None, None

        labels.append(
            packet[offset:offset + length].decode(
                "ascii",
                errors="replace",
            )
        )
        offset += length

    if offset + 4 > len(packet):
        return None, None

    return ".".join(labels).lower(), offset + 4


def build_a_response(request, question_end):
    transaction_id = request[:2]
    flags = struct.pack("!H", 0x8180)
    counts = struct.pack("!HHHH", 1, 1, 0, 0)
    question = request[12:question_end]
    answer = (
        b"\xc0\x0c"
        + struct.pack("!HHI", 1, 1, 60)
        + struct.pack("!H", 4)
        + VALID_IP
    )
    return transaction_id + flags + counts + question + answer


class DnsResponder:
    def __init__(self, bind_ip):
        self.bind_ip = bind_ip
        self.valid_queries = 0
        self.timeout_queries = 0
        self.other_queries = 0
        self.error = None
        self.stop_event = threading.Event()
        self.ready_event = threading.Event()
        self.thread = threading.Thread(
            target=self._run,
            daemon=True,
        )

    def start(self):
        self.thread.start()

    def stop(self):
        self.stop_event.set()
        self.thread.join(timeout=1.0)

    def _run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        try:
            sock.setsockopt(
                socket.SOL_SOCKET,
                socket.SO_REUSEADDR,
                1,
            )
            sock.bind((self.bind_ip, 53))
            sock.settimeout(0.1)
            self.ready_event.set()

            while not self.stop_event.is_set():
                try:
                    packet, peer = sock.recvfrom(2048)
                except socket.timeout:
                    continue

                host, question_end = parse_question(packet)

                if host is None:
                    self.other_queries += 1
                    continue

                if host == VALID_HOST:
                    self.valid_queries += 1
                    sock.sendto(
                        build_a_response(packet, question_end),
                        peer,
                    )
                elif host == TIMEOUT_HOST:
                    self.timeout_queries += 1
                else:
                    self.other_queries += 1
        except Exception as exc:
            self.error = repr(exc)
            self.ready_event.set()
        finally:
            sock.close()


def main():
    args = parse_args()

    lines = []
    result_end = threading.Event()
    stop_serial = threading.Event()
    ready_event = threading.Event()
    ready_ip = {"value": None}

    ser = serial.Serial(
        args.serial,
        args.baud,
        timeout=0.1,
    )

    def serial_reader():
        while not stop_serial.is_set():
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
            if match is not None and ready_ip["value"] is None:
                ready_ip["value"] = match.group(1)
                ready_event.set()

            if line == "NB2_DNS_PHYSICAL_RESULT=END":
                result_end.set()

    reader = threading.Thread(
        target=serial_reader,
        daemon=True,
    )
    reader.start()

    dns = None

    try:
        deadline = time.monotonic() + args.timeout_s

        if not ready_event.wait(
            timeout=max(
                0.1,
                deadline - time.monotonic(),
            )
        ):
            print(
                "NB3_DNS_READY_TIMEOUT=YES",
                flush=True,
            )
            return 4

        dut_ip = ready_ip["value"]
        if dut_ip is None:
            print(
                "NB3_DNS_READY_IP_MISSING=YES",
                flush=True,
            )
            return 7

        print(f"DUT_IP_EFFECTIVE={dut_ip}", flush=True)
        print(
            "DUT_IP_SOURCE=DNS_PROBE_SERIAL_READY",
            flush=True,
        )

        host_ip = local_ip_for_dut(dut_ip)
        print(f"PC_DNS_SERVER_IP={host_ip}", flush=True)

        dns = DnsResponder(host_ip)
        dns.start()

        if not dns.ready_event.wait(timeout=2.0):
            print(
                "DNS_SERVER_START_TIMEOUT=YES",
                flush=True,
            )
            return 2

        if dns.error is not None:
            print(
                f"DNS_SERVER_ERROR={dns.error}",
                flush=True,
            )
            return 3

        ser.write(f"RUN {host_ip}\n".encode("ascii"))
        ser.flush()
        print(
            "DNS_RUN_COMMAND_SENT=YES",
            flush=True,
        )

        if not result_end.wait(
            timeout=max(
                1.0,
                deadline - time.monotonic(),
            )
        ):
            print(
                "NB3_DNS_RESULT_TIMEOUT=YES",
                flush=True,
            )
            return 5

        time.sleep(0.1)

        print(
            f"DNS_VALID_QUERY_COUNT={dns.valid_queries}",
            flush=True,
        )
        print(
            f"DNS_TIMEOUT_QUERY_COUNT={dns.timeout_queries}",
            flush=True,
        )
        print(
            f"DNS_OTHER_QUERY_COUNT={dns.other_queries}",
            flush=True,
        )

        if dns.error is not None:
            print(
                f"DNS_SERVER_ERROR={dns.error}",
                flush=True,
            )
            return 6

        print(
            "NB3_DNS_CLIENT_PASS=YES",
            flush=True,
        )
        return 0
    finally:
        stop_serial.set()
        reader.join(timeout=1.0)

        if ser.is_open:
            ser.close()

        if dns is not None:
            dns.stop()


if __name__ == "__main__":
    sys.exit(main())
