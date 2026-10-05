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


MODES = [
    ("BYTE", b"-\n"),
    ("BULK", b"+\n"),
]


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def snapshots(master, slave):
    return (
        q.request_snapshot(master, echo=False),
        p5b.request_slave_snapshot(slave, 5.0),
    )


def configure_fixed_profile(master, slave) -> None:
    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)

    for port in (slave, master):
        budget.send_command_wait(
            port,
            b"9\n",
            "RTU_BAUD_REQUESTED=500000",
        )
        budget.send_command_wait(
            port,
            b"4\n",
            "RTU_FRAME_GAP_US=100",
        )
        budget.send_command_wait(
            port,
            b"?\n",
            "RTU_RX_FIFO_FULL=1",
        )
        budget.send_command_wait(
            port,
            b"-\n",
            "RTU_RX_MODE=BYTE",
        )

    budget.send_command_wait(
        master,
        b"U\n",
        "RTU_RATE_MODE=UNPACED",
    )
    time.sleep(0.10)

    ms, ss = snapshots(master, slave)

    checks = {
        "master_baud": iv(ms, "RTU_BAUD_EFFECTIVE") == 500000,
        "slave_baud": iv(ss, "RTU_BAUD_EFFECTIVE") == 500000,
        "master_clock": sv(ms, "RTU_CLOCK_PROFILE") == "APB_FORCED",
        "slave_clock": sv(ss, "RTU_CLOCK_PROFILE") == "APB_FORCED",
        "master_gap": iv(ms, "RTU_FRAME_GAP_US") == 100,
        "slave_gap": iv(ss, "RTU_FRAME_GAP_US") == 100,
        "master_fifo": iv(ms, "RTU_RX_FIFO_FULL") == 1,
        "slave_fifo": iv(ss, "RTU_RX_FIFO_FULL") == 1,
        "master_rx": sv(ms, "RTU_RX_MODE") == "BYTE",
        "slave_rx": sv(ss, "RTU_RX_MODE") == "BYTE",
        "master_motor": sv(ms, "RTU_MOTOR") == "ASYNC",
        "slave_motor": sv(ss, "RTU_MOTOR") == "ASYNC",
        "master_tx": sv(ms, "RTU_TX_MODE") == "QUEUED",
        "slave_tx": sv(ss, "RTU_TX_MODE") == "QUEUED",
    }

    print(
        "RTUH3B_FIXED_PROFILE "
        f"MASTER_BAUD={iv(ms, 'RTU_BAUD_EFFECTIVE')} "
        f"SLAVE_BAUD={iv(ss, 'RTU_BAUD_EFFECTIVE')} "
        f"MASTER_FIFO={iv(ms, 'RTU_RX_FIFO_FULL')} "
        f"SLAVE_FIFO={iv(ss, 'RTU_RX_FIFO_FULL')} "
        f"MASTER_RX={sv(ms, 'RTU_RX_MODE')} "
        f"SLAVE_RX={sv(ss, 'RTU_RX_MODE')} "
        f"PROFILE_PASS={'YES' if all(checks.values()) else 'NO'}"
    )

    if not all(checks.values()):
        raise RuntimeError("perfil fijo H3B invalido")


def configure_mode(master, slave, mode: str, command: bytes) -> None:
    ack = f"RTU_RX_MODE={mode}"
    budget.send_command_wait(slave, command, ack)
    budget.send_command_wait(master, command, ack)
    time.sleep(0.05)

    ms, ss = snapshots(master, slave)
    ok = (
        sv(ms, "RTU_RX_MODE") == mode
        and sv(ss, "RTU_RX_MODE") == mode
    )

    print(
        "RTUH3B_MODE_CONFIG "
        f"MODE={mode} "
        f"MASTER={sv(ms, 'RTU_RX_MODE')} "
        f"SLAVE={sv(ss, 'RTU_RX_MODE')} "
        f"PASS={'YES' if ok else 'NO'}"
    )

    if not ok:
        raise RuntimeError(f"modo RX invalido: {mode}")


