from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_perf_fc03_qualification_sweep as q
import a14_p5b_master_slave_qualification as p5b
import a14_p5rtu_tcp_budget_frontier as budget


GAP_CASES = [
    (1000, b"L\n"),
    (750, b"M\n"),
    (600, b"N\n"),
    (500, b"O\n"),
    (400, b"Q\n"),
    (350, b"V\n"),
    (300, b"W\n"),
]


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def configure_gap(master, slave, gap_us: int, command: bytes) -> None:
    ack = f"RTU_FRAME_GAP_US={gap_us}"

    budget.send_command_wait(master, command, ack)
    budget.send_command_wait(slave, command, ack)
    time.sleep(0.05)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    master_gap = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap = iv(ss, "RTU_FRAME_GAP_US")

    print(
        "RTUF2B_GAP_CONFIG "
        f"GAP_US={gap_us} "
        f"MASTER_GAP_US={master_gap} "
        f"SLAVE_GAP_US={slave_gap}"
    )

    if master_gap != gap_us or slave_gap != gap_us:
        raise RuntimeError(
            f"gap efectivo invalido: "
            f"master={master_gap} "
            f"slave={slave_gap} "
            f"esperado={gap_us}"
        )


def run_case(
    master,
    slave,
    duration_s: float,
    gap_us: int,
) -> dict[str, object]:
    print()
    print("-" * 78)
    print(
        f"RTUF2B_CASE_BEGIN "
        f"GAP_US={gap_us} "
        "TCP=OFF RTU=UNPACED"
    )
    print("-" * 78)

    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    q.wait_server_disconnected(
        master,
        timeout_s=20.0,
    )

    budget.send_command_wait(
        master,
        b"U\n",
        "RTU_RATE_MODE=UNPACED",
    )

    q.reset_stats(master)
    p5b.reset_slave_stats(slave, 3.0)

    budget.send_command_wait(
        master,
        b"G\n",
        p5b.MASTER_START_ACK,
    )

    time.sleep(duration_s)

    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    master_gap = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap = iv(ss, "RTU_FRAME_GAP_US")
    timing_pass = (
        master_gap == gap_us
        and slave_gap == gap_us
    )

    duration_ms = iv(
        ms,
        "RTU_TRAFFIC_DURATION_MS",
    )
    started = iv(
        ms,
        "RTU_REQUESTS_STARTED",
    )
    rejected = iv(
        ms,
        "RTU_REQUESTS_REJECTED",
    )
    completed = iv(
        ms,
        "RTU_REQUESTS_COMPLETED",
    )
    success = iv(
        ms,
        "RTU_REQUESTS_SUCCESS",
    )
    failed = iv(
        ms,
        "RTU_REQUESTS_FAILED",
    )
    verify = iv(
        ms,
        "RTU_VERIFY_FAILS",
    )
    timeouts = iv(
        ms,
        "RTU_MASTER_TIMEOUTS",
    )
    master_crc = iv(
        ms,
        "RTU_CRC_ERRORS",
    )

    rtu_hz = (
        completed / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )

    slave_rx = iv(
        ss,
        "RTU_RX_FRAMES",
    )
    slave_tx = iv(
        ss,
        "RTU_TX_FRAMES",
    )
    slave_ok = iv(
        ss,
        "RTU_REQUESTS_OK",
    )
    slave_crc = iv(
        ss,
        "RTU_CRC_ERRORS",
    )

    rtu_clean = (
        started == completed == success
        and rejected == 0
        and failed == 0
        and verify == 0
        and timeouts == 0
        and master_crc == 0
        and slave_crc == 0
        and slave_rx == completed
        and slave_tx == completed
        and slave_ok == completed
    )

    peripheral_failures = iv(
        ms,
        "PERIPHERAL_FAILURE_COUNT",
    )
    sd_failed = iv(
        ms,
        "SD_DATALOG_FAILED_COMMITS",
    )

    runtime_clean = (
        timing_pass
        and rtu_clean
        and peripheral_failures == 0
        and sd_failed == 0
    )

    row = {
        "gap_us": gap_us,
        "rtu_hz": rtu_hz,
        "started": started,
        "completed": completed,
        "success": success,
        "failed": failed,
        "timeouts": timeouts,
        "master_crc": master_crc,
        "slave_crc": slave_crc,
        "verify": verify,
        "rtu_clean": rtu_clean,
        "runtime_clean": runtime_clean,
    }

    print(
        "RTUF2B_CASE "
        f"GAP_US={gap_us} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"STARTED={started} "
        f"COMPLETED={completed} "
        f"SUCCESS={success} "
        f"FAILED={failed} "
        f"VERIFY_FAILS={verify} "
        f"TIMEOUTS={timeouts} "
        f"MASTER_CRC={master_crc} "
        f"SLAVE_CRC={slave_crc} "
        f"TIMING_PASS={'YES' if timing_pass else 'NO'} "
        f"RTU_CLEAN={'YES' if rtu_clean else 'NO'} "
        f"RUNTIME_CLEAN={'YES' if runtime_clean else 'NO'}"
    )

    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--master-serial",
        default="COM14",
    )
    parser.add_argument(
        "--slave-serial",
        default="COM4",
    )
    parser.add_argument(
        "--duration-per-case",
        type=float,
        default=30.0,
    )
    args = parser.parse_args()

    if args.duration_per_case < 20.0:
        raise ValueError(
            "duration-per-case debe ser >= 20 s"
        )

    master = p5b.open_serial_no_dtr(
        args.master_serial
    )
    slave = p5b.open_serial_no_dtr(
        args.slave_serial
    )

    rows: list[dict[str, object]] = []

    print("=" * 78)
    print(" A14 RTU-F2B - FRAME GAP FLOOR")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(
        f"DURATION_PER_CASE_S="
        f"{args.duration_per_case:.0f}"
    )
    print(
        "GAPS_US=1000,750,600,500,400,350,300"
    )
    print("TCP=OFF")
    print("RTU_BAUD=115200")
    print("RTU_CONFIG=8N1")
    print("RTU_MODE=UNPACED")

    try:
        time.sleep(1.0)

        for gap_us, command in GAP_CASES:
            budget.send_command_wait(
                master,
                b"X\n",
                p5b.MASTER_STOP_ACK,
            )
            time.sleep(0.10)

            configure_gap(
                master,
                slave,
                gap_us,
                command,
            )

            rows.append(
                run_case(
                    master,
                    slave,
                    args.duration_per_case,
                    gap_us,
                )
            )

        print()
        print("=" * 78)
        print(" RTU-F2B SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUF2B_SUMMARY "
                f"GAP_US={row['gap_us']} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"FAILED={row['failed']} "
                f"TIMEOUTS={row['timeouts']} "
                f"MASTER_CRC={row['master_crc']} "
                f"SLAVE_CRC={row['slave_crc']} "
                f"RUNTIME_CLEAN="
                f"{'YES' if row['runtime_clean'] else 'NO'}"
            )

        control = next(
            row
            for row in rows
            if row["gap_us"] == 1000
        )

        if not bool(control["runtime_clean"]):
            print(
                "A14_RTU_F2B="
                "REVIEW_CONTROL_1000_FAILURE"
            )
            return 2

        clean_rows = [
            row
            for row in rows
            if row["runtime_clean"]
        ]

        if not clean_rows:
            print(
                "A14_RTU_F2B="
                "REVIEW_NO_CLEAN_POINT"
            )
            return 3

        lowest_clean = min(
            clean_rows,
            key=lambda row: int(row["gap_us"]),
        )
        fastest_clean = max(
            clean_rows,
            key=lambda row: float(row["rtu_hz"]),
        )

        print(
            "RTUF2B_LOWEST_CLEAN_GAP_US="
            f"{lowest_clean['gap_us']}"
        )
        print(
            "RTUF2B_LOWEST_CLEAN_GAP_RTU_HZ="
            f"{lowest_clean['rtu_hz']:.3f}"
        )
        print(
            "RTUF2B_FASTEST_CLEAN_GAP_US="
            f"{fastest_clean['gap_us']}"
        )
        print(
            "RTUF2B_FASTEST_CLEAN_RTU_HZ="
            f"{fastest_clean['rtu_hz']:.3f}"
        )

        if int(lowest_clean["gap_us"]) == 300:
            print(
                "RTUF2B_FLOOR_WITHIN_SCOPE="
                "NOT_FOUND_AT_OR_ABOVE_300US"
            )
        else:
            lower_failed = [
                row
                for row in rows
                if int(row["gap_us"]) <
                    int(lowest_clean["gap_us"])
                and not row["runtime_clean"]
            ]

            if lower_failed:
                first_failed = max(
                    lower_failed,
                    key=lambda row: int(row["gap_us"]),
                )
                print(
                    "RTUF2B_FIRST_LOWER_FAILED_GAP_US="
                    f"{first_failed['gap_us']}"
                )

        print(
            "A14_RTU_F2B="
            "PASS_CHARACTERIZED"
        )
        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
