#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import time

from a14_h4a04p1_tcp_rx_case import DutSerial, intval, mbps, wait_ready


LOADS = ("IDLE", "CONTROLLED", "SATURATED")
CONTROLLED_HZ = 1000.0
CONTROLLED_BYTES = 12
SATURATED_BYTES = 4096


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def pct(numerator: float, denominator: float) -> float:
    if denominator <= 0:
        return 0.0
    return numerator * 100.0 / denominator


def run_load(sock: socket.socket, load: str, duration_s: float) -> tuple[int, int, float]:
    start = time.perf_counter()
    deadline = start + duration_s
    pc_bytes = 0
    pc_operations = 0

    if load == "IDLE":
        while True:
            now = time.perf_counter()
            if now >= deadline:
                break
            time.sleep(min(0.010, deadline - now))
    elif load == "CONTROLLED":
        payload = bytes((i & 0xFF) for i in range(CONTROLLED_BYTES))
        period = 1.0 / CONTROLLED_HZ
        next_send = start

        while True:
            now = time.perf_counter()
            if now >= deadline:
                break

            if now < next_send:
                time.sleep(min(0.0005, next_send - now))
                continue

            sock.sendall(payload)
            pc_bytes += len(payload)
            pc_operations += 1
            next_send += period
    else:
        payload = bytes((i & 0xFF) for i in range(SATURATED_BYTES))

        while time.perf_counter() < deadline:
            sock.sendall(payload)
            pc_bytes += len(payload)
            pc_operations += 1

    end = time.perf_counter()
    return pc_bytes, pc_operations, end - start


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Alpha14 G1 TCP SPI waste baseline case"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=float, required=True)
    parser.add_argument("--load", choices=LOADS, required=True)
    parser.add_argument("--variant", choices=("BASE", "PROFILE"), required=True)
    args = parser.parse_args()

    if args.duration <= 0:
        raise SystemExit("G1_DURATION_MUST_BE_POSITIVE")

    emit("G1_LOAD", args.load)
    emit("G1_VARIANT", args.variant)
    emit("G1_DURATION_TARGET_S", f"{args.duration:.3f}")
    emit("G1_CONTROLLED_TARGET_HZ", f"{CONTROLLED_HZ:.3f}")
    emit("G1_CONTROLLED_BYTES", CONTROLLED_BYTES)
    emit("G1_SATURATED_BYTES", SATURATED_BYTES)

    expected_profile = "YES" if args.variant == "PROFILE" else "NO"
    dut = DutSerial(args.serial)
    sock: socket.socket | None = None

    try:
        dut.open()
        host, ready = wait_ready(dut)
        emit("G1_DUT_IP", host)

        if ready.get("TCP_PROFILE_ENABLED") != expected_profile:
            raise RuntimeError(
                "G1_PROFILE_VARIANT_MISMATCH "
                f"EXPECTED={expected_profile} ACTUAL={ready.get('TCP_PROFILE_ENABLED')}"
            )

        sock = socket.create_connection((host, 5001), timeout=3.0)
        sock.settimeout(3.0)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4 * 1024 * 1024)

        # Enter TCP_RX first; the measured window starts only after the
        # connection/mode transition and after a serial-side reset.
        sock.sendall(b"R")
        time.sleep(0.08)

        armed = dut.snapshot()
        if armed.get("MODE") != "TCP_RX":
            raise RuntimeError(f"G1_ARM_FAILED MODE={armed.get('MODE')}")

        dut.command_ack(b"R", b"H4A04P1_RESET=PASS")
        time.sleep(0.02)
        zero = dut.snapshot()

        # RX accounting must still be zero. Service-pass counters are allowed
        # to advance while the DUT is connected and idle between the reset ACK
        # and this diagnostic snapshot; they are reset again immediately below
        # before the timed window.
        for key in ("RX_BYTES", "RX_OPERATIONS"):
            if intval(zero, key) != 0:
                raise RuntimeError(f"G1_ZERO_RX_COUNTER_NONZERO {key}={zero.get(key)}")

        # Exclude the zero snapshot and its idle polling from the measured
        # accounting.
        dut.command_ack(b"R", b"H4A04P1_RESET=PASS")

        pc_bytes, pc_operations, send_elapsed = run_load(
            sock,
            args.load,
            args.duration,
        )
        send_end = time.perf_counter()

        freeze_request, freeze_ack = dut.command_ack(
            b"F",
            b"H4A04P1_FREEZE=PASS",
        )
        measured = dut.snapshot()

        if measured.get("TCP_RX_FROZEN") != "YES":
            raise RuntimeError("G1_FREEZE_MISSING")
        if measured.get("TCP_PROFILE_ENABLED") != expected_profile:
            raise RuntimeError("G1_PROFILE_SNAPSHOT_MISMATCH")

        dut_bytes = intval(measured, "RX_BYTES")
        dut_ops = intval(measured, "RX_OPERATIONS")
        transport_errors = intval(measured, "TRANSPORT_ERRORS")
        lock_errors = intval(measured, "TCP_SPI_LOCK_ERRORS")
        hold_count = intval(measured, "TCP_SPI_HOLD_COUNT")
        hold_total_us = intval(measured, "TCP_SPI_HOLD_TOTAL_US")
        hold_max_us = intval(measured, "TCP_SPI_HOLD_MAX_US")
        service_passes = intval(measured, "TCP_SERVICE_PASSES")
        service_active = intval(measured, "TCP_SERVICE_ACTIVE_PASSES")
        service_empty = intval(measured, "TCP_SERVICE_EMPTY_PASSES")

        if service_active + service_empty != service_passes:
            raise RuntimeError(
                "G1_SERVICE_ACCOUNTING_MISMATCH "
                f"PASSES={service_passes} ACTIVE={service_active} EMPTY={service_empty}"
            )

        freeze_request_tail = freeze_request - send_end
        freeze_ack_tail = freeze_ack - send_end
        measured_window_s = send_elapsed + max(0.0, freeze_ack_tail)

        if measured_window_s <= 0:
            raise RuntimeError("G1_INVALID_MEASURED_WINDOW")

        dut_mbps = mbps(dut_bytes, measured_window_s)
        pc_mbps = mbps(pc_bytes, send_elapsed) if pc_bytes else 0.0
        pc_rate = pc_operations / send_elapsed if send_elapsed > 0 else 0.0
        hold_occupancy = pct(hold_total_us, measured_window_s * 1_000_000.0)

        emit("G1_DURATION_SEND_S", f"{send_elapsed:.6f}")
        emit("G1_MEASURED_WINDOW_S", f"{measured_window_s:.6f}")
        emit("G1_PC_BYTES", pc_bytes)
        emit("G1_PC_OPERATIONS", pc_operations)
        emit("G1_PC_OPERATIONS_PER_S", f"{pc_rate:.3f}")
        emit("G1_PC_MBPS", f"{pc_mbps:.6f}")
        emit("G1_DUT_RX_BYTES", dut_bytes)
        emit("G1_DUT_RX_OPERATIONS", dut_ops)
        emit("G1_DUT_MBPS", f"{dut_mbps:.6f}")
        emit("G1_TRANSPORT_ERRORS", transport_errors)
        emit("G1_TCP_SPI_LOCK_ERRORS", lock_errors)
        emit("G1_TCP_SPI_HOLD_COUNT", hold_count)
        emit("G1_TCP_SPI_HOLD_TOTAL_US", hold_total_us)
        emit(
            "G1_TCP_SPI_HOLD_AVG_US",
            f"{(hold_total_us / hold_count) if hold_count else 0.0:.3f}",
        )
        emit("G1_TCP_SPI_HOLD_MAX_US", hold_max_us)
        emit("G1_SPI_HOLD_OCCUPANCY_PCT", f"{hold_occupancy:.3f}")
        emit("G1_TCP_SERVICE_PASSES", service_passes)
        emit("G1_TCP_SERVICE_ACTIVE_PASSES", service_active)
        emit("G1_TCP_SERVICE_EMPTY_PASSES", service_empty)
        emit("G1_TCP_SERVICE_EMPTY_PCT", f"{pct(service_empty, service_passes):.3f}")

        if args.variant == "PROFILE":
            available_calls = intval(measured, "TCP_PROF_AVAILABLE_CALLS")
            available_zero = intval(measured, "TCP_PROF_AVAILABLE_ZERO_CALLS")
            available_nonzero = intval(
                measured,
                "TCP_PROF_AVAILABLE_NONZERO_CALLS",
            )
            status_calls = intval(measured, "TCP_PROF_SOCKET_STATUS_CALLS")
            status_total_us = intval(
                measured,
                "TCP_PROF_SOCKET_STATUS_TOTAL_US",
            )
            available_total_us = intval(
                measured,
                "TCP_PROF_AVAILABLE_TOTAL_US",
            )
            payload_bytes = intval(measured, "TCP_PROF_PAYLOAD_BYTES")
            commit_calls = intval(measured, "TCP_PROF_COMMIT_CALLS")
            commit_total_us = intval(measured, "TCP_PROF_COMMIT_TOTAL_US")

            if available_zero + available_nonzero != available_calls:
                raise RuntimeError(
                    "G1_AVAILABLE_ACCOUNTING_MISMATCH "
                    f"CALLS={available_calls} ZERO={available_zero} "
                    f"NONZERO={available_nonzero}"
                )
            if payload_bytes != dut_bytes:
                raise RuntimeError(
                    "G1_PAYLOAD_BYTES_MISMATCH "
                    f"PROFILE={payload_bytes} DUT={dut_bytes}"
                )

            emit("G1_TCP_STATUS_CALLS", status_calls)
            emit("G1_TCP_STATUS_TOTAL_US", status_total_us)
            emit("G1_TCP_AVAILABLE_CALLS", available_calls)
            emit("G1_TCP_AVAILABLE_ZERO", available_zero)
            emit("G1_TCP_AVAILABLE_NONZERO", available_nonzero)
            emit(
                "G1_TCP_AVAILABLE_ZERO_PCT",
                f"{pct(available_zero, available_calls):.3f}",
            )
            emit("G1_TCP_AVAILABLE_TOTAL_US", available_total_us)
            emit("G1_TCP_PAYLOAD_BYTES", payload_bytes)
            emit("G1_TCP_COMMIT_CALLS", commit_calls)
            emit("G1_TCP_COMMIT_TOTAL_US", commit_total_us)
            emit(
                "G1_TCP_SPI_US_PER_RX_BYTE",
                f"{(hold_total_us / dut_bytes) if dut_bytes else 0.0:.9f}",
            )
            emit(
                "G1_TCP_SPI_HOLDS_PER_RX_OPERATION",
                f"{(hold_count / dut_ops) if dut_ops else 0.0:.6f}",
            )

        idle_ok = (
            args.load != "IDLE"
            or (dut_bytes == 0 and service_active == 0)
        )
        active_ok = args.load == "IDLE" or dut_bytes > 0

        functional = (
            service_passes > 0
            and service_active + service_empty == service_passes
            and transport_errors == 0
            and lock_errors == 0
            and idle_ok
            and active_ok
            and freeze_request_tail >= 0.0
            and freeze_ack_tail >= freeze_request_tail
        )

        emit("G1_FUNCTIONAL_PASS", "YES" if functional else "NO")
        return 0 if functional else 2

    finally:
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            sock.close()
        dut.close()


if __name__ == "__main__":
    raise SystemExit(main())
