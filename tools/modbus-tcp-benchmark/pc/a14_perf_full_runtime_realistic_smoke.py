import argparse
import sys
import time
from pathlib import Path

import serial

sys.path.insert(
    0,
    str(Path(__file__).resolve().parent)
)

import a14_perf_fc03_qualification_sweep as q
import a14_perf_fc03_125_formal_frontier as frontier


RATE = 100.0
DURATION_S = 60.0


def iv(snapshot, key, default=-1):
    return q.intval(
        snapshot,
        key,
        default
    )


def wait_full_runtime(
    ser,
    timeout_s=45.0
):
    deadline = (
        time.monotonic() +
        timeout_s
    )

    last = {}

    while time.monotonic() < deadline:
        try:
            snap = q.request_snapshot(
                ser,
                echo=False
            )
        except TimeoutError:
            time.sleep(0.25)
            continue

        last = snap

        ready = (
            snap.get("SERVER_READY") == "YES" and
            snap.get("FULL_RUNTIME_READY") == "YES" and
            snap.get("CLIENT_CONNECTED") == "NO"
        )

        if ready:
            return snap

        print(
            "WAIT_READY "
            f"SERVER={snap.get('SERVER_READY', '?')} "
            f"FULL={snap.get('FULL_RUNTIME_READY', '?')} "
            f"DISPLAY={snap.get('DISPLAY_READY', '?')} "
            f"FRAM={snap.get('FRAM_READY', '?')} "
            f"SD={snap.get('SD_READY', '?')} "
            f"RTC={snap.get('RTC_PRESENT', '?')} "
            f"BUTTONS={snap.get('BUTTONS_READY', '?')} "
            f"IO={snap.get('IO_INITIALIZED', '?')}"
        )

        time.sleep(0.5)

    raise TimeoutError(
        "FULL_RUNTIME_READY no llegó a YES. "
        f"Último snapshot: {last}"
    )


