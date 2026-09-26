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


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def yes(values: dict[str, str], key: str) -> bool:
    return sv(values, key).upper() == "YES"


def configure_auto_230400(master, slave) -> dict[str, object]:
    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    budget.send_command_wait(
        slave,
        b"8\n",
        "RTU_BAUD_REQUESTED=230400",
    )
    budget.send_command_wait(
        master,
        b"8\n",
        "RTU_BAUD_REQUESTED=230400",
    )

    budget.send_command_wait(
        master,
        b"O\n",
        "RTU_FRAME_GAP_US=500",
    )
    budget.send_command_wait(
        slave,
        b"O\n",
        "RTU_FRAME_GAP_US=500",
    )

    time.sleep(0.10)

    return validate_profile(
        master,
        slave,
        "AUTO",
    )


def configure_apb_230400(master, slave) -> dict[str, object]:
    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    budget.send_command_wait(
        slave,
        b"@\n",
        "RTU_CLOCK_PROFILE=APB_FORCED",
    )
    budget.send_command_wait(
        master,
        b"@\n",
        "RTU_CLOCK_PROFILE=APB_FORCED",
    )

    time.sleep(0.10)

    return validate_profile(
        master,
        slave,
        "APB_FORCED",
    )


def validate_profile(
    master,
    slave,
    expected_clock: str,
) -> dict[str, object]:
    ms = q.request_snapshot(
        master,
        echo=False,
    )
    ss = p5b.request_slave_snapshot(
        slave,
        5.0,
    )

    master_effective = iv(
        ms,
        "RTU_BAUD_EFFECTIVE",
    )
    slave_effective = iv(
        ss,
        "RTU_BAUD_EFFECTIVE",
    )

    master_error_pct = (
        (master_effective / 230400.0 - 1.0)
        * 100.0
        if master_effective > 0
        else 0.0
    )
    slave_error_pct = (
        (slave_effective / 230400.0 - 1.0)
        * 100.0
        if slave_effective > 0
        else 0.0
    )

    checks = {
        "master_baud": iv(ms, "RTU_BAUD") == 230400,
        "slave_baud": iv(ss, "RTU_BAUD") == 230400,
        "effective_match": (
            master_effective > 0
            and master_effective == slave_effective
        ),
        "master_clock": (
            sv(ms, "RTU_CLOCK_PROFILE")
            == expected_clock
        ),
        "slave_clock": (
            sv(ss, "RTU_CLOCK_PROFILE")
            == expected_clock
        ),
        "master_gap": (
            iv(ms, "RTU_FRAME_GAP_US")
            == 500
        ),
        "slave_gap": (
            iv(ss, "RTU_FRAME_GAP_US")
            == 500
        ),
        "master_motor": (
            sv(ms, "RTU_MOTOR")
            == "ASYNC"
        ),
        "slave_motor": (
            sv(ss, "RTU_MOTOR")
            == "ASYNC"
        ),
        "master_tx": (
            sv(ms, "RTU_TX_MODE")
            == "QUEUED"
        ),
        "slave_tx": (
            sv(ss, "RTU_TX_MODE")
            == "QUEUED"
        ),
        "master_auto_direction": yes(
            ms,
            "RS485_AUTO_DIRECTION",
        ),
        "slave_auto_direction": yes(
            ss,
            "RS485_AUTO_DIRECTION",
        ),
    }

    profile_pass = all(
        checks.values()
    )

    print(
        "RTUH1C_PROFILE "
        f"CLOCK={expected_clock} "
        f"MASTER_REQUESTED={iv(ms, 'RTU_BAUD')} "
        f"SLAVE_REQUESTED={iv(ss, 'RTU_BAUD')} "
        f"MASTER_EFFECTIVE={master_effective} "
        f"SLAVE_EFFECTIVE={slave_effective} "
        f"MASTER_ERROR_PCT={master_error_pct:.4f} "
        f"SLAVE_ERROR_PCT={slave_error_pct:.4f} "
        f"MASTER_GAP_US={iv(ms, 'RTU_FRAME_GAP_US')} "
        f"SLAVE_GAP_US={iv(ss, 'RTU_FRAME_GAP_US')} "
        f"MASTER_CLOCK={sv(ms, 'RTU_CLOCK_PROFILE')} "
        f"SLAVE_CLOCK={sv(ss, 'RTU_CLOCK_PROFILE')} "
        f"PROFILE_PASS={'YES' if profile_pass else 'NO'}"
    )

    if not profile_pass:
        raise RuntimeError(
            f"perfil H1C invalido: {expected_clock}"
        )

    return {
        "clock": expected_clock,
        "effective_baud": master_effective,
        "error_pct": master_error_pct,
    }


