import argparse
import socket
import struct
import sys
import time

import serial


def percentile(values, q):
    if not values:
        return 0.0

    data = sorted(values)

    if len(data) == 1:
        return float(data[0])

    pos = (len(data) - 1) * q
    lo = int(pos)
    hi = min(lo + 1, len(data) - 1)
    frac = pos - lo

    return data[lo] * (1.0 - frac) + data[hi] * frac


def recv_exact(sock, count):
    data = bytearray()

    while len(data) < count:
        chunk = sock.recv(count - len(data))

        if not chunk:
            raise ConnectionError(
                f"peer closed with {len(data)}/{count} bytes"
            )

        data.extend(chunk)

    return bytes(data)


def read_serial_line(ser):
    raw = ser.readline()

    if not raw:
        return None

    return raw.decode(
        "utf-8",
        errors="replace"
    ).strip()


def wait_for_ready(ser, timeout_s):
    deadline = time.monotonic() + timeout_s

    while time.monotonic() < deadline:
        line = read_serial_line(ser)

        if not line:
            continue

        print(f"SERIAL>{line}")

        if "A14_PERF_SERVER_CONFIG=FAIL" in line:
            raise RuntimeError(
                "JWPLC reportó fallo de configuración"
            )

        marker = "A14_PERF_SERVER_READY=PASS IP="

        if line.startswith(marker):
            tail = line[len(marker):]
            ip = tail.split(" PORT=", 1)[0].strip()

            return ip

    raise TimeoutError(
        "No apareció A14_PERF_SERVER_READY"
    )


def wait_for_reset_ack(ser, timeout_s):
    deadline = time.monotonic() + timeout_s

    while time.monotonic() < deadline:
        line = read_serial_line(ser)

        if not line:
            continue

        print(f"SERIAL>{line}")

        if line == "A14_PERF_RESET=PASS":
            return

    raise TimeoutError(
        "No apareció A14_PERF_RESET=PASS"
    )


def drain_runtime_serial(ser):
    lines = []

    while ser.in_waiting > 0:
        line = read_serial_line(ser)

        if line:
            lines.append(line)
            print(f"SERIAL>{line}")

    return lines


