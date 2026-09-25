from __future__ import annotations

import argparse
import math
import socket
import struct
import sys
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_perf_fc03_qualification_sweep as q
import a14_p5b_master_slave_qualification as p5b


def expected_body(start: int, quantity: int) -> bytes:
    body = bytearray()
    body.append(0x03)
    body.append(quantity * 2)

    for address in range(start, start + quantity):
        body.extend(
            struct.pack(
                ">H",
                0x1000 + address,
            )
        )

    return bytes(body)


def split_block(block_registers: int) -> list[tuple[int, int]]:
    chunks: list[tuple[int, int]] = []
    start = 0
    remaining = block_registers

    while remaining > 0:
        quantity = min(125, remaining)
        chunks.append((start, quantity))
        start += quantity
        remaining -= quantity

    return chunks


def run_block_case(
    host: str,
    port: int,
    duration_s: float,
    block_registers: int,
) -> dict[str, object]:
    chunks = split_block(block_registers)

    sock = socket.create_connection(
        (host, port),
        timeout=3.0,
    )
    sock.settimeout(1.0)

    scans = 0
    requests = 0
    regs = 0
    tx_bytes = 0
    rx_bytes = 0
    timeouts = 0
    transport_errors = 0
    protocol_errors = 0
    scan_latencies_us: list[float] = []

    tid = 0
    start_t = time.perf_counter()
    deadline = start_t + duration_s

    try:
        while time.perf_counter() < deadline:
            scan_t0 = time.perf_counter_ns()
            scan_ok = True

            for start_addr, quantity in chunks:
                tid = (tid + 1) & 0xFFFF

                request = struct.pack(
                    ">HHHBBHH",
                    tid,
                    0,
                    6,
                    1,
                    3,
                    start_addr,
                    quantity,
                )

                try:
                    sock.sendall(request)
                    requests += 1
                    tx_bytes += len(request)

                    header = q.recv_exact(sock, 7)
                    rx_tid, pid, length, unit = struct.unpack(
                        ">HHHB",
                        header,
                    )

                    if length < 2:
                        raise ValueError(
                            f"MBAP length invalido: {length}"
                        )

                    body = q.recv_exact(
                        sock,
                        length - 1,
                    )

                    rx_bytes += len(header) + len(body)

                    valid = (
                        rx_tid == tid
                        and pid == 0
                        and unit == 1
                        and length == (3 + 2 * quantity)
                        and body == expected_body(start_addr, quantity)
                    )

                    if not valid:
                        protocol_errors += 1
                        scan_ok = False
                        break

                    regs += quantity

                except socket.timeout:
                    timeouts += 1
                    scan_ok = False
                    break

                except (ConnectionError, OSError):
                    transport_errors += 1
                    scan_ok = False
                    break

                except ValueError:
                    protocol_errors += 1
                    scan_ok = False
                    break

            if not scan_ok:
                break

            scans += 1
            scan_latencies_us.append(
                (time.perf_counter_ns() - scan_t0) / 1000.0
            )

    finally:
        elapsed = time.perf_counter() - start_t

        try:
            sock.close()
        except Exception:
            pass

    scans_s = scans / elapsed if elapsed > 0.0 else 0.0
    req_s = requests / elapsed if elapsed > 0.0 else 0.0
    regs_s = regs / elapsed if elapsed > 0.0 else 0.0
    useful_mbps = (
        regs * 2 * 8 / elapsed / 1_000_000.0
        if elapsed > 0.0
        else 0.0
    )
    total_mbps = (
        (tx_bytes + rx_bytes) * 8 / elapsed / 1_000_000.0
        if elapsed > 0.0
        else 0.0
    )

    if scan_latencies_us:
        avg = sum(scan_latencies_us) / len(scan_latencies_us)
        p95 = q.percentile(scan_latencies_us, 0.95)
        p99 = q.percentile(scan_latencies_us, 0.99)
        max_us = max(scan_latencies_us)
    else:
        avg = p95 = p99 = max_us = 0.0

    return {
        "elapsed_s": elapsed,
        "block_registers": block_registers,
        "requests_per_scan": len(chunks),
        "scans": scans,
        "requests": requests,
        "registers": regs,
        "scans_s": scans_s,
        "req_s": req_s,
        "regs_s": regs_s,
        "useful_mbps": useful_mbps,
        "total_mbps": total_mbps,
        "scan_avg_us": avg,
        "scan_p95_us": p95,
        "scan_p99_us": p99,
        "scan_max_us": max_us,
        "timeouts": timeouts,
        "transport_errors": transport_errors,
        "protocol_errors": protocol_errors,
    }


