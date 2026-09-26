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


BAUD_CASES = [
    (230400, b"8\n"),
    (250000, b"0\n"),
    (460800, b"6\n"),
    (500000, b"9\n"),
]


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def yes(values: dict[str, str], key: str) -> bool:
    return sv(values, key).upper() == "YES"


def configure_baud(
    master,
    slave,
    requested_baud: int,
    command: bytes,
) -> dict[str, float | int | str | bool]:
    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    ack = f"RTU_BAUD_REQUESTED={requested_baud}"

    budget.send_command_wait(
        slave,
        command,
        ack,
    )
    budget.send_command_wait(
        master,
        command,
        ack,
    )

    time.sleep(0.10)

    ms = q.request_snapshot(
        master,
        echo=False,
    )
    ss = p5b.request_slave_snapshot(
        slave,
        5.0,
    )

    master_requested = iv(ms, "RTU_BAUD")
    slave_requested = iv(ss, "RTU_BAUD")
    master_effective = iv(ms, "RTU_BAUD_EFFECTIVE")
    slave_effective = iv(ss, "RTU_BAUD_EFFECTIVE")

    master_gap = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap = iv(ss, "RTU_FRAME_GAP_US")
    master_motor = sv(ms, "RTU_MOTOR")
    slave_motor = sv(ss, "RTU_MOTOR")
    master_tx = sv(ms, "RTU_TX_MODE")
    slave_tx = sv(ss, "RTU_TX_MODE")

    master_error_pct = (
        (master_effective / requested_baud - 1.0) * 100.0
        if master_effective > 0
        else 0.0
    )
    slave_error_pct = (
        (slave_effective / requested_baud - 1.0) * 100.0
        if slave_effective > 0
        else 0.0
    )

    clock_zone = (
        "REF_TICK_EXPECTED"
        if requested_baud <= 250000
        else "APB_EXPECTED"
    )

    profile_pass = (
        master_requested == requested_baud
        and slave_requested == requested_baud
        and master_effective > 0
        and slave_effective > 0
        and master_effective == slave_effective
        and master_gap == 500
        and slave_gap == 500
        and master_motor == "ASYNC"
        and slave_motor == "ASYNC"
        and master_tx == "QUEUED"
        and slave_tx == "QUEUED"
        and yes(ms, "RS485_AUTO_DIRECTION")
        and yes(ss, "RS485_AUTO_DIRECTION")
        and yes(ms, "RS485_QUEUED_TX_SUPPORTED")
        and yes(ss, "RS485_QUEUED_TX_SUPPORTED")
    )

    print(
        "RTUH1B_BAUD_CONFIG "
        f"REQUESTED_BAUD={requested_baud} "
        f"CLOCK_ZONE={clock_zone} "
        f"MASTER_REQUESTED={master_requested} "
        f"SLAVE_REQUESTED={slave_requested} "
        f"MASTER_EFFECTIVE={master_effective} "
        f"SLAVE_EFFECTIVE={slave_effective} "
        f"MASTER_ERROR_PCT={master_error_pct:.4f} "
        f"SLAVE_ERROR_PCT={slave_error_pct:.4f} "
        f"MASTER_GAP_US={master_gap} "
        f"SLAVE_GAP_US={slave_gap} "
        f"MASTER_MOTOR={master_motor} "
        f"SLAVE_MOTOR={slave_motor} "
        f"MASTER_TX={master_tx} "
        f"SLAVE_TX={slave_tx} "
        f"PROFILE_PASS={'YES' if profile_pass else 'NO'}"
    )

    if not profile_pass:
        raise RuntimeError(
            f"perfil invalido para baud {requested_baud}"
        )

    return {
        "requested_baud": requested_baud,
        "effective_baud": master_effective,
        "error_pct": master_error_pct,
        "clock_zone": clock_zone,
        "profile_pass": profile_pass,
    }