def run_case(
    master,
    slave,
    host: str,
    port: int,
    duration_s: float,
    mode: str,
    tcp_enabled: bool,
) -> dict[str, object]:
    tcp_mode = "TCP500" if tcp_enabled else "OFF"

    print()
    print("-" * 78)
    print(
        "RTUH3B_CASE_BEGIN "
        f"RX_MODE={mode} TCP={tcp_mode} "
        "BAUD=500000 GAP_US=100 FIFO=1 CLOCK=APB_FORCED "
        "MOTOR=ASYNC TX=QUEUED RTU=UNPACED"
    )
    print("-" * 78)

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)
    q.wait_server_disconnected(master, timeout_s=20.0)

    q.reset_stats(master)
    p5b.reset_slave_stats(slave, 3.0)

    budget.send_command_wait(master, b"G\n", p5b.MASTER_START_ACK)

    tcp = None
    if tcp_enabled:
        tcp = budget.run_paced_fc03(
            host,
            port,
            duration_s,
            500.0,
            125,
        )
    else:
        time.sleep(duration_s)

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)

    ms, ss = snapshots(master, slave)

    duration_ms = iv(ms, "RTU_TRAFFIC_DURATION_MS")
    started = iv(ms, "RTU_REQUESTS_STARTED")
    rejected = iv(ms, "RTU_REQUESTS_REJECTED")
    completed = iv(ms, "RTU_REQUESTS_COMPLETED")
    success = iv(ms, "RTU_REQUESTS_SUCCESS")
    failed = iv(ms, "RTU_REQUESTS_FAILED")
    verify = iv(ms, "RTU_VERIFY_FAILS")
    timeouts = iv(ms, "RTU_MASTER_TIMEOUTS")
    master_crc = iv(ms, "RTU_CRC_ERRORS")
    slave_crc = iv(ss, "RTU_CRC_ERRORS")

    rtu_hz = (
        completed / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )

    profile_pass = (
        iv(ms, "RTU_BAUD_EFFECTIVE") == 500000
        and iv(ss, "RTU_BAUD_EFFECTIVE") == 500000
        and sv(ms, "RTU_CLOCK_PROFILE") == "APB_FORCED"
        and sv(ss, "RTU_CLOCK_PROFILE") == "APB_FORCED"
        and iv(ms, "RTU_FRAME_GAP_US") == 100
        and iv(ss, "RTU_FRAME_GAP_US") == 100
        and iv(ms, "RTU_RX_FIFO_FULL") == 1
        and iv(ss, "RTU_RX_FIFO_FULL") == 1
        and sv(ms, "RTU_RX_MODE") == mode
        and sv(ss, "RTU_RX_MODE") == mode
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
        and iv(ss, "RTU_RX_FRAMES") == completed
        and iv(ss, "RTU_TX_FRAMES") == completed
        and iv(ss, "RTU_REQUESTS_OK") == completed
    )

    tcp_req_s = 0.0
    tcp_avg_us = 0.0
    tcp_p95_us = 0.0
    tcp_p99_us = 0.0
    tcp_clean = True

    if tcp_enabled:
        assert tcp is not None
        tcp_req_s = float(tcp["achieved_req_s"])
        tcp_avg_us = float(tcp["avg_us"])
        tcp_p95_us = float(tcp["p95_us"])
        tcp_p99_us = float(tcp["p99_us"])

        tcp_clean = (
            float(tcp["target_pct"]) >= 99.0
            and tcp["timeouts"] == 0
            and tcp["transport_errors"] == 0
            and tcp["protocol_errors"] == 0
            and iv(ms, "FRAME_TIMEOUTS") == 0
            and iv(ms, "BUS_LOCK_TIMEOUTS") == 0
            and iv(ms, "PROTOCOL_ERRORS") == 0
            and iv(ms, "REQUESTS_OK") == int(tcp["ok"])
        )

    runtime_clean = (
        profile_pass
        and rtu_clean
        and tcp_clean
        and iv(ms, "PERIPHERAL_FAILURE_COUNT") == 0
        and iv(ms, "SD_DATALOG_FAILED_COMMITS") == 0
    )

    row = {
        "mode": mode,
        "tcp": tcp_mode,
        "rtu_hz": rtu_hz,
        "tcp_req_s": tcp_req_s,
        "tcp_avg_us": tcp_avg_us,
        "tcp_p95_us": tcp_p95_us,
        "tcp_p99_us": tcp_p99_us,
        "service_gap_max_us": iv(ms, "RTU_SERVICE_GAP_MAX_US"),
        "loop_gap_max_us": iv(ms, "LOOP_GAP_MAX_US"),
        "failed": failed,
        "timeouts": timeouts,
        "master_crc": master_crc,
        "slave_crc": slave_crc,
        "runtime_clean": runtime_clean,
    }

    print(
        "RTUH3B_CASE "
        f"RX_MODE={mode} TCP={tcp_mode} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"TCP_REQ_S={tcp_req_s:.3f} "
        f"TCP_AVG_US={tcp_avg_us:.1f} "
        f"TCP_P95_US={tcp_p95_us:.1f} "
        f"TCP_P99_US={tcp_p99_us:.1f} "
        f"RTU_SERVICE_GAP_MAX_US={row['service_gap_max_us']} "
        f"LOOP_GAP_MAX_US={row['loop_gap_max_us']} "
        f"FAILED={failed} TIMEOUTS={timeouts} "
        f"MASTER_CRC={master_crc} SLAVE_CRC={slave_crc} "
        f"RUNTIME_CLEAN={'YES' if runtime_clean else 'NO'}"
    )

    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=502)
    parser.add_argument("--duration-per-case", type=float, default=60.0)
    args = parser.parse_args()

    if args.duration_per_case < 30.0:
        raise ValueError("duration-per-case debe ser >= 30 s")

    master = p5b.open_serial_no_dtr(args.master_serial)
    slave = p5b.open_serial_no_dtr(args.slave_serial)
    rows: list[dict[str, object]] = []

    print("=" * 78)
    print(" A14 RTU-H3B - BYTE RX VS BULK RX")
    print("=" * 78)
    print("BAUD=500000")
    print("FRAME_GAP_US=100")
    print("RX_FIFO_FULL=1")
    print("CLOCK=APB_FORCED")
    print("RX_MODES=BYTE,BULK")
    print("TCP_CASES=500,OFF")
    print("MOTOR=ASYNC")
    print("TX_MODE=QUEUED")

    try:
        time.sleep(1.0)
        configure_fixed_profile(master, slave)

        for tcp_enabled in (True, False):
            for mode, command in MODES:
                configure_mode(master, slave, mode, command)
                rows.append(
                    run_case(
                        master,
                        slave,
                        args.host,
                        args.port,
                        args.duration_per_case,
                        mode,
                        tcp_enabled,
                    )
                )

        print()
        print("=" * 78)
        print(" RTU-H3B SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUH3B_SUMMARY "
                f"RX_MODE={row['mode']} TCP={row['tcp']} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"TCP_REQ_S={row['tcp_req_s']:.3f} "
                f"TCP_AVG_US={row['tcp_avg_us']:.1f} "
                f"TCP_P95_US={row['tcp_p95_us']:.1f} "
                f"TCP_P99_US={row['tcp_p99_us']:.1f} "
                f"RUNTIME_CLEAN={'YES' if row['runtime_clean'] else 'NO'}"
            )

        for tcp_mode in ("TCP500", "OFF"):
            byte = next(
                row for row in rows
                if row["tcp"] == tcp_mode and row["mode"] == "BYTE"
            )
            bulk = next(
                row for row in rows
                if row["tcp"] == tcp_mode and row["mode"] == "BULK"
            )

            if not bool(byte["runtime_clean"]):
                print("A14_RTU_H3B=REVIEW_BYTE_CONTROL_FAILURE")
                return 2

            gain = (
                (float(bulk["rtu_hz"]) / float(byte["rtu_hz"]) - 1.0)
                * 100.0
            )

            print(
                "RTUH3B_DELTA "
                f"TCP={tcp_mode} "
                f"BULK_RTU_GAIN_PCT={gain:.3f} "
                f"TCP_AVG_DELTA_US="
                f"{float(bulk['tcp_avg_us']) - float(byte['tcp_avg_us']):.1f} "
                f"TCP_P95_DELTA_US="
                f"{float(bulk['tcp_p95_us']) - float(byte['tcp_p95_us']):.1f} "
                f"BULK_CLEAN={'YES' if bulk['runtime_clean'] else 'NO'}"
            )

        bulk_tcp = next(
            row for row in rows
            if row["tcp"] == "TCP500" and row["mode"] == "BULK"
        )

        target_pass = (
            bool(bulk_tcp["runtime_clean"])
            and float(bulk_tcp["rtu_hz"]) >= 480.0
            and float(bulk_tcp["tcp_req_s"]) >= 495.0
        )

        print(
            "RTUH3B_TCP480_TARGET_PASS="
            f"{'YES' if target_pass else 'NO'}"
        )
        print(
            "RTUH3B_BULK_TCP500_RTU_HZ="
            f"{float(bulk_tcp['rtu_hz']):.3f}"
        )
        print(
            "A14_RTU_H3B="
            + (
                "PASS_BULK_RX_TCP480"
                if target_pass
                else "PASS_CHARACTERIZED"
            )
        )
        return 0
    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
