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


RATE = 1000.0
QUANTITY = 125
REFERENCE_MIN_RATIO = 0.995


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
        default=1800.0
    )

    parser.add_argument(
        "--csv",
        required=True
    )

    args = parser.parse_args()

    print()
    print("=" * 76)
    print(
        " A14.3 PERF-S1 FC03/125 "
        "30MIN TCP-ONLY OPERATIONAL REFERENCE"
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
        f"REQUESTED_REQ_S="
        f"{RATE:.0f}"
    )

    print(
        f"QUANTITY_REGISTERS="
        f"{QUANTITY}"
    )

    print(
        f"DURATION_S="
        f"{args.duration:.0f}"
    )

    print(
        "REFERENCE_CRITERION="
        "CLEAN_AND_ACHIEVED_GTE_99.5_PERCENT"
    )

    print(
        "EXPECTED_TCP_TOTAL_PAYLOAD_MBPS=2.168"
    )

    print(
        "EXPECTED_USEFUL_DATA_MBPS=2.000"
    )

    print(
        "MAX_5MIN_STABLE_CANDIDATE_REQ_S=1140"
    )

    ser = serial.Serial(
        args.serial,
        args.baud,
        timeout=0.10,
        write_timeout=1.0
    )

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

        print()
        print(
            "CASE_BEGIN "
            "RATE=1000 "
            "Q=125 "
            f"DURATION_S={args.duration:.0f}"
        )

        row = frontier.run_case(
            ser,
            args.host,
            args.port,
            RATE,
            args.duration
        )

        frontier.print_result(
            row
        )

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
                row.keys()
            )
        )

        writer.writeheader()
        writer.writerow(
            row
        )

    reference_pass = (
        row["clean_case"] and
        row["achieved_percent"] >=
        REFERENCE_MIN_RATIO * 100.0
    )

    expected_requests = int(
        round(
            RATE *
            args.duration
        )
    )

    request_loss = (
        expected_requests -
        row["requests_ok"]
    )

    print()
    print("=" * 76)
    print(
        " 30MIN TCP-ONLY REFERENCE SUMMARY"
    )
    print("=" * 76)

    print(
        "REFERENCE_REQUESTED_REQ_S="
        f"{RATE:.0f}"
    )

    print(
        "REFERENCE_ACHIEVED_REQ_S="
        f"{row['achieved_req_s']:.2f}"
    )

    print(
        "REFERENCE_ACHIEVED_PCT="
        f"{row['achieved_percent']:.4f}"
    )

    print(
        "REFERENCE_REQUESTS_EXPECTED="
        f"{expected_requests}"
    )

    print(
        "REFERENCE_REQUESTS_OK="
        f"{row['requests_ok']}"
    )

    print(
        "REFERENCE_REQUEST_DEFICIT="
        f"{request_loss}"
    )

    print(
        "REFERENCE_TOTAL_MBPS="
        f"{row['total_tcp_payload_mbps']:.4f}"
    )

    print(
        "REFERENCE_USEFUL_MBPS="
        f"{row['useful_register_data_mbps']:.4f}"
    )

    print(
        "REFERENCE_LATENCY_AVG_US="
        f"{row['latency_avg_us']:.1f}"
    )

    print(
        "REFERENCE_LATENCY_P95_US="
        f"{row['latency_p95_us']:.1f}"
    )

    print(
        "REFERENCE_LATENCY_P99_US="
        f"{row['latency_p99_us']:.1f}"
    )

    print(
        "REFERENCE_LATENCY_MAX_US="
        f"{row['latency_max_us']:.1f}"
    )

    print(
        "REFERENCE_LOOP_GAP_AVG_US="
        f"{row['loop_gap_avg_us']}"
    )

    print(
        "REFERENCE_LOOP_GAP_MAX_US="
        f"{row['loop_gap_max_us']}"
    )

    print(
        "REFERENCE_CONNECT_ATTEMPTS="
        f"{row['tcp_connect_attempts']}"
    )

    print(
        "REFERENCE_CLEAN="
        f"{'YES' if row['clean_case'] else 'NO'}"
    )

    print(
        "REFERENCE_TIMEOUTS="
        f"{row['timeouts']}"
    )

    print(
        "REFERENCE_TRANSPORT_ERRORS="
        f"{row['transport_errors']}"
    )

    print(
        "REFERENCE_PROTOCOL_ERRORS="
        f"{row['protocol_errors']}"
    )

    print(
        "REFERENCE_BUS_LOCK_TIMEOUTS="
        f"{row['server_bus_lock_timeouts']}"
    )

    print(
        "REFERENCE_CROSS_COUNT_PASS="
        f"{'YES' if row['cross_count_pass'] else 'NO'}"
    )

    print(
        "TCP_ONLY_30MIN_REFERENCE_PASS="
        f"{'YES' if reference_pass else 'NO'}"
    )

    print(
        f"CSV={args.csv}"
    )

    if reference_pass:
        print()
        print(
            "A14_3_FC03_125_30MIN_"
            "TCP_ONLY_REFERENCE=PASS_PHYSICAL"
        )

        print(
            "TCP_ONLY_OPERATIONAL_REFERENCE_REQ_S=1000"
        )

        print(
            "NEXT=FULL_RUNTIME_REALISTIC_PREP"
        )

        return 0

    print()
    print(
        "A14_3_FC03_125_30MIN_"
        "TCP_ONLY_REFERENCE=REVIEW"
    )

    print(
        "NEXT=ANALYZE_LONG_RUN_VARIATION"
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
            "A14_3_FC03_125_30MIN_"
            "TCP_ONLY_REFERENCE=FAIL_HARNESS"
        )

        sys.exit(2)