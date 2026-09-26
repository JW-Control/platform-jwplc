from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_perf_fc03_qualification_sweep as q
import a14_p5b_master_slave_qualification as p5b
import a14_p5e1_unpaced_fc03_ceiling as e1


def send_command_wait(ser, command: bytes, ack: str, timeout_s: float = 3.0):
    ser.reset_input_buffer()
    ser.write(command)
    ser.flush()
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        line = p5b.read_line(ser)
        if line == ack:
            return
    raise TimeoutError(f"command sin ACK esperado={ack}")


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=502)
    parser.add_argument("--duration-per-case", type=float, default=30.0)
    args = parser.parse_args()

    if args.duration_per_case < 30.0:
        raise ValueError("duration-per-case debe ser >= 30 s")

    cases = [
        ("OFF", None, None, None),
        ("50", 50, b"A\n", "RTU_RATE_HZ=50"),
        ("100", 100, b"B\n", "RTU_RATE_HZ=100"),
        ("150", 150, b"C\n", "RTU_RATE_HZ=150"),
        ("200", 200, b"D\n", "RTU_RATE_HZ=200"),
        ("250", 250, b"E\n", "RTU_RATE_HZ=250"),
        ("300", 300, b"F\n", "RTU_RATE_HZ=300"),
        ("UNPACED", None, b"U\n", "RTU_RATE_MODE=UNPACED"),
    ]

    master = p5b.open_serial_no_dtr(args.master_serial)
    slave = p5b.open_serial_no_dtr(args.slave_serial)
    rows = []

    print("=" * 78)
    print(" A14 P5-RTU/TCP BALANCE - FULL RUNTIME")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"DURATION_PER_CASE_S={args.duration_per_case:.0f}")
    print("TCP_FC03_QUANTITY=125")
    print("TCP_PACING=NONE")
    print("TCP_OUTSTANDING_REQUESTS=1")
    print("RTU_BAUD=115200")
    print("RTU_CONFIG=8N1")
    print("RTU_SLAVE_FRAME_GAP_MS=2")
    print("RTU_CASES=OFF,50,100,150,200,250,300,UNPACED")

    try:
        time.sleep(1.0)
        send_command_wait(slave, b"2\n", "RTU_FRAME_GAP_MS=2")
        probe = p5b.request_slave_snapshot(slave, 5.0)
        if probe.get("RTU_FRAME_GAP_MS") != "2":
            raise RuntimeError("slave frame gap no quedo en 2 ms")

        baseline_tcp = None

        for label, target_hz, command, ack in cases:
            print()
            print("-" * 78)
            print(f"CASE_BEGIN RTU={label}")
            print("-" * 78)

            send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
            q.wait_server_disconnected(master, timeout_s=20.0)

            if command is not None:
                send_command_wait(master, command, ack)

            q.reset_stats(master)
            p5b.reset_slave_stats(slave, 3.0)

            if label != "OFF":
                send_command_wait(master, b"G\n", p5b.MASTER_START_ACK)

            result = e1.run_unpaced(
                args.host,
                args.port,
                args.duration_per_case,
                125,
                args.duration_per_case,
            )

            if label != "OFF":
                send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)

            time.sleep(0.10)
            ms = q.request_snapshot(master, echo=False)
            ss = p5b.request_slave_snapshot(slave, 5.0)

            tcp_req_s = float(result["achieved_req_s"])
            tcp_clean = (
                result["timeouts"] == 0
                and result["transport_errors"] == 0
                and result["protocol_errors"] == 0
                and iv(ms, "FRAME_TIMEOUTS") == 0
                and iv(ms, "BUS_LOCK_TIMEOUTS") == 0
                and iv(ms, "PROTOCOL_ERRORS") == 0
                and iv(ms, "REQUESTS_OK") == int(result["ok"])
            )

            duration_ms = iv(ms, "RTU_TRAFFIC_DURATION_MS")
            started = iv(ms, "RTU_REQUESTS_STARTED")
            rejected = iv(ms, "RTU_REQUESTS_REJECTED")
            completed = iv(ms, "RTU_REQUESTS_COMPLETED")
            success = iv(ms, "RTU_REQUESTS_SUCCESS")
            failed = iv(ms, "RTU_REQUESTS_FAILED")
            verify = iv(ms, "RTU_VERIFY_FAILS")
            timeouts = iv(ms, "RTU_MASTER_TIMEOUTS")
            crc = iv(ms, "RTU_CRC_ERRORS")
            skipped = iv(ms, "RTU_PERIODS_SKIPPED")

            rtu_hz = completed / (duration_ms / 1000.0) if duration_ms > 0 else 0.0

            slave_rx = iv(ss, "RTU_RX_FRAMES")
            slave_tx = iv(ss, "RTU_TX_FRAMES")
            slave_ok = iv(ss, "RTU_REQUESTS_OK")
            slave_crc = iv(ss, "RTU_CRC_ERRORS")

            if label == "OFF":
                rtu_clean = started == 0 and completed == 0 and slave_rx == 0 and slave_tx == 0
            else:
                rtu_clean = (
                    started == completed == success
                    and rejected == 0
                    and failed == 0
                    and verify == 0
                    and timeouts == 0
                    and crc == 0
                    and slave_crc == 0
                    and slave_rx == completed
                    and slave_tx == completed
                    and slave_ok == completed
                )

            peripheral_failures = iv(ms, "PERIPHERAL_FAILURE_COUNT")
            sd_failed = iv(ms, "SD_DATALOG_FAILED_COMMITS")
            sd_committed = iv(ms, "SD_DATALOG_COMMITTED_BYTES")
            runtime_clean = (
                tcp_clean and rtu_clean
                and peripheral_failures == 0
                and sd_failed == 0
            )

            if baseline_tcp is None:
                baseline_tcp = tcp_req_s

            retention = tcp_req_s / baseline_tcp * 100.0
            drop = 100.0 - retention
            target_pct = (
                rtu_hz / target_hz * 100.0
                if target_hz else 0.0
            )

            row = {
                "label": label,
                "rtu_hz": rtu_hz,
                "target_pct": target_pct,
                "skipped": skipped,
                "tcp_req_s": tcp_req_s,
                "retention": retention,
                "drop": drop,
                "useful": float(result["useful_mbps"]),
                "avg": float(result["latency_avg_us"]),
                "p95": float(result["latency_p95_us"]),
                "p99": float(result["latency_p99_us"]),
                "runtime_clean": runtime_clean,
            }
            rows.append(row)

            print(
                "P5RTUTCP_CASE "
                f"RTU={label} "
                f"RTU_HZ={rtu_hz:.3f} "
                f"RTU_TARGET_PCT={target_pct:.2f} "
                f"RTU_SKIPPED={skipped} "
                f"TCP_REQ_S={tcp_req_s:.3f} "
                f"TCP_RETENTION_PCT={retention:.3f} "
                f"TCP_DROP_PCT={drop:.3f} "
                f"TCP_USEFUL_MBPS={result['useful_mbps']:.4f} "
                f"TCP_AVG_US={result['latency_avg_us']:.1f} "
                f"TCP_P95_US={result['latency_p95_us']:.1f} "
                f"TCP_P99_US={result['latency_p99_us']:.1f} "
                f"SD_COMMITTED_B={sd_committed} "
                f"TCP_CLEAN={'YES' if tcp_clean else 'NO'} "
                f"RTU_CLEAN={'YES' if rtu_clean else 'NO'} "
                f"RUNTIME_CLEAN={'YES' if runtime_clean else 'NO'}"
            )

            if not runtime_clean:
                return 2

        print()
        print("=" * 78)
        print(" P5-RTU/TCP BALANCE SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "P5RTUTCP_SUMMARY "
                f"RTU={row['label']} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"TCP_REQ_S={row['tcp_req_s']:.3f} "
                f"TCP_RETENTION_PCT={row['retention']:.3f} "
                f"TCP_DROP_PCT={row['drop']:.3f} "
                f"TCP_USEFUL_MBPS={row['useful']:.4f}"
            )

        for threshold in (99.0, 95.0, 90.0):
            candidates = [
                row for row in rows
                if row["label"] != "OFF"
                and row["retention"] >= threshold
                and row["runtime_clean"]
            ]
            key = int(threshold)
            if candidates:
                best = max(candidates, key=lambda row: row["rtu_hz"])
                print(f"P5RTUTCP_MAX_RTU_AT_TCP_RETENTION_{key}={best['rtu_hz']:.3f}")
                print(f"P5RTUTCP_PROFILE_AT_TCP_RETENTION_{key}={best['label']}")
            else:
                print(f"P5RTUTCP_MAX_RTU_AT_TCP_RETENTION_{key}=NONE")

        print("A14_P5_RTU_TCP_BALANCE=PASS_CHARACTERIZED")
        return 0
    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
