#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import time
from pathlib import Path
import sys

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
        "H4A02_SERIAL_ACK_MISSING "
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
            print(f"H4A02_DUT_IP={ip}")
            print("H4A02_DUT_READY=PASS")
            return ip
        time.sleep(0.25)

    print(last.get("_RAW", ""))
    raise RuntimeError("H4A02_DUT_READY_TIMEOUT")


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
                f"H4A02_IDLE_MODE_LOST MODE={snap.get('MODE')}"
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
            print("H4A02_QUIESCENCE_PASS=YES")
            print(f"H4A02_QUIESCENCE_SAMPLES={samples}")
            print(f"H4A02_QUIESCENCE_DISCARDED_TOTAL={latest}")
            return latest

    raise RuntimeError(
        "H4A02_QUIESCENCE_TIMEOUT "
        f"LAST_DISCARDED={latest} SAMPLES={samples}"
    )


def emit_snapshot(snap: dict[str, str]) -> None:
    keys = (
        "RAW_SERVER_READY",
        "ETH_READY",
        "ETH_LINK",
        "IP",
        "MODE",
        "RX_BYTES",
        "RX_OPERATIONS",
        "TRANSPORT_ERRORS",
        "LOOP_GAP_AVG_US",
        "LOOP_GAP_MAX_US",
        "UDP_SPI_LOCK_ERRORS",
        "UDP_RX_PACKETS",
        "UDP_RX_SPI_READ_CALLS",
        "UDP_RX_SPI_READ_BYTES",
        "UDP_RX_SPI_READS_PER_PACKET_X1000",
        "UDP_RX_SERVICE_HOLD_COUNT",
        "UDP_RX_ACTIVE_HOLD_COUNT",
        "UDP_RX_ACTIVE_HOLD_US_AVG",
        "UDP_RX_ACTIVE_HOLD_US_MAX",
        "UDP_RX_EMPTY_HOLD_COUNT",
        "ETH_INT_CONFIGURED",
        "ETH_INT_PIN",
        "ETH_INT_UDP_SOCKET",
        "ETH_INT_ISR_COUNT",
        "UDP_RX_INT_SKIP_COUNT",
        "UDP_RX_INT_WAKE_COUNT",
        "UDP_RX_INT_LOW_FALLBACK_COUNT",
        "W5100_DIAG_READ_CALLS_TOTAL",
        "W5100_DIAG_READ_BYTES_TOTAL",
    )
    for key in keys:
        if key in snap:
            print(f"H4A02_SNAPSHOT_{key}={snap[key]}")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.2 matched clean UDP RX case"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=float, default=15.0)
    parser.add_argument("--udp-payload", type=int, default=1016)
    parser.add_argument("--variant", required=True)
    args = parser.parse_args()

    if args.duration < 10.0:
        raise SystemExit("H4A02_DURATION_MUST_BE_AT_LEAST_10S")
    if args.udp_payload != 1016:
        raise SystemExit("H4A02_UDP_PAYLOAD_MUST_BE_1016")

    print("============================================================")
    print(" A14 H4A0.2 - MATCHED CLEAN UDP RX SINGLE CASE")
    print("============================================================")
    print(f"H4A02_VARIANT={args.variant}")
    print(f"H4A02_SERIAL={args.serial}")
    print(f"H4A02_DURATION_S={args.duration}")
    print(f"H4A02_UDP_PAYLOAD={args.udp_payload}")

    dut = rawbench.DutSerial(args.serial)
    sock: socket.socket | None = None
    measured: dict[str, str] = {}

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
                f"H4A02_ARM_FAILED MODE={armed.get('MODE')}"
            )

        serial_reset(dut)
        time.sleep(0.02)

        zero = dut.snapshot()
        zero_bytes = rawbench.intval(zero, "RX_BYTES")
        zero_ops = rawbench.intval(zero, "RX_OPERATIONS")
        zero_errors = rawbench.intval(zero, "TRANSPORT_ERRORS")

        if (
            zero.get("MODE") != "UDP_RX"
            or zero_bytes != 0
            or zero_ops != 0
            or zero_errors != 0
        ):
            raise RuntimeError(
                "H4A02_ZERO_ARM_FAILED "
                f"MODE={zero.get('MODE')} "
                f"RX_BYTES={zero_bytes} "
                f"RX_OPERATIONS={zero_ops} "
                f"TRANSPORT_ERRORS={zero_errors}"
            )

        print("H4A02_ZERO_ARM=PASS")

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

        time.sleep(0.40)
        measured = dut.snapshot()

        dut_packets = rawbench.intval(measured, "RX_OPERATIONS")
        dut_bytes = rawbench.intval(measured, "RX_BYTES")
        errors = rawbench.intval(measured, "TRANSPORT_ERRORS")

        force_idle(dut)
        discarded = wait_quiescent(dut)

    finally:
        if sock is not None:
            sock.close()
        dut.close()

    pc_mbps = rawbench.mbps(sent_bytes, elapsed)
    dut_mbps = rawbench.mbps(dut_bytes, elapsed)
    loss_ops = max(0, sent_packets - dut_packets)
    loss_pct = (
        loss_ops * 100.0 / sent_packets
        if sent_packets > 0
        else 0.0
    )

    emit_snapshot(measured)

    print(f"H4A02_PC_PACKETS={sent_packets}")
    print(f"H4A02_DUT_PACKETS={dut_packets}")
    print(f"H4A02_PC_MBPS={pc_mbps:.6f}")
    print(f"H4A02_DUT_MBPS={dut_mbps:.6f}")
    print(f"H4A02_LOSS_PERCENT={loss_pct:.6f}")
    print(f"H4A02_TRANSPORT_ERRORS={errors}")
    print(f"H4A02_POSTRUN_DISCARDED={discarded}")

    functional = (
        sent_packets > 0
        and dut_packets > 0
        and dut_bytes > 0
        and errors == 0
    )

    print(
        "H4A02_FUNCTIONAL_PASS="
        f"{'YES' if functional else 'NO'}"
    )

    return 0 if functional else 2


if __name__ == "__main__":
    raise SystemExit(main())
