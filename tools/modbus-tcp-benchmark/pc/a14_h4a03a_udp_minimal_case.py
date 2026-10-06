#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))

import eth14_raw_transport_benchmark as rawbench


def serial_command_ack(
    dut: rawbench.DutSerial,
    command: bytes,
    expected_ack: bytes,
    timeout_s: float = 1.5,
) -> None:
    dut.ser.reset_input_buffer()
    dut.ser.write(command)
    dut.ser.flush()

    deadline = time.perf_counter() + timeout_s
    raw = bytearray()

    while time.perf_counter() < deadline:
        chunk = dut.ser.read(max(1, dut.ser.in_waiting))
        if chunk:
            raw.extend(chunk)
            if expected_ack in raw:
                return

    raise RuntimeError(
        "H4A03A_SERIAL_ACK_MISSING "
        f"COMMAND={command!r} EXPECTED={expected_ack!r} "
        f"RAW={raw.decode('utf-8', errors='replace')!r}"
    )


def force_idle(dut: rawbench.DutSerial) -> None:
    serial_command_ack(dut, b"I", b"ETH14_RAW_IDLE=PASS")


def serial_reset(dut: rawbench.DutSerial) -> None:
    serial_command_ack(dut, b"R", b"ETH14_RAW_RESET=PASS")


def wait_ready(dut: rawbench.DutSerial, timeout_s: float = 30.0) -> str:
    deadline = time.perf_counter() + timeout_s
    last: dict[str, str] = {}

    while time.perf_counter() < deadline:
        last = dut.snapshot()
        ip = last.get("IP", "").strip()
        if (
            last.get("RAW_SERVER_READY") == "YES"
            and last.get("ETH_READY") == "YES"
            and last.get("ETH_LINK") == "UP"
            and ip
            and ip != "0.0.0.0"
        ):
            print(f"H4A03A_DUT_IP={ip}")
            print("H4A03A_DUT_READY=PASS")
            return ip
        time.sleep(0.25)

    print(last.get("_RAW", ""))
    raise RuntimeError("H4A03A_DUT_READY_TIMEOUT")


def wait_quiescent(
    dut: rawbench.DutSerial,
    timeout_s: float = 3.0,
    sample_period_s: float = 0.10,
    stable_intervals: int = 3,
) -> int:
    deadline = time.perf_counter() + timeout_s
    previous: int | None = None
    stable = 0
    samples = 0
    latest = 0

    while time.perf_counter() < deadline:
        time.sleep(sample_period_s)
        snap = dut.snapshot()
        samples += 1

        if snap.get("MODE") != "IDLE":
            raise RuntimeError(
                f"H4A03A_IDLE_MODE_LOST MODE={snap.get('MODE')}"
            )

        latest = rawbench.intval(
            snap,
            "P3J_R2_IDLE_UDP_DISCARDED",
        )

        if previous is not None and latest == previous:
            stable += 1
        else:
            stable = 0

        previous = latest

        if stable >= stable_intervals:
            print("H4A03A_QUIESCENCE_PASS=YES")
            print(f"H4A03A_QUIESCENCE_SAMPLES={samples}")
            print(f"H4A03A_QUIESCENCE_DISCARDED_TOTAL={latest}")
            return latest

    raise RuntimeError(
        "H4A03A_QUIESCENCE_TIMEOUT "
        f"LAST_DISCARDED={latest} SAMPLES={samples}"
    )


