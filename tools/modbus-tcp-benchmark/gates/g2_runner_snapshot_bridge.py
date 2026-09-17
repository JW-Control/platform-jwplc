"""G2 snapshot bridge for the frozen ETH14 raw benchmark runner.

This file intentionally does not modify the frozen runner. It imports it,
wraps DutSerial.snapshot(), and emits the last snapshot captured by the
runner itself. When each benchmark mode is launched as a separate process,
this gives the exact final snapshot used by that mode instead of a later
out-of-band serial snapshot that may accumulate IDLE telemetry.
"""

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
    raise SystemExit(f"G2_FROZEN_RUNNER_NOT_FOUND={RUNNER_PATH}")

spec = importlib.util.spec_from_file_location(
    "eth14_raw_transport_benchmark_frozen",
    RUNNER_PATH,
)

if spec is None or spec.loader is None:
    raise SystemExit("G2_FROZEN_RUNNER_IMPORT_SPEC_FAILED")

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
    "TX_BYTES",
    "RX_OPERATIONS",
    "TX_OPERATIONS",
    "TRANSPORT_ERRORS",
    "LOOP_GAP_AVG_US",
    "LOOP_GAP_MAX_US",
    "UDP_BEGIN_PACKET_ERRORS",
    "UDP_WRITE_ERRORS",
    "UDP_END_PACKET_ERRORS",
    "UDP_SPI_LOCK_ERRORS",
    "TCP_SPI_LOCK_ERRORS",
    "TCP_SPI_HOLD_COUNT",
    "TCP_SPI_HOLD_TOTAL_US",
    "TCP_SPI_HOLD_AVG_US",
    "TCP_SPI_HOLD_MAX_US",
    "UDP_LAST_SHORT_WRITE_BYTES",
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
        print(f"G2_RUNNER_SYSTEM_EXIT={exc.code}")
        exit_code = 1
finally:
    print()
    print("========================================")
    print(" G2 EXACT RUNNER FINAL SNAPSHOT")
    print("========================================")

    if _last_snapshot:
        print("G2_FINAL_SNAPSHOT_PRESENT=YES")
        for key in SNAPSHOT_KEYS:
            print(f"G2_FINAL_{key}={_last_snapshot.get(key, '')}")
    else:
        print("G2_FINAL_SNAPSHOT_PRESENT=NO")

if exit_code != 0:
    raise SystemExit(exit_code)
