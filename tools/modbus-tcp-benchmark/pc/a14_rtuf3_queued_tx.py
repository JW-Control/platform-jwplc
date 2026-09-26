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
    ("BLOCKING", b"Y\n"),
    ("QUEUED", b"Z\n"),
]


def iv(values: dict[str, str], key: str) -> int:
    return p5b.int_value(values, key, -1)


def sv(values: dict[str, str], key: str) -> str:
    return values.get(key, "").strip()


def yes(values: dict[str, str], key: str) -> bool:
    return sv(values, key).upper() == "YES"


def configure_common(master, slave) -> None:
    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)

    budget.send_command_wait(master, b"O\n", "RTU_FRAME_GAP_US=500")
    budget.send_command_wait(slave, b"O\n", "RTU_FRAME_GAP_US=500")

    budget.send_command_wait(master, b"U\n", "RTU_RATE_MODE=UNPACED")
    time.sleep(0.05)


def configure_mode(
    master,
    slave,
    mode: str,
    command: bytes,
) -> None:
    ack = f"RTU_TX_MODE={mode}"

    budget.send_command_wait(master, command, ack)
    budget.send_command_wait(slave, command, ack)
    time.sleep(0.05)

    ms = q.request_snapshot(master, echo=False)
    ss = p5b.request_slave_snapshot(slave, 5.0)

    expected_active = mode == "QUEUED"

    master_mode = sv(ms, "RTU_TX_MODE")
    slave_mode = sv(ss, "RTU_TX_MODE")
    master_gap = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap = iv(ss, "RTU_FRAME_GAP_US")
    master_buffer = iv(ms, "RS485_TX_BUFFER_BYTES")
    slave_buffer = iv(ss, "RS485_TX_BUFFER_BYTES")
    master_auto = yes(ms, "RS485_AUTO_DIRECTION")
    slave_auto = yes(ss, "RS485_AUTO_DIRECTION")
    master_supported = yes(ms, "RS485_QUEUED_TX_SUPPORTED")
    slave_supported = yes(ss, "RS485_QUEUED_TX_SUPPORTED")
    master_active = yes(ms, "RTU_TX_QUEUED_ACTIVE")
    slave_active = yes(ss, "RTU_TX_QUEUED_ACTIVE")

    print(
        "RTUF3_MODE_CONFIG "
        f"MODE={mode} "
        f"MASTER_MODE={master_mode} "
        f"SLAVE_MODE={slave_mode} "
        f"MASTER_GAP_US={master_gap} "
        f"SLAVE_GAP_US={slave_gap} "
        f"MASTER_TX_BUFFER={master_buffer} "
        f"SLAVE_TX_BUFFER={slave_buffer} "
        f"MASTER_AUTO_DIRECTION={'YES' if master_auto else 'NO'} "
        f"SLAVE_AUTO_DIRECTION={'YES' if slave_auto else 'NO'} "
        f"MASTER_QUEUED_SUPPORTED={'YES' if master_supported else 'NO'} "
        f"SLAVE_QUEUED_SUPPORTED={'YES' if slave_supported else 'NO'} "
        f"MASTER_QUEUED_ACTIVE={'YES' if master_active else 'NO'} "
        f"SLAVE_QUEUED_ACTIVE={'YES' if slave_active else 'NO'}"
    )

    if master_mode != mode or slave_mode != mode:
        raise RuntimeError("modo TX efectivo no coincide")

    if master_gap != 500 or slave_gap != 500:
        raise RuntimeError("gap efectivo distinto de 500 us")

    if master_buffer < 257 or slave_buffer < 257:
        raise RuntimeError("TX buffer insuficiente")

    if not master_auto or not slave_auto:
        raise RuntimeError("AutoDirection no detectado")

    if not master_supported or not slave_supported:
        raise RuntimeError("queued TX no soportado")

    if master_active != expected_active or slave_active != expected_active:
        raise RuntimeError("estado queued activo inesperado")


