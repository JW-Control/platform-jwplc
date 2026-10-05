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
    baud: int,
    command: bytes,
) -> None:
    budget.send_command_wait(
        master,
        b"X\n",
        p5b.MASTER_STOP_ACK,
    )
    time.sleep(0.10)

    ack = f"RTU_BAUD_EFFECTIVE={baud}"

    # Se cambia primero el Slave. No hay trafico RTU activo en esta ventana.
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

    master_baud = iv(ms, "RTU_BAUD")
    slave_baud = iv(ss, "RTU_BAUD")
    master_gap = iv(ms, "RTU_FRAME_GAP_US")
    slave_gap = iv(ss, "RTU_FRAME_GAP_US")
    master_motor = sv(ms, "RTU_MOTOR")
    slave_motor = sv(ss, "RTU_MOTOR")
    master_tx = sv(ms, "RTU_TX_MODE")
    slave_tx = sv(ss, "RTU_TX_MODE")
    master_buffer = iv(ms, "RS485_TX_BUFFER_BYTES")
    slave_buffer = iv(ss, "RS485_TX_BUFFER_BYTES")

    print(
        "RTUH1_BAUD_CONFIG "
        f"BAUD={baud} "
        f"MASTER_BAUD={master_baud} "
        f"SLAVE_BAUD={slave_baud} "
        f"MASTER_GAP_US={master_gap} "
        f"SLAVE_GAP_US={slave_gap} "
        f"MASTER_MOTOR={master_motor} "
        f"SLAVE_MOTOR={slave_motor} "
        f"MASTER_TX={master_tx} "
        f"SLAVE_TX={slave_tx} "
        f"MASTER_BUFFER={master_buffer} "
        f"SLAVE_BUFFER={slave_buffer}"
    )

    if master_baud != baud or slave_baud != baud:
        raise RuntimeError(
            "baud efectivo distinto al solicitado"
        )

    if master_gap != 500 or slave_gap != 500:
        raise RuntimeError(
            "gap efectivo distinto de 500 us"
        )

    if master_motor != "ASYNC" or slave_motor != "ASYNC":
        raise RuntimeError(
            "motor ASYNC no activo"
        )

    if master_tx != "QUEUED" or slave_tx != "QUEUED":
        raise RuntimeError(
            "TX queued no activo"
        )

    if (
        not yes(ms, "RS485_AUTO_DIRECTION")
        or not yes(ss, "RS485_AUTO_DIRECTION")
    ):
        raise RuntimeError(
            "AutoDirection no detectado"
        )

    if (
        not yes(ms, "RS485_QUEUED_TX_SUPPORTED")
        or not yes(ss, "RS485_QUEUED_TX_SUPPORTED")
    ):
        raise RuntimeError(
            "queued TX no soportado"
        )

    if (
        master_buffer < 257
        or slave_buffer < 257
    ):
        raise RuntimeError(
            "buffer TX insuficiente"
        )


