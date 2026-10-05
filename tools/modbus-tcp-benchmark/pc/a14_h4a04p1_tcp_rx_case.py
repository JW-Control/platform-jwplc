#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import time

import serial

SNAPSHOT_END = "H4A04P1_SNAPSHOT=END"


class DutSerial:
    def __init__(self, port: str, baudrate: int = 115200):
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baudrate
        self.ser.timeout = 0.05
        self.ser.write_timeout = 1.0
        self.ser.dtr = False
        self.ser.rts = False

    def open(self) -> None:
        self.ser.open()
        time.sleep(2.0)

    def close(self) -> None:
        if self.ser.is_open:
            self.ser.close()

    def command_ack(
        self,
        command: bytes,
        expected: bytes,
        timeout_s: float = 1.5,
    ) -> tuple[float, float]:
        self.ser.reset_input_buffer()
        request = time.perf_counter()
        self.ser.write(command)
        self.ser.flush()

        deadline = request + timeout_s
        raw = bytearray()

        while time.perf_counter() < deadline:
            chunk = self.ser.read(max(1, self.ser.in_waiting))
            if chunk:
                raw.extend(chunk)
                if expected in raw:
                    return request, time.perf_counter()

        raise RuntimeError(
            "H4A04P1_SERIAL_ACK_MISSING "
            f"COMMAND={command!r} EXPECTED={expected!r} "
            f"RAW={raw.decode('utf-8', errors='replace')!r}"
        )

    def snapshot(self, timeout_s: float = 2.5) -> dict[str, str]:
        self.ser.reset_input_buffer()
        self.ser.write(b"S")
        self.ser.flush()

        deadline = time.perf_counter() + timeout_s
        raw = bytearray()

        while time.perf_counter() < deadline:
            chunk = self.ser.read(max(1, self.ser.in_waiting))
            if chunk:
                raw.extend(chunk)
                if SNAPSHOT_END.encode() in raw:
                    break

        text = raw.decode("utf-8", errors="replace")
        values: dict[str, str] = {}

        for line in text.splitlines():
            if "=" not in line:
                continue
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()

        values["_RAW"] = text

        if SNAPSHOT_END not in text:
            raise RuntimeError(
                "H4A04P1_SNAPSHOT_TIMEOUT "
                f"RAW={text!r}"
            )

        return values


def intval(values: dict[str, str], key: str) -> int:
    try:
        return int(values[key])
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError(f"H4A04P1_INVALID_INT_{key}") from exc


def mbps(byte_count: int, elapsed_s: float) -> float:
    if elapsed_s <= 0.0:
        raise RuntimeError("H4A04P1_NONPOSITIVE_WINDOW")
    return byte_count * 8.0 / elapsed_s / 1_000_000.0


