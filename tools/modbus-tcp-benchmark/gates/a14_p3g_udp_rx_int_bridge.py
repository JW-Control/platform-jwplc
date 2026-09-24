"""P3G UDP RX instrumentation bridge with W5500 INT diagnostics."""

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys


RUNNER_PATH = (
    Path(__file__).resolve().parents[1]
    / "pc"
    / "eth14_raw_transport_benchmark.py"
)

if not RUNNER_PATH.is_file():
    raise SystemExit(f"P3G_RUNNER_NOT_FOUND={RUNNER_PATH}")

spec = importlib.util.spec_from_file_location(
    "eth14_raw_transport_benchmark_p3g",
    RUNNER_PATH,
)

if spec is None or spec.loader is None:
    raise SystemExit("P3G_RUNNER_IMPORT_SPEC_FAILED")

runner = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = runner
spec.loader.exec_module(runner)

_last_snapshot: dict[str, str] = {}
_original_snapshot = runner.DutSerial.snapshot


def _snapshot_with_capture(self, *args, **kwargs):
    global _last_snapshot
    values = _original_snapshot(self, *args, **kwargs)
    _last_snapshot = dict(values)
    return values


runner.DutSerial.snapshot = _snapshot_with_capture

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
    "TCP_SPI_LOCK_ERRORS",
    "UDP_RX_PACKETS",
    "UDP_RX_PARSE_CALLS",
    "UDP_RX_PARSE_US_AVG",
    "UDP_RX_PARSE_US_MAX",
    "UDP_RX_PACKET_PARSE_CALLS",
    "UDP_RX_PACKET_PARSE_US_AVG",
    "UDP_RX_PACKET_PARSE_US_MAX",
    "UDP_RX_READ_CALLS",
    "UDP_RX_READ_US_AVG",
    "UDP_RX_READ_US_MAX",
    "UDP_RX_SPI_READ_CALLS",
    "UDP_RX_SPI_READ_BYTES",
    "UDP_RX_SPI_READS_PER_PACKET_X1000",
    "UDP_RX_SPI_READ_BYTES_PER_PACKET_X1000",
    "UDP_RX_SERVICE_HOLD_COUNT",
    "UDP_RX_SERVICE_HOLD_US_AVG",
    "UDP_RX_SERVICE_HOLD_US_MAX",
    "UDP_RX_ACTIVE_HOLD_COUNT",
    "UDP_RX_ACTIVE_HOLD_US_AVG",
    "UDP_RX_ACTIVE_HOLD_US_MAX",
    "UDP_RX_EMPTY_HOLD_COUNT",
    "UDP_RX_BYTES_PER_ACTIVE_HOLD_X1000",
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
        print(f"P3G_RUNNER_SYSTEM_EXIT={exc.code}")
        exit_code = 1
finally:
    print()
    print("========================================")
    print(" P3G UDP RX FINAL SNAPSHOT")
    print("========================================")

    if _last_snapshot:
        print("P3G_FINAL_SNAPSHOT_PRESENT=YES")
        for key in SNAPSHOT_KEYS:
            print(f"P3G_FINAL_{key}={_last_snapshot.get(key, '')}")
    else:
        print("P3G_FINAL_SNAPSHOT_PRESENT=NO")

if exit_code != 0:
    raise SystemExit(exit_code)
