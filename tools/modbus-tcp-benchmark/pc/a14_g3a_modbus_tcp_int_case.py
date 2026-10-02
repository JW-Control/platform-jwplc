#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
from pathlib import Path

import serial

sys.path.insert(0, str(Path(__file__).resolve().parent))

import a14_perf_fc03_qualification_sweep as q
import a14_perf_fc03_125_formal_frontier as formal


QUANTITY = 125
RATE = 1000.0
REQUEST_BYTES = 12


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def valid_ipv4(value: str) -> bool:
    parts = value.split(".")
    if len(parts) != 4:
        return False

    try:
        octets = [int(part) for part in parts]
    except ValueError:
        return False

    return (
        value != "0.0.0.0"
        and all(0 <= item <= 255 for item in octets)
    )


def read_line(ser: serial.Serial) -> str | None:
    raw = ser.readline()
    if not raw:
        return None

    return raw.decode(
        "utf-8",
        errors="replace",
    ).strip()


def snapshot(
    ser: serial.Serial,
    timeout_s: float = 5.0,
) -> dict[str, str]:
    ser.reset_input_buffer()
    ser.write(b"S\n")
    ser.flush()

    deadline = time.perf_counter() + timeout_s
    values: dict[str, str] = {}

    while time.perf_counter() < deadline:
        line = read_line(ser)
        if not line:
            continue

        if "=" in line:
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()

        if line == "A14_G3A_SNAPSHOT=END":
            return values

    raise TimeoutError("G3A_SNAPSHOT_TIMEOUT")


def wait_ready(
    ser: serial.Serial,
    timeout_s: float = 30.0,
) -> dict[str, str]:
    deadline = time.perf_counter() + timeout_s
    last: dict[str, str] = {}

    while time.perf_counter() < deadline:
        try:
            last = snapshot(ser)
        except TimeoutError:
            time.sleep(0.2)
            continue

        ip = last.get("ETH_IP", "")

        if (
            last.get("SERVER_READY") == "YES"
            and last.get("CLIENT_CONNECTED") == "NO"
            and valid_ipv4(ip)
        ):
            return last

        time.sleep(0.2)

    raise TimeoutError(
        "G3A_SERVER_NOT_READY "
        f"READY={last.get('SERVER_READY')} "
        f"CLIENT={last.get('CLIENT_CONNECTED')} "
        f"IP={last.get('ETH_IP')}"
    )


def wait_connected(
    ser: serial.Serial,
    timeout_s: float = 5.0,
) -> dict[str, str]:
    deadline = time.perf_counter() + timeout_s
    last: dict[str, str] = {}

    while time.perf_counter() < deadline:
        last = snapshot(ser)

        if (
            last.get("SERVER_READY") == "YES"
            and last.get("CLIENT_CONNECTED") == "YES"
        ):
            return last

        time.sleep(0.05)

    raise TimeoutError(
        "G3A_CLIENT_ACCEPT_TIMEOUT "
        f"READY={last.get('SERVER_READY')} "
        f"CLIENT={last.get('CLIENT_CONNECTED')}"
    )


def reset_stats(ser: serial.Serial) -> None:
    ser.reset_input_buffer()
    ser.write(b"R\n")
    ser.flush()

    deadline = time.perf_counter() + 3.0

    while time.perf_counter() < deadline:
        line = read_line(ser)
        if line == "A14_G3A_RESET=PASS":
            return

    raise TimeoutError("G3A_RESET_ACK_TIMEOUT")


def intval(values: dict[str, str], key: str) -> int:
    try:
        return int(values[key])
    except (KeyError, ValueError) as exc:
        raise RuntimeError(
            f"G3A_INVALID_INT {key}={values.get(key)}"
        ) from exc


def warmup_transaction(sock: socket.socket) -> None:
    tid = 0xA14A
    request = struct.pack(
        ">HHHBBHH",
        tid,
        0,
        6,
        1,
        3,
        0,
        QUANTITY,
    )

    sock.sendall(request)

    header = q.recv_exact(sock, 7)
    rx_tid, pid, length, unit = struct.unpack(
        ">HHHB",
        header,
    )

    if length < 2:
        raise RuntimeError("G3A_WARMUP_INVALID_MBAP_LENGTH")

    body = q.recv_exact(sock, length - 1)

    if not (
        rx_tid == tid
        and pid == 0
        and unit == 1
        and length == 253
        and body == q.expected_body(QUANTITY)
    ):
        raise RuntimeError("G3A_WARMUP_RESPONSE_MISMATCH")