def run_case(
    master,
    slave,
    duration_s: float,
    config: dict[str, float | int | str | bool],
) -> dict[str, object]:
    requested_baud = int(config["requested_baud"])
    effective_baud = int(config["effective_baud"])
    error_pct = float(config["error_pct"])
    clock_zone = str(config["clock_zone"])

    print()
    print("-" * 78)
    print(
        "RTUH1B_CASE_BEGIN "
        f"REQUESTED_BAUD={requested_baud} "
        f"EFFECTIVE_BAUD={effective_baud} "
        f"CLOCK_ZONE={clock_zone} "
        "TCP=OFF GAP_US=500 MOTOR=ASYNC TX=QUEUED RTU=UNPACED"
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

    ms = q.request_snapshot(
        master,
        echo=False,
    )
    ss = p5b.request_slave_snapshot(
        slave,
        5.0,
    )

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
        iv(ms, "RTU_BAUD") == requested_baud
        and iv(ss, "RTU_BAUD") == requested_baud
        and iv(ms, "RTU_BAUD_EFFECTIVE") == effective_baud
        and iv(ss, "RTU_BAUD_EFFECTIVE") == effective_baud
        and iv(ms, "RTU_FRAME_GAP_US") == 500
        and iv(ss, "RTU_FRAME_GAP_US") == 500
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
        "requested_baud": requested_baud,
        "effective_baud": effective_baud,
        "error_pct": error_pct,
        "clock_zone": clock_zone,
        "rtu_hz": rtu_hz,
        "failed": failed,
        "timeouts": timeouts,
        "master_crc": master_crc,
        "slave_crc": slave_crc,
        "rtu_clean": rtu_clean,
        "runtime_clean": runtime_clean,
    }

    print(
        "RTUH1B_CASE "
        f"REQUESTED_BAUD={requested_baud} "
        f"EFFECTIVE_BAUD={effective_baud} "
        f"ERROR_PCT={error_pct:.4f} "
        f"CLOCK_ZONE={clock_zone} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"RTU_FAILED={failed} "
        f"RTU_TIMEOUTS={timeouts} "
        f"MASTER_CRC={master_crc} "
        f"SLAVE_CRC={slave_crc} "
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
    print(" A14 RTU-H1B - BAUD CLOCK SANITY")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(
        f"DURATION_PER_CASE_S="
        f"{args.duration_per_case:.0f}"
    )
    print(
        "REQUESTED_BAUDS="
        "230400,250000,460800,500000"
    )
    print("TCP=OFF")
    print("FRAME_GAP_US=500")
    print("MOTOR=ASYNC")
    print("TX_MODE=QUEUED")
    print("RTU_TIMEOUT_MS=25")

    try:
        time.sleep(1.0)

        for requested_baud, command in BAUD_CASES:
            config = configure_baud(
                master,
                slave,
                requested_baud,
                command,
            )

            rows.append(
                run_case(
                    master,
                    slave,
                    args.duration_per_case,
                    config,
                )
            )

        print()
        print("=" * 78)
        print(" RTU-H1B SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUH1B_SUMMARY "
                f"REQUESTED_BAUD={row['requested_baud']} "
                f"EFFECTIVE_BAUD={row['effective_baud']} "
                f"ERROR_PCT={row['error_pct']:.4f} "
                f"CLOCK_ZONE={row['clock_zone']} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"FAILED={row['failed']} "
                f"TIMEOUTS={row['timeouts']} "
                f"MASTER_CRC={row['master_crc']} "
                f"SLAVE_CRC={row['slave_crc']} "
                f"RUNTIME_CLEAN="
                f"{'YES' if row['runtime_clean'] else 'NO'}"
            )

        clean_bauds = [
            int(row["requested_baud"])
            for row in rows
            if bool(row["runtime_clean"])
        ]

        print(
            "RTUH1B_CLEAN_REQUESTED_BAUDS="
            + ",".join(
                str(value)
                for value in clean_bauds
            )
        )

        ref_rows = [
            row
            for row in rows
            if int(row["requested_baud"]) <= 250000
        ]
        apb_rows = [
            row
            for row in rows
            if int(row["requested_baud"]) > 250000
        ]

        ref_clean = [
            int(row["requested_baud"])
            for row in ref_rows
            if bool(row["runtime_clean"])
        ]
        apb_clean = [
            int(row["requested_baud"])
            for row in apb_rows
            if bool(row["runtime_clean"])
        ]

        print(
            "RTUH1B_REF_TICK_CLEAN="
            + (
                ",".join(str(v) for v in ref_clean)
                if ref_clean
                else "NONE"
            )
        )
        print(
            "RTUH1B_APB_CLEAN="
            + (
                ",".join(str(v) for v in apb_clean)
                if apb_clean
                else "NONE"
            )
        )

        if 250000 in clean_bauds and 230400 not in clean_bauds:
            print(
                "RTUH1B_REF_TICK_PATTERN="
                "250000_PASS_230400_FAIL"
            )

        if 460800 in clean_bauds and 500000 in clean_bauds:
            print(
                "RTUH1B_APB_PATTERN="
                "460800_AND_500000_PASS"
            )

        print("A14_RTU_H1B=PASS_CHARACTERIZED")
        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
