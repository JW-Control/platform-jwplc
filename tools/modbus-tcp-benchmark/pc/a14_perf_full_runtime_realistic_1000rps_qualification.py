import argparse
import csv
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
import a14_perf_full_runtime_realistic_smoke as smoke


RATE = 1000.0
DURATION_S = 60.0

TCP_ONLY_TOTAL_MBPS = 2.1680
TCP_ONLY_USEFUL_MBPS = 2.0000


def iv(snapshot, key, default=-1):
    return q.intval(
        snapshot,
        key,
        default
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
        "--rate",
        type=float,
        default=RATE
    )

    parser.add_argument(
        "--duration",
        type=float,
        default=DURATION_S
    )

    parser.add_argument(
        "--csv",
        required=True
    )

    args = parser.parse_args()

    print()
    print("=" * 76)
    print(
        " A14.3 PERF-S2 FULL_RUNTIME_REALISTIC "
        "@ 1000 req/s QUALIFICATION"
    )
    print("=" * 76)

    print(
        f"TARGET={args.host}:{args.port}"
    )

    print(
        f"SERIAL={args.serial}"
    )

    print(
        f"REQUESTED_REQ_S={args.rate:.0f}"
    )

    print(
        f"DURATION_S={args.duration:.0f}"
    )

    print(
        "FC03_QUANTITY_REGISTERS=125"
    )

    print(
        "STABLE_THRESHOLD_PCT=95.0"
    )

    print(
        "TCP_ONLY_REFERENCE_REQ_S=1000"
    )

    print(
        "TCP_ONLY_REFERENCE_TOTAL_MBPS=2.1680"
    )

    print(
        "TCP_ONLY_REFERENCE_USEFUL_MBPS=2.0000"
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

        initial = smoke.wait_full_runtime(
            ser,
            timeout_s=45.0
        )

        print()
        print(
            "INITIAL_FULL_RUNTIME_READY=PASS"
        )

        smoke.print_peripheral_snapshot(
            initial
        )

        print()
        print(
            "=== RUN 60S FC03/125 @ 1000 req/s ==="
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

        final = q.wait_server_ready(
            ser,
            timeout_s=20.0,
            require_disconnected=True
        )

        print()
        print(
            "=== FINAL PERIPHERAL SNAPSHOT ==="
        )

        smoke.print_peripheral_snapshot(
            final
        )

    finally:
        ser.close()

    # ------------------------------------------------------------
    # TCP
    # ------------------------------------------------------------

    tcp_clean = (
        row["clean_case"] and
        row["cross_count_pass"] and
        row["timeouts"] == 0 and
        row["transport_errors"] == 0 and
        row["protocol_errors"] == 0 and
        row["server_bus_lock_timeouts"] == 0
    )

    rate_pass = (
        row["achieved_percent"] >= 95.0
    )

    tcp_stable_pass = (
        tcp_clean and
        rate_pass
    )

    # ------------------------------------------------------------
    # Readiness
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
    # Fallos periféricos
    # ------------------------------------------------------------

    peripheral_failures_pass = (
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
    # Actividad periférica.
    #
    # Nominal en 60 s:
    # TFT       ~600
    # FRAM      ~240
    # SD append ~60
    # SD verify ~12
    # RTC       ~240
    # I/O       ~3000
    # buttons   ~3000
    # SPI probe ~600
    #
    # El gate exige al menos 50 % de la actividad nominal.
    # ------------------------------------------------------------

    scale = (
        args.duration /
        60.0
    )

    activity_pass = (
        iv(final, "DISPLAY_FRAMES") >= int(300 * scale) and
        iv(final, "FRAM_CYCLES") >= int(120 * scale) and
        iv(final, "SD_APPEND_CYCLES") >= int(30 * scale) and
        iv(final, "SD_VERIFY_CYCLES") >= int(6 * scale) and
        iv(final, "RTC_SAMPLES") >= int(120 * scale) and
        iv(final, "IO_SAMPLES") >= int(1500 * scale) and
        iv(final, "BUTTON_SAMPLES") >= int(1500 * scale) and
        iv(final, "SPI_PROBE_SAMPLES") >= int(300 * scale)
    )

    # ------------------------------------------------------------
    # Freshness adicional
    # ------------------------------------------------------------

    freshness_pass = (
        iv(final, "DISPLAY_GAP_MAX_MS") <= 500 and
        iv(final, "BUTTON_SAMPLE_GAP_MAX_MS") <= 500 and
        iv(final, "IO_MAX_AGE_MS") <= 100 and
        iv(final, "RTC_MAX_AGE_MS") <= 2500
    )

    peripheral_pass = (
        readiness_pass and
        peripheral_failures_pass and
        activity_pass and
        freshness_pass
    )

    qualification_pass = (
        tcp_stable_pass and
        peripheral_pass
    )

    clean_saturation = (
        tcp_clean and
        not rate_pass and
        peripheral_pass
    )

    total_retention_pct = (
        row["total_tcp_payload_mbps"] /
        TCP_ONLY_TOTAL_MBPS *
        100.0
    )

    useful_retention_pct = (
        row["useful_register_data_mbps"] /
        TCP_ONLY_USEFUL_MBPS *
        100.0
    )

    # ------------------------------------------------------------
    # CSV único
    # ------------------------------------------------------------

    csv_row = dict(row)

    csv_row.update({
        "full_runtime_ready":
            final.get("FULL_RUNTIME_READY", ""),
        "display_frames":
            iv(final, "DISPLAY_FRAMES"),
        "display_gap_max_ms":
            iv(final, "DISPLAY_GAP_MAX_MS"),
        "fram_cycles":
            iv(final, "FRAM_CYCLES"),
        "fram_fails":
            iv(final, "FRAM_FAILS"),
        "fram_max_us":
            iv(final, "FRAM_MAX_US"),
        "sd_append_cycles":
            iv(final, "SD_APPEND_CYCLES"),
        "sd_append_fails":
            iv(final, "SD_APPEND_FAILS"),
        "sd_append_max_us":
            iv(final, "SD_APPEND_MAX_US"),
        "sd_verify_cycles":
            iv(final, "SD_VERIFY_CYCLES"),
        "sd_verify_fails":
            iv(final, "SD_VERIFY_FAILS"),
        "sd_verify_max_us":
            iv(final, "SD_VERIFY_MAX_US"),
        "rtc_stale":
            iv(final, "RTC_STALE"),
        "rtc_max_age_ms":
            iv(final, "RTC_MAX_AGE_MS"),
        "io_stale":
            iv(final, "IO_STALE"),
        "io_max_age_ms":
            iv(final, "IO_MAX_AGE_MS"),
        "button_sample_gap_max_ms":
            iv(final, "BUTTON_SAMPLE_GAP_MAX_MS"),
        "spi_probe_fails":
            iv(final, "SPI_PROBE_FAILS"),
        "spi_probe_max_wait_us":
            iv(final, "SPI_PROBE_MAX_WAIT_US"),
        "peripheral_failure_count":
            iv(final, "PERIPHERAL_FAILURE_COUNT"),
        "readiness_pass":
            readiness_pass,
        "peripheral_failures_pass":
            peripheral_failures_pass,
        "activity_pass":
            activity_pass,
        "freshness_pass":
            freshness_pass,
        "peripheral_pass":
            peripheral_pass,
        "total_retention_pct":
            total_retention_pct,
        "useful_retention_pct":
            useful_retention_pct,
        "qualification_pass":
            qualification_pass,
    })

    with open(
        args.csv,
        "w",
        newline="",
        encoding="utf-8"
    ) as f:
        writer = csv.DictWriter(
            f,
            fieldnames=list(
                csv_row.keys()
            )
        )

        writer.writeheader()
        writer.writerow(
            csv_row
        )

    # ------------------------------------------------------------
    # Resumen
    # ------------------------------------------------------------

    print()
    print("=" * 76)
    print(
        " FULL_RUNTIME_REALISTIC 1000 req/s SUMMARY"
    )
    print("=" * 76)

    print(
        "REQUESTED_REQ_S="
        f"{args.rate:.0f}"
    )

    print(
        "ACHIEVED_REQ_S="
        f"{row['achieved_req_s']:.2f}"
    )

    print(
        "ACHIEVED_PCT="
        f"{row['achieved_percent']:.3f}"
    )

    print(
        "TOTAL_TCP_PAYLOAD_MBPS="
        f"{row['total_tcp_payload_mbps']:.4f}"
    )

    print(
        "USEFUL_DATA_MBPS="
        f"{row['useful_register_data_mbps']:.4f}"
    )

    print(
        "TOTAL_MBPS_RETENTION_VS_TCP_ONLY_PCT="
        f"{total_retention_pct:.2f}"
    )

    print(
        "USEFUL_MBPS_RETENTION_VS_TCP_ONLY_PCT="
        f"{useful_retention_pct:.2f}"
    )

    print(
        "LATENCY_AVG_US="
        f"{row['latency_avg_us']:.1f}"
    )

    print(
        "LATENCY_P95_US="
        f"{row['latency_p95_us']:.1f}"
    )

    print(
        "LATENCY_P99_US="
        f"{row['latency_p99_us']:.1f}"
    )

    print(
        "LATENCY_MAX_US="
        f"{row['latency_max_us']:.1f}"
    )

    print(
        "LOOP_GAP_AVG_US="
        f"{row['loop_gap_avg_us']}"
    )

    print(
        "LOOP_GAP_MAX_US="
        f"{row['loop_gap_max_us']}"
    )

    print(
        "TCP_CLEAN="
        f"{'YES' if tcp_clean else 'NO'}"
    )

    print(
        "TCP_RATE_PASS="
        f"{'YES' if rate_pass else 'NO'}"
    )

    print(
        "READINESS_PASS="
        f"{'YES' if readiness_pass else 'NO'}"
    )

    print(
        "PERIPHERAL_FAILURES_PASS="
        f"{'YES' if peripheral_failures_pass else 'NO'}"
    )

    print(
        "PERIPHERAL_ACTIVITY_PASS="
        f"{'YES' if activity_pass else 'NO'}"
    )

    print(
        "PERIPHERAL_FRESHNESS_PASS="
        f"{'YES' if freshness_pass else 'NO'}"
    )

    print(
        "PERIPHERAL_PASS="
        f"{'YES' if peripheral_pass else 'NO'}"
    )

    print(
        f"CSV={args.csv}"
    )

    if qualification_pass:
        print()
        print(
            "A14_3_FULL_RUNTIME_REALISTIC_"
            "1000RPS_QUALIFICATION=PASS_PHYSICAL"
        )

        print(
            "NEXT=FULL_RUNTIME_REALISTIC_1000RPS_LONG_RUN"
        )

        return 0

    if clean_saturation:
        print()
        print(
            "A14_3_FULL_RUNTIME_REALISTIC_"
            "1000RPS_QUALIFICATION=SATURATION_FAIL_CLEAN"
        )

        print(
            "NEXT=REBRACKET_FULL_RUNTIME_REALISTIC_LOWER"
        )

        return 1

    print()
    print(
        "A14_3_FULL_RUNTIME_REALISTIC_"
        "1000RPS_QUALIFICATION=REVIEW"
    )

    print(
        "NEXT=ANALYZE_TCP_OR_PERIPHERAL_DEGRADATION"
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
            "1000RPS_QUALIFICATION=FAIL_HARNESS"
        )

        sys.exit(2)