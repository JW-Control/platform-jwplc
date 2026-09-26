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
    (115200, b"7\n"),
    (230400, b"8\n"),
    (250000, b"0\n"),
    (460800, b"6\n"),
    (500000, b"9\n"),
]

EXPECTED_CLOCK = "APB_FORCED"


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def yes(values: dict[str, str], key: str) -> bool:
    return sv(values, key).upper() == "YES"


def configure_case(master, slave, baud: int, command: bytes) -> dict[str, object]:
    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)

    ack = f"RTU_BAUD_REQUESTED={baud}"
    budget.send_command_wait(slave, command, ack)
    budget.send_command_wait(master, command, ack)

    budget.send_command_wait(master, b"O\n", "RTU_FRAME_GAP_US=500")
    budget.send_command_wait(slave, b"O\n", "RTU_FRAME_GAP_US=500")
    time.sleep(0.10)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    master_effective = iv(ms, "RTU_BAUD_EFFECTIVE")
    slave_effective = iv(ss, "RTU_BAUD_EFFECTIVE")
    error_pct = (
        (master_effective / float(baud) - 1.0) * 100.0
        if master_effective > 0
        else 0.0
    )

    profile_pass = (
        iv(ms, "RTU_BAUD") == baud
        and iv(ss, "RTU_BAUD") == baud
        and master_effective > 0
        and master_effective == slave_effective
        and sv(ms, "RTU_CLOCK_PROFILE") == EXPECTED_CLOCK
        and sv(ss, "RTU_CLOCK_PROFILE") == EXPECTED_CLOCK
        and iv(ms, "RTU_FRAME_GAP_US") == 500
        and iv(ss, "RTU_FRAME_GAP_US") == 500
        and sv(ms, "RTU_MOTOR") == "ASYNC"
        and sv(ss, "RTU_MOTOR") == "ASYNC"
        and sv(ms, "RTU_TX_MODE") == "QUEUED"
        and sv(ss, "RTU_TX_MODE") == "QUEUED"
        and yes(ms, "RS485_AUTO_DIRECTION")
        and yes(ss, "RS485_AUTO_DIRECTION")
        and yes(ms, "RS485_QUEUED_TX_SUPPORTED")
        and yes(ss, "RS485_QUEUED_TX_SUPPORTED")
    )

    print(
        "RTUH1D_PROFILE "
        f"BAUD={baud} "
        f"MASTER_EFFECTIVE={master_effective} "
        f"SLAVE_EFFECTIVE={slave_effective} "
        f"ERROR_PCT={error_pct:.4f} "
        f"MASTER_CLOCK={sv(ms, 'RTU_CLOCK_PROFILE')} "
        f"SLAVE_CLOCK={sv(ss, 'RTU_CLOCK_PROFILE')} "
        f"PROFILE_PASS={'YES' if profile_pass else 'NO'}"
    )

    if not profile_pass:
        raise RuntimeError(f"perfil H1D invalido para {baud}")

    return {
        "baud": baud,
        "effective": master_effective,
        "error_pct": error_pct,
    }


def run_case(master, slave, duration_s: float, profile: dict[str, object]) -> dict[str, object]:
    baud = int(profile["baud"])

    print()
    print("-" * 78)
    print(
        "RTUH1D_CASE_BEGIN "
        f"BAUD={baud} CLOCK={EXPECTED_CLOCK} "
        "TCP=OFF GAP_US=500 MOTOR=ASYNC TX=QUEUED RTU=UNPACED"
    )
    print("-" * 78)

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)
    q.wait_server_disconnected(master, timeout_s=20.0)
    budget.send_command_wait(master, b"U\n", "RTU_RATE_MODE=UNPACED")

    q.reset_stats(master)
    p5b.reset_slave_stats(slave, 3.0)

    budget.send_command_wait(master, b"G\n", p5b.MASTER_START_ACK)
    time.sleep(duration_s)
    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
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
        iv(ms, "RTU_BAUD") == baud
        and iv(ss, "RTU_BAUD") == baud
        and iv(ms, "RTU_BAUD_EFFECTIVE") == int(profile["effective"])
        and iv(ss, "RTU_BAUD_EFFECTIVE") == int(profile["effective"])
        and sv(ms, "RTU_CLOCK_PROFILE") == EXPECTED_CLOCK
        and sv(ss, "RTU_CLOCK_PROFILE") == EXPECTED_CLOCK
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

    runtime_clean = (
        profile_pass
        and rtu_clean
        and iv(ms, "PERIPHERAL_FAILURE_COUNT") == 0
        and iv(ms, "SD_DATALOG_FAILED_COMMITS") == 0
    )

    row = {
        "baud": baud,
        "effective": int(profile["effective"]),
        "error_pct": float(profile["error_pct"]),
        "rtu_hz": rtu_hz,
        "started": started,
        "completed": completed,
        "failed": failed,
        "timeouts": timeouts,
        "master_crc": master_crc,
        "slave_crc": slave_crc,
        "runtime_clean": runtime_clean,
    }

    print(
        "RTUH1D_CASE "
        f"BAUD={baud} "
        f"EFFECTIVE_BAUD={row['effective']} "
        f"ERROR_PCT={row['error_pct']:.4f} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"STARTED={started} COMPLETED={completed} "
        f"FAILED={failed} TIMEOUTS={timeouts} "
        f"MASTER_CRC={master_crc} SLAVE_CRC={slave_crc} "
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
        raise ValueError("duration-per-case debe ser >= 20 s")

    master = p5b.open_serial_no_dtr(args.master_serial)
    slave = p5b.open_serial_no_dtr(args.slave_serial)
    rows: list[dict[str, object]] = []

    print("=" * 78)
    print(" A14 RTU-H1D - PACKAGE APB POLICY MATRIX")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"DURATION_PER_CASE_S={args.duration_per_case:.0f}")
    print("BAUDS=115200,230400,250000,460800,500000")
    print(f"CLOCK_POLICY={EXPECTED_CLOCK}")
    print("TCP=OFF")
    print("FRAME_GAP_US=500")
    print("MOTOR=ASYNC")
    print("TX_MODE=QUEUED")

    try:
        time.sleep(1.0)

        for baud, command in BAUD_CASES:
            profile = configure_case(master, slave, baud, command)
            rows.append(run_case(master, slave, args.duration_per_case, profile))

        print()
        print("=" * 78)
        print(" RTU-H1D SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUH1D_SUMMARY "
                f"BAUD={row['baud']} "
                f"EFFECTIVE_BAUD={row['effective']} "
                f"ERROR_PCT={row['error_pct']:.4f} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"FAILED={row['failed']} "
                f"TIMEOUTS={row['timeouts']} "
                f"MASTER_CRC={row['master_crc']} "
                f"SLAVE_CRC={row['slave_crc']} "
                f"RUNTIME_CLEAN={'YES' if row['runtime_clean'] else 'NO'}"
            )

        clean = [
            int(row["baud"])
            for row in rows
            if bool(row["runtime_clean"])
        ]
        print("RTUH1D_CLEAN_BAUDS=" + ",".join(str(v) for v in clean))

        expected = [baud for baud, _ in BAUD_CASES]
        if clean != expected:
            print("A14_RTU_H1D=FAIL_APB_POLICY_REGRESSION")
            return 2

        print("A14_RTU_H1D=PASS_APB_POLICY_MATRIX")
        return 0
    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
