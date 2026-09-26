"""P3J-R2 UDP RX bridge with an explicit IDLE/quiescence barrier."""

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
    raise SystemExit(f"P3J_R2_RUNNER_NOT_FOUND={RUNNER_PATH}")

spec = importlib.util.spec_from_file_location(
    "eth14_raw_transport_benchmark_p3j_r2",
    RUNNER_PATH,
)

if spec is None or spec.loader is None:
    raise SystemExit("P3J_R2_RUNNER_IMPORT_SPEC_FAILED")

runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)


def serial_command_ack(
    dut,
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
        chunk = dut.ser.read(
            max(1, dut.ser.in_waiting)
        )

        if chunk:
            raw.extend(chunk)

            if expected_ack in raw:
                return

    text = raw.decode("utf-8", errors="replace")
    raise RuntimeError(
        "P3J_R2_SERIAL_ACK_MISSING "
        f"COMMAND={command!r} "
        f"EXPECTED={expected_ack!r} "
        f"RAW={text!r}"
    )


def force_idle(dut) -> None:
    serial_command_ack(
        dut,
        b"I",
        b"ETH14_RAW_IDLE=PASS",
    )


def serial_reset(dut) -> None:
    serial_command_ack(
        dut,
        b"R",
        b"ETH14_RAW_RESET=PASS",
    )


def wait_udp_quiescent(
    dut,
    timeout_s: float = 2.5,
    sample_period_s: float = 0.10,
    stable_intervals: int = 3,
) -> tuple[int, int]:
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
                "P3J_R2_IDLE_MODE_LOST "
                f"MODE={snap.get('MODE')}"
            )

        latest = runner.intval(
            snap,
            "P3J_R2_IDLE_UDP_DISCARDED",
        )

        if previous is not None and latest == previous:
            stable += 1
        else:
            stable = 0

        previous = latest

        if stable >= stable_intervals:
            print("P3J_R2_QUIESCENCE_PASS=YES")
            print(
                "P3J_R2_QUIESCENCE_STABLE_INTERVALS="
                f"{stable_intervals}"
            )
            print(
                "P3J_R2_QUIESCENCE_DISCARDED_TOTAL="
                f"{latest}"
            )
            print(
                "P3J_R2_QUIESCENCE_SAMPLES="
                f"{samples}"
            )
            return latest, samples

    raise RuntimeError(
        "P3J_R2_QUIESCENCE_TIMEOUT "
        f"LAST_DISCARDED={latest} "
        f"SAMPLES={samples}"
    )


def udp_rx_bench_p3j_r2(
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
    snap = {}

    try:
        # Hard lifecycle barrier: stop accounting UDP_RX before waiting for
        # residual host/network datagrams to disappear.
        force_idle(dut)

        discarded, _ = wait_udp_quiescent(dut)

        # Reset generic counters after the idle drain, then arm a fresh RX run.
        serial_reset(dut)

        sock.sendto(b"URX", target)
        time.sleep(0.08)

        armed = dut.snapshot()

        if armed.get("MODE") != "UDP_RX":
            raise RuntimeError(
                "P3J_R2_DUT_NO_ENTRO_EN_UDP_RX"
            )

        # Start the measured window from exact zero after arming.
        serial_reset(dut)
        time.sleep(0.02)

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
                "P3J_R2_ARM_RESET_NOT_CLEAN "
                f"MODE={zero.get('MODE')} "
                f"RX_BYTES={rx_bytes_zero} "
                f"RX_OPERATIONS={rx_ops_zero} "
                f"TRANSPORT_ERRORS={errors_zero} "
                f"PREARM_DISCARDED={discarded}"
            )

        print("P3J_R2_ARM_RESET_PASS=YES")
        print("P3J_R2_ARM_RESET_RX_BYTES=0")
        print("P3J_R2_ARM_RESET_RX_OPERATIONS=0")

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

        # Preserve the established raw benchmark tail-settle window.
        time.sleep(0.40)
        snap = dut.snapshot()

        # Unlike the legacy bridge, end the case with an out-of-band serial
        # IDLE barrier. This cannot be hidden behind queued UDP payload.
        force_idle(dut)

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


runner.udp_rx_bench = udp_rx_bench_p3j_r2

_original_tcp_rx_bench = runner.tcp_rx_bench


def tcp_rx_bench_p3k(
    host: str,
    port: int,
    duration_s: float,
    chunk_size: int,
    dut,
):
    # Apply the same out-of-band lifecycle barrier used by UDP before
    # starting TCP, so residual UDP traffic cannot bias transport parity.
    force_idle(dut)
    wait_udp_quiescent(dut)

    result = _original_tcp_rx_bench(
        host,
        port,
        duration_s,
        chunk_size,
        dut,
    )

    # Leave the DUT out of measurement mode for the following case.
    force_idle(dut)
    return result


runner.tcp_rx_bench = tcp_rx_bench_p3k

print("P3K_TRANSPORT_PARITY_BRIDGE=YES")
print("P3K_SHARED_IDLE_QUIESCENCE_FOR_TCP_AND_UDP=YES")

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
    "P3J_R2_IDLE_UDP_DISCARDED",
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
        print(f"P3J_R2_RUNNER_SYSTEM_EXIT={exc.code}")
        exit_code = 1
finally:
    print()
    print("========================================")
    print(" P3J-R2 UDP RX FINAL SNAPSHOT")
    print("========================================")

    if _last_snapshot:
        print("P3J_R2_FINAL_SNAPSHOT_PRESENT=YES")
        for key in SNAPSHOT_KEYS:
            print(
                f"P3J_R2_FINAL_{key}="
                f"{_last_snapshot.get(key, '')}"
            )
    else:
        print("P3J_R2_FINAL_SNAPSHOT_PRESENT=NO")

if exit_code != 0:
    raise SystemExit(exit_code)
