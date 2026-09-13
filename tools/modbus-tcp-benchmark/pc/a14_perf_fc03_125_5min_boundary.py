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
        default=300.0
    )

    parser.add_argument(
        "--csv",
        required=True
    )

    args = parser.parse_args()

    rates = [
        1200,
        1225
    ]

    print()
    print("=" * 76)
    print(
        " A14.3 PERF-S1 FC03/125 "
        "5MIN BOUNDARY CONFIRMATION"
    )
    print("=" * 76)

    print(
        f"TARGET={args.host}:"
        f"{args.port}"
    )

    print(
        f"SERIAL={args.serial}"
    )

    print(
        f"DURATION_PER_CASE_S="
        f"{args.duration:.1f}"
    )

    print(
        "EXPECTED_BOUNDARY="
        "1200_STABLE_1225_SATURATION"
    )

    print(
        "STABLE_CRITERION="
        "CLEAN_AND_ACHIEVED_GTE_95_PERCENT"
    )

    ser = serial.Serial(
        args.serial,
        args.baud,
        timeout=0.10,
        write_timeout=1.0
    )

    rows = []

    try:
        time.sleep(0.3)

        ser.reset_input_buffer()

        initial = q.request_snapshot(
            ser,
            echo=True
        )

        if (
            initial.get(
                "SERVER_READY"
            ) != "YES"
        ):
            print(
                "INITIAL_SERVER_READY=NO"
            )

            q.wait_server_ready(
                ser,
                timeout_s=20.0,
                require_disconnected=True
            )

        print(
            "ACTIVE_SERIAL_HANDSHAKE=PASS"
        )

        for index, rate in enumerate(
            rates,
            start=1
        ):
            print()
            print(
                f"CASE_BEGIN="
                f"{index}/{len(rates)} "
                f"RATE={rate} "
                f"Q=125 "
                f"DURATION_S="
                f"{args.duration:.0f}"
            )

            row = frontier.run_case(
                ser,
                args.host,
                args.port,
                float(rate),
                args.duration
            )

            rows.append(
                row
            )

            frontier.print_result(
                row
            )

            print(
                "FIVE_MIN_RESULT "
                f"RATE={rate} "
                f"ACHIEVED="
                f"{row['achieved_req_s']:.1f} "
                f"ACHIEVED_PCT="
                f"{row['achieved_percent']:.2f} "
                f"TOTAL_MBPS="
                f"{row['total_tcp_payload_mbps']:.3f} "
                f"USEFUL_MBPS="
                f"{row['useful_register_data_mbps']:.3f} "
                f"P95_US="
                f"{row['latency_p95_us']:.1f} "
                f"P99_US="
                f"{row['latency_p99_us']:.1f} "
                f"MAX_US="
                f"{row['latency_max_us']:.1f} "
                f"RESULT="
                f"{row['classification']}"
            )

            time.sleep(0.5)

    finally:
        ser.close()

    with open(
        args.csv,
        "w",
        newline="",
        encoding="utf-8"
    ) as f:
        writer = csv.DictWriter(
            f,
            fieldnames=list(
                rows[0].keys()
            )
        )

        writer.writeheader()
        writer.writerows(
            rows
        )

    r1200 = next(
        r for r in rows
        if int(
            r["requested_req_s"]
        ) == 1200
    )

    r1225 = next(
        r for r in rows
        if int(
            r["requested_req_s"]
        ) == 1225
    )

    stable_1200 = (
        r1200["stable_pass"]
    )

    saturated_1225 = (
        not r1225["stable_pass"] and
        r1225["clean_case"] and
        r1225["classification"] ==
        "SATURATION_FAIL"
    )

    boundary_confirmed = (
        stable_1200 and
        saturated_1225
    )

    print()
    print("=" * 76)
    print(
        " 5MIN BOUNDARY SUMMARY"
    )
    print("=" * 76)

    print(
        "RATE_1200_RESULT="
        f"{r1200['classification']}"
    )

    print(
        "RATE_1200_ACHIEVED_REQ_S="
        f"{r1200['achieved_req_s']:.1f}"
    )

    print(
        "RATE_1200_ACHIEVED_PCT="
        f"{r1200['achieved_percent']:.2f}"
    )

    print(
        "RATE_1200_TOTAL_MBPS="
        f"{r1200['total_tcp_payload_mbps']:.3f}"
    )

    print(
        "RATE_1200_USEFUL_MBPS="
        f"{r1200['useful_register_data_mbps']:.3f}"
    )

    print(
        "RATE_1200_P99_US="
        f"{r1200['latency_p99_us']:.1f}"
    )

    print(
        "RATE_1225_RESULT="
        f"{r1225['classification']}"
    )

    print(
        "RATE_1225_ACHIEVED_REQ_S="
        f"{r1225['achieved_req_s']:.1f}"
    )

    print(
        "RATE_1225_ACHIEVED_PCT="
        f"{r1225['achieved_percent']:.2f}"
    )

    print(
        "RATE_1225_TOTAL_MBPS="
        f"{r1225['total_tcp_payload_mbps']:.3f}"
    )

    print(
        "RATE_1225_USEFUL_MBPS="
        f"{r1225['useful_register_data_mbps']:.3f}"
    )

    print(
        "RATE_1225_P99_US="
        f"{r1225['latency_p99_us']:.1f}"
    )

    print(
        "RATE_1200_CLEAN="
        f"{'YES' if r1200['clean_case'] else 'NO'}"
    )

    print(
        "RATE_1225_CLEAN="
        f"{'YES' if r1225['clean_case'] else 'NO'}"
    )

    print(
        "BOUNDARY_1200_STABLE="
        f"{'YES' if stable_1200 else 'NO'}"
    )

    print(
        "BOUNDARY_1225_SATURATED="
        f"{'YES' if saturated_1225 else 'NO'}"
    )

    print(
        "FC03_125_5MIN_BOUNDARY_CONFIRMED="
        f"{'YES' if boundary_confirmed else 'NO'}"
    )

    print(
        f"CSV={args.csv}"
    )

    if boundary_confirmed:
        print(
            "A14_3_FC03_125_5MIN_"
            "BOUNDARY=PASS_PHYSICAL"
        )

        print(
            "NEXT=FULL_RUNTIME_"
            "PERIPHERAL_BENCHMARK_PREP"
        )

        return 0

    print(
        "A14_3_FC03_125_5MIN_"
        "BOUNDARY=REVIEW"
    )

    print(
        "NEXT=ANALYZE_BOUNDARY_VARIATION"
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
            "A14_3_FC03_125_5MIN_"
            "BOUNDARY=FAIL_HARNESS"
        )

        sys.exit(2)