def run_case(
    master,
    slave,
    host: str,
    port: int,
    duration_s: float,
    baud: int,
    tcp_target: int | None,
) -> dict[str, object]:
    tcp_label = "OFF" if tcp_target is None else str(tcp_target)

    print()
    print("-" * 78)
    print(
        f"RTUH1_CASE_BEGIN "
        f"BAUD={baud} "
        f"TCP_TARGET={tcp_label} "
        "GAP_US=500 MOTOR=ASYNC TX=QUEUED RTU=UNPACED"
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

    profile_pass = (
        iv(ms, "RTU_BAUD") == baud
        and iv(ss, "RTU_BAUD") == baud
        and iv(ms, "RTU_FRAME_GAP_US") == 500
        and iv(ss, "RTU_FRAME_GAP_US") == 500
        and sv(ms, "RTU_MOTOR") == "ASYNC"
        and sv(ss, "RTU_MOTOR") == "ASYNC"
        and sv(ms, "RTU_TX_MODE") == "QUEUED"
        and sv(ss, "RTU_TX_MODE") == "QUEUED"
        and yes(ms, "RTU_TX_QUEUED_ACTIVE")
        and yes(ss, "RTU_TX_QUEUED_ACTIVE")
        and yes(ms, "RS485_AUTO_DIRECTION")
        and yes(ss, "RS485_AUTO_DIRECTION")
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
    rtu_timeouts = iv(
        ms,
        "RTU_MASTER_TIMEOUTS",
    )
    rtu_crc = iv(
        ms,
        "RTU_CRC_ERRORS",
    )

    rtu_hz = (
        completed / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
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
        tcp_target_pass = (
            float(tcp["target_pct"]) >= 99.0
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
        and tcp_clean
        and tcp_target_pass
        and peripheral_failures == 0
        and sd_failed == 0
    )

    row = {
        "baud": baud,
        "tcp_label": tcp_label,
        "tcp_req_s": float(tcp["achieved_req_s"]),
        "tcp_target_pct": float(tcp["target_pct"]),
        "tcp_useful_mbps": float(tcp["useful_mbps"]),
        "tcp_avg_us": float(tcp["avg_us"]),
        "tcp_p95_us": float(tcp["p95_us"]),
        "tcp_p99_us": float(tcp["p99_us"]),
        "rtu_hz": rtu_hz,
        "failed": failed,
        "rtu_timeouts": rtu_timeouts,
        "rtu_crc": rtu_crc,
        "slave_crc": slave_crc,
        "profile_pass": profile_pass,
        "tcp_clean": tcp_clean,
        "rtu_clean": rtu_clean,
        "runtime_clean": runtime_clean,
    }

    print(
        "RTUH1_CASE "
        f"BAUD={baud} "
        f"TCP_TARGET={tcp_label} "
        f"TCP_REQ_S={row['tcp_req_s']:.3f} "
        f"TCP_TARGET_PCT={row['tcp_target_pct']:.3f} "
        f"TCP_USEFUL_MBPS={row['tcp_useful_mbps']:.4f} "
        f"TCP_AVG_US={row['tcp_avg_us']:.1f} "
        f"TCP_P95_US={row['tcp_p95_us']:.1f} "
        f"TCP_P99_US={row['tcp_p99_us']:.1f} "
        f"RTU_HZ={rtu_hz:.3f} "
        f"RTU_FAILED={failed} "
        f"RTU_TIMEOUTS={rtu_timeouts} "
        f"RTU_CRC={rtu_crc} "
        f"SLAVE_CRC={slave_crc} "
        f"PROFILE_PASS={'YES' if profile_pass else 'NO'} "
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
        raise ValueError(
            "duration-per-case debe ser >= 30 s"
        )

    master = p5b.open_serial_no_dtr(
        args.master_serial
    )
    slave = p5b.open_serial_no_dtr(
        args.slave_serial
    )

    rows: list[dict[str, object]] = []

    print("=" * 78)
    print(" A14 RTU-H1 - BAUDRATE SWEEP")
    print("=" * 78)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(
        f"DURATION_PER_CASE_S="
        f"{args.duration_per_case:.0f}"
    )
    print("BAUDS=115200,230400,500000")
    print("FRAME_GAP_US=500")
    print("MOTOR=ASYNC")
    print("TX_MODE=QUEUED")
    print("TCP_CASES_PER_BAUD=500,OFF")
    print("RTU_TIMEOUT_MS=25")

    try:
        time.sleep(1.0)

        for baud, command in BAUD_CASES:
            configure_baud(
                master,
                slave,
                baud,
                command,
            )

            rows.append(
                run_case(
                    master,
                    slave,
                    args.host,
                    args.port,
                    args.duration_per_case,
                    baud,
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
                    baud,
                    None,
                )
            )

        print()
        print("=" * 78)
        print(" RTU-H1 SUMMARY")
        print("=" * 78)

        for row in rows:
            print(
                "RTUH1_SUMMARY "
                f"BAUD={row['baud']} "
                f"TCP_TARGET={row['tcp_label']} "
                f"TCP_REQ_S={row['tcp_req_s']:.3f} "
                f"RTU_HZ={row['rtu_hz']:.3f} "
                f"RUNTIME_CLEAN="
                f"{'YES' if row['runtime_clean'] else 'NO'}"
            )

        control_rows = [
            row
            for row in rows
            if row["baud"] == 115200
        ]

        if not all(
            bool(row["runtime_clean"])
            for row in control_rows
        ):
            print(
                "A14_RTU_H1="
                "REVIEW_115200_CONTROL_FAILURE"
            )
            return 2

        baseline500 = next(
            row
            for row in rows
            if row["baud"] == 115200
            and row["tcp_label"] == "500"
        )
        baselineoff = next(
            row
            for row in rows
            if row["baud"] == 115200
            and row["tcp_label"] == "OFF"
        )

        for baud, _ in BAUD_CASES:
            row500 = next(
                row
                for row in rows
                if row["baud"] == baud
                and row["tcp_label"] == "500"
            )
            rowoff = next(
                row
                for row in rows
                if row["baud"] == baud
                and row["tcp_label"] == "OFF"
            )

            gain500 = (
                (float(row500["rtu_hz"]) /
                 float(baseline500["rtu_hz"]) - 1.0)
                * 100.0
            )
            gainoff = (
                (float(rowoff["rtu_hz"]) /
                 float(baselineoff["rtu_hz"]) - 1.0)
                * 100.0
            )

            print(
                "RTUH1_GAIN "
                f"BAUD={baud} "
                f"TCP500_RTU_GAIN_PCT={gain500:.3f} "
                f"OFF_RTU_GAIN_PCT={gainoff:.3f}"
            )

        clean_bauds: list[int] = []

        for baud, _ in BAUD_CASES:
            baud_rows = [
                row
                for row in rows
                if row["baud"] == baud
            ]

            if all(
                bool(row["runtime_clean"])
                for row in baud_rows
            ):
                clean_bauds.append(baud)

        print(
            "RTUH1_CLEAN_BAUDS="
            + ",".join(
                str(value)
                for value in clean_bauds
            )
        )

        if clean_bauds:
            highest = max(clean_bauds)
            best500 = next(
                row
                for row in rows
                if row["baud"] == highest
                and row["tcp_label"] == "500"
            )
            bestoff = next(
                row
                for row in rows
                if row["baud"] == highest
                and row["tcp_label"] == "OFF"
            )

            print(
                "RTUH1_HIGHEST_CLEAN_BAUD="
                f"{highest}"
            )
            print(
                "RTUH1_HIGHEST_CLEAN_TCP500_RTU_HZ="
                f"{best500['rtu_hz']:.3f}"
            )
            print(
                "RTUH1_HIGHEST_CLEAN_OFF_RTU_HZ="
                f"{bestoff['rtu_hz']:.3f}"
            )

        print("A14_RTU_H1=PASS_CHARACTERIZED")
        return 0

    finally:
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())