def run_case(
    master,
    slave,
    duration_s: float,
    profile: dict[str, object],
) -> dict[str, object]:
    clock = str(
        profile["clock"]
    )

    print()
    print("-" * 78)
    print(
        "RTUH1C_CASE_BEGIN "
        f"CLOCK={clock} "
        "BAUD=230400 GAP_US=500 "
        "TCP=OFF MOTOR=ASYNC TX=QUEUED RTU=UNPACED"
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
    p5b.reset_slave_stats(
        slave,
        3.0,
    )

    budget.send_command_wait(
        master,
        b"G\n",
        p5b.MASTER_START_ACK,
    )

    time.sleep(
        duration_s
    )

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

    rtu_hz = (
        completed /
        (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )

    profile_pass = (
        iv(ms, "RTU_BAUD") == 230400
        and iv(ss, "RTU_BAUD") == 230400
        and iv(ms, "RTU_BAUD_EFFECTIVE")
            == int(profile["effective_baud"])
        and iv(ss, "RTU_BAUD_EFFECTIVE")
            == int(profile["effective_baud"])
        and sv(ms, "RTU_CLOCK_PROFILE")
            == clock
        and sv(ss, "RTU_CLOCK_PROFILE")
            == clock
        and iv(ms, "RTU_FRAME_GAP_US")
            == 500
        and iv(ss, "RTU_FRAME_GAP_US")
            == 500
        and sv(ms, "RTU_MOTOR")
            == "ASYNC"
        and sv(ss, "RTU_MOTOR")
            == "ASYNC"
        and sv(ms, "RTU_TX_MODE")
            == "QUEUED"
        and sv(ss, "RTU_TX_MODE")
            == "QUEUED"
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
        "clock": clock,
        "effective_baud": int(
            profile["effective_baud"]
        ),
        "error_pct": float(
            profile["error_pct"]
        ),
        "rtu_hz": rtu_hz,
        "started": started,
        "completed": completed,
        "success": success,
        "failed": failed,
        "timeouts": timeouts,
        "master_crc": master_crc,
        "slave_crc": slave_crc,
        "profile_pass": profile_pass,
        "rtu_clean": rtu_clean,
        "runtime_clean": runtime_clean,
    }

    print(
        "RTUH1C_CASE "
        f"CLOCK={clock} "
        f"EFFECTIVE_BAUD={row['effective_baud']} "
        f"ERROR_PCT={row['error_pct']:.4f} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"STARTED={started} "
        f"COMPLETED={completed} "
        f"SUCCESS={success} "
        f"FAILED={failed} "
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
    print(" A14 RTU-H1C - 230400 UART CLOCK A/B")
    print("=" * 78)
    print(
        f"MASTER_SERIAL={args.master_serial}"
    )
    print(
        f"SLAVE_SERIAL={args.slave_serial}"
    )
    print(
        f"DURATION_PER_CASE_S="
        f"{args.duration_per_case:.0f}"
    )
    print("BAUD=230400")
    print("FRAME_GAP_US=500")
    print("CLOCK_CASES=AUTO,APB_FORCED")
    print("TCP=OFF")
    print("MOTOR=ASYNC")
    print("TX_MODE=QUEUED")
    print("RTU_TIMEOUT_MS=25")

    try:
        time.sleep(1.0)

        auto_profile = (
            configure_auto_230400(
                master,
                slave,
            )
        )
        rows.append(
            run_case(
                master,
                slave,
                args.duration_per_case,
                auto_profile,
            )
        )

        apb_profile = (
            configure_apb_230400(
                master,
                slave,
            )
        )
        rows.append(
            run_case(
                master,
                slave,
                args.duration_per_case,
                apb_profile,
            )
        )

        print()
        print("=" * 78)
        print(" RTU-H1C SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUH1C_SUMMARY "
                f"CLOCK={row['clock']} "
                f"EFFECTIVE_BAUD={row['effective_baud']} "
                f"ERROR_PCT={row['error_pct']:.4f} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"FAILED={row['failed']} "
                f"TIMEOUTS={row['timeouts']} "
                f"MASTER_CRC={row['master_crc']} "
                f"SLAVE_CRC={row['slave_crc']} "
                f"RTU_CLEAN="
                f"{'YES' if row['rtu_clean'] else 'NO'}"
            )

        auto = rows[0]
        apb = rows[1]

        error_reduction = (
            abs(float(auto["error_pct"]))
            - abs(float(apb["error_pct"]))
        )

        print(
            "RTUH1C_EFFECTIVE_BAUD_DELTA="
            f"{int(apb['effective_baud']) - int(auto['effective_baud'])}"
        )
        print(
            "RTUH1C_ABS_ERROR_REDUCTION_PCT_POINTS="
            f"{error_reduction:.4f}"
        )

        causal_pass = (
            bool(auto["profile_pass"])
            and bool(apb["profile_pass"])
            and not bool(auto["rtu_clean"])
            and bool(apb["rtu_clean"])
            and abs(float(apb["error_pct"]))
                < abs(float(auto["error_pct"]))
        )

        if causal_pass:
            print(
                "RTUH1C_CAUSAL_RESULT="
                "AUTO_FAIL_APB_PASS"
            )
            print(
                "A14_RTU_H1C="
                "PASS_CAUSAL_CLOCK_SOURCE"
            )
            return 0

        print(
            "RTUH1C_CAUSAL_RESULT="
            "INCONCLUSIVE"
        )
        print(
            "A14_RTU_H1C="
            "REVIEW_INCONCLUSIVE"
        )
        return 2

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
