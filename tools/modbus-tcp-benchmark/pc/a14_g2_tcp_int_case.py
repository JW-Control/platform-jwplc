#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import time

from a14_g1_tcp_spi_waste_case import (
    DutSerial,
    intval,
    mbps,
    pct,
    run_load,
    wait_ready,
)


LOADS = ("IDLE", "CONTROLLED", "SATURATED")
VARIANTS = ("POLLING", "INT_GUIDED")


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def wait_mode(
    dut: DutSerial,
    expected: str,
    timeout_s: float = 5.0,
) -> dict[str, str]:
    deadline = time.perf_counter() + timeout_s
    last: dict[str, str] = {}

    while time.perf_counter() < deadline:
        last = dut.snapshot()
        if last.get("MODE") == expected:
            return last
        time.sleep(0.05)

    raise RuntimeError(
        f"G2_MODE_TIMEOUT EXPECTED={expected} ACTUAL={last.get('MODE')}"
    )


def reconnect_probe(
    dut: DutSerial,
    host: str,
    sock: socket.socket,
) -> socket.socket:
    try:
        sock.shutdown(socket.SHUT_RDWR)
    except OSError:
        pass
    sock.close()

    # Release the freeze barrier so DISCON can wake the INT-guided scheduler.
    dut.command_ack(b"R", b"H4A04P1_RESET=PASS")

    idle = wait_mode(dut, "IDLE", 5.0)
    if idle.get("ETH_INT_CONFIGURED") != "NO":
        raise RuntimeError(
            "G2_RECONNECT_OLD_INT_NOT_DISABLED "
            f"VALUE={idle.get('ETH_INT_CONFIGURED')}"
        )

    second = socket.create_connection((host, 5001), timeout=3.0)
    second.settimeout(3.0)
    second.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

    second.sendall(b"R")
    time.sleep(0.08)

    armed = wait_mode(dut, "TCP_RX", 3.0)
    if armed.get("ETH_INT_CONFIGURED") != "YES":
        raise RuntimeError(
            "G2_RECONNECT_INT_NOT_RECONFIGURED "
            f"VALUE={armed.get('ETH_INT_CONFIGURED')}"
        )

    dut.command_ack(b"R", b"H4A04P1_RESET=PASS")
    second.sendall(bytes((i & 0xFF) for i in range(1024)))
    time.sleep(0.08)

    dut.command_ack(b"F", b"H4A04P1_FREEZE=PASS")
    snap = dut.snapshot()

    if intval(snap, "RX_BYTES") <= 0:
        raise RuntimeError("G2_RECONNECT_NO_RX")
    if intval(snap, "TRANSPORT_ERRORS") != 0:
        raise RuntimeError("G2_RECONNECT_TRANSPORT_ERROR")
    if intval(snap, "TCP_SPI_LOCK_ERRORS") != 0:
        raise RuntimeError("G2_RECONNECT_SPI_LOCK_ERROR")

    emit("G2_RECONNECT_RX_BYTES", intval(snap, "RX_BYTES"))
    emit("G2_RECONNECT_PASS", "YES")
    return second


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Alpha14 G2 TCP polling vs INT-guided case"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=float, required=True)
    parser.add_argument("--load", choices=LOADS, required=True)
    parser.add_argument("--variant", choices=VARIANTS, required=True)
    parser.add_argument("--reconnect-probe", action="store_true")
    args = parser.parse_args()

    if args.duration <= 0:
        raise SystemExit("G2_DURATION_MUST_BE_POSITIVE")

    expected_int = "YES" if args.variant == "INT_GUIDED" else "NO"

    emit("G2_LOAD", args.load)
    emit("G2_VARIANT", args.variant)
    emit("G2_DURATION_TARGET_S", f"{args.duration:.3f}")
    emit("G2_EXPECTED_INT_GUIDED", expected_int)

    dut = DutSerial(args.serial)
    sock: socket.socket | None = None

    try:
        dut.open()
        host, ready = wait_ready(dut)
        emit("G2_DUT_IP", host)

        if ready.get("TCP_PROFILE_ENABLED") != "YES":
            raise RuntimeError("G2_PROFILE_HOOKS_NOT_ENABLED")
        if ready.get("TCP_G2_INT_GUIDED") != expected_int:
            raise RuntimeError(
                "G2_INT_VARIANT_MISMATCH "
                f"EXPECTED={expected_int} ACTUAL={ready.get('TCP_G2_INT_GUIDED')}"
            )

        sock = socket.create_connection((host, 5001), timeout=3.0)
        sock.settimeout(3.0)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4 * 1024 * 1024)

        sock.sendall(b"R")
        time.sleep(0.08)

        armed = dut.snapshot()
        if armed.get("MODE") != "TCP_RX":
            raise RuntimeError(f"G2_ARM_FAILED MODE={armed.get('MODE')}")
        if armed.get("TCP_G2_INT_GUIDED") != expected_int:
            raise RuntimeError("G2_ARM_VARIANT_MISMATCH")
        if args.variant == "INT_GUIDED":
            if armed.get("ETH_INT_CONFIGURED") != "YES":
                raise RuntimeError("G2_INT_NOT_CONFIGURED")
            if intval(armed, "ETH_INT_PIN") != 15:
                raise RuntimeError("G2_INT_PIN_MISMATCH")

        dut.command_ack(b"R", b"H4A04P1_RESET=PASS")
        time.sleep(0.02)
        zero = dut.snapshot()

        for key in ("RX_BYTES", "RX_OPERATIONS"):
            if intval(zero, key) != 0:
                raise RuntimeError(
                    f"G2_ZERO_RX_COUNTER_NONZERO {key}={zero.get(key)}"
                )

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
            raise RuntimeError("G2_SERVICE_ACCOUNTING_MISMATCH")

        freeze_request_tail = freeze_request - send_end
        freeze_ack_tail = freeze_ack - send_end
        measured_window_s = send_elapsed + max(0.0, freeze_ack_tail)

        if measured_window_s <= 0:
            raise RuntimeError("G2_INVALID_MEASURED_WINDOW")

        dut_mbps = mbps(dut_bytes, measured_window_s)
        pc_mbps = mbps(pc_bytes, send_elapsed) if pc_bytes else 0.0
        pc_rate = pc_operations / send_elapsed if send_elapsed > 0 else 0.0
        occupancy = pct(hold_total_us, measured_window_s * 1_000_000.0)

        available_calls = intval(measured, "TCP_PROF_AVAILABLE_CALLS")
        available_zero = intval(measured, "TCP_PROF_AVAILABLE_ZERO_CALLS")
        available_nonzero = intval(measured, "TCP_PROF_AVAILABLE_NONZERO_CALLS")
        status_calls = intval(measured, "TCP_PROF_SOCKET_STATUS_CALLS")
        status_total_us = intval(measured, "TCP_PROF_SOCKET_STATUS_TOTAL_US")

        if available_zero + available_nonzero != available_calls:
            raise RuntimeError("G2_AVAILABLE_ACCOUNTING_MISMATCH")

        int_skip = intval(measured, "TCP_INT_SKIP_COUNT")
        int_wake = intval(measured, "TCP_INT_WAKE_COUNT")
        int_low = intval(measured, "TCP_INT_LOW_FALLBACK_COUNT")
        int_rsr_rearm = intval(measured, "TCP_INT_RSR_REARM_COUNT")
        int_pin_rearm = intval(measured, "TCP_INT_PIN_REARM_COUNT")
        int_isr = intval(measured, "ETH_INT_ISR_COUNT")

        emit("G2_DURATION_SEND_S", f"{send_elapsed:.6f}")
        emit("G2_MEASURED_WINDOW_S", f"{measured_window_s:.6f}")
        emit("G2_PC_BYTES", pc_bytes)
        emit("G2_PC_OPERATIONS", pc_operations)
        emit("G2_PC_OPERATIONS_PER_S", f"{pc_rate:.3f}")
        emit("G2_PC_MBPS", f"{pc_mbps:.6f}")
        emit("G2_DUT_RX_BYTES", dut_bytes)
        emit("G2_DUT_RX_OPERATIONS", dut_ops)
        emit("G2_DUT_MBPS", f"{dut_mbps:.6f}")
        emit("G2_TRANSPORT_ERRORS", transport_errors)
        emit("G2_TCP_SPI_LOCK_ERRORS", lock_errors)
        emit("G2_TCP_SPI_HOLD_COUNT", hold_count)
        emit("G2_TCP_SPI_HOLD_TOTAL_US", hold_total_us)
        emit(
            "G2_TCP_SPI_HOLD_AVG_US",
            f"{(hold_total_us / hold_count) if hold_count else 0.0:.3f}",
        )
        emit("G2_TCP_SPI_HOLD_MAX_US", hold_max_us)
        emit("G2_SPI_HOLD_OCCUPANCY_PCT", f"{occupancy:.3f}")
        emit("G2_TCP_SERVICE_PASSES", service_passes)
        emit("G2_TCP_SERVICE_ACTIVE_PASSES", service_active)
        emit("G2_TCP_SERVICE_EMPTY_PASSES", service_empty)
        emit("G2_TCP_SERVICE_EMPTY_PCT", f"{pct(service_empty, service_passes):.3f}")
        emit("G2_TCP_STATUS_CALLS", status_calls)
        emit("G2_TCP_STATUS_TOTAL_US", status_total_us)
        emit("G2_TCP_AVAILABLE_CALLS", available_calls)
        emit("G2_TCP_AVAILABLE_ZERO", available_zero)
        emit("G2_TCP_AVAILABLE_NONZERO", available_nonzero)
        emit("G2_TCP_AVAILABLE_ZERO_PCT", f"{pct(available_zero, available_calls):.3f}")
        emit("G2_ETH_INT_ISR_COUNT", int_isr)
        emit("G2_TCP_INT_SKIP_COUNT", int_skip)
        emit("G2_TCP_INT_WAKE_COUNT", int_wake)
        emit("G2_TCP_INT_LOW_FALLBACK_COUNT", int_low)
        emit("G2_TCP_INT_RSR_REARM_COUNT", int_rsr_rearm)
        emit("G2_TCP_INT_PIN_REARM_COUNT", int_pin_rearm)

        idle_ok = (
            args.load != "IDLE"
            or (dut_bytes == 0 and service_active == 0)
        )
        active_ok = args.load == "IDLE" or dut_bytes > 0
        int_ok = (
            args.variant != "INT_GUIDED"
            or (
                measured.get("ETH_INT_CONFIGURED") == "YES"
                and intval(measured, "ETH_INT_PIN") == 15
                and int_skip > 0
            )
        )

        functional = (
            transport_errors == 0
            and lock_errors == 0
            and idle_ok
            and active_ok
            and int_ok
            and freeze_request_tail >= 0.0
            and freeze_ack_tail >= freeze_request_tail
        )

        if args.reconnect_probe:
            if args.variant != "INT_GUIDED":
                raise RuntimeError("G2_RECONNECT_PROBE_REQUIRES_INT")
            if not functional:
                raise RuntimeError("G2_RECONNECT_SKIPPED_PRIMARY_FAIL")
            sock = reconnect_probe(dut, host, sock)

        emit("G2_FUNCTIONAL_PASS", "YES" if functional else "NO")
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
