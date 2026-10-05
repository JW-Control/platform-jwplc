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
    (500, b"O\n"),
    (300, b"W\n"),
    (200, b"1\n"),
    (150, b"3\n"),
    (100, b"4\n"),
    (75, b"T\n"),
    (50, b"!\n"),
]


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def yes(values: dict[str, str], key: str) -> bool:
    return sv(values, key).upper() == "YES"


def configure_500k(master, slave) -> None:
    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    ack = "RTU_BAUD_REQUESTED=500000"

    budget.send_command_wait(
        slave,
        b"9\n",
        ack,
    )
    budget.send_command_wait(
        master,
        b"9\n",
        ack,
    )
    time.sleep(0.10)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    checks = {
        "master_requested": iv(ms, "RTU_BAUD") == 500000,
        "slave_requested": iv(ss, "RTU_BAUD") == 500000,
        "master_effective": iv(ms, "RTU_BAUD_EFFECTIVE") == 500000,
        "slave_effective": iv(ss, "RTU_BAUD_EFFECTIVE") == 500000,
        "master_motor": sv(ms, "RTU_MOTOR") == "ASYNC",
        "slave_motor": sv(ss, "RTU_MOTOR") == "ASYNC",
        "master_tx": sv(ms, "RTU_TX_MODE") == "QUEUED",
        "slave_tx": sv(ss, "RTU_TX_MODE") == "QUEUED",
        "master_auto": yes(ms, "RS485_AUTO_DIRECTION"),
        "slave_auto": yes(ss, "RS485_AUTO_DIRECTION"),
        "master_queue": yes(ms, "RS485_QUEUED_TX_SUPPORTED"),
        "slave_queue": yes(ss, "RS485_QUEUED_TX_SUPPORTED"),
        "master_buffer": iv(ms, "RS485_TX_BUFFER_BYTES") >= 257,
        "slave_buffer": iv(ss, "RS485_TX_BUFFER_BYTES") >= 257,
    }

    print(
        "RTUH2_500K_CONFIG "
        f"MASTER_REQUESTED={iv(ms, 'RTU_BAUD')} "
        f"SLAVE_REQUESTED={iv(ss, 'RTU_BAUD')} "
        f"MASTER_EFFECTIVE={iv(ms, 'RTU_BAUD_EFFECTIVE')} "
        f"SLAVE_EFFECTIVE={iv(ss, 'RTU_BAUD_EFFECTIVE')} "
        f"MASTER_MOTOR={sv(ms, 'RTU_MOTOR')} "
        f"SLAVE_MOTOR={sv(ss, 'RTU_MOTOR')} "
        f"MASTER_TX={sv(ms, 'RTU_TX_MODE')} "
        f"SLAVE_TX={sv(ss, 'RTU_TX_MODE')} "
        f"MASTER_BUFFER={iv(ms, 'RS485_TX_BUFFER_BYTES')} "
        f"SLAVE_BUFFER={iv(ss, 'RS485_TX_BUFFER_BYTES')} "
        f"PROFILE_PASS={'YES' if all(checks.values()) else 'NO'}"
    )

    if not all(checks.values()):
        raise RuntimeError(
            "perfil 500k/ASYNC/QUEUED invalido"
        )