def run_case(
    master,
    slave,
    host: str,
    port: int,
    duration_s: float,
    mode: str,
    tcp_target: int | None,
) -> dict[str, object]:
    tcp_label = "OFF" if tcp_target is None else str(tcp_target)

    print()
    print("-" * 78)
    print(
        f"RTUF3_CASE_BEGIN MODE={mode} "
        f"TCP_TARGET={tcp_label} "
        "GAP_US=500 RTU=UNPACED"
    )
    print("-" * 78)

    budget.send_command_wait(master, b"X\n", p5b.MASTER_STOP_ACK)
    time.sleep(0.10)
    q.wait_server_disconnected(master, timeout_s=20.0)

    q.reset_stats(master)
    p5b.reset_slave_stats(slave, 3.0)

    budget.send_command_wait(master, b"G\n", p5b.MASTER_START_ACK)

    if tcp_target is None:
        time.sleep(duration_s)
        tcp = {
            "ok": 0,
            "timeouts": 0,
            "transport_errors": 0,
            "protocol_errors": 0,
            "achieved_req_s": 0.0,
            "target_pct": 100.0,
            "useful_mbps": 0.0,
            "avg_us": 0.0,
            "p95_us": 0.0,
            "p99_us": 0.0,
        }
    else:
        tcp = budget.run_paced_fc03(
            host,
            port,
            duration_s,
            float(tcp_target),
            125,
        )

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
    rtu_timeouts = iv(ms, "RTU_MASTER_TIMEOUTS")
    rtu_crc = iv(ms, "RTU_CRC_ERRORS")

    rtu_hz = (
        completed / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )

    slave_rx = iv(ss, "RTU_RX_FRAMES")
    slave_tx = iv(ss, "RTU_TX_FRAMES")
    slave_ok = iv(ss, "RTU_REQUESTS_OK")
    slave_crc = iv(ss, "RTU_CRC_ERRORS")

    expected_active = mode == "QUEUED"

    mode_pass = (
        sv(ms, "RTU_TX_MODE") == mode
        and sv(ss, "RTU_TX_MODE") == mode
        and yes(ms, "RTU_TX_QUEUED_ACTIVE") == expected_active
        and yes(ss, "RTU_TX_QUEUED_ACTIVE") == expected_active
        and yes(ms, "RS485_QUEUED_TX_SUPPORTED")
        and yes(ss, "RS485_QUEUED_TX_SUPPORTED")
        and yes(ms, "RS485_AUTO_DIRECTION")
        and yes(ss, "RS485_AUTO_DIRECTION")
        and iv(ms, "RS485_TX_BUFFER_BYTES") >= 257
        and iv(ss, "RS485_TX_BUFFER_BYTES") >= 257
        and iv(ms, "RTU_FRAME_GAP_US") == 500
        and iv(ss, "RTU_FRAME_GAP_US") == 500
    )

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

    if tcp_target is None:
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

    runtime_clean = (
        mode_pass
        and rtu_clean
        and tcp_clean
        and tcp_target_pass
        and peripheral_failures == 0
        and sd_failed == 0
    )

    row = {
        "mode": mode,
        "tcp_label": tcp_label,
        "tcp_req_s": float(tcp["achieved_req_s"]),
        "tcp_target_pct": float(tcp["target_pct"]),
        "tcp_avg_us": float(tcp["avg_us"]),
        "tcp_p95_us": float(tcp["p95_us"]),
        "tcp_p99_us": float(tcp["p99_us"]),
        "rtu_hz": rtu_hz,
        "runtime_clean": runtime_clean,
        "rtu_clean": rtu_clean,
        "tcp_clean": tcp_clean,
        "mode_pass": mode_pass,
    }

    print(
        "RTUF3_CASE "
        f"MODE={mode} "
        f"TCP_TARGET={tcp_label} "
        f"TCP_REQ_S={row['tcp_req_s']:.3f} "
        f"TCP_TARGET_PCT={row['tcp_target_pct']:.3f} "
        f"TCP_AVG_US={row['tcp_avg_us']:.1f} "
        f"TCP_P95_US={row['tcp_p95_us']:.1f} "
        f"TCP_P99_US={row['tcp_p99_us']:.1f} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"RTU_FAILED={failed} "
        f"RTU_TIMEOUTS={rtu_timeouts} "
        f"RTU_CRC={rtu_crc} "
        f"SLAVE_CRC={slave_crc} "
        f"MODE_PASS={'YES' if mode_pass else 'NO'} "
        f"TCP_CLEAN={'YES' if tcp_clean else 'NO'} "
        f"RTU_CLEAN={'YES' if rtu_clean else 'NO'} "
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
    print(" A14 RTU-F3 - BLOCKING VS QUEUED TX")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"DURATION_PER_CASE_S={args.duration_per_case:.0f}")
    print("FRAME_GAP_US=500")
    print("RTU_BAUD=115200")
    print("RTU_CONFIG=8N1")
    print("RTU_MODE=UNPACED")
    print("TX_MODES=BLOCKING,QUEUED")
    print("TCP_CASES_PER_MODE=500,OFF")

    try:
        time.sleep(1.0)
        configure_common(master, slave)

        for mode, command in MODES:
            configure_mode(
                master,
                slave,
                mode,
                command,
            )

            rows.append(
                run_case(
                    master,
                    slave,
                    args.host,
                    args.port,
                    args.duration_per_case,
                    mode,
                    500,
                )
            )

            rows.append(
                run_case(
                    master,
                    slave,
                    args.host,
                    args.port,
                    args.duration_per_case,
                    mode,
                    None,
                )
            )

        print()
        print("=" * 78)
        print(" RTU-F3 SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUF3_SUMMARY "
                f"MODE={row['mode']} "
                f"TCP_TARGET={row['tcp_label']} "
                f"TCP_REQ_S={row['tcp_req_s']:.3f} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"RUNTIME_CLEAN="
                f"{'YES' if row['runtime_clean'] else 'NO'}"
            )

        if not all(bool(row["runtime_clean"]) for row in rows):
            print("A14_RTU_F3=REVIEW_RUNTIME_FAILURE")
            return 2

        block500 = next(
            row for row in rows
            if row["mode"] == "BLOCKING"
            and row["tcp_label"] == "500"
        )
        queue500 = next(
            row for row in rows
            if row["mode"] == "QUEUED"
            and row["tcp_label"] == "500"
        )
        blockoff = next(
            row for row in rows
            if row["mode"] == "BLOCKING"
            and row["tcp_label"] == "OFF"
        )
        queueoff = next(
            row for row in rows
            if row["mode"] == "QUEUED"
            and row["tcp_label"] == "OFF"
        )

        gain500 = (
            (float(queue500["rtu_hz"]) /
             float(block500["rtu_hz"]) - 1.0)
            * 100.0
        )
        gainoff = (
            (float(queueoff["rtu_hz"]) /
             float(blockoff["rtu_hz"]) - 1.0)
            * 100.0
        )

        avg_delta = (
            float(queue500["tcp_avg_us"])
            - float(block500["tcp_avg_us"])
        )
        p95_delta = (
            float(queue500["tcp_p95_us"])
            - float(block500["tcp_p95_us"])
        )
        p99_delta = (
            float(queue500["tcp_p99_us"])
            - float(block500["tcp_p99_us"])
        )

        print(
            "RTUF3_QUEUED_VS_BLOCKING_TCP500_RTU_GAIN_PCT="
            f"{gain500:.3f}"
        )
        print(
            "RTUF3_QUEUED_VS_BLOCKING_OFF_RTU_GAIN_PCT="
            f"{gainoff:.3f}"
        )
        print(
            "RTUF3_TCP500_AVG_DELTA_US="
            f"{avg_delta:.1f}"
        )
        print(
            "RTUF3_TCP500_P95_DELTA_US="
            f"{p95_delta:.1f}"
        )
        print(
            "RTUF3_TCP500_P99_DELTA_US="
            f"{p99_delta:.1f}"
        )

        print("A14_RTU_F3=PASS_CHARACTERIZED")
        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
