from __future__ import annotations

import argparse
import statistics
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_perf_fc03_qualification_sweep as q
import a14_p5b_master_slave_qualification as p5b
import a14_p5rtu_tcp_budget_frontier as budget
import a14_rtuh3b_bulk_rx_ab as h3b


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def run_window(
    master,
    slave,
    host: str,
    port: int,
    duration_s: float,
    label: str,
    mode: str,
    command: bytes,
) -> dict[str, object]:
    h3b.configure_mode(master, slave, mode, command)

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)
    q.wait_server_disconnected(master, timeout_s=20.0)

    q.reset_stats(master)
    p5b.reset_slave_stats(slave, 3.0)

    budget.send_command_wait(master, b"G\n", p5b.MASTER_START_ACK)

    tcp = budget.run_paced_fc03(
        host,
        port,
        duration_s,
        500.0,
        125,
    )

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)

    ms, ss = h3b.snapshots(master, slave)

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
    master_rx = iv(ms, "RTU_RX_FRAMES")
    master_tx = iv(ms, "RTU_TX_FRAMES")
    slave_rx = iv(ss, "RTU_RX_FRAMES")
    slave_tx = iv(ss, "RTU_TX_FRAMES")
    slave_ok = iv(ss, "RTU_REQUESTS_OK")

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

    rtu_clean = (
        started == completed == success
        and rejected == 0
        and failed == 0
        and verify == 0
        and timeouts == 0
        and master_crc == 0
        and slave_crc == 0
        and master_tx == started
        and master_rx == completed
        and slave_rx == completed
        and slave_tx == completed
        and slave_ok == completed
    )

    runtime_clean = (
        profile_pass
        and tcp_clean
        and rtu_clean
        and iv(ms, "PERIPHERAL_FAILURE_COUNT") == 0
        and iv(ms, "SD_DATALOG_FAILED_COMMITS") == 0
    )

    request_path_gap = max(0, started - slave_rx)
    response_path_gap = max(0, slave_tx - master_rx)

    row = {
        "label": label,
        "mode": mode,
        "rtu_hz": rtu_hz,
        "tcp_req_s": float(tcp["achieved_req_s"]),
        "tcp_avg_us": float(tcp["avg_us"]),
        "tcp_p95_us": float(tcp["p95_us"]),
        "tcp_p99_us": float(tcp["p99_us"]),
        "started": started,
        "completed": completed,
        "success": success,
        "failed": failed,
        "timeouts": timeouts,
        "master_rx": master_rx,
        "master_tx": master_tx,
        "slave_rx": slave_rx,
        "slave_tx": slave_tx,
        "slave_ok": slave_ok,
        "request_path_gap": request_path_gap,
        "response_path_gap": response_path_gap,
        "service_gap_max_us": iv(ms, "RTU_SERVICE_GAP_MAX_US"),
        "loop_gap_max_us": iv(ms, "LOOP_GAP_MAX_US"),
        "txn_max_us": iv(ms, "RTU_TRANSACTION_MAX_US"),
        "over_5ms": iv(ms, "RTU_TRANSACTIONS_OVER_5MS"),
        "over_10ms": iv(ms, "RTU_TRANSACTIONS_OVER_10MS"),
        "over_20ms": iv(ms, "RTU_TRANSACTIONS_OVER_20MS"),
        "last_failure_us": iv(ms, "RTU_LAST_FAILURE_DURATION_US"),
        "max_failure_us": iv(ms, "RTU_MAX_FAILURE_DURATION_US"),
        "last_failure_result": iv(ms, "RTU_LAST_FAILURE_RESULT"),
        "runtime_clean": runtime_clean,
    }

    print(
        "RTUH3B1_RUN "
        f"LABEL={label} RX_MODE={mode} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"TCP_REQ_S={row['tcp_req_s']:.3f} "
        f"TCP_AVG_US={row['tcp_avg_us']:.1f} "
        f"TCP_P95_US={row['tcp_p95_us']:.1f} "
        f"TCP_P99_US={row['tcp_p99_us']:.1f} "
        f"STARTED={started} COMPLETED={completed} SUCCESS={success} "
        f"FAILED={failed} TIMEOUTS={timeouts} "
        f"MASTER_TX={master_tx} MASTER_RX={master_rx} "
        f"SLAVE_RX={slave_rx} SLAVE_TX={slave_tx} SLAVE_OK={slave_ok} "
        f"REQUEST_PATH_GAP={request_path_gap} "
        f"RESPONSE_PATH_GAP={response_path_gap} "
        f"TXN_MAX_US={row['txn_max_us']} "
        f"OVER_5MS={row['over_5ms']} "
        f"OVER_10MS={row['over_10ms']} "
        f"OVER_20MS={row['over_20ms']} "
        f"LAST_FAILURE_US={row['last_failure_us']} "
        f"MAX_FAILURE_US={row['max_failure_us']} "
        f"LAST_FAILURE_RESULT={row['last_failure_result']} "
        f"RTU_SERVICE_GAP_MAX_US={row['service_gap_max_us']} "
        f"LOOP_GAP_MAX_US={row['loop_gap_max_us']} "
        f"RUNTIME_CLEAN={'YES' if runtime_clean else 'NO'}"
    )

    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=502)
    parser.add_argument("--duration-per-run", type=float, default=60.0)
    parser.add_argument("--bulk-runs", type=int, default=5)
    args = parser.parse_args()

    if args.duration_per_run < 30.0:
        raise ValueError("duration-per-run debe ser >= 30 s")
    if args.bulk_runs < 3:
        raise ValueError("bulk-runs debe ser >= 3")

    master = p5b.open_serial_no_dtr(args.master_serial)
    slave = p5b.open_serial_no_dtr(args.slave_serial)

    rows: list[dict[str, object]] = []

    print("=" * 78)
    print(" A14 RTU-H3B.1 - BULK RX TCP500 REPEATABILITY")
    print("=" * 78)
    print("BAUD=500000")
    print("FRAME_GAP_US=100")
    print("RX_FIFO_FULL=1")
    print("CLOCK=APB_FORCED")
    print("TCP=500")
    print("MOTOR=ASYNC")
    print("TX_MODE=QUEUED")
    print(f"DURATION_PER_RUN_S={args.duration_per_run}")
    print(f"BULK_RUNS={args.bulk_runs}")

    try:
        time.sleep(1.0)
        h3b.configure_fixed_profile(master, slave)

        rows.append(
            run_window(
                master,
                slave,
                args.host,
                args.port,
                args.duration_per_run,
                "BYTE_CONTROL",
                "BYTE",
                b"-\n",
            )
        )

        for index in range(1, args.bulk_runs + 1):
            rows.append(
                run_window(
                    master,
                    slave,
                    args.host,
                    args.port,
                    args.duration_per_run,
                    f"BULK_R{index}",
                    "BULK",
                    b"+\n",
                )
            )

        print()
        print("=" * 78)
        print(" RTU-H3B.1 SUMMARY")
        print("=" * 78)

        control = rows[0]
        bulk_rows = rows[1:]

        for row in rows:
            print(
                "RTUH3B1_SUMMARY "
                f"LABEL={row['label']} RX_MODE={row['mode']} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"TCP_REQ_S={row['tcp_req_s']:.3f} "
                f"FAILED={row['failed']} TIMEOUTS={row['timeouts']} "
                f"REQUEST_PATH_GAP={row['request_path_gap']} "
                f"RESPONSE_PATH_GAP={row['response_path_gap']} "
                f"TXN_MAX_US={row['txn_max_us']} "
                f"RUNTIME_CLEAN={'YES' if row['runtime_clean'] else 'NO'}"
            )

        bulk_hz = [float(row["rtu_hz"]) for row in bulk_rows]
        total_failed = sum(int(row["failed"]) for row in bulk_rows)
        total_timeouts = sum(int(row["timeouts"]) for row in bulk_rows)
        total_request_gap = sum(
            int(row["request_path_gap"]) for row in bulk_rows
        )
        total_response_gap = sum(
            int(row["response_path_gap"]) for row in bulk_rows
        )
        clean_runs = sum(
            1 for row in bulk_rows if bool(row["runtime_clean"])
        )

        stable = (
            bool(control["runtime_clean"])
            and clean_runs == len(bulk_rows)
            and total_failed == 0
            and total_timeouts == 0
        )

        print(
            "RTUH3B1_AGGREGATE "
            f"BULK_RUNS={len(bulk_rows)} "
            f"BULK_CLEAN_RUNS={clean_runs} "
            f"BULK_RTU_HZ_MIN={min(bulk_hz):.3f} "
            f"BULK_RTU_HZ_AVG={statistics.mean(bulk_hz):.3f} "
            f"BULK_RTU_HZ_MAX={max(bulk_hz):.3f} "
            f"BULK_FAILED_TOTAL={total_failed} "
            f"BULK_TIMEOUTS_TOTAL={total_timeouts} "
            f"REQUEST_PATH_GAP_TOTAL={total_request_gap} "
            f"RESPONSE_PATH_GAP_TOTAL={total_response_gap}"
        )

        print(
            "RTUH3B1_TIMEOUT_REPRODUCED="
            f"{'YES' if total_timeouts > 0 else 'NO'}"
        )
        print(
            "RTUH3B1_BULK_REPEATABILITY_PASS="
            f"{'YES' if stable else 'NO'}"
        )

        if stable:
            print("A14_RTU_H3B1=PASS_BULK_REPEATABILITY")
        else:
            print("A14_RTU_H3B1=PASS_CHARACTERIZED_REVIEW_REQUIRED")

        return 0
    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