def snapshot_int(snapshot: dict[str, str], key: str) -> int:
    try:
        return int(snapshot.get(key, "-1"))
    except ValueError:
        return -1


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=502)
    parser.add_argument("--duration-per-case", type=float, default=30.0)
    args = parser.parse_args()

    blocks = [125, 250, 500, 1000]

    if args.duration_per_case < 30.0:
        raise ValueError("duration-per-case debe ser >= 30 s")

    master_ser = p5b.open_serial_no_dtr(args.master_serial)
    slave_ser = p5b.open_serial_no_dtr(args.slave_serial)

    rows: list[dict[str, object]] = []

    print("=" * 76)
    print(" A14 P5-CAP2 - LOGICAL BLOCK SCAN")
    print("=" * 76)
    print(f"MASTER_SERIAL={args.master_serial}")
    print(f"SLAVE_SERIAL={args.slave_serial}")
    print(f"TARGET={args.host}:{args.port}")
    print(f"DURATION_PER_CASE_S={args.duration_per_case:.0f}")
    print("BLOCK_REGISTERS=125,250,500,1000")
    print("FC03_MAX_REGISTERS_PER_REQUEST=125")
    print("TCP_PACING=NONE")
    print("TCP_OUTSTANDING_REQUESTS=1")
    print("RTU_TARGET_HZ=50")

    try:
        time.sleep(1.0)

        for block in blocks:
            p5b_snapshot = p5b.request_slave_snapshot(
                slave_ser,
                5.0,
            )

            if not p5b.slave_initial_pass(p5b_snapshot):
                print(f"P5CAP2_BLOCK_{block}_SLAVE_PREFLIGHT=FAIL")
                return 2

            q.wait_server_disconnected(
                master_ser,
                20.0,
            )

            p5b.send_master_command(
                master_ser,
                b"X\n",
                p5b.MASTER_STOP_ACK,
                3.0,
            )

            time.sleep(0.10)

            q.reset_stats(master_ser)
            p5b.reset_slave_stats(slave_ser, 3.0)

            p5b.send_master_command(
                master_ser,
                b"G\n",
                p5b.MASTER_START_ACK,
                3.0,
            )

            chunks = split_block(block)

            print()
            print("-" * 76)
            print(
                f"P5CAP2_CASE_BEGIN BLOCK={block} "
                f"REQUESTS_PER_SCAN={len(chunks)} "
                f"CHUNKS="
                + ",".join(str(qty) for _, qty in chunks)
            )

            result = run_block_case(
                args.host,
                args.port,
                args.duration_per_case,
                block,
            )

            time.sleep(0.10)

            master_snapshot = p5b.request_master_snapshot(
                master_ser,
                5.0,
            )
            slave_snapshot = p5b.request_slave_snapshot(
                slave_ser,
                5.0,
            )

            tcp_clean = (
                result["timeouts"] == 0
                and result["transport_errors"] == 0
                and result["protocol_errors"] == 0
                and snapshot_int(master_snapshot, "FRAME_TIMEOUTS") == 0
                and snapshot_int(master_snapshot, "BUS_LOCK_TIMEOUTS") == 0
                and snapshot_int(master_snapshot, "PROTOCOL_ERRORS") == 0
            )

            rtu_started = snapshot_int(
                master_snapshot,
                "RTU_REQUESTS_STARTED",
            )
            rtu_completed = snapshot_int(
                master_snapshot,
                "RTU_REQUESTS_COMPLETED",
            )
            rtu_success = snapshot_int(
                master_snapshot,
                "RTU_REQUESTS_SUCCESS",
            )
            rtu_fail = (
                snapshot_int(master_snapshot, "RTU_REQUESTS_FAILED")
                + snapshot_int(master_snapshot, "RTU_REQUESTS_REJECTED")
                + snapshot_int(master_snapshot, "RTU_VERIFY_FAILS")
            )

            duration_ms = snapshot_int(
                master_snapshot,
                "RUN_DURATION_MS",
            )

            rtu_hz = (
                rtu_completed / (duration_ms / 1000.0)
                if duration_ms > 0
                else 0.0
            )

            rtu_pass = (
                rtu_started == rtu_completed
                and rtu_completed == rtu_success
                and rtu_fail == 0
                and snapshot_int(slave_snapshot, "CRC_ERRORS") == 0
            )

            sd_failed = snapshot_int(
                master_snapshot,
                "SD_DATALOG_FAILED_COMMITS",
            )
            peripheral_failures = snapshot_int(
                master_snapshot,
                "PERIPHERAL_FAILURE_COUNT",
            )
            sd_committed = snapshot_int(
                master_snapshot,
                "SD_DATALOG_COMMITTED_BYTES",
            )

            runtime_pass = (
                tcp_clean
                and rtu_pass
                and sd_failed == 0
                and peripheral_failures == 0
            )

            print(
                "P5CAP2_CASE "
                f"BLOCK={block} "
                f"REQ_PER_SCAN={result['requests_per_scan']} "
                f"SCANS_S={result['scans_s']:.3f} "
                f"REQ_S={result['req_s']:.3f} "
                f"REGS_S={result['regs_s']:.1f} "
                f"USEFUL_MBPS={result['useful_mbps']:.4f} "
                f"TOTAL_MBPS={result['total_mbps']:.4f} "
                f"SCAN_AVG_US={result['scan_avg_us']:.1f} "
                f"SCAN_P95_US={result['scan_p95_us']:.1f} "
                f"SCAN_P99_US={result['scan_p99_us']:.1f} "
                f"RTU_HZ={rtu_hz:.3f} "
                f"SD_COMMITTED_B={sd_committed} "
                f"TCP_CLEAN={'YES' if tcp_clean else 'NO'} "
                f"RUNTIME_PASS={'YES' if runtime_pass else 'NO'}"
            )

            result["rtu_hz"] = rtu_hz
            result["sd_committed"] = sd_committed
            result["runtime_pass"] = runtime_pass
            rows.append(result)

            if not runtime_pass:
                print(f"P5CAP2_BLOCK_{block}=FAIL")
                return 2

        print()
        print("=" * 76)
        print(" P5-CAP2 LOGICAL BLOCK SUMMARY")
        print("=" * 76)

        for row in rows:
            print(
                "P5CAP2_SUMMARY "
                f"BLOCK={row['block_registers']} "
                f"REQ_PER_SCAN={row['requests_per_scan']} "
                f"SCANS_S={row['scans_s']:.3f} "
                f"REQ_S={row['req_s']:.3f} "
                f"REGS_S={row['regs_s']:.1f} "
                f"USEFUL_MBPS={row['useful_mbps']:.4f} "
                f"SCAN_AVG_US={row['scan_avg_us']:.1f} "
                f"SCAN_P95_US={row['scan_p95_us']:.1f} "
                f"SCAN_P99_US={row['scan_p99_us']:.1f}"
            )

        regs_values = [float(r["regs_s"]) for r in rows]
        regs_min = min(regs_values)
        regs_max = max(regs_values)
        regs_spread_pct = (
            (regs_max - regs_min) / regs_min * 100.0
            if regs_min > 0.0
            else 0.0
        )

        print(f"P5CAP2_REGS_S_MIN={regs_min:.1f}")
        print(f"P5CAP2_REGS_S_MAX={regs_max:.1f}")
        print(f"P5CAP2_REGS_S_SPREAD_PCT={regs_spread_pct:.3f}")
        print("A14_P5CAP2_LOGICAL_BLOCK_SCAN=PASS")

        return 0

    finally:
        master_ser.close()
        slave_ser.close()


if __name__ == "__main__":
    raise SystemExit(main())
