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
        "H4A03AR3_SERIAL_ACK_MISSING "
        f"COMMAND={command!r} EXPECTED={expected_ack!r} "
        f"RAW={raw.decode('utf-8', errors='replace')!r}"
    )


def freeze_without_reset(
    dut: rawbench.DutSerial,
) -> tuple[float, float]:
    dut.ser.reset_input_buffer()
    request_time = time.perf_counter()
    dut.ser.write(b"F")
    dut.ser.flush()

    deadline = request_time + 1.5
    raw = bytearray()

    while time.perf_counter() < deadline:
        chunk = dut.ser.read(max(1, dut.ser.in_waiting))
        if chunk:
            raw.extend(chunk)
            if b"ETH14_RAW_FREEZE=PASS" in raw:
                ack_time = time.perf_counter()
                return request_time, ack_time

    raise RuntimeError(
        "H4A03AR3_FREEZE_ACK_MISSING "
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
            print(f"H4A03AR3_DUT_IP={ip}")
            print("H4A03AR3_DUT_READY=PASS")
            return ip
        time.sleep(0.25)

    print(last.get("_RAW", ""))
    raise RuntimeError("H4A03AR3_DUT_READY_TIMEOUT")


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
                f"H4A03AR3_IDLE_MODE_LOST MODE={snap.get('MODE')}"
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
            print("H4A03AR3_QUIESCENCE_PASS=YES")
            print(f"H4A03AR3_QUIESCENCE_SAMPLES={samples}")
            print(f"H4A03AR3_QUIESCENCE_DISCARDED_TOTAL={latest}")
            return latest

    raise RuntimeError(
        "H4A03AR3_QUIESCENCE_TIMEOUT "
        f"LAST_DISCARDED={latest} SAMPLES={samples}"
    )


def get_int(snap: dict[str, str], key: str) -> int:
    return rawbench.intval(snap, key)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.3A-R3 matched UDP RX freeze-window case"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=float, default=15.0)
    parser.add_argument("--udp-payload", type=int, default=1016)
    parser.add_argument("--variant", choices=("HIST", "CURRENT"), required=True)
    args = parser.parse_args()

    if args.duration <= 0.0:
        raise SystemExit("H4A03AR3_DURATION_MUST_BE_POSITIVE")
    if args.udp_payload != 1016:
        raise SystemExit("H4A03AR3_UDP_PAYLOAD_MUST_BE_1016")

    print("=" * 64)
    print(" A14 H4A0.3A-R3 - MATCHED UDP RX FREEZE-WINDOW CASE")
    print("=" * 64)
    print(f"H4A03AR3_VARIANT={args.variant}")
    print(f"H4A03AR3_SERIAL={args.serial}")
    print(f"H4A03AR3_DURATION_TARGET_S={args.duration}")
    print(f"H4A03AR3_UDP_PAYLOAD={args.udp_payload}")
    print("H4A03AR3_TAIL_SETTLE_S=0")
    print("H4A03AR3_STOP_BARRIER=SERIAL_FREEZE_NO_RESET")
    print(
        "H4A03AR3_PERFORMANCE_COUNTERS="
        "RX_BYTES+RX_PACKETS+TRANSPORT_ERRORS+SPI_LOCK_ERRORS"
    )
    print(
        "H4A03AR3_PC_MBPS_SEMANTICS="
        "OFFERED_ENQUEUED_NOT_WIRE_THROUGHPUT"
    )

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
                f"H4A03AR3_ARM_FAILED MODE={armed.get('MODE')}"
            )

        serial_reset(dut)
        time.sleep(0.02)

        zero = dut.snapshot()
        zero_values = {
            "RX_BYTES": get_int(zero, "RX_BYTES"),
            "RX_OPERATIONS": get_int(zero, "RX_OPERATIONS"),
            "TRANSPORT_ERRORS": get_int(zero, "TRANSPORT_ERRORS"),
            "UDP_SPI_LOCK_ERRORS": get_int(zero, "UDP_SPI_LOCK_ERRORS"),
            "TCP_SPI_LOCK_ERRORS": get_int(zero, "TCP_SPI_LOCK_ERRORS"),
        }
        if zero.get("MODE") != "UDP_RX" or any(zero_values.values()):
            raise RuntimeError(
                "H4A03AR3_ZERO_ARM_FAILED "
                f"MODE={zero.get('MODE')} VALUES={zero_values}"
            )
        print("H4A03AR3_ZERO_ARM=PASS")

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

        flood_end = time.perf_counter()
        elapsed_send = flood_end - start

        freeze_request, freeze_ack = freeze_without_reset(dut)
        freeze_request_tail = freeze_request - flood_end
        freeze_ack_tail = freeze_ack - flood_end

        measured = dut.snapshot()
        if measured.get("MODE") != "IDLE":
            raise RuntimeError(
                f"H4A03AR3_FREEZE_MODE_NOT_IDLE={measured.get('MODE')}"
            )

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

    upper_window = elapsed_send + max(0.0, freeze_request_tail)
    lower_window = elapsed_send + max(0.0, freeze_ack_tail)

    if upper_window <= 0.0 or lower_window <= 0.0:
        raise RuntimeError("H4A03AR3_INVALID_COUNTING_WINDOW")
    if lower_window < upper_window:
        raise RuntimeError("H4A03AR3_FREEZE_BOUNDS_INVERTED")

    offered_mbps = rawbench.mbps(sent_bytes, elapsed_send)
    dut_mbps_upper = rawbench.mbps(dut_bytes, upper_window)
    dut_mbps_lower = rawbench.mbps(dut_bytes, lower_window)
    dut_mbps_mid = (dut_mbps_lower + dut_mbps_upper) / 2.0

    print(f"H4A03AR3_DURATION_SEND_S={elapsed_send:.6f}")
    print(
        "H4A03AR3_FREEZE_REQUEST_TAIL_S="
        f"{freeze_request_tail:.6f}"
    )
    print(f"H4A03AR3_FREEZE_ACK_TAIL_S={freeze_ack_tail:.6f}")
    print(f"H4A03AR3_PC_OFFERED_PACKETS={sent_packets}")
    print(f"H4A03AR3_PC_OFFERED_BYTES={sent_bytes}")
    print(f"H4A03AR3_PC_OFFERED_MBPS={offered_mbps:.6f}")
    print(f"H4A03AR3_DUT_RX_PACKETS={dut_packets}")
    print(f"H4A03AR3_DUT_RX_BYTES={dut_bytes}")
    print(f"H4A03AR3_DUT_MBPS_LOWER={dut_mbps_lower:.6f}")
    print(f"H4A03AR3_DUT_MBPS_UPPER={dut_mbps_upper:.6f}")
    print(f"H4A03AR3_DUT_MBPS_MID={dut_mbps_mid:.6f}")
    print(f"H4A03AR3_TRANSPORT_ERRORS={transport_errors}")
    print(f"H4A03AR3_UDP_SPI_LOCK_ERRORS={udp_spi_lock_errors}")
    print(f"H4A03AR3_TCP_SPI_LOCK_ERRORS={tcp_spi_lock_errors}")
    print(f"H4A03AR3_SPI_LOCK_ERRORS_TOTAL={spi_lock_errors}")
    print(f"H4A03AR3_POSTRUN_DISCARDED={discarded}")

    functional = (
        sent_packets > 0
        and dut_packets > 0
        and dut_bytes > 0
        and transport_errors == 0
        and spi_lock_errors == 0
        and freeze_request_tail >= 0.0
        and freeze_ack_tail >= freeze_request_tail
    )

    print(
        "H4A03AR3_FUNCTIONAL_PASS="
        f"{'YES' if functional else 'NO'}"
    )

    return 0 if functional else 2


if __name__ == "__main__":
    raise SystemExit(main())
