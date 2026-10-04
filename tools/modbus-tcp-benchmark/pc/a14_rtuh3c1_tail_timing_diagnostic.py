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
import a14_rtuh2c_500k_100us_longrun as h2c
import a14_rtuh3b_bulk_rx_ab as h3b


RTU_STRUCTURAL_FLOOR_HZ = 650.0


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def yes(values: dict[str, str], key: str) -> bool:
    return sv(values, key).upper() == "YES"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=502)
    parser.add_argument("--duration", type=float, default=600.0)
    parser.add_argument("--bucket-seconds", type=float, default=60.0)
    args = parser.parse_args()

    if args.duration < 600.0:
        raise ValueError("duration debe ser >= 600 s")
    if args.bucket_seconds <= 0.0:
        raise ValueError("bucket-seconds debe ser > 0")

    master = p5b.open_serial_no_dtr(args.master_serial)
    slave = p5b.open_serial_no_dtr(args.slave_serial)

    print("=" * 78)
    print(" A14 RTU-H3C.1 - STRUCTURAL TAIL TIMING DIAGNOSTIC")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"DURATION_S={args.duration:.0f}")
    print(f"BUCKET_SECONDS={args.bucket_seconds:.0f}")
    print("BAUD=500000")
    print("FRAME_GAP_US=100")
    print("RX_FIFO_FULL=1")
    print("CLOCK=APB_FORCED")
    print("RX_MODE=BULK")
    print("MOTOR=ASYNC")
    print("TX_MODE=QUEUED")
    print("RTU_MODE=UNPACED")
    print("RTU_TIMEOUT_MS=25")
    print("TCP_TARGET_REQ_S=500")
    print("TCP_FC03_QUANTITY=125")
    print(f"RTU_STRUCTURAL_FLOOR_HZ={RTU_STRUCTURAL_FLOOR_HZ:.0f}")

    try:
        time.sleep(1.0)

        h3b.configure_fixed_profile(master, slave)
        h3b.configure_mode(master, slave, "BULK", b"+\n")

        budget.send_command_wait(
            master,
            b")\n",
            "RTU_SERVER_FRAMING=GAP",
        )
        budget.send_command_wait(
            slave,
            b"(\n",
            "RTU_SERVER_FRAMING=STRUCTURAL",
        )

        ms0, ss0 = h3b.snapshots(master, slave)

        profile_checks = {
            "master_baud": iv(ms0, "RTU_BAUD_EFFECTIVE") == 500000,
            "slave_baud": iv(ss0, "RTU_BAUD_EFFECTIVE") == 500000,
            "master_clock": sv(ms0, "RTU_CLOCK_PROFILE") == "APB_FORCED",
            "slave_clock": sv(ss0, "RTU_CLOCK_PROFILE") == "APB_FORCED",
            "master_gap": iv(ms0, "RTU_FRAME_GAP_US") == 100,
            "slave_gap": iv(ss0, "RTU_FRAME_GAP_US") == 100,
            "master_fifo": iv(ms0, "RTU_RX_FIFO_FULL") == 1,
            "slave_fifo": iv(ss0, "RTU_RX_FIFO_FULL") == 1,
            "master_rx": sv(ms0, "RTU_RX_MODE") == "BULK",
            "slave_rx": sv(ss0, "RTU_RX_MODE") == "BULK",
            "master_motor": sv(ms0, "RTU_MOTOR") == "ASYNC",
            "slave_motor": sv(ss0, "RTU_MOTOR") == "ASYNC",
            "master_tx": sv(ms0, "RTU_TX_MODE") == "QUEUED",
            "slave_tx": sv(ss0, "RTU_TX_MODE") == "QUEUED",
            "master_framing": sv(ms0, "RTU_SERVER_FRAMING") == "GAP",
            "slave_framing": sv(ss0, "RTU_SERVER_FRAMING") == "STRUCTURAL",
        }
        profile_pass = all(profile_checks.values())

        print(
            "RTUH3C1_PROFILE "
            f"MASTER_BAUD={iv(ms0, 'RTU_BAUD_EFFECTIVE')} "
            f"SLAVE_BAUD={iv(ss0, 'RTU_BAUD_EFFECTIVE')} "
            f"MASTER_FIFO={iv(ms0, 'RTU_RX_FIFO_FULL')} "
            f"SLAVE_FIFO={iv(ss0, 'RTU_RX_FIFO_FULL')} "
            f"MASTER_RX={sv(ms0, 'RTU_RX_MODE')} "
            f"SLAVE_RX={sv(ss0, 'RTU_RX_MODE')} "
            f"MASTER_FRAMING={sv(ms0, 'RTU_SERVER_FRAMING')} "
            f"SLAVE_FRAMING={sv(ss0, 'RTU_SERVER_FRAMING')} "
            f"PROFILE_PASS={'YES' if profile_pass else 'NO'}"
        )

        if not profile_pass:
            raise RuntimeError("perfil fijo H3C invalido")

        budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
        time.sleep(0.10)
        q.wait_server_disconnected(master, timeout_s=20.0)

        q.reset_stats(master)
        p5b.reset_slave_stats(slave, 3.0)

        budget.send_command_wait(master, b"G\n", p5b.MASTER_START_ACK)

        tcp = h2c.run_paced_fc03_bucketed(
            args.host,
            args.port,
            args.duration,
            500.0,
            125,
            args.bucket_seconds,
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
        rtu_timeouts = iv(ms, "RTU_MASTER_TIMEOUTS")
        master_crc = iv(ms, "RTU_CRC_ERRORS")
        slave_crc = iv(ss, "RTU_CRC_ERRORS")

        master_tx = iv(ms, "RTU_TX_FRAMES")
        master_rx = iv(ms, "RTU_RX_FRAMES")
        slave_rx = iv(ss, "RTU_RX_FRAMES")
        slave_tx = iv(ss, "RTU_TX_FRAMES")
        slave_ok = iv(ss, "RTU_REQUESTS_OK")

        request_path_gap = max(0, started - slave_rx)
        response_path_gap = max(0, slave_tx - master_rx)

        master_rx_bytes = iv(ms, "RTU_RX_BYTES")
        master_tx_bytes = iv(ms, "RTU_TX_BYTES")
        slave_rx_bytes = iv(ss, "RTU_RX_BYTES")
        slave_tx_bytes = iv(ss, "RTU_TX_BYTES")
        master_discarded_tails = iv(ms, "RTU_SERVER_DISCARDED_TAILS")
        master_discarded_bytes = iv(ms, "RTU_SERVER_DISCARDED_BYTES")
        slave_discarded_tails = iv(ss, "RTU_SERVER_DISCARDED_TAILS")
        slave_discarded_bytes = iv(ss, "RTU_SERVER_DISCARDED_BYTES")
        slave_discarded_last_length = iv(ss, "RTU_SERVER_DISCARDED_LAST_LENGTH")
        slave_discarded_last_age_us = iv(ss, "RTU_SERVER_DISCARDED_LAST_AGE_US")
        slave_discarded_max_age_us = iv(ss, "RTU_SERVER_DISCARDED_MAX_AGE_US")
        slave_discarded_len = {
            length: iv(ss, f"RTU_SERVER_DISCARDED_LEN{length}")
            for length in range(1, 9)
        }
        slave_discarded_len_gt8 = iv(ss, "RTU_SERVER_DISCARDED_LEN_GT8")

        expected_master_tx_bytes = master_tx * 8
        expected_slave_tx_bytes = slave_tx * 9

        master_tx_byte_accounting_pass = (
            master_tx_bytes == expected_master_tx_bytes
        )
        slave_tx_byte_accounting_pass = (
            slave_tx_bytes == expected_slave_tx_bytes
        )

        request_byte_gap = max(
            0,
            master_tx_bytes - slave_rx_bytes,
        )
        response_byte_gap = max(
            0,
            slave_tx_bytes - master_rx_bytes,
        )

        rtu_hz = (
            completed / (duration_ms / 1000.0)
            if duration_ms > 0
            else 0.0
        )

        final_profile_pass = (
            iv(ms, "RTU_BAUD_EFFECTIVE") == 500000
            and iv(ss, "RTU_BAUD_EFFECTIVE") == 500000
            and sv(ms, "RTU_CLOCK_PROFILE") == "APB_FORCED"
            and sv(ss, "RTU_CLOCK_PROFILE") == "APB_FORCED"
            and iv(ms, "RTU_FRAME_GAP_US") == 100
            and iv(ss, "RTU_FRAME_GAP_US") == 100
            and iv(ms, "RTU_RX_FIFO_FULL") == 1
            and iv(ss, "RTU_RX_FIFO_FULL") == 1
            and sv(ms, "RTU_RX_MODE") == "BULK"
            and sv(ss, "RTU_RX_MODE") == "BULK"
            and sv(ms, "RTU_MOTOR") == "ASYNC"
            and sv(ss, "RTU_MOTOR") == "ASYNC"
            and sv(ms, "RTU_TX_MODE") == "QUEUED"
            and sv(ss, "RTU_TX_MODE") == "QUEUED"
            and sv(ms, "RTU_SERVER_FRAMING") == "GAP"
            and sv(ss, "RTU_SERVER_FRAMING") == "STRUCTURAL"
        )

        rtu_clean = (
            started == completed == success
            and rejected == 0
            and failed == 0
            and verify == 0
            and rtu_timeouts == 0
            and master_crc == 0
            and slave_crc == 0
            and master_tx == started
            and master_rx == completed
            and slave_rx == completed
            and slave_tx == completed
            and slave_ok == completed
            and request_path_gap == 0
            and response_path_gap == 0
            and request_byte_gap == 0
            and response_byte_gap == 0
            and master_discarded_tails == 0
            and master_discarded_bytes == 0
            and slave_discarded_tails == 0
            and slave_discarded_bytes == 0
            and master_tx_byte_accounting_pass
            and slave_tx_byte_accounting_pass
        )

        rtu_floor_pass = rtu_hz >= RTU_STRUCTURAL_FLOOR_HZ

        tcp_clean = (
            tcp["timeouts"] == 0
            and tcp["transport_errors"] == 0
            and tcp["protocol_errors"] == 0
        )
        tcp_target_pass = float(tcp["target_pct"]) >= 99.0

        buckets = list(tcp["buckets"])
        bucket_target_pass = all(
            float(bucket["req_s"]) >= 495.0
            for bucket in buckets
        )

        bucket_rates = [float(bucket["req_s"]) for bucket in buckets]
        bucket_avg_lats = [float(bucket["avg_us"]) for bucket in buckets]
        bucket_p95_lats = [float(bucket["p95_us"]) for bucket in buckets]

        bucket_min = min(bucket_rates)
        bucket_max = max(bucket_rates)

        half = len(bucket_rates) // 2
        first_rates = bucket_rates[:half]
        second_rates = bucket_rates[half:]
        first_avg_lat = bucket_avg_lats[:half]
        second_avg_lat = bucket_avg_lats[half:]
        first_p95 = bucket_p95_lats[:half]
        second_p95 = bucket_p95_lats[half:]

        first_rate_avg = sum(first_rates) / len(first_rates)
        second_rate_avg = sum(second_rates) / len(second_rates)
        first_latency_avg = sum(first_avg_lat) / len(first_avg_lat)
        second_latency_avg = sum(second_avg_lat) / len(second_avg_lat)
        first_p95_avg = sum(first_p95) / len(first_p95)
        second_p95_avg = sum(second_p95) / len(second_p95)

        rate_drift_pct = (
            (second_rate_avg - first_rate_avg)
            / first_rate_avg
            * 100.0
            if first_rate_avg > 0.0
            else 0.0
        )
        latency_drift_pct = (
            (second_latency_avg - first_latency_avg)
            / first_latency_avg
            * 100.0
            if first_latency_avg > 0.0
            else 0.0
        )

        sd_active = yes(ms, "SD_DATALOG_ACTIVE")
        sd_pending = iv(ms, "SD_DATALOG_PENDING_BYTES")
        sd_capacity = iv(ms, "SD_DATALOG_BUFFER_BYTES")
        sd_accepted = iv(ms, "SD_DATALOG_ACCEPTED_BYTES")
        sd_committed = iv(ms, "SD_DATALOG_COMMITTED_BYTES")
        sd_failed = iv(ms, "SD_DATALOG_FAILED_COMMITS")
        sd_append_fails = iv(ms, "SD_APPEND_FAILS")
        peripheral_failures = iv(ms, "PERIPHERAL_FAILURE_COUNT")

        sd_clean = (
            sd_active
            and sd_capacity > 0
            and 0 <= sd_pending < sd_capacity
            and sd_accepted > 0
            and sd_committed > 0
            and sd_failed == 0
            and sd_append_fails == 0
        )

        txn_max_us = iv(ms, "RTU_TRANSACTION_MAX_US")
        over_5ms = iv(ms, "RTU_TRANSACTIONS_OVER_5MS")
        over_10ms = iv(ms, "RTU_TRANSACTIONS_OVER_10MS")
        over_20ms = iv(ms, "RTU_TRANSACTIONS_OVER_20MS")
        last_failure_us = iv(ms, "RTU_LAST_FAILURE_DURATION_US")
        max_failure_us = iv(ms, "RTU_MAX_FAILURE_DURATION_US")
        last_failure_result = iv(ms, "RTU_LAST_FAILURE_RESULT")
        service_gap_max_us = iv(ms, "RTU_SERVICE_GAP_MAX_US")
        loop_gap_max_us = iv(ms, "LOOP_GAP_MAX_US")

        runtime_clean = (
            final_profile_pass
            and rtu_clean
            and rtu_floor_pass
            and tcp_clean
            and tcp_target_pass
            and bucket_target_pass
            and sd_clean
            and peripheral_failures == 0
        )

        if slave_discarded_tails == 0:
            tail_diagnosis = "NO_TAIL_REPRODUCED"
        elif slave_discarded_max_age_us <= 1750:
            tail_diagnosis = "DISCARD_BEFORE_HOLD_LIMIT_UNEXPECTED"
        elif slave_discarded_max_age_us <= 5000:
            tail_diagnosis = "HOLD_1750_TOO_SHORT_CANDIDATE"
        else:
            tail_diagnosis = "LONG_FRAGMENT_GAP_REQUIRES_REVIEW"

        print()
        print("=" * 78)
        print(" RTU-H3C.1 STRUCTURAL TAIL TIMING SUMMARY")
        print("=" * 78)

        for bucket in buckets:
            print(
                "RTUH3C1_BUCKET "
                f"INDEX={bucket['index']} "
                f"START_S={bucket['start_s']:.0f} "
                f"END_S={bucket['end_s']:.0f} "
                f"OK={bucket['ok']} "
                f"REQ_S={bucket['req_s']:.3f} "
                f"AVG_US={bucket['avg_us']:.1f} "
                f"P95_US={bucket['p95_us']:.1f} "
                f"P99_US={bucket['p99_us']:.1f}"
            )

        print(f"RTUH3C1_TCP_REQ_S={float(tcp['achieved_req_s']):.3f}")
        print(f"RTUH3C1_TCP_TARGET_PCT={float(tcp['target_pct']):.3f}")
        print(f"RTUH3C1_TCP_AVG_US={float(tcp['avg_us']):.1f}")
        print(f"RTUH3C1_TCP_P95_US={float(tcp['p95_us']):.1f}")
        print(f"RTUH3C1_TCP_P99_US={float(tcp['p99_us']):.1f}")
        print(f"RTUH3C1_TCP_MAX_US={float(tcp['max_us']):.1f}")
        print(f"RTUH3C1_TCP_BUCKET_MIN_REQ_S={bucket_min:.3f}")
        print(f"RTUH3C1_TCP_BUCKET_MAX_REQ_S={bucket_max:.3f}")
        print(f"RTUH3C1_TCP_FIRST_HALF_AVG_REQ_S={first_rate_avg:.3f}")
        print(f"RTUH3C1_TCP_SECOND_HALF_AVG_REQ_S={second_rate_avg:.3f}")
        print(f"RTUH3C1_TCP_HALF_RATE_DRIFT_PCT={rate_drift_pct:.3f}")
        print(f"RTUH3C1_TCP_FIRST_HALF_AVG_US={first_latency_avg:.1f}")
        print(f"RTUH3C1_TCP_SECOND_HALF_AVG_US={second_latency_avg:.1f}")
        print(f"RTUH3C1_TCP_HALF_LATENCY_DRIFT_PCT={latency_drift_pct:.3f}")
        print(f"RTUH3C1_TCP_FIRST_HALF_P95_AVG_US={first_p95_avg:.1f}")
        print(f"RTUH3C1_TCP_SECOND_HALF_P95_AVG_US={second_p95_avg:.1f}")

        print(f"RTUH3C1_RTU_HZ={rtu_hz:.3f}")
        print(f"RTUH3C1_RTU_STARTED={started}")
        print(f"RTUH3C1_RTU_COMPLETED={completed}")
        print(f"RTUH3C1_RTU_SUCCESS={success}")
        print(f"RTUH3C1_RTU_FAILED={failed}")
        print(f"RTUH3C1_RTU_TIMEOUTS={rtu_timeouts}")
        print(f"RTUH3C1_MASTER_TX={master_tx}")
        print(f"RTUH3C1_MASTER_RX={master_rx}")
        print(f"RTUH3C1_SLAVE_RX={slave_rx}")
        print(f"RTUH3C1_SLAVE_TX={slave_tx}")
        print(f"RTUH3C1_SLAVE_OK={slave_ok}")
        print(f"RTUH3C1_REQUEST_PATH_GAP={request_path_gap}")
        print(f"RTUH3C1_RESPONSE_PATH_GAP={response_path_gap}")
        print(f"RTUH3C1_MASTER_RX_BYTES={master_rx_bytes}")
        print(f"RTUH3C1_MASTER_TX_BYTES={master_tx_bytes}")
        print(f"RTUH3C1_SLAVE_RX_BYTES={slave_rx_bytes}")
        print(f"RTUH3C1_SLAVE_TX_BYTES={slave_tx_bytes}")
        print(f"RTUH3C1_EXPECTED_MASTER_TX_BYTES={expected_master_tx_bytes}")
        print(f"RTUH3C1_EXPECTED_SLAVE_TX_BYTES={expected_slave_tx_bytes}")
        print(f"RTUH3C1_REQUEST_BYTE_GAP={request_byte_gap}")
        print(f"RTUH3C1_RESPONSE_BYTE_GAP={response_byte_gap}")
        print(f"RTUH3C1_MASTER_DISCARDED_TAILS={master_discarded_tails}")
        print(f"RTUH3C1_MASTER_DISCARDED_BYTES={master_discarded_bytes}")
        print(f"RTUH3C1_SLAVE_DISCARDED_TAILS={slave_discarded_tails}")
        print(f"RTUH3C1_SLAVE_DISCARDED_BYTES={slave_discarded_bytes}")
        print(f"RTUH3C1_SLAVE_DISCARDED_LAST_LENGTH={slave_discarded_last_length}")
        print(f"RTUH3C1_SLAVE_DISCARDED_LAST_AGE_US={slave_discarded_last_age_us}")
        print(f"RTUH3C1_SLAVE_DISCARDED_MAX_AGE_US={slave_discarded_max_age_us}")
        for length in range(1, 9):
            print(
                f"RTUH3C1_SLAVE_DISCARDED_LEN{length}="
                f"{slave_discarded_len[length]}"
            )
        print(f"RTUH3C1_SLAVE_DISCARDED_LEN_GT8={slave_discarded_len_gt8}")
        print(f"RTUH3C1_TAIL_DIAGNOSIS={tail_diagnosis}")
        print(
            "RTUH3C1_MASTER_TX_BYTE_ACCOUNTING_PASS="
            f"{'YES' if master_tx_byte_accounting_pass else 'NO'}"
        )
        print(
            "RTUH3C1_SLAVE_TX_BYTE_ACCOUNTING_PASS="
            f"{'YES' if slave_tx_byte_accounting_pass else 'NO'}"
        )
        print(f"RTUH3C1_MASTER_CRC={master_crc}")
        print(f"RTUH3C1_SLAVE_CRC={slave_crc}")

        print(f"RTUH3C1_TRANSACTION_MAX_US={txn_max_us}")
        print(f"RTUH3C1_TRANSACTIONS_OVER_5MS={over_5ms}")
        print(f"RTUH3C1_TRANSACTIONS_OVER_10MS={over_10ms}")
        print(f"RTUH3C1_TRANSACTIONS_OVER_20MS={over_20ms}")
        print(f"RTUH3C1_LAST_FAILURE_DURATION_US={last_failure_us}")
        print(f"RTUH3C1_MAX_FAILURE_DURATION_US={max_failure_us}")
        print(f"RTUH3C1_LAST_FAILURE_RESULT={last_failure_result}")
        print(f"RTUH3C1_RTU_SERVICE_GAP_MAX_US={service_gap_max_us}")
        print(f"RTUH3C1_LOOP_GAP_MAX_US={loop_gap_max_us}")

        print(f"RTUH3C1_SD_ACTIVE={'YES' if sd_active else 'NO'}")
        print(f"RTUH3C1_SD_ACCEPTED_BYTES={sd_accepted}")
        print(f"RTUH3C1_SD_COMMITTED_BYTES={sd_committed}")
        print(f"RTUH3C1_SD_PENDING_BYTES={sd_pending}")
        print(f"RTUH3C1_SD_FAILED_COMMITS={sd_failed}")
        print(f"RTUH3C1_PERIPHERAL_FAILURE_COUNT={peripheral_failures}")

        print(f"RTUH3C1_PROFILE_PASS={'YES' if final_profile_pass else 'NO'}")
        print(f"RTUH3C1_RTU_CLEAN={'YES' if rtu_clean else 'NO'}")
        print(f"RTUH3C1_RTU_FLOOR_PASS={'YES' if rtu_floor_pass else 'NO'}")
        print(f"RTUH3C1_TCP_CLEAN={'YES' if tcp_clean else 'NO'}")
        print(f"RTUH3C1_TCP_TARGET_PASS={'YES' if tcp_target_pass else 'NO'}")
        print(f"RTUH3C1_BUCKET_TARGET_PASS={'YES' if bucket_target_pass else 'NO'}")
        print(f"RTUH3C1_SD_CLEAN={'YES' if sd_clean else 'NO'}")
        print(f"RTUH3C1_RUNTIME_CLEAN={'YES' if runtime_clean else 'NO'}")

        if runtime_clean:
            print("A14_RTU_H3C1=PASS_DIAGNOSTIC_CLEAN")
        else:
            print("A14_RTU_H3C1=PASS_DIAGNOSTIC_CAPTURE_WITH_FAILURE")

        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