def print_peripheral_snapshot(s):
    keys = [
        "FULL_RUNTIME_READY",
        "DISPLAY_READY",
        "DISPLAY_FRAMES",
        "DISPLAY_GAP_MAX_MS",

        "FRAM_READY",
        "FRAM_CYCLES",
        "FRAM_FAILS",
        "FRAM_MAX_US",

        "SD_READY",
        "SD_APPEND_CYCLES",
        "SD_APPEND_FAILS",
        "SD_APPEND_MAX_US",
        "SD_VERIFY_CYCLES",
        "SD_VERIFY_FAILS",
        "SD_VERIFY_MAX_US",

        "RTC_PRESENT",
        "RTC_SAMPLES",
        "RTC_UNAVAILABLE",
        "RTC_STALE",
        "RTC_MAX_AGE_MS",
        "RTC_CURRENT_AGE_MS",

        "IO_INITIALIZED",
        "IO_SAMPLES",
        "IO_STALE",
        "IO_MAX_AGE_MS",
        "IO_CURRENT_AGE_MS",

        "BUTTONS_READY",
        "BUTTON_SAMPLES",
        "BUTTON_NOT_READY",
        "BUTTON_SAMPLE_GAP_MAX_MS",

        "SPI_PROBE_SAMPLES",
        "SPI_PROBE_FAILS",
        "SPI_PROBE_MAX_WAIT_US",
        "SPI_PROBE_OVER_1MS",
        "SPI_PROBE_OVER_10MS",

        "PERIPHERAL_FAILURE_COUNT",
    ]

    for key in keys:
        print(
            f"{key}="
            f"{s.get(key, 'MISSING')}"
        )


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--serial",
        default="COM14"
    )

    parser.add_argument(
        "--baud",
        type=int,
        default=115200
    )

    parser.add_argument(
        "--host",
        default="192.168.0.31"
    )

    parser.add_argument(
        "--port",
        type=int,
        default=502
    )

    parser.add_argument(
        "--duration",
        type=float,
        default=DURATION_S
    )

    parser.add_argument(
        "--rate",
        type=float,
        default=RATE
    )

    args = parser.parse_args()

    print()
    print("=" * 76)
    print(
        " A14.3 PERF-S2 "
        "FULL_RUNTIME_REALISTIC PHYSICAL SMOKE"
    )
    print("=" * 76)

    print(
        f"TARGET={args.host}:{args.port}"
    )
    print(
        f"SERIAL={args.serial}"
    )
    print(
        f"RATE_REQ_S={args.rate:.0f}"
    )
    print(
        f"DURATION_S={args.duration:.0f}"
    )
    print(
        "FC03_QUANTITY_REGISTERS=125"
    )

    ser = serial.Serial(
        args.serial,
        args.baud,
        timeout=0.10,
        write_timeout=1.0
    )

    try:
        time.sleep(0.5)

        ser.reset_input_buffer()

        initial = wait_full_runtime(
            ser,
            45.0
        )

        print()
        print(
            "INITIAL_FULL_RUNTIME_READY=PASS"
        )

        print_peripheral_snapshot(
            initial
        )

        print()
        print(
            "=== RUN 60S FC03/125 @ 100 req/s ==="
        )

        row = frontier.run_case(
            ser,
            args.host,
            args.port,
            args.rate,
            args.duration
        )

        frontier.print_result(
            row
        )

        # run_case() cierra el socket al terminar.
        # Esperamos que el server vuelva a estado
        # disconnected y tomamos un snapshot final
        # con los contadores de periféricos.
        final = q.wait_server_ready(
            ser,
            timeout_s=20.0,
            require_disconnected=True
        )

        print()
        print(
            "=== FINAL PERIPHERAL SNAPSHOT ==="
        )

        print_peripheral_snapshot(
            final
        )

    finally:
        ser.close()

    # ------------------------------------------------------------
    # Criterio TCP
    # ------------------------------------------------------------

    tcp_pass = (
        row["clean_case"] and
        row["stable_pass"] and
        row["achieved_percent"] >= 95.0
    )

    # ------------------------------------------------------------
    # Readiness permanente
    # ------------------------------------------------------------

    readiness_pass = (
        final.get("FULL_RUNTIME_READY") == "YES" and
        final.get("DISPLAY_READY") == "YES" and
        final.get("FRAM_READY") == "YES" and
        final.get("SD_READY") == "YES" and
        final.get("RTC_PRESENT") == "YES" and
        final.get("IO_INITIALIZED") == "YES" and
        final.get("BUTTONS_READY") == "YES"
    )

    # ------------------------------------------------------------
    # Cero fallos
    # ------------------------------------------------------------

    failures_pass = (
        iv(final, "PERIPHERAL_FAILURE_COUNT") == 0 and
        iv(final, "FRAM_FAILS") == 0 and
        iv(final, "SD_APPEND_FAILS") == 0 and
        iv(final, "SD_VERIFY_FAILS") == 0 and
        iv(final, "RTC_UNAVAILABLE") == 0 and
        iv(final, "RTC_STALE") == 0 and
        iv(final, "IO_STALE") == 0 and
        iv(final, "BUTTON_NOT_READY") == 0 and
        iv(final, "SPI_PROBE_FAILS") == 0
    )

    # ------------------------------------------------------------
    # Actividad mínima esperada en 60 s
    # Márgenes deliberadamente amplios:
    # sólo queremos demostrar que cada workload
    # estuvo realmente activo durante el smoke.
    # ------------------------------------------------------------

    activity_pass = (
        iv(final, "DISPLAY_FRAMES") >= 450 and
        iv(final, "FRAM_CYCLES") >= 180 and
        iv(final, "SD_APPEND_CYCLES") >= 45 and
        iv(final, "SD_VERIFY_CYCLES") >= 8 and
        iv(final, "RTC_SAMPLES") >= 180 and
        iv(final, "IO_SAMPLES") >= 2000 and
        iv(final, "BUTTON_SAMPLES") >= 2000 and
        iv(final, "SPI_PROBE_SAMPLES") >= 450
    )

    smoke_pass = (
        tcp_pass and
        readiness_pass and
        failures_pass and
        activity_pass
    )

    print()
    print("=" * 76)
    print(
        " FULL_RUNTIME_REALISTIC SMOKE SUMMARY"
    )
    print("=" * 76)

    print(
        "TCP_RESULT="
        f"{row['classification']}"
    )

    print(
        "TCP_ACHIEVED_REQ_S="
        f"{row['achieved_req_s']:.2f}"
    )

    print(
        "TCP_ACHIEVED_PCT="
        f"{row['achieved_percent']:.3f}"
    )

    print(
        "TCP_TOTAL_MBPS="
        f"{row['total_tcp_payload_mbps']:.4f}"
    )

    print(
        "TCP_USEFUL_MBPS="
        f"{row['useful_register_data_mbps']:.4f}"
    )

    print(
        "TCP_P95_US="
        f"{row['latency_p95_us']:.1f}"
    )

    print(
        "TCP_P99_US="
        f"{row['latency_p99_us']:.1f}"
    )

    print(
        "TCP_MAX_US="
        f"{row['latency_max_us']:.1f}"
    )

    print(
        "TCP_ERRORS="
        f"{row['timeouts'] + row['transport_errors'] + row['protocol_errors']}"
    )

    print(
        "TCP_CROSS_COUNT_PASS="
        f"{'YES' if row['cross_count_pass'] else 'NO'}"
    )

    print(
        "READINESS_PASS="
        f"{'YES' if readiness_pass else 'NO'}"
    )

    print(
        "PERIPHERAL_FAILURES_PASS="
        f"{'YES' if failures_pass else 'NO'}"
    )

    print(
        "PERIPHERAL_ACTIVITY_PASS="
        f"{'YES' if activity_pass else 'NO'}"
    )

    if smoke_pass:
        print(
            "A14_3_FULL_RUNTIME_REALISTIC_"
            "AUTOMATED_SMOKE=PASS_PHYSICAL"
        )

        print(
            "NEXT=VISUAL_TFT_CONFIRMATION"
        )

        return 0

    print(
        "A14_3_FULL_RUNTIME_REALISTIC_"
        "AUTOMATED_SMOKE=REVIEW"
    )

    print(
        "NEXT=ANALYZE_PERIPHERAL_OR_TCP_FAILURE"
    )

    return 1


if __name__ == "__main__":
    try:
        sys.exit(
            main()
        )

    except Exception as exc:
        print()
        print(
            "HARNESS_FATAL="
            f"{type(exc).__name__}: "
            f"{exc}"
        )

        print(
            "A14_3_FULL_RUNTIME_REALISTIC_"
            "AUTOMATED_SMOKE=FAIL_HARNESS"
        )

        sys.exit(2)