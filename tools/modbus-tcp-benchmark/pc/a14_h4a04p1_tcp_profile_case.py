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


PROFILE_KEYS = (
    "TCP_PROF_SERVICE_CALLS",
    "TCP_PROF_LOCK_WAIT_TOTAL_US",
    "TCP_PROF_LOCK_WAIT_MAX_US",
    "TCP_PROF_ACCEPT_CALLS",
    "TCP_PROF_ACCEPT_TOTAL_US",
    "TCP_PROF_CONNECTED_CALLS",
    "TCP_PROF_CONNECTED_TOTAL_US",
    "TCP_PROF_AVAILABLE_CALLS",
    "TCP_PROF_AVAILABLE_TOTAL_US",
    "TCP_PROF_READ_CALLS",
    "TCP_PROF_READ_TOTAL_US",
    "TCP_PROF_READ_BYTES",
    "TCP_PROF_UNLOCKED_TOTAL_US",
    "TCP_PROF_UNLOCKED_MAX_US",
)


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
        "H4A04P1_SERIAL_ACK_MISSING "
        f"COMMAND={command!r} EXPECTED={expected_ack!r} "
        f"RAW={raw.decode('utf-8', errors='replace')!r}"
    )


def freeze_tcp_rx(
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
            if b"ETH14_TCP_RX_FREEZE=PASS" in raw:
                return request_time, time.perf_counter()

    raise RuntimeError(
        "H4A04P1_FREEZE_ACK_MISSING "
        f"RAW={raw.decode('utf-8', errors='replace')!r}"
    )


def wait_ready(
    dut: rawbench.DutSerial,
    timeout_s: float = 30.0,
) -> str:
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
            print(f"H4A04P1_DUT_IP={ip}")
            print("H4A04P1_DUT_READY=PASS")
            return ip
        time.sleep(0.25)

    print(last.get("_RAW", ""))
    raise RuntimeError("H4A04P1_DUT_READY_TIMEOUT")


def get_int(snap: dict[str, str], key: str) -> int:
    return rawbench.intval(snap, key)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-P1 TCP RX freeze-window case"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=float, default=15.0)
    parser.add_argument("--chunk", type=int, default=4096)
    parser.add_argument("--variant", choices=("BASE", "PROFILE"), required=True)
    args = parser.parse_args()

    if args.duration <= 0.0:
        raise SystemExit("H4A04P1_DURATION_MUST_BE_POSITIVE")
    if args.chunk != 4096:
        raise SystemExit("H4A04P1_TCP_CHUNK_MUST_BE_4096")

    print("=" * 68)
    print(" A14 H4A0.4-P1 - TCP RX FREEZE-WINDOW BOTTLENECK CASE")
    print("=" * 68)
    print(f"H4A04P1_VARIANT={args.variant}")
    print(f"H4A04P1_SERIAL={args.serial}")
    print(f"H4A04P1_DURATION_TARGET_S={args.duration}")
    print(f"H4A04P1_TCP_CHUNK={args.chunk}")
    print("H4A04P1_TAIL_SETTLE_S=0")
    print("H4A04P1_STOP_BARRIER=TCP_RX_FREEZE_NO_RESET")

    dut = rawbench.DutSerial(args.serial)
    sock: socket.socket | None = None
    measured: dict[str, str] = {}

    payload = bytes((i & 0xFF) for i in range(args.chunk))
    pc_bytes = 0
    pc_operations = 0

    try:
        dut.open()
        host = wait_ready(dut)

        sock = socket.create_connection(
            (host, 5001),
            timeout=3.0,
        )
        sock.settimeout(3.0)
        sock.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_SNDBUF,
            4 * 1024 * 1024,
        )

        sock.sendall(b"R")
        time.sleep(0.05)

        # The TCP command puts the DUT in TCP_RX. Reset once more over Serial
        # so all throughput/profile counters start from the same clean instant
        # while preserving TCP_RX mode and the established connection.
        serial_command_ack(
            dut,
            b"R",
            b"ETH14_RAW_RESET=PASS",
        )
        time.sleep(0.02)

        zero = dut.snapshot()
        if zero.get("MODE") != "TCP_RX":
            raise RuntimeError(
                f"H4A04P1_ZERO_MODE_INVALID={zero.get('MODE')}"
            )

        zero_keys = (
            "RX_BYTES",
            "RX_OPERATIONS",
            "TRANSPORT_ERRORS",
            "TCP_SPI_LOCK_ERRORS",
        )
        zero_values = {key: get_int(zero, key) for key in zero_keys}
        if any(zero_values.values()):
            raise RuntimeError(
                f"H4A04P1_ZERO_ARM_FAILED={zero_values}"
            )

        print("H4A04P1_ZERO_ARM=PASS")

        start = time.perf_counter()
        deadline = start + args.duration

        while time.perf_counter() < deadline:
            sock.sendall(payload)
            pc_bytes += len(payload)
            pc_operations += 1

        flood_end = time.perf_counter()
        elapsed_send = flood_end - start

        freeze_request, freeze_ack = freeze_tcp_rx(dut)
        freeze_request_tail = freeze_request - flood_end
        freeze_ack_tail = freeze_ack - flood_end

        measured = dut.snapshot()

        if measured.get("MODE") != "TCP_RX":
            raise RuntimeError(
                f"H4A04P1_FROZEN_MODE_INVALID={measured.get('MODE')}"
            )

    finally:
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            sock.close()
        dut.close()

    dut_bytes = get_int(measured, "RX_BYTES")
    dut_ops = get_int(measured, "RX_OPERATIONS")
    errors = get_int(measured, "TRANSPORT_ERRORS")
    tcp_locks = get_int(measured, "TCP_SPI_LOCK_ERRORS")
    udp_locks = get_int(measured, "UDP_SPI_LOCK_ERRORS")
    tcp_hold_count = get_int(measured, "TCP_SPI_HOLD_COUNT")
    tcp_hold_total_us = get_int(measured, "TCP_SPI_HOLD_TOTAL_US")
    tcp_hold_max_us = get_int(measured, "TCP_SPI_HOLD_MAX_US")

    upper_window = elapsed_send + max(0.0, freeze_request_tail)
    lower_window = elapsed_send + max(0.0, freeze_ack_tail)

    if lower_window < upper_window or upper_window <= 0.0:
        raise RuntimeError("H4A04P1_FREEZE_BOUNDS_INVALID")

    upper_mbps = rawbench.mbps(dut_bytes, upper_window)
    lower_mbps = rawbench.mbps(dut_bytes, lower_window)
    mid_mbps = (upper_mbps + lower_mbps) / 2.0
    pc_mbps = rawbench.mbps(pc_bytes, elapsed_send)

    print(f"H4A04P1_DURATION_SEND_S={elapsed_send:.6f}")
    print(
        "H4A04P1_FREEZE_REQUEST_TAIL_S="
        f"{freeze_request_tail:.6f}"
    )
    print(f"H4A04P1_FREEZE_ACK_TAIL_S={freeze_ack_tail:.6f}")
    print(f"H4A04P1_PC_BYTES={pc_bytes}")
    print(f"H4A04P1_PC_OPERATIONS={pc_operations}")
    print(f"H4A04P1_PC_MBPS={pc_mbps:.6f}")
    print(f"H4A04P1_DUT_RX_BYTES={dut_bytes}")
    print(f"H4A04P1_DUT_RX_OPERATIONS={dut_ops}")
    print(f"H4A04P1_DUT_MBPS_LOWER={lower_mbps:.6f}")
    print(f"H4A04P1_DUT_MBPS_UPPER={upper_mbps:.6f}")
    print(f"H4A04P1_DUT_MBPS_MID={mid_mbps:.6f}")
    print(f"H4A04P1_TRANSPORT_ERRORS={errors}")
    print(f"H4A04P1_TCP_SPI_LOCK_ERRORS={tcp_locks}")
    print(f"H4A04P1_UDP_SPI_LOCK_ERRORS={udp_locks}")
    print(f"H4A04P1_TCP_SPI_HOLD_COUNT={tcp_hold_count}")
    print(f"H4A04P1_TCP_SPI_HOLD_TOTAL_US={tcp_hold_total_us}")
    print(f"H4A04P1_TCP_SPI_HOLD_MAX_US={tcp_hold_max_us}")

    profile_present = all(key in measured for key in PROFILE_KEYS)
    print(
        "H4A04P1_PROFILE_PRESENT="
        f"{'YES' if profile_present else 'NO'}"
    )

    if args.variant == "PROFILE" and not profile_present:
        raise RuntimeError("H4A04P1_PROFILE_COUNTERS_MISSING")
    if args.variant == "BASE" and profile_present:
        raise RuntimeError("H4A04P1_BASE_UNEXPECTED_PROFILE_COUNTERS")

    if profile_present:
        for key in PROFILE_KEYS:
            print(f"H4A04P1_{key}={measured[key]}")

    functional = (
        pc_bytes > 0
        and dut_bytes > 0
        and dut_ops > 0
        and errors == 0
        and tcp_locks == 0
        and udp_locks == 0
        and freeze_request_tail >= 0.0
        and freeze_ack_tail >= freeze_request_tail
    )

    print(
        "H4A04P1_FUNCTIONAL_PASS="
        f"{'YES' if functional else 'NO'}"
    )
    return 0 if functional else 2


if __name__ == "__main__":
    raise SystemExit(main())