def collect_snapshot(ser, timeout_s):
    result = {}
    raw_lines = []

    deadline = time.monotonic() + timeout_s

    while time.monotonic() < deadline:
        line = read_serial_line(ser)

        if not line:
            continue

        raw_lines.append(line)
        print(f"SERIAL>{line}")

        if "=" in line:
            key, value = line.split("=", 1)
            result[key.strip()] = value.strip()

        if line == "A14_PERF_SNAPSHOT=END":
            return result, raw_lines

    raise TimeoutError(
        "No terminó el snapshot del JWPLC"
    )


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--serial",
        default="COM14"
    )

    parser.add_argument(
        "--host",
        default="192.168.0.31"
    )

    parser.add_argument(
        "--baud",
        type=int,
        default=115200
    )

    parser.add_argument(
        "--port",
        type=int,
        default=502
    )

    parser.add_argument(
        "--rate",
        type=float,
        default=10.0
    )

    parser.add_argument(
        "--duration",
        type=float,
        default=5.0
    )

    args = parser.parse_args()

    interval_s = 1.0 / args.rate
    request_count = int(
        round(args.rate * args.duration)
    )

    print()
    print("=" * 60)
    print(" A14.3 PERF-S1 FC03 PHYSICAL SMOKE")
    print("=" * 60)

    print(f"SERIAL_PORT={args.serial}")
    print(f"TARGET_RATE_REQ_S={args.rate:.3f}")
    print(f"DURATION_S={args.duration:.3f}")
    print(f"PLANNED_REQUESTS={request_count}")

    ser = serial.Serial(
        args.serial,
        args.baud,
        timeout=0.10,
        write_timeout=1.0
    )

    sock = None

    try:
        # Al abrir COM14 no se asume que el ESP32 vuelva a emitir el
        # banner READY. Se consulta activamente el estado mediante S.
        time.sleep(0.3)
        ser.reset_input_buffer()

        print()
        print("Consultando estado activo del JWPLC...")

        ser.write(b"S\n")
        ser.flush()

        initial_snapshot, _ = collect_snapshot(
            ser,
            5.0
        )

        if initial_snapshot.get("SERVER_READY") != "YES":
            raise RuntimeError(
                "JWPLC Server no está READY"
            )

        server_ip = args.host

        print(f"SERVER_IP={server_ip}")
        print("ACTIVE_SERIAL_HANDSHAKE=PASS")

        # Reset estadístico antes de crear la conexión TCP.
        ser.write(b"R\n")
        ser.flush()

        wait_for_reset_ack(
            ser,
            3.0
        )

        sock = socket.create_connection(
            (server_ip, args.port),
            timeout=3.0
        )

        sock.settimeout(1.0)

        print("TCP_CONNECT=PASS")

        latencies_us = []

        requests_sent = 0
        requests_ok = 0

        protocol_errors = 0
        transport_errors = 0
        timeouts = 0
        reconnects = 0
        unexpected_resets = 0

        tx_bytes = 0
        rx_bytes = 0

        start = time.perf_counter()

        for i in range(request_count):
            scheduled = start + (
                i * interval_s
            )

            now = time.perf_counter()

            if scheduled > now:
                time.sleep(
                    scheduled - now
                )

            tid = (i + 1) & 0xFFFF

            # MBAP:
            # TID, PID=0, Length=6,
            # Unit=1
            #
            # PDU:
            # FC03, address=0, quantity=1
            request = struct.pack(
                ">HHHBBHH",
                tid,
                0,
                6,
                1,
                3,
                0,
                1
            )

            t0_ns = time.perf_counter_ns()

            try:
                sock.sendall(request)

                requests_sent += 1
                tx_bytes += len(request)

                header = recv_exact(
                    sock,
                    7
                )

                (
                    rx_tid,
                    protocol_id,
                    length,
                    unit_id
                ) = struct.unpack(
                    ">HHHB",
                    header
                )

                if length < 2:
                    raise ValueError(
                        f"invalid MBAP length {length}"
                    )

                body = recv_exact(
                    sock,
                    length - 1
                )

                rx_bytes += (
                    len(header) +
                    len(body)
                )

                t1_ns = time.perf_counter_ns()

                latency_us = (
                    t1_ns - t0_ns
                ) / 1000.0

                latencies_us.append(
                    latency_us
                )

                expected_body = bytes(
                    [0x03, 0x02, 0x10, 0x00]
                )

                valid = (
                    rx_tid == tid and
                    protocol_id == 0 and
                    unit_id == 1 and
                    length == 5 and
                    body == expected_body
                )

                if valid:
                    requests_ok += 1
                else:
                    protocol_errors += 1

                    print(
                        "PROTOCOL_MISMATCH "
                        f"i={i} "
                        f"tid={tid} "
                        f"rx_tid={rx_tid} "
                        f"pid={protocol_id} "
                        f"len={length} "
                        f"unit={unit_id} "
                        f"body={body.hex(' ')}"
                    )

            except socket.timeout:
                timeouts += 1

            except (
                ConnectionError,
                OSError
            ) as exc:
                transport_errors += 1

                print(
                    "TRANSPORT_ERROR="
                    f"{type(exc).__name__}: {exc}"
                )

                break

            except ValueError as exc:
                protocol_errors += 1

                print(
                    "PROTOCOL_ERROR="
                    f"{exc}"
                )

            for line in drain_runtime_serial(ser):
                if (
                    line.startswith(
                        "A14_PERF_SERVER_CONFIG="
                    ) or
                    line.startswith(
                        "A14_PERF_SERVER_READY="
                    )
                ):
                    unexpected_resets += 1

        # Completar la ventana nominal de 5 s.
        target_end = start + args.duration

        now = time.perf_counter()

        if target_end > now:
            time.sleep(
                target_end - now
            )

        elapsed = time.perf_counter() - start

        achieved_req_s = (
            requests_ok / elapsed
            if elapsed > 0
            else 0.0
        )

        app_bytes_s = (
            (tx_bytes + rx_bytes) / elapsed
            if elapsed > 0
            else 0.0
        )

        if latencies_us:
            latency_min = min(
                latencies_us
            )

            latency_avg = (
                sum(latencies_us) /
                len(latencies_us)
            )

            latency_p50 = percentile(
                latencies_us,
                0.50
            )

            latency_p95 = percentile(
                latencies_us,
                0.95
            )

            latency_p99 = percentile(
                latencies_us,
                0.99
            )

            latency_max = max(
                latencies_us
            )

        else:
            latency_min = 0.0
            latency_avg = 0.0
            latency_p50 = 0.0
            latency_p95 = 0.0
            latency_p99 = 0.0
            latency_max = 0.0

        # Snapshot interno con la sesión TCP aún abierta.
        ser.write(b"S\n")
        ser.flush()

        snapshot, _ = collect_snapshot(
            ser,
            5.0
        )

        print()
        print("=" * 60)
        print(" RESULTADO PC")
        print("=" * 60)

        print("FUNCTION=FC03")
        print("QUANTITY_REGISTERS=1")

        print(
            f"REQUESTED_REQ_S="
            f"{args.rate:.3f}"
        )

        print(
            f"ACHIEVED_REQ_S="
            f"{achieved_req_s:.3f}"
        )

        print(
            f"REQUESTS_TOTAL="
            f"{requests_sent}"
        )

        print(
            f"REQUESTS_OK="
            f"{requests_ok}"
        )

        print(
            f"ERRORS="
            f"{protocol_errors + transport_errors + timeouts}"
        )

        print(
            f"TIMEOUTS="
            f"{timeouts}"
        )

        print(
            f"RECONNECTS="
            f"{reconnects}"
        )

        print(
            f"PROTOCOL_ERRORS="
            f"{protocol_errors}"
        )

        print(
            f"TRANSPORT_ERRORS="
            f"{transport_errors}"
        )

        print(
            f"UNEXPECTED_RESETS="
            f"{unexpected_resets}"
        )

        print(
            f"LATENCY_MIN_US="
            f"{latency_min:.1f}"
        )

        print(
            f"LATENCY_AVG_US="
            f"{latency_avg:.1f}"
        )

        print(
            f"LATENCY_P50_US="
            f"{latency_p50:.1f}"
        )

        print(
            f"LATENCY_P95_US="
            f"{latency_p95:.1f}"
        )

        print(
            f"LATENCY_P99_US="
            f"{latency_p99:.1f}"
        )

        print(
            f"LATENCY_MAX_US="
            f"{latency_max:.1f}"
        )

        print(
            f"APPLICATION_BYTES_S="
            f"{app_bytes_s:.1f}"
        )

        # --------------------------------------------------------
        # Validación cruzada del snapshot JWPLC
        # --------------------------------------------------------

        def intval(name, default=-1):
            try:
                return int(
                    snapshot.get(
                        name,
                        str(default)
                    )
                )
            except ValueError:
                return default

        server_connections = intval(
            "CLIENT_CONNECTIONS"
        )

        server_rx = intval(
            "RX_FRAMES"
        )

        server_tx = intval(
            "TX_FRAMES"
        )

        server_ok = intval(
            "REQUESTS_OK"
        )

        server_ex = intval(
            "EXCEPTIONS_SENT"
        )

        server_protocol = intval(
            "PROTOCOL_ERRORS"
        )

        server_timeouts = intval(
            "FRAME_TIMEOUTS"
        )

        server_bus = intval(
            "BUS_LOCK_TIMEOUTS"
        )

        loop_avg = intval(
            "LOOP_GAP_AVG_US"
        )

        loop_max = intval(
            "LOOP_GAP_MAX_US"
        )

        print()
        print("=" * 60)
        print(" VALIDACIÓN CRUZADA")
        print("=" * 60)

        print(
            f"SERVER_CLIENT_CONNECTIONS="
            f"{server_connections}"
        )

        print(
            f"SERVER_RX_FRAMES="
            f"{server_rx}"
        )

        print(
            f"SERVER_TX_FRAMES="
            f"{server_tx}"
        )

        print(
            f"SERVER_REQUESTS_OK="
            f"{server_ok}"
        )

        print(
            f"SERVER_EXCEPTIONS_SENT="
            f"{server_ex}"
        )

        print(
            f"SERVER_PROTOCOL_ERRORS="
            f"{server_protocol}"
        )

        print(
            f"SERVER_FRAME_TIMEOUTS="
            f"{server_timeouts}"
        )

        print(
            f"SERVER_BUS_LOCK_TIMEOUTS="
            f"{server_bus}"
        )

        print(
            f"SERVER_LOOP_GAP_AVG_US="
            f"{loop_avg}"
        )

        print(
            f"SERVER_LOOP_GAP_MAX_US="
            f"{loop_max}"
        )

        pc_pass = (
            requests_sent == request_count and
            requests_ok == request_count and
            protocol_errors == 0 and
            transport_errors == 0 and
            timeouts == 0 and
            reconnects == 0 and
            unexpected_resets == 0 and
            achieved_req_s >= (
                args.rate * 0.95
            )
        )

        server_pass = (
            server_connections == 1 and
            server_rx == request_count and
            server_tx == request_count and
            server_ok == request_count and
            server_ex == 0 and
            server_protocol == 0 and
            server_timeouts == 0 and
            server_bus == 0 and
            snapshot.get(
                "SERVER_READY"
            ) == "YES" and
            snapshot.get(
                "CLIENT_CONNECTED"
            ) == "YES"
        )

        print()
        print(
            "A14_3_PERF_S1_SMOKE_PC="
            + ("PASS" if pc_pass else "FAIL")
        )

        print(
            "A14_3_PERF_S1_SMOKE_SERVER="
            + ("PASS" if server_pass else "FAIL")
        )

        overall = (
            pc_pass and
            server_pass
        )

        print(
            "A14_3_PERF_S1_SMOKE="
            + ("PASS" if overall else "FAIL")
        )

        return 0 if overall else 2

    finally:
        if sock is not None:
            try:
                sock.close()
            except Exception:
                pass

        try:
            ser.close()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())