def wait_ready(
    dut: DutSerial,
    timeout_s: float = 30.0,
) -> tuple[str, dict[str, str]]:
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
            return ip, last

        time.sleep(0.25)

    raise RuntimeError(
        "H4A04P1_DUT_READY_TIMEOUT "
        f"LAST={last.get('_RAW', '')}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-P1 package-first TCP RX case"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=float, default=15.0)
    parser.add_argument("--chunk", type=int, default=4096)
    parser.add_argument("--max-chunks", type=int, default=8)
    parser.add_argument(
        "--variant",
        choices=("BASE", "PROFILE"),
        required=True,
    )
    parser.add_argument(
        "--prepare-next-reconnect",
        action="store_true",
        help=(
            "Cierra el socket, libera la barrera F y espera MODE=IDLE "
            "antes de terminar. Solo se usa entre intentos de reconexion."
        ),
    )
    args = parser.parse_args()

    if args.duration <= 0.0:
        raise SystemExit("H4A04P1_DURATION_MUST_BE_POSITIVE")
    if args.chunk != 4096:
        raise SystemExit("H4A04P1_TCP_CHUNK_MUST_BE_4096")
    if args.max_chunks not in (8, 16, 32):
        raise SystemExit("H4A04P1_RX_MAX_CHUNKS_MUST_BE_8_16_OR_32")

    print("=" * 72)
    print(" A14 H4A0.4-P1 - PACKAGE-FIRST TCP RX CASE")
    print("=" * 72)
    print(f"H4A04P1_VARIANT={args.variant}")
    print(f"H4A04P1_SERIAL={args.serial}")
    print(f"H4A04P1_DURATION_TARGET_S={args.duration}")
    print(f"H4A04P1_TCP_CHUNK={args.chunk}")
    print(f"H4A04P1_TCP_RX_MAX_CHUNKS_PER_LOCK={args.max_chunks}")
    print("H4A04P1_STOP_BARRIER=SERIAL_FREEZE_NO_RESET")
    print("H4A04P1_PRODUCT_SOURCE=JWPLC/2.1.0_CANONICAL_PACKAGE")

    dut = DutSerial(args.serial)
    sock: socket.socket | None = None

    try:
        dut.open()
        host, ready = wait_ready(dut)

        print(f"H4A04P1_DUT_IP={host}")
        print("H4A04P1_DUT_READY=PASS")

        actual_max_chunks = intval(
            ready,
            "TCP_RX_MAX_CHUNKS_PER_LOCK",
        )
        if actual_max_chunks != args.max_chunks:
            raise RuntimeError(
                "H4A04P1_RX_MAX_CHUNKS_MISMATCH "
                f"EXPECTED={args.max_chunks} ACTUAL={actual_max_chunks}"
            )

        expected_profile = (
            "YES"
            if args.variant == "PROFILE"
            else "NO"
        )
        actual_profile = ready.get(
            "TCP_PROFILE_ENABLED",
            "",
        )

        if actual_profile != expected_profile:
            raise RuntimeError(
                "H4A04P1_PROFILE_VARIANT_MISMATCH "
                f"EXPECTED={expected_profile} ACTUAL={actual_profile}"
            )

        print(f"TCP_PROFILE_ENABLED={actual_profile}")

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

        # Arm TCP RX from the network side.
        sock.sendall(b"R")
        time.sleep(0.08)

        armed = dut.snapshot()
        if armed.get("MODE") != "TCP_RX":
            raise RuntimeError(
                "H4A04P1_ARM_FAILED "
                f"MODE={armed.get('MODE')}"
            )

        # Reset counters/profile after the mode and connection are established.
        dut.command_ack(
            b"R",
            b"H4A04P1_RESET=PASS",
        )
        time.sleep(0.02)

        zero = dut.snapshot()
        zero_values = {
            "RX_BYTES": intval(zero, "RX_BYTES"),
            "RX_OPERATIONS": intval(zero, "RX_OPERATIONS"),
            "TRANSPORT_ERRORS": intval(
                zero,
                "TRANSPORT_ERRORS",
            ),
            "TCP_SPI_LOCK_ERRORS": intval(
                zero,
                "TCP_SPI_LOCK_ERRORS",
            ),
        }

        if zero.get("MODE") != "TCP_RX":
            raise RuntimeError(
                "H4A04P1_ZERO_MODE_INVALID "
                f"MODE={zero.get('MODE')}"
            )

        if any(zero_values.values()):
            raise RuntimeError(
                f"H4A04P1_ZERO_ARM_FAILED={zero_values}"
            )

        print("H4A04P1_ZERO_ARM=PASS")

        # Reset once more immediately before the timed flood so PROFILE does
        # not include the idle status/available calls spent producing the
        # zero snapshot above.
        dut.command_ack(
            b"R",
            b"H4A04P1_RESET=PASS",
        )

        payload = bytes(
            (i & 0xFF)
            for i in range(args.chunk)
        )
        pc_bytes = 0
        pc_operations = 0

        start = time.perf_counter()
        deadline = start + args.duration

        while time.perf_counter() < deadline:
            sock.sendall(payload)
            pc_bytes += len(payload)
            pc_operations += 1

        send_end = time.perf_counter()
        send_elapsed = send_end - start

        freeze_request, freeze_ack = dut.command_ack(
            b"F",
            b"H4A04P1_FREEZE=PASS",
        )

        freeze_request_tail = freeze_request - send_end
        freeze_ack_tail = freeze_ack - send_end

        measured = dut.snapshot()

        if measured.get("MODE") != "TCP_RX":
            raise RuntimeError(
                "H4A04P1_FROZEN_MODE_INVALID "
                f"MODE={measured.get('MODE')}"
            )

        if measured.get("TCP_RX_FROZEN") != "YES":
            raise RuntimeError(
                "H4A04P1_FROZEN_FLAG_MISSING"
            )

        actual_profile = measured.get(
            "TCP_PROFILE_ENABLED",
            "",
        )
        if actual_profile != expected_profile:
            raise RuntimeError(
                "H4A04P1_PROFILE_SNAPSHOT_MISMATCH "
                f"EXPECTED={expected_profile} ACTUAL={actual_profile}"
            )

        dut_bytes = intval(measured, "RX_BYTES")
        dut_ops = intval(measured, "RX_OPERATIONS")
        transport_errors = intval(
            measured,
            "TRANSPORT_ERRORS",
        )
        lock_errors = intval(
            measured,
            "TCP_SPI_LOCK_ERRORS",
        )
        hold_count = intval(
            measured,
            "TCP_SPI_HOLD_COUNT",
        )
        hold_total_us = intval(
            measured,
            "TCP_SPI_HOLD_TOTAL_US",
        )
        hold_max_us = intval(
            measured,
            "TCP_SPI_HOLD_MAX_US",
        )

        upper_window = (
            send_elapsed
            + max(0.0, freeze_request_tail)
        )
        lower_window = (
            send_elapsed
            + max(0.0, freeze_ack_tail)
        )

        if (
            upper_window <= 0.0
            or lower_window < upper_window
        ):
            raise RuntimeError(
                "H4A04P1_FREEZE_BOUNDS_INVALID"
            )

        dut_upper = mbps(
            dut_bytes,
            upper_window,
        )
        dut_lower = mbps(
            dut_bytes,
            lower_window,
        )
        dut_mid = (
            dut_lower
            + dut_upper
        ) / 2.0

        pc_mbps = mbps(
            pc_bytes,
            send_elapsed,
        )

        print(f"H4A04P1_DURATION_SEND_S={send_elapsed:.6f}")
        print(
            "H4A04P1_FREEZE_REQUEST_TAIL_S="
            f"{freeze_request_tail:.6f}"
        )
        print(
            "H4A04P1_FREEZE_ACK_TAIL_S="
            f"{freeze_ack_tail:.6f}"
        )
        print(f"H4A04P1_PC_BYTES={pc_bytes}")
        print(f"H4A04P1_PC_OPERATIONS={pc_operations}")
        print(f"H4A04P1_PC_MBPS={pc_mbps:.6f}")
        print(f"H4A04P1_DUT_RX_BYTES={dut_bytes}")
        print(f"H4A04P1_DUT_RX_OPERATIONS={dut_ops}")
        print(
            f"H4A04P1_DUT_MBPS_LOWER={dut_lower:.6f}"
        )
        print(
            f"H4A04P1_DUT_MBPS_UPPER={dut_upper:.6f}"
        )
        print(
            f"H4A04P1_DUT_MBPS_MID={dut_mid:.6f}"
        )
        print(
            f"H4A04P1_TRANSPORT_ERRORS={transport_errors}"
        )
        print(
            f"H4A04P1_TCP_SPI_LOCK_ERRORS={lock_errors}"
        )
        print(
            f"H4A04P1_TCP_SPI_HOLD_COUNT={hold_count}"
        )
        print(
            f"H4A04P1_TCP_SPI_HOLD_TOTAL_US={hold_total_us}"
        )
        print(
            f"H4A04P1_TCP_SPI_HOLD_MAX_US={hold_max_us}"
        )

        if "PAYLOAD_VERIFY_ENABLED" in measured:
            print(
                "H4A04P1_PAYLOAD_VERIFY_ENABLED="
                f"{measured['PAYLOAD_VERIFY_ENABLED']}"
            )

        if "RX_FNV1A32" in measured:
            print(
                "H4A04P1_RX_FNV1A32="
                f"{measured['RX_FNV1A32']}"
            )

        if "SPI_CHUNK_PROFILE_ENABLED" in measured:
            chunk_profile_enabled = measured[
                "SPI_CHUNK_PROFILE_ENABLED"
            ]
            print(
                "H4A04P1_SPI_CHUNK_PROFILE_ENABLED="
                f"{chunk_profile_enabled}"
            )

            if chunk_profile_enabled == "YES":
                chunk_profile_keys = (
                    "SPI_CHUNK_COUNT",
                    "SPI_CHUNK_BYTES",
                    "SPI_CHUNK_SETUP_TOTAL_US",
                    "SPI_CHUNK_WIRE_WAIT_TOTAL_US",
                    "SPI_CHUNK_COPY_OUT_TOTAL_US",
                    "SPI_CHUNK_OTHER_TOTAL_US",
                )

                for key in chunk_profile_keys:
                    print(
                        f"H4A04P1_{key}="
                        f"{intval(measured, key)}"
                    )

        if args.variant == "PROFILE":
            profile_keys = (
                "TCP_PROF_SOCKET_STATUS_CALLS",
                "TCP_PROF_SOCKET_STATUS_TOTAL_US",
                "TCP_PROF_AVAILABLE_CALLS",
                "TCP_PROF_AVAILABLE_TOTAL_US",
                "TCP_PROF_AVAILABLE_RSR_REFRESH_CALLS",
                "TCP_PROF_AVAILABLE_RSR_REFRESH_TOTAL_US",
                "TCP_PROF_RECV_CALLS",
                "TCP_PROF_RECV_TOTAL_US",
                "TCP_PROF_RECV_RSR_REFRESH_CALLS",
                "TCP_PROF_RECV_RSR_REFRESH_TOTAL_US",
                "TCP_PROF_PAYLOAD_READ_CALLS",
                "TCP_PROF_PAYLOAD_READ_TOTAL_US",
                "TCP_PROF_PAYLOAD_BYTES",
                "TCP_PROF_COMMIT_CALLS",
                "TCP_PROF_COMMIT_TOTAL_US",
            )

            for key in profile_keys:
                print(
                    f"H4A04P1_{key}="
                    f"{intval(measured, key)}"
                )

        functional = (
            pc_bytes > 0
            and dut_bytes > 0
            and dut_ops > 0
            and transport_errors == 0
            and lock_errors == 0
            and freeze_request_tail >= 0.0
            and freeze_ack_tail >= freeze_request_tail
        )

        print(
            "H4A04P1_FUNCTIONAL_PASS="
            f"{'YES' if functional else 'NO'}"
        )

        if args.prepare_next_reconnect and functional:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            sock.close()
            sock = None

            # F is an accounting barrier: explicitly release it after the
            # measured socket is closed so the DUT can drain any queued TCP
            # bytes, observe CLOSE_WAIT, and return to IDLE before the next
            # connection attempt. Counters after this point are not measured.
            dut.command_ack(
                b"R",
                b"H4A04P1_RESET=PASS",
            )
            cleanup_deadline = time.perf_counter() + 12.0
            cleanup_snapshot: dict[str, str] = {}

            while time.perf_counter() < cleanup_deadline:
                cleanup_snapshot = dut.snapshot()
                if cleanup_snapshot.get("MODE") == "IDLE":
                    break
                time.sleep(0.10)
            else:
                raise RuntimeError(
                    "H4A04P1_RECONNECT_CLEANUP_TIMEOUT "
                    f"MODE={cleanup_snapshot.get('MODE')}"
                )

            print("H4A04P1_RECONNECT_CLEANUP=PASS")

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
