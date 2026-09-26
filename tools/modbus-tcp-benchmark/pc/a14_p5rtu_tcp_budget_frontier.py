from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_perf_fc03_qualification_sweep as q
import a14_p5b_master_slave_qualification as p5b


def send_command_wait(ser, command: bytes, ack: str, timeout_s: float = 3.0) -> None:
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


def run_paced_fc03(
    host: str,
    port: int,
    duration_s: float,
    target_req_s: float,
    quantity: int = 125,
) -> dict[str, float | int]:
    expected = q.expected_body(quantity)
    interval_s = 1.0 / target_req_s

    sock = socket.create_connection((host, port), timeout=3.0)
    sock.settimeout(1.0)

    sent = ok = 0
    timeouts = transport_errors = protocol_errors = 0
    tx_bytes = rx_bytes = 0
    latencies_us: list[float] = []

    start = time.perf_counter()
    deadline = start + duration_s
    next_release = start

    try:
        while True:
            now = time.perf_counter()
            if now >= deadline:
                break

            if now < next_release:
                remaining = next_release - now
                if remaining > 0.0015:
                    time.sleep(remaining - 0.0008)
                while time.perf_counter() < next_release:
                    pass

            if time.perf_counter() >= deadline:
                break

            tid = (sent + 1) & 0xFFFF
            request = struct.pack(
                ">HHHBBHH",
                tid, 0, 6, 1, 3, 0, quantity,
            )
            t0 = time.perf_counter_ns()

            try:
                sock.sendall(request)
                sent += 1
                tx_bytes += len(request)

                header = q.recv_exact(sock, 7)
                rx_tid, pid, length, unit = struct.unpack(">HHHB", header)

                if length < 2:
                    raise ValueError(f"MBAP length invalido: {length}")

                body = q.recv_exact(sock, length - 1)
                t1 = time.perf_counter_ns()
                latencies_us.append((t1 - t0) / 1000.0)
                rx_bytes += len(header) + len(body)

                valid = (
                    rx_tid == tid
                    and pid == 0
                    and unit == 1
                    and length == (3 + 2 * quantity)
                    and body == expected
                )
                if not valid:
                    protocol_errors += 1
                    break
                ok += 1

            except socket.timeout:
                timeouts += 1
                break
            except (ConnectionError, OSError):
                transport_errors += 1
                break
            except ValueError:
                protocol_errors += 1
                break

            next_release += interval_s

    finally:
        elapsed = time.perf_counter() - start
        try:
            sock.close()
        except Exception:
            pass

    achieved = ok / elapsed if elapsed > 0.0 else 0.0
    target_pct = achieved / target_req_s * 100.0
    useful_mbps = ok * quantity * 2 * 8 / elapsed / 1_000_000.0 if elapsed > 0.0 else 0.0
    total_mbps = (tx_bytes + rx_bytes) * 8 / elapsed / 1_000_000.0 if elapsed > 0.0 else 0.0

    if latencies_us:
        avg_us = sum(latencies_us) / len(latencies_us)
        p95_us = q.percentile(latencies_us, 0.95)
        p99_us = q.percentile(latencies_us, 0.99)
        max_us = max(latencies_us)
    else:
        avg_us = p95_us = p99_us = max_us = 0.0

    return {
        "elapsed_s": elapsed,
        "sent": sent,
        "ok": ok,
        "timeouts": timeouts,
        "transport_errors": transport_errors,
        "protocol_errors": protocol_errors,
        "achieved_req_s": achieved,
        "target_pct": target_pct,
        "useful_mbps": useful_mbps,
        "total_mbps": total_mbps,
        "avg_us": avg_us,
        "p95_us": p95_us,
        "p99_us": p99_us,
        "max_us": max_us,
    }


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

    tcp_targets: list[int | None] = [1000, 900, 800, 700, 600, 500, None]

    master = p5b.open_serial_no_dtr(args.master_serial)
    slave = p5b.open_serial_no_dtr(args.slave_serial)
    rows: list[dict[str, object]] = []

    print("=" * 78)
    print(" A14 P5 - RTU/TCP BUDGET FRONTIER")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"DURATION_PER_CASE_S={args.duration_per_case:.0f}")
    print("TCP_TARGETS_REQ_S=1000,900,800,700,600,500,OFF")
    print("TCP_FC03_QUANTITY=125")
    print("TCP_OUTSTANDING_REQUESTS=1")
    print("RTU_MODE=UNPACED")
    print("RTU_BAUD=115200")
    print("RTU_CONFIG=8N1")
    print("RTU_SLAVE_FRAME_GAP_MS=2")

    try:
        time.sleep(1.0)

        send_command_wait(slave, b"2\n", "RTU_FRAME_GAP_MS=2")
        probe = p5b.request_slave_snapshot(slave, 5.0)
        if probe.get("RTU_FRAME_GAP_MS") != "2":
            raise RuntimeError("slave frame gap no quedo en 2 ms")

        for target in tcp_targets:
            label = "OFF" if target is None else str(target)

            print()
            print("-" * 78)
            print(f"CASE_BEGIN TCP_TARGET_REQ_S={label} RTU=UNPACED")
            print("-" * 78)

            send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
            q.wait_server_disconnected(master, timeout_s=20.0)
            send_command_wait(master, b"U\n", "RTU_RATE_MODE=UNPACED")

            q.reset_stats(master)
            p5b.reset_slave_stats(slave, 3.0)
            send_command_wait(master, b"G\n", p5b.MASTER_START_ACK)

            if target is None:
                t0 = time.perf_counter()
                time.sleep(args.duration_per_case)
                elapsed = time.perf_counter() - t0
                tcp = {
                    "elapsed_s": elapsed,
                    "sent": 0,
                    "ok": 0,
                    "timeouts": 0,
                    "transport_errors": 0,
                    "protocol_errors": 0,
                    "achieved_req_s": 0.0,
                    "target_pct": 100.0,
                    "useful_mbps": 0.0,
                    "total_mbps": 0.0,
                    "avg_us": 0.0,
                    "p95_us": 0.0,
                    "p99_us": 0.0,
                    "max_us": 0.0,
                }
            else:
                tcp = run_paced_fc03(
                    args.host,
                    args.port,
                    args.duration_per_case,
                    float(target),
                    125,
                )

            send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
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
            rtu_timeouts = iv(ms, "RTU_MASTER_TIMEOUTS")
            rtu_crc = iv(ms, "RTU_CRC_ERRORS")

            rtu_hz = completed / (duration_ms / 1000.0) if duration_ms > 0 else 0.0

            slave_rx = iv(ss, "RTU_RX_FRAMES")
            slave_tx = iv(ss, "RTU_TX_FRAMES")
            slave_ok = iv(ss, "RTU_REQUESTS_OK")
            slave_crc = iv(ss, "RTU_CRC_ERRORS")

            rtu_clean = (
                started == completed == success
                and rejected == 0
                and failed == 0
                and verify == 0
                and rtu_timeouts == 0
                and rtu_crc == 0
                and slave_crc == 0
                and slave_rx == completed
                and slave_tx == completed
                and slave_ok == completed
            )

            if target is None:
                tcp_clean = True
                tcp_target_pass = True
            else:
                tcp_clean = (
                    tcp["timeouts"] == 0
                    and tcp["transport_errors"] == 0
                    and tcp["protocol_errors"] == 0
                    and iv(ms, "FRAME_TIMEOUTS") == 0
                    and iv(ms, "BUS_LOCK_TIMEOUTS") == 0
                    and iv(ms, "PROTOCOL_ERRORS") == 0
                    and iv(ms, "REQUESTS_OK") == int(tcp["ok"])
                )
                tcp_target_pass = float(tcp["target_pct"]) >= 99.0

            peripheral_failures = iv(ms, "PERIPHERAL_FAILURE_COUNT")
            sd_failed = iv(ms, "SD_DATALOG_FAILED_COMMITS")
            sd_committed = iv(ms, "SD_DATALOG_COMMITTED_BYTES")

            runtime_clean = (
                tcp_clean
                and rtu_clean
                and peripheral_failures == 0
                and sd_failed == 0
            )

            row = {
                "label": label,
                "target": target,
                "tcp_req_s": float(tcp["achieved_req_s"]),
                "tcp_target_pct": float(tcp["target_pct"]),
                "tcp_target_pass": tcp_target_pass,
                "tcp_useful_mbps": float(tcp["useful_mbps"]),
                "tcp_avg_us": float(tcp["avg_us"]),
                "tcp_p95_us": float(tcp["p95_us"]),
                "tcp_p99_us": float(tcp["p99_us"]),
                "rtu_hz": rtu_hz,
                "sd_committed": sd_committed,
                "runtime_clean": runtime_clean,
            }
            rows.append(row)

            print(
                "P5RTUBUDGET_CASE "
                f"TCP_TARGET={label} "
                f"TCP_REQ_S={row['tcp_req_s']:.3f} "
                f"TCP_TARGET_PCT={row['tcp_target_pct']:.3f} "
                f"TCP_TARGET_PASS={'YES' if tcp_target_pass else 'NO'} "
                f"TCP_USEFUL_MBPS={row['tcp_useful_mbps']:.4f} "
                f"TCP_AVG_US={row['tcp_avg_us']:.1f} "
                f"TCP_P95_US={row['tcp_p95_us']:.1f} "
                f"TCP_P99_US={row['tcp_p99_us']:.1f} "
                f"RTU_HZ={rtu_hz:.3f} "
                f"SD_COMMITTED_B={sd_committed} "
                f"TCP_CLEAN={'YES' if tcp_clean else 'NO'} "
                f"RTU_CLEAN={'YES' if rtu_clean else 'NO'} "
                f"RUNTIME_CLEAN={'YES' if runtime_clean else 'NO'}"
            )

            if not runtime_clean:
                return 2

        print()
        print("=" * 78)
        print(" P5-RTU/TCP BUDGET FRONTIER SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "P5RTUBUDGET_SUMMARY "
                f"TCP_TARGET={row['label']} "
                f"TCP_REQ_S={row['tcp_req_s']:.3f} "
                f"TCP_TARGET_PCT={row['tcp_target_pct']:.3f} "
                f"TCP_TARGET_PASS={'YES' if row['tcp_target_pass'] else 'NO'} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"TCP_USEFUL_MBPS={row['tcp_useful_mbps']:.4f}"
            )

        valid = [
            row
            for row in rows
            if row["target"] is not None
            and row["tcp_target_pass"]
            and row["runtime_clean"]
        ]

        if valid:
            best = max(valid, key=lambda row: float(row["rtu_hz"]))
            print(
                "P5RTUBUDGET_MAX_RTU_WITH_TCP_TARGET_PASS="
                f"{best['rtu_hz']:.3f}"
            )
            print(
                "P5RTUBUDGET_TCP_TARGET_AT_MAX_RTU="
                f"{best['label']}"
            )

        off = next(row for row in rows if row["target"] is None)
        print(
            "P5RTUBUDGET_RTU_ABSOLUTE_CEILING_HZ="
            f"{off['rtu_hz']:.3f}"
        )
        print("A14_P5_RTU_TCP_BUDGET_FRONTIER=PASS_CHARACTERIZED")
        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
