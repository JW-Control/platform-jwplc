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
        1150,
        1175
    ]

    print()
    print("=" * 76)
    print(
        " A14.3 PERF-S1 FC03/125 "
        "5MIN REBRACKET"
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
        "1150_STABLE_1175_SATURATION"
    )

    print(
        "STABLE_CRITERION="
        "CLEAN_AND_ACHIEVED_GTE_95_PERCENT"
    )

    print(
        "RATE_1150_MIN_PASS_REQ_S=1092.5"
    )

    print(
        "RATE_1175_MIN_PASS_REQ_S=1116.25"
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
                "REBRACKET_RESULT "
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

    r1150 = next(
        r for r in rows
        if int(
            r["requested_req_s"]
        ) == 1150
    )

    r1175 = next(
        r for r in rows
        if int(
            r["requested_req_s"]
        ) == 1175
    )

    stable_1150 = (
        r1150["stable_pass"]
    )

    saturated_1175 = (
        not r1175["stable_pass"] and
        r1175["clean_case"] and
        r1175["classification"] ==
        "SATURATION_FAIL"
    )

    boundary_confirmed = (
        stable_1150 and
        saturated_1175
    )

    print()
    print("=" * 76)
    print(
        " 5MIN REBRACKET SUMMARY"
    )
    print("=" * 76)

    for rate, row in [
        (1150, r1150),
        (1175, r1175),
    ]:
        print(
            f"RATE_{rate}_RESULT="
            f"{row['classification']}"
        )

        print(
            f"RATE_{rate}_ACHIEVED_REQ_S="
            f"{row['achieved_req_s']:.1f}"
        )

        print(
            f"RATE_{rate}_ACHIEVED_PCT="
            f"{row['achieved_percent']:.2f}"
        )

        print(
            f"RATE_{rate}_TOTAL_MBPS="
            f"{row['total_tcp_payload_mbps']:.3f}"
        )

        print(
            f"RATE_{rate}_USEFUL_MBPS="
            f"{row['useful_register_data_mbps']:.3f}"
        )

        print(
            f"RATE_{rate}_P99_US="
            f"{row['latency_p99_us']:.1f}"
        )

        print(
            f"RATE_{rate}_CLEAN="
            f"{'YES' if row['clean_case'] else 'NO'}"
        )

    print(
        "BOUNDARY_1150_STABLE="
        f"{'YES' if stable_1150 else 'NO'}"
    )

    print(
        "BOUNDARY_1175_SATURATED="
        f"{'YES' if saturated_1175 else 'NO'}"
    )

    print(
        "FC03_125_5MIN_REBRACKET_CONFIRMED="
        f"{'YES' if boundary_confirmed else 'NO'}"
    )

    print(
        f"CSV={args.csv}"
    )

    if boundary_confirmed:
        print(
            "A14_3_FC03_125_5MIN_"
            "REBRACKET=PASS_PHYSICAL"
        )

        print(
            "CANDIDATE_FINAL_STABLE_REQ_S=1150"
        )

        print(
            "NEXT=30MIN_TCP_ONLY_SOAK_1150"
        )

        return 0

    print(
        "A14_3_FC03_125_5MIN_"
        "REBRACKET=REVIEW"
    )

    if not stable_1150:
        print(
            "NEXT=REBRACKET_LOWER_1125"
        )
    elif r1175["stable_pass"]:
        print(
            "NEXT=REVIEW_1175_1200_VARIABILITY"
        )
    else:
        print(
            "NEXT=ANALYZE_REBRACKET_VARIATION"
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
            "REBRACKET=FAIL_HARNESS"
        )

        sys.exit(2)