def get_int(snap: dict[str, str], key: str) -> int:
    return rawbench.intval(snap, key)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.3A FAST UDP RX minimal-instrumentation case"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=float, required=True)
    parser.add_argument("--udp-payload", type=int, default=1016)
    args = parser.parse_args()

    if args.duration <= 0.0:
        raise SystemExit("H4A03A_DURATION_MUST_BE_POSITIVE")
    if args.udp_payload != 1016:
        raise SystemExit("H4A03A_UDP_PAYLOAD_MUST_BE_1016")

    print("============================================================")
    print(" A14 H4A0.3A - FAST UDP RX MINIMAL PERFORMANCE CASE")
    print("============================================================")
    print(f"H4A03A_SERIAL={args.serial}")
    print(f"H4A03A_DURATION_TARGET_S={args.duration}")
    print(f"H4A03A_UDP_PAYLOAD={args.udp_payload}")
    print("H4A03A_PERFORMANCE_COUNTERS=RX_BYTES+RX_PACKETS+TRANSPORT_ERRORS+SPI_LOCK_ERRORS")
    print("H4A03A_TAIL_SETTLE_S=0.40")
    print("H4A03A_PC_MBPS_SEMANTICS=OFFERED_ENQUEUED_NOT_WIRE_THROUGHPUT")

    dut = rawbench.DutSerial(args.serial)
    sock: socket.socket | None = None
    measured: dict[str, str] = {}
    discarded = 0

    try:
        dut.open()
        host = wait_ready(dut)

        force_idle(dut)
        wait_quiescent(dut)
        serial_reset(dut)

        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_SNDBUF,
            4 * 1024 * 1024,
        )

        target = (host, 5002)
        sock.sendto(b"URX", target)
        time.sleep(0.08)

        armed = dut.snapshot()
        if armed.get("MODE") != "UDP_RX":
            raise RuntimeError(
                f"H4A03A_ARM_FAILED MODE={armed.get('MODE')}"
            )

        serial_reset(dut)
        time.sleep(0.02)

        zero = dut.snapshot()
        zero_bytes = get_int(zero, "RX_BYTES")
        zero_packets = get_int(zero, "RX_OPERATIONS")
        zero_errors = get_int(zero, "TRANSPORT_ERRORS")
        zero_udp_locks = get_int(zero, "UDP_SPI_LOCK_ERRORS")
        zero_tcp_locks = get_int(zero, "TCP_SPI_LOCK_ERRORS")

        if (
            zero.get("MODE") != "UDP_RX"
            or zero_bytes != 0
            or zero_packets != 0
            or zero_errors != 0
            or zero_udp_locks != 0
            or zero_tcp_locks != 0
        ):
            raise RuntimeError(
                "H4A03A_ZERO_ARM_FAILED "
                f"MODE={zero.get('MODE')} "
                f"RX_BYTES={zero_bytes} "
                f"RX_PACKETS={zero_packets} "
                f"TRANSPORT_ERRORS={zero_errors} "
                f"UDP_SPI_LOCK_ERRORS={zero_udp_locks} "
                f"TCP_SPI_LOCK_ERRORS={zero_tcp_locks}"
            )

        print("H4A03A_ZERO_ARM=PASS")

        payload = bytearray(args.udp_payload)
        sent_packets = 0
        sent_bytes = 0
        sequence = 0

        start = time.perf_counter()
        deadline = start + args.duration

        while time.perf_counter() < deadline:
            payload[0:4] = sequence.to_bytes(
                4,
                byteorder="big",
                signed=False,
            )
            sent = sock.sendto(payload, target)
            if sent == args.udp_payload:
                sent_packets += 1
                sent_bytes += sent
            sequence = (sequence + 1) & 0xFFFFFFFF

        elapsed = time.perf_counter() - start

        # Keep the historical P3J-R2/P3K raw RX lifecycle so H4A0.3A changes
        # measurement intrusion, not the post-flood accounting window.
        time.sleep(0.40)
        measured = dut.snapshot()

        force_idle(dut)
        discarded = wait_quiescent(dut)

    finally:
        if sock is not None:
            sock.close()
        dut.close()

    dut_packets = get_int(measured, "RX_OPERATIONS")
    dut_bytes = get_int(measured, "RX_BYTES")
    transport_errors = get_int(measured, "TRANSPORT_ERRORS")
    udp_spi_lock_errors = get_int(measured, "UDP_SPI_LOCK_ERRORS")
    tcp_spi_lock_errors = get_int(measured, "TCP_SPI_LOCK_ERRORS")
    spi_lock_errors = udp_spi_lock_errors + tcp_spi_lock_errors

    offered_mbps = rawbench.mbps(sent_bytes, elapsed)
    dut_mbps = rawbench.mbps(dut_bytes, elapsed)

    print(f"H4A03A_DURATION_ACTUAL_S={elapsed:.6f}")
    print(f"H4A03A_PC_OFFERED_PACKETS={sent_packets}")
    print(f"H4A03A_PC_OFFERED_BYTES={sent_bytes}")
    print(f"H4A03A_PC_OFFERED_MBPS={offered_mbps:.6f}")
    print(f"H4A03A_DUT_RX_PACKETS={dut_packets}")
    print(f"H4A03A_DUT_RX_BYTES={dut_bytes}")
    print(f"H4A03A_DUT_MBPS={dut_mbps:.6f}")
    print(f"H4A03A_TRANSPORT_ERRORS={transport_errors}")
    print(f"H4A03A_UDP_SPI_LOCK_ERRORS={udp_spi_lock_errors}")
    print(f"H4A03A_TCP_SPI_LOCK_ERRORS={tcp_spi_lock_errors}")
    print(f"H4A03A_SPI_LOCK_ERRORS_TOTAL={spi_lock_errors}")
    print(f"H4A03A_POSTRUN_DISCARDED={discarded}")

    functional = (
        sent_packets > 0
        and dut_packets > 0
        and dut_bytes > 0
        and transport_errors == 0
        and spi_lock_errors == 0
    )

    print(
        "H4A03A_FUNCTIONAL_PASS="
        f"{'YES' if functional else 'NO'}"
    )

    return 0 if functional else 2


if __name__ == "__main__":
    raise SystemExit(main())