def configure_gap(
    master,
    slave,
    gap_us: int,
    command: bytes,
) -> None:
    ack = f"RTU_FRAME_GAP_US={gap_us}"

    budget.send_command_wait(
        master,
        command,
        ack,
    )
    budget.send_command_wait(
        slave,
        command,
        ack,
    )
    time.sleep(0.05)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    master_gap = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap = iv(ss, "RTU_FRAME_GAP_US")

    print(
        "RTUH2_GAP_CONFIG "
        f"GAP_US={gap_us} "
        f"MASTER_GAP_US={master_gap} "
        f"SLAVE_GAP_US={slave_gap}"
    )

    if master_gap != gap_us or slave_gap != gap_us:
        raise RuntimeError(
            f"gap efectivo invalido para {gap_us} us"
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
        "RTUH2_CASE_BEGIN "
        f"GAP_US={gap_us} "
        "BAUD=500000 TCP=OFF MOTOR=ASYNC TX=QUEUED RTU=UNPACED"
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

    duration_ms = iv(ms, "RTU_TRAFFIC_DURATION_MS")
    started = iv(ms, "RTU_REQUESTS_STARTED")
    rejected = iv(ms, "RTU_REQUESTS_REJECTED")
    completed = iv(ms, "RTU_REQUESTS_COMPLETED")
    success = iv(ms, "RTU_REQUESTS_SUCCESS")
    failed = iv(ms, "RTU_REQUESTS_FAILED")
    verify = iv(ms, "RTU_VERIFY_FAILS")
    timeouts = iv(ms, "RTU_MASTER_TIMEOUTS")
    master_crc = iv(ms, "RTU_CRC_ERRORS")

    slave_rx = iv(ss, "RTU_RX_FRAMES")
    slave_tx = iv(ss, "RTU_TX_FRAMES")
    slave_ok = iv(ss, "RTU_REQUESTS_OK")
    slave_crc = iv(ss, "RTU_CRC_ERRORS")

    rtu_hz = (
        completed / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )

    profile_pass = (
        iv(ms, "RTU_BAUD") == 500000
        and iv(ss, "RTU_BAUD") == 500000
        and iv(ms, "RTU_BAUD_EFFECTIVE") == 500000
        and iv(ss, "RTU_BAUD_EFFECTIVE") == 500000
        and iv(ms, "RTU_FRAME_GAP_US") == gap_us
        and iv(ss, "RTU_FRAME_GAP_US") == gap_us
        and sv(ms, "RTU_MOTOR") == "ASYNC"
        and sv(ss, "RTU_MOTOR") == "ASYNC"
        and sv(ms, "RTU_TX_MODE") == "QUEUED"
        and sv(ss, "RTU_TX_MODE") == "QUEUED"
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
        profile_pass
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
        "verify": verify,
        "timeouts": timeouts,
        "master_crc": master_crc,
        "slave_crc": slave_crc,
        "profile_pass": profile_pass,
        "rtu_clean": rtu_clean,
        "runtime_clean": runtime_clean,
    }

    print(
        "RTUH2_CASE "
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
        f"PROFILE_PASS={'YES' if profile_pass else 'NO'} "
        f"RTU_CLEAN={'YES' if rtu_clean else 'NO'} "
        f"RUNTIME_CLEAN={'YES' if runtime_clean else 'NO'}"
    )

    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--duration-per-case", type=float, default=30.0)
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
    print(" A14 RTU-H2 - 500K GAP SWEEP")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(
        f"DURATION_PER_CASE_S="
        f"{args.duration_per_case:.0f}"
    )
    print("BAUD=500000")
    print("GAPS_US=500,300,200,150,100,75,50")
    print("TCP=OFF")
    print("MOTOR=ASYNC")
    print("TX_MODE=QUEUED")
    print("RTU_TIMEOUT_MS=25")

    try:
        time.sleep(1.0)

        configure_500k(master, slave)

        for gap_us, command in GAP_CASES:
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
        print(" RTU-H2 SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUH2_SUMMARY "
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
            row for row in rows
            if int(row["gap_us"]) == 500
        )

        if not bool(control["runtime_clean"]):
            print(
                "A14_RTU_H2="
                "REVIEW_500US_CONTROL_FAILURE"
            )
            return 2

        clean_rows = [
            row
            for row in rows
            if bool(row["runtime_clean"])
        ]

        lowest = min(
            clean_rows,
            key=lambda row: int(row["gap_us"]),
        )
        fastest = max(
            clean_rows,
            key=lambda row: float(row["rtu_hz"]),
        )

        baseline_hz = float(control["rtu_hz"])

        for row in rows:
            gain = (
                (float(row["rtu_hz"]) / baseline_hz - 1.0)
                * 100.0
            )

            print(
                "RTUH2_GAIN "
                f"GAP_US={row['gap_us']} "
                f"VS_500US_PCT={gain:.3f}"
            )

        print(
            "RTUH2_LOWEST_CLEAN_GAP_US="
            f"{lowest['gap_us']}"
        )
        print(
            "RTUH2_LOWEST_CLEAN_RTU_HZ="
            f"{lowest['rtu_hz']:.3f}"
        )
        print(
            "RTUH2_FASTEST_CLEAN_GAP_US="
            f"{fastest['gap_us']}"
        )
        print(
            "RTUH2_FASTEST_CLEAN_RTU_HZ="
            f"{fastest['rtu_hz']:.3f}"
        )

        fifty = next(
            row for row in rows
            if int(row["gap_us"]) == 50
        )

        if bool(fifty["runtime_clean"]):
            print(
                "RTUH2_FLOOR_WITHIN_SCOPE="
                "NOT_FOUND_AT_OR_ABOVE_50US"
            )
        else:
            lower_failures = sorted(
                int(row["gap_us"])
                for row in rows
                if not bool(row["runtime_clean"])
                and int(row["gap_us"]) <
                    int(lowest["gap_us"])
            )

            if lower_failures:
                print(
                    "RTUH2_FLOOR_WITHIN_SCOPE="
                    f"BRACKETED_BELOW_{lowest['gap_us']}US"
                )
            else:
                print(
                    "RTUH2_FLOOR_WITHIN_SCOPE="
                    "NON_MONOTONIC_REVIEW"
                )

        print("A14_RTU_H2=PASS_CHARACTERIZED")
        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