def run_case(
    *,
    serial_port: str,
    duration_s: float,
    expected_int: bool,
    expected_hot_poll_us: int = 0,
    rate: float = RATE,
) -> dict[str, object]:
    ser = serial.Serial()
    ser.port = serial_port
    ser.baudrate = 115200
    ser.timeout = 0.05
    ser.write_timeout = 1.0
    ser.dtr = False
    ser.rts = False

    sock: socket.socket | None = None

    try:
        ser.open()
        time.sleep(0.3)

        ready = wait_ready(ser)
        host = ready["ETH_IP"]

        expected_build = "YES" if expected_int else "NO"

        if ready.get("INT_GUIDED_RX_BUILD") != expected_build:
            raise RuntimeError(
                "G3A_VARIANT_MISMATCH "
                f"EXPECTED={expected_build} "
                f"ACTUAL={ready.get('INT_GUIDED_RX_BUILD')}"
            )

        if ready.get("TCP_PROFILE_ENABLED") != "YES":
            raise RuntimeError("G3A_PROFILE_NOT_ENABLED")

        sock = socket.create_connection(
            (host, 502),
            timeout=3.0,
        )
        sock.settimeout(1.0)
        sock.setsockopt(
            socket.IPPROTO_TCP,
            socket.TCP_NODELAY,
            1,
        )

        # EthernetServer.available() sólo entrega el cliente cuando ya existe
        # payload RX. Por eso una conexión TCP vacía no basta para que
        # JWPLC_ModbusTCP marque CLIENT_CONNECTED=YES. Se ejecuta una transacción
        # válida de warmup para forzar accept/configuración INT fuera de la
        # ventana medida; después se confirma el estado y se resetean contadores.
        warmup_transaction(sock)

        accepted = wait_connected(ser)

        if accepted.get("INT_GUIDED_RX_BUILD") != expected_build:
            raise RuntimeError("G3A_ACCEPTED_VARIANT_MISMATCH")

        reset_stats(ser)

        expected_body = q.expected_body(QUANTITY)
        target = int(round(rate * duration_s))

        latencies: list[float] = []
        sent = 0
        ok = 0
        timeouts = 0
        transport_errors = 0
        protocol_errors = 0

        start = time.perf_counter()
        deadline = start + duration_s

        for index in range(target):
            scheduled = start + index / rate

            if scheduled >= deadline:
                break

            formal.precise_wait_until(scheduled)

            if time.perf_counter() >= deadline:
                break

            tid = (index + 1) & 0xFFFF

            request = struct.pack(
                ">HHHBBHH",
                tid,
                0,
                6,
                1,
                3,
                0,
                QUANTITY,
            )

            t0 = time.perf_counter_ns()

            try:
                sock.sendall(request)
                sent += 1

                header = q.recv_exact(sock, 7)
                rx_tid, pid, length, unit = struct.unpack(
                    ">HHHB",
                    header,
                )

                if length < 2:
                    raise ValueError("invalid MBAP length")

                body = q.recv_exact(sock, length - 1)
                t1 = time.perf_counter_ns()

                valid = (
                    rx_tid == tid
                    and pid == 0
                    and unit == 1
                    and length == 253
                    and body == expected_body
                )

                if not valid:
                    protocol_errors += 1
                    break

                ok += 1
                latencies.append((t1 - t0) / 1000.0)

            except socket.timeout:
                timeouts += 1
                break
            except (ConnectionError, OSError):
                transport_errors += 1
                break
            except ValueError:
                protocol_errors += 1
                break

        now = time.perf_counter()
        if now < deadline:
            time.sleep(deadline - now)

        elapsed = time.perf_counter() - start
        measured = snapshot(ser)

        server_connections = intval(
            measured,
            "CLIENT_CONNECTIONS",
        )
        server_rx = intval(measured, "RX_FRAMES")
        server_tx = intval(measured, "TX_FRAMES")
        server_ok = intval(measured, "REQUESTS_OK")
        server_ex = intval(measured, "EXCEPTIONS_SENT")
        server_protocol = intval(
            measured,
            "PROTOCOL_ERRORS",
        )
        server_timeouts = intval(
            measured,
            "FRAME_TIMEOUTS",
        )
        server_bus = intval(
            measured,
            "BUS_LOCK_TIMEOUTS",
        )

        status_calls = intval(
            measured,
            "TCP_PROF_SOCKET_STATUS_CALLS",
        )
        available_calls = intval(
            measured,
            "TCP_PROF_AVAILABLE_CALLS",
        )
        available_zero = intval(
            measured,
            "TCP_PROF_AVAILABLE_ZERO_CALLS",
        )
        available_nonzero = intval(
            measured,
            "TCP_PROF_AVAILABLE_NONZERO_CALLS",
        )
        payload_bytes = intval(
            measured,
            "TCP_PROF_PAYLOAD_BYTES",
        )

        if (
            available_zero + available_nonzero
            != available_calls
        ):
            raise RuntimeError(
                "G3A_AVAILABLE_ACCOUNTING_MISMATCH"
            )

        req_s = ok / elapsed if elapsed > 0 else 0.0
        avg = (
            sum(latencies) / len(latencies)
            if latencies
            else 0.0
        )
        p95 = q.percentile(latencies, 0.95)
        p99 = q.percentile(latencies, 0.99)
        max_us = max(latencies) if latencies else 0.0

        # clientConnections se reinicia después de aceptar la sesión, por lo
        # que debe permanecer en cero durante la ventana medida: una subida
        # indicaría reconnect inesperado.
        cross = (
            server_connections == 0
            and server_rx == sent
            and server_tx == ok
            and server_ok == ok
        )

        clean = (
            sent == target
            and ok == target
            and timeouts == 0
            and transport_errors == 0
            and protocol_errors == 0
            and server_ex == 0
            and server_protocol == 0
            and server_timeouts == 0
            and server_bus == 0
            and cross
            and measured.get("SERVER_READY") == "YES"
            and payload_bytes == sent * REQUEST_BYTES
        )

        return {
            "target": target,
            "sent": sent,
            "ok": ok,
            "req_s": req_s,
            "avg_us": avg,
            "p95_us": p95,
            "p99_us": p99,
            "max_us": max_us,
            "loop_avg_us":
                intval(measured, "LOOP_GAP_AVG_US"),
            "loop_max_us":
                intval(measured, "LOOP_GAP_MAX_US"),
            "status_calls": status_calls,
            "status_total_us":
                intval(
                    measured,
                    "TCP_PROF_SOCKET_STATUS_TOTAL_US",
                ),
            "available_calls": available_calls,
            "available_zero": available_zero,
            "available_nonzero": available_nonzero,
            "payload_bytes": payload_bytes,
            "functional": clean,
            "host": host,
        }

    finally:
        if sock is not None:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            sock.close()

        if ser.is_open:
            ser.close()


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Alpha14 G3A real Modbus TCP "
            "INT product candidate case"
        )
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument(
        "--duration",
        type=float,
        default=30.0,
    )
    parser.add_argument(
        "--variant",
        choices=("POLLING", "INT_GUIDED"),
        required=True,
    )
    parser.add_argument(
        "--rate",
        type=float,
        default=RATE,
    )
    parser.add_argument(
        "--expected-hot-poll-us",
        type=int,
        default=0,
    )
    args = parser.parse_args()

    if args.duration <= 0:
        raise RuntimeError("G3A_DURATION_INVALID")
    if args.rate <= 0:
        raise RuntimeError("G3A_RATE_INVALID")
    if args.expected_hot_poll_us < 0:
        raise RuntimeError("G3A_HOT_POLL_INVALID")

    expected_int = args.variant == "INT_GUIDED"

    emit("G3A_VARIANT", args.variant)
    emit(
        "G3A_DURATION_TARGET_S",
        f"{args.duration:.3f}",
    )
    emit("G3A_TARGET_REQ_S", f"{RATE:.3f}")
    emit("G3A_QUANTITY", QUANTITY)

    row = run_case(
        serial_port=args.serial,
        duration_s=args.duration,
        expected_int=expected_int,
    )

    emit("G3A_DUT_IP", row["host"])
    emit("G3A_TARGET_REQUESTS", row["target"])
    emit("G3A_REQUESTS_SENT", row["sent"])
    emit("G3A_REQUESTS_OK", row["ok"])
    emit(
        "G3A_ACHIEVED_REQ_S",
        f"{row['req_s']:.3f}",
    )
    emit(
        "G3A_LAT_AVG_US",
        f"{row['avg_us']:.3f}",
    )
    emit("G3A_P95_US", f"{row['p95_us']:.3f}")
    emit("G3A_P99_US", f"{row['p99_us']:.3f}")
    emit("G3A_MAX_US", f"{row['max_us']:.3f}")
    emit("G3A_LOOP_AVG_US", row["loop_avg_us"])
    emit("G3A_LOOP_MAX_US", row["loop_max_us"])
    emit("G3A_STATUS_CALLS", row["status_calls"])
    emit(
        "G3A_STATUS_TOTAL_US",
        row["status_total_us"],
    )
    emit(
        "G3A_AVAILABLE_CALLS",
        row["available_calls"],
    )
    emit(
        "G3A_AVAILABLE_ZERO",
        row["available_zero"],
    )
    emit(
        "G3A_AVAILABLE_NONZERO",
        row["available_nonzero"],
    )
    emit("G3A_PAYLOAD_BYTES", row["payload_bytes"])
    emit(
        "G3A_FUNCTIONAL_PASS",
        "YES" if row["functional"] else "NO",
    )

    return 0 if row["functional"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
