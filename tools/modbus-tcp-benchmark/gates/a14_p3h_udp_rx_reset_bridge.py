"""P3H UDP RX bridge: enforce a clean counter reset after UDP_RX arming."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import socket
import sys
import time


RUNNER_PATH = (
    Path(__file__).resolve().parents[1]
    / "pc"
    / "eth14_raw_transport_benchmark.py"
)

if not RUNNER_PATH.is_file():
    raise SystemExit(f"P3H_RUNNER_NOT_FOUND={RUNNER_PATH}")

spec = importlib.util.spec_from_file_location(
    "eth14_raw_transport_benchmark_p3h",
    RUNNER_PATH,
)

if spec is None or spec.loader is None:
    raise SystemExit("P3H_RUNNER_IMPORT_SPEC_FAILED")

runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)


def serial_reset(dut, timeout_s: float = 1.5) -> None:
    dut.ser.reset_input_buffer()
    dut.ser.write(b"R")
    dut.ser.flush()

    deadline = time.perf_counter() + timeout_s
    raw = bytearray()

    while time.perf_counter() < deadline:
        chunk = dut.ser.read(max(1, dut.ser.in_waiting))

        if chunk:
            raw.extend(chunk)
            if b"ETH14_RAW_RESET=PASS" in raw:
                return

    text = raw.decode("utf-8", errors="replace")
    raise RuntimeError(
        "P3H_SERIAL_RESET_ACK_MISSING "
        f"RAW={text!r}"
    )


def udp_rx_bench_p3h(
    host: str,
    port: int,
    duration_s: float,
    payload_size: int,
    dut,
):
    sock = socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM,
    )

    sock.setsockopt(
        socket.SOL_SOCKET,
        socket.SO_SNDBUF,
        4 * 1024 * 1024,
    )

    target = (host, port)

    try:
        # Arm UDP_RX using the normal RAW protocol.
        sock.sendto(b"URX", target)
        time.sleep(0.10)

        armed = dut.snapshot()

        if armed.get("MODE") != "UDP_RX":
            raise RuntimeError(
                "P3H_DUT_NO_ENTRO_EN_UDP_RX"
            )

        # P3H guard: regardless of whether URX was consumed by the legacy
        # command path or seen by the fused data path, start measurement
        # from an explicit serial counter reset.
        serial_reset(dut)
        time.sleep(0.05)

        zero = dut.snapshot()

        rx_bytes_zero = runner.intval(
            zero,
            "RX_BYTES",
        )
        rx_ops_zero = runner.intval(
            zero,
            "RX_OPERATIONS",
        )
        errors_zero = runner.intval(
            zero,
            "TRANSPORT_ERRORS",
        )

        if (
            zero.get("MODE") != "UDP_RX"
            or rx_bytes_zero != 0
            or rx_ops_zero != 0
            or errors_zero != 0
        ):
            raise RuntimeError(
                "P3H_ARM_RESET_NOT_CLEAN "
                f"MODE={zero.get('MODE')} "
                f"RX_BYTES={rx_bytes_zero} "
                f"RX_OPERATIONS={rx_ops_zero} "
                f"TRANSPORT_ERRORS={errors_zero}"
            )

        print("P3H_ARM_RESET_PASS=YES")
        print("P3H_ARM_RESET_RX_BYTES=0")
        print("P3H_ARM_RESET_RX_OPERATIONS=0")

        payload = bytearray(payload_size)
        sent_packets = 0
        sent_bytes = 0

        start = time.perf_counter()
        deadline = start + duration_s
        sequence = 0

        while time.perf_counter() < deadline:
            payload[0:4] = sequence.to_bytes(
                4,
                byteorder="big",
                signed=False,
            )

            sent = sock.sendto(
                payload,
                target,
            )

            if sent == payload_size:
                sent_packets += 1
                sent_bytes += sent

            sequence = (
                sequence + 1
            ) & 0xFFFFFFFF

        elapsed = time.perf_counter() - start

        time.sleep(0.40)
        snap = dut.snapshot()

        # Preserve the original benchmark cleanup attempt. P3H validity does
        # not depend on this packet because the next run has the serial-reset
        # guard above.
        sock.sendto(b"STOP", target)

    finally:
        sock.close()

    dut_packets = runner.intval(
        snap,
        "RX_OPERATIONS",
    )
    dut_bytes = runner.intval(
        snap,
        "RX_BYTES",
    )
    errors = runner.intval(
        snap,
        "TRANSPORT_ERRORS",
    )

    loss_ops = max(
        0,
        sent_packets - dut_packets,
    )

    loss_pct = (
        (loss_ops * 100.0 / sent_packets)
        if sent_packets > 0
        else 0.0
    )

    functional = (
        sent_packets > 0
        and dut_packets > 0
        and dut_bytes > 0
        and errors == 0
    )

    return runner.BenchResult(
        mode="UDP_RX",
        duration_s=elapsed,
        pc_bytes=sent_bytes,
        dut_bytes=dut_bytes,
        pc_operations=sent_packets,
        dut_operations=dut_packets,
        errors=errors,
        pc_mbps=runner.mbps(
            sent_bytes,
            elapsed,
        ),
        dut_mbps=runner.mbps(
            dut_bytes,
            elapsed,
        ),
        loss_operations=loss_ops,
        loss_percent=loss_pct,
        pass_functional=functional,
    )


runner.udp_rx_bench = udp_rx_bench_p3h

_last_snapshot: dict[str, str] = {}
_original_snapshot = runner.DutSerial.snapshot


def snapshot_with_capture(self, *args, **kwargs):
    global _last_snapshot
    values = _original_snapshot(self, *args, **kwargs)
    _last_snapshot = dict(values)
    return values


runner.DutSerial.snapshot = snapshot_with_capture

SNAPSHOT_KEYS = (
    "RAW_SERVER_READY",
    "ETH_READY",
    "ETH_LINK",
    "IP",
    "MODE",
    "RX_BYTES",
    "RX_OPERATIONS",
    "TRANSPORT_ERRORS",
    "UDP_SPI_LOCK_ERRORS",
    "UDP_RX_PACKETS",
    "UDP_RX_PARSE_CALLS",
    "UDP_RX_PACKET_PARSE_CALLS",
    "UDP_RX_SPI_READ_CALLS",
    "UDP_RX_SPI_READ_BYTES",
    "UDP_RX_SPI_READS_PER_PACKET_X1000",
    "UDP_RX_SERVICE_HOLD_COUNT",
    "UDP_RX_ACTIVE_HOLD_COUNT",
    "UDP_RX_ACTIVE_HOLD_US_AVG",
    "UDP_RX_ACTIVE_HOLD_US_MAX",
    "UDP_RX_EMPTY_HOLD_COUNT",
    "W5100_DIAG_READ_CALLS_TOTAL",
    "W5100_DIAG_READ_BYTES_TOTAL",
    "ETH_INT_CONFIGURED",
    "ETH_INT_PIN",
    "ETH_INT_UDP_SOCKET",
    "ETH_INT_ISR_COUNT",
    "UDP_RX_INT_SKIP_COUNT",
    "UDP_RX_INT_WAKE_COUNT",
    "UDP_RX_INT_LOW_FALLBACK_COUNT",
)

exit_code = 0

try:
    runner.main()
except SystemExit as exc:
    if exc.code is None:
        exit_code = 0
    elif isinstance(exc.code, int):
        exit_code = exc.code
    else:
        print(f"P3H_RUNNER_SYSTEM_EXIT={exc.code}")
        exit_code = 1
finally:
    print()
    print("========================================")
    print(" P3H UDP RX FINAL SNAPSHOT")
    print("========================================")

    if _last_snapshot:
        print("P3H_FINAL_SNAPSHOT_PRESENT=YES")
        for key in SNAPSHOT_KEYS:
            print(
                f"P3H_FINAL_{key}="
                f"{_last_snapshot.get(key, '')}"
            )
    else:
        print("P3H_FINAL_SNAPSHOT_PRESENT=NO")

if exit_code != 0:
    raise SystemExit(exit_code)
