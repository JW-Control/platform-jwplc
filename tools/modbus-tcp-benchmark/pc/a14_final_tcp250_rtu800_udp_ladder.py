from __future__ import annotations

import argparse
import csv
import json
import multiprocessing as mp
import queue as pyqueue
import socket
import struct
import sys
import threading
import time
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import a14_final_tcp_rtu_expmix_matrix as exp


UDP_PAYLOAD_BYTES = 1016
UDP_PORT = 5002
TCP_TARGET_REQ_S = 250.0
RTU_TARGET_REQ_S = 800
DEFAULT_UDP_LADDER = "0,1,2,4,6,8,10,12"
UDP_HOST_PACING_MODE = "DEDICATED_PROCESS_ACTUAL_SEND_INTERVAL"
UDP_HOST_SPIN_THRESHOLD_NS = 2_500_000
UDP_HOST_MIN_GAP_RATIO = 0.90


def parse_ladder(spec: str) -> list[float]:
    values: list[float] = []
    for token in spec.split(","):
        token = token.strip()
        if not token:
            continue
        value = float(token)
        if value < 0.0:
            raise ValueError("UDP target no puede ser negativo")
        values.append(value)
    if not values or values[0] != 0.0:
        raise ValueError("UDP ladder debe comenzar en 0 Mbps")
    if any(b <= a for a, b in zip(values, values[1:])):
        raise ValueError("UDP ladder debe ser estrictamente creciente")
    return values


def write_csv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        return
    with path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def set_rtu_scan_100(master) -> None:
    exp.send(master, b"&\n", "RTU_SCAN_RATE_HZ=100", 4.0)


def set_udp_service(master, enabled: bool) -> None:
    if enabled:
        exp.send(master, b"~\n", "UDP_FAST_SERVICE=ON", 4.0)
    else:
        exp.send(master, b"`\n", "UDP_FAST_SERVICE=OFF", 4.0)


def profile_pass(
    master_values: dict[str, str],
    slave_values: dict[str, str],
    udp_enabled: bool,
) -> bool:
    return (
        exp.fast_profile_pass(master_values, slave_values)
        and exp.sv(master_values, "RTU_RATE_MODE") == "SCAN_PACED"
        and exp.iv(master_values, "RTU_TARGET_HZ", -1) == RTU_TARGET_REQ_S
        and exp.iv(master_values, "RTU_SCAN_TARGET_HZ", -1) == 100
        and exp.iv(master_values, "RTU_SCAN_PERIOD_US", -1) == 10000
        and exp.sv(master_values, "UDP_FAST_READY") == "YES"
        and exp.sv(master_values, "UDP_FAST_SERVICE_ENABLED")
        == ("YES" if udp_enabled else "NO")
        and exp.iv(master_values, "UDP_FAST_PORT", -1) == UDP_PORT
        and exp.iv(master_values, "UDP_FAST_PAYLOAD_BYTES", -1)
        == UDP_PAYLOAD_BYTES
    )


def wait_udp_ready(master, slave) -> tuple[str, dict[str, str], dict[str, str]]:
    deadline = time.perf_counter() + 45.0
    last_master: dict[str, str] = {}
    last_slave: dict[str, str] = {}

    while time.perf_counter() < deadline:
        last_master = exp.q.request_snapshot(master, echo=False)
        last_slave = exp.p5b.request_slave_snapshot(slave, 5.0)
        host = exp.sv(last_master, "ETH_IP")

        if (
            exp.sv(last_master, "FULL_RUNTIME_READY") == "YES"
            and exp.sv(last_master, "SERVER_READY") == "YES"
            and exp.sv(last_master, "ETH_READY") == "YES"
            and exp.sv(last_master, "ETH_LINK") == "UP"
            and exp.sv(last_master, "UDP_FAST_READY") == "YES"
            and exp.sv(last_slave, "SLAVE_READY") == "YES"
            and host
            and host != "0.0.0.0"
        ):
            return host, last_master, last_slave

        time.sleep(0.25)

    raise RuntimeError(
        "TRIPLE_READY_TIMEOUT\n"
        + last_master.get("_RAW", "")
        + "\n--- SLAVE ---\n"
        + last_slave.get("_RAW", "")
    )


def udp_sender(
    host: str,
    target_mbps: float,
    start_event,
    stop_event,
    out: dict[str, object],
) -> None:
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    payload = bytearray((i * 17) & 0xFF for i in range(UDP_PAYLOAD_BYTES))

    sent_packets = 0
    sent_bytes = 0
    send_errors = 0
    sequence = 0
    pacing_deadlines_skipped = 0
    max_late_us = 0.0
    min_start_gap_us = 0.0
    start_too_close_packets = 0
    min_completion_gap_us = 0.0
    completion_too_close_packets = 0
    send_call_max_us = 0.0
    last_send_begin_ns: int | None = None
    last_send_end_ns: int | None = None

    try:
        start_event.wait()
        started_ns = time.perf_counter_ns()

        if target_mbps <= 0.0:
            stop_event.wait()
            elapsed = (
                time.perf_counter_ns() - started_ns
            ) / 1_000_000_000.0
        else:
            interval_ns = int(
                UDP_PAYLOAD_BYTES * 8.0 * 1_000_000_000.0
                / (target_mbps * 1_000_000.0)
            )
            min_allowed_gap_ns = int(
                interval_ns * UDP_HOST_MIN_GAP_RATIO
            )
            next_send_ns = started_ns + interval_ns

            while not stop_event.is_set():
                while True:
                    now_ns = time.perf_counter_ns()
                    remaining_ns = next_send_ns - now_ns

                    if remaining_ns <= 0:
                        break

                    if remaining_ns > UDP_HOST_SPIN_THRESHOLD_NS:
                        sleep_ns = (
                            remaining_ns
                            - UDP_HOST_SPIN_THRESHOLD_NS
                        )
                        time.sleep(sleep_ns / 1_000_000_000.0)
                    else:
                        # Dedicated process: high-resolution final wait.
                        pass

                send_begin_ns = time.perf_counter_ns()

                if last_send_begin_ns is not None:
                    start_gap_ns = (
                        send_begin_ns - last_send_begin_ns
                    )
                    start_gap_us = start_gap_ns / 1000.0
                    if (
                        min_start_gap_us == 0.0
                        or start_gap_us < min_start_gap_us
                    ):
                        min_start_gap_us = start_gap_us
                    if start_gap_ns < min_allowed_gap_ns:
                        start_too_close_packets += 1

                lateness_us = max(
                    0.0,
                    (send_begin_ns - next_send_ns) / 1000.0,
                )
                if lateness_us > max_late_us:
                    max_late_us = lateness_us

                if stop_event.is_set():
                    break

                sequence += 1
                struct.pack_into(">I", payload, 0, sequence)

                try:
                    n = sock.sendto(payload, (host, UDP_PORT))
                    send_ns = time.perf_counter_ns()

                    send_call_us = (
                        send_ns - send_begin_ns
                    ) / 1000.0
                    if send_call_us > send_call_max_us:
                        send_call_max_us = send_call_us

                    if n == UDP_PAYLOAD_BYTES:
                        sent_packets += 1
                        sent_bytes += n

                        if last_send_end_ns is not None:
                            completion_gap_ns = (
                                send_ns - last_send_end_ns
                            )
                            completion_gap_us = (
                                completion_gap_ns / 1000.0
                            )
                            if (
                                min_completion_gap_us == 0.0
                                or completion_gap_us
                                < min_completion_gap_us
                            ):
                                min_completion_gap_us = (
                                    completion_gap_us
                                )
                            if (
                                completion_gap_ns
                                < min_allowed_gap_ns
                            ):
                                completion_too_close_packets += 1

                        last_send_begin_ns = send_begin_ns
                        last_send_end_ns = send_ns
                    else:
                        send_errors += 1
                except OSError:
                    send_errors += 1
                    send_ns = time.perf_counter_ns()

                # Preserve the requested start-to-start spacing without
                # accumulating sendto() cost. If the send itself overruns the
                # next slot, drop that slot instead of catching up in a burst.
                candidate_next_ns = send_begin_ns + interval_ns
                now_after_send_ns = time.perf_counter_ns()
                if candidate_next_ns <= now_after_send_ns:
                    pacing_deadlines_skipped += 1
                    next_send_ns = now_after_send_ns + interval_ns
                else:
                    next_send_ns = candidate_next_ns

            elapsed = (
                time.perf_counter_ns() - started_ns
            ) / 1_000_000_000.0

    finally:
        sock.close()

    offered_mbps = (
        sent_bytes * 8.0 / elapsed / 1_000_000.0
        if elapsed > 0.0
        else 0.0
    )

    target_interval_us = (
        UDP_PAYLOAD_BYTES * 8.0 / target_mbps
        if target_mbps > 0.0
        else 0.0
    )
    min_start_gap_target_pct = (
        min_start_gap_us / target_interval_us * 100.0
        if target_interval_us > 0.0
        and min_start_gap_us > 0.0
        else 100.0
    )
    min_completion_gap_target_pct = (
        min_completion_gap_us / target_interval_us * 100.0
        if target_interval_us > 0.0
        and min_completion_gap_us > 0.0
        else 100.0
    )

    out.update(
        {
            "elapsed_s": elapsed,
            "sent_packets": sent_packets,
            "sent_bytes": sent_bytes,
            "send_errors": send_errors,
            "offered_mbps": offered_mbps,
            "pacing_mode": UDP_HOST_PACING_MODE,
            "pacing_deadlines_skipped": pacing_deadlines_skipped,
            "max_late_us": max_late_us,
            "target_interval_us": target_interval_us,
            "min_start_gap_us": min_start_gap_us,
            "min_start_gap_target_pct": min_start_gap_target_pct,
            "start_too_close_packets": start_too_close_packets,
            "min_completion_gap_us": min_completion_gap_us,
            "min_completion_gap_target_pct":
                min_completion_gap_target_pct,
            "completion_too_close_packets":
                completion_too_close_packets,
            "send_call_max_us": send_call_max_us,
        }
    )


def udp_sender_process_entry(
    host: str,
    target_mbps: float,
    start_event,
    stop_event,
    result_queue,
) -> None:
    out: dict[str, object] = {}
    udp_sender(
        host,
        target_mbps,
        start_event,
        stop_event,
        out,
    )
    result_queue.put(out)


def start_udp_sender_process(
    host: str,
    target_mbps: float,
):
    ctx = mp.get_context("spawn")
    start_event = ctx.Event()
    stop_event = ctx.Event()
    result_queue = ctx.Queue()
    process = ctx.Process(
        target=udp_sender_process_entry,
        args=(
            host,
            target_mbps,
            start_event,
            stop_event,
            result_queue,
        ),
        daemon=True,
    )
    process.start()
    return process, start_event, stop_event, result_queue


def stop_udp_sender_process(
    process,
    stop_event,
    result_queue,
    timeout_s: float = 10.0,
) -> dict[str, object]:
    stop_event.set()
    process.join(timeout=timeout_s)

    if process.is_alive():
        process.terminate()
        process.join(timeout=2.0)
        raise RuntimeError("UDP_SENDER_PROCESS_STUCK")

    if process.exitcode != 0:
        raise RuntimeError(
            f"UDP_SENDER_PROCESS_EXIT_{process.exitcode}"
        )

    try:
        return result_queue.get(timeout=2.0)
    except pyqueue.Empty as exc:
        raise RuntimeError(
            "UDP_SENDER_PROCESS_RESULT_MISSING"
        ) from exc


def run_host_pacing_self_test() -> int:
    sink = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sink.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sink.bind(("127.0.0.1", UDP_PORT))
    sink.settimeout(0.05)

    sink_stop = threading.Event()

    def drain_sink() -> None:
        while not sink_stop.is_set():
            try:
                sink.recvfrom(65535)
            except socket.timeout:
                continue
            except OSError:
                break

    sink_thread = threading.Thread(
        target=drain_sink,
        daemon=True,
    )
    sink_thread.start()

    overall_pass = True

    try:
        print("UDP_HOST_PACING_SELF_TEST=BEGIN", flush=True)
        print(
            f"UDP_HOST_PACING_MODE={UDP_HOST_PACING_MODE}",
            flush=True,
        )

        for target_mbps in (1.0, 2.0, 4.0, 6.0, 8.0, 10.0, 12.0):
            (
                sender,
                start_event,
                stop_event,
                result_queue,
            ) = start_udp_sender_process(
                "127.0.0.1",
                target_mbps,
            )

            start_event.set()
            time.sleep(3.0)

            try:
                out = stop_udp_sender_process(
                    sender,
                    stop_event,
                    result_queue,
                    timeout_s=5.0,
                )
            except RuntimeError as exc:
                print(
                    f"HOST_PACING_TARGET={target_mbps:g} "
                    f"RESULT=FAIL_{exc}",
                    flush=True,
                )
                overall_pass = False
                continue

            offered = float(out.get("offered_mbps", 0.0))
            offered_pct = offered / target_mbps * 100.0
            min_gap_us = float(
                out.get("min_interpacket_us", 0.0)
            )
            target_interval_us = float(
                out.get("target_interval_us", 0.0)
            )
            min_gap_pct = float(
                out.get("min_gap_target_pct", 0.0)
            )
            too_close = int(
                out.get("too_close_packets", 0)
            )
            send_errors = int(out.get("send_errors", 0))

            case_pass = (
                offered_pct >= 98.5
                and min_gap_pct >= (
                    UDP_HOST_MIN_GAP_RATIO * 100.0
                )
                and too_close == 0
                and send_errors == 0
            )
            overall_pass = overall_pass and case_pass

            print(
                f"HOST_PACING_TARGET={target_mbps:g} "
                f"OFFERED={offered:.3f} "
                f"PCT={offered_pct:.3f} "
                f"TARGET_GAP_US={target_interval_us:.1f} "
                f"MIN_GAP_US={min_gap_us:.1f} "
                f"MIN_GAP_PCT={min_gap_pct:.2f} "
                f"TOO_CLOSE={too_close} "
                f"SEND_ERRORS={send_errors} "
                f"RESULT={'PASS' if case_pass else 'FAIL'}",
                flush=True,
            )

        print(
            "UDP_HOST_PACING_SELF_TEST="
            + ("PASS" if overall_pass else "FAIL"),
            flush=True,
        )
    finally:
        sink_stop.set()
        sink.close()
        sink_thread.join(timeout=1.0)

    return 0 if overall_pass else 3


def run_case(
    root: Path,
    master,
    slave,
    host: str,
    udp_target_mbps: float,
    duration_s: float,
    label_prefix: str,
) -> dict[str, object]:
    label_rate = str(udp_target_mbps).replace(".", "p")
    label = (
        f"{label_prefix}_TCP250_RTU800_UDP{label_rate}M"
    )
    case_root = root / label
    case_root.mkdir(parents=True, exist_ok=True)

    exp.send(master, b"X\n", exp.p5b.MASTER_STOP_ACK, 8.0)
    exp.q.wait_server_disconnected(master, timeout_s=20.0)

    set_rtu_scan_100(master)
    set_udp_service(master, udp_target_mbps > 0.0)

    pre_master = exp.q.request_snapshot(master, echo=False)
    pre_slave = exp.p5b.request_slave_snapshot(slave, 5.0)
    exp.write_snapshot(case_root / "master_pre.log", pre_master)
    exp.write_snapshot(case_root / "slave_pre.log", pre_slave)

    if not profile_pass(
        pre_master,
        pre_slave,
        udp_target_mbps > 0.0,
    ):
        raise RuntimeError(f"TRIPLE_PROFILE_DRIFT_{label}")

    exp.p5b.reset_slave_stats(slave, 3.0)
    exp.q.reset_stats(master)
    exp.send(master, b"G\n", exp.p5b.MASTER_START_ACK)

    (
        udp_process,
        start_event,
        stop_event,
        result_queue,
    ) = start_udp_sender_process(
        host,
        udp_target_mbps,
    )

    print(
        f"CASE_BEGIN={label} DURATION_S={duration_s:.0f} "
        f"TCP_TARGET={TCP_TARGET_REQ_S:.0f} "
        f"RTU_TARGET={RTU_TARGET_REQ_S} "
        f"UDP_TARGET_MBPS={udp_target_mbps:g}",
        flush=True,
    )

    start_event.set()
    tcp = exp.budget.run_paced_fc03(
        host,
        502,
        duration_s,
        TCP_TARGET_REQ_S,
        125,
    )
    try:
        udp_result = stop_udp_sender_process(
            udp_process,
            stop_event,
            result_queue,
            timeout_s=10.0,
        )
    except RuntimeError as exc:
        raise RuntimeError(
            f"{exc}_{label}"
        ) from exc

    exp.send(master, b"X\n", exp.p5b.MASTER_STOP_ACK, 8.0)

    # Allow the already-received UDP tail to be consumed before disabling
    # the FAST service and taking the final snapshot.
    time.sleep(0.25)
    set_udp_service(master, False)
    time.sleep(0.05)

    post_master = exp.q.request_snapshot(master, echo=False)
    post_slave = exp.p5b.request_slave_snapshot(slave, 5.0)
    exp.write_snapshot(case_root / "master_final.log", post_master)
    exp.write_snapshot(case_root / "slave_final.log", post_slave)

    duration_ms = exp.iv(post_master, "RTU_TRAFFIC_DURATION_MS", 0)
    started = exp.iv(post_master, "RTU_REQUESTS_STARTED", 0)
    completed = exp.iv(post_master, "RTU_REQUESTS_COMPLETED", 0)
    success = exp.iv(post_master, "RTU_REQUESTS_SUCCESS", 0)
    failed = exp.iv(post_master, "RTU_REQUESTS_FAILED", 0)
    rejected = exp.iv(post_master, "RTU_REQUESTS_REJECTED", 0)
    verify = exp.iv(post_master, "RTU_VERIFY_FAILS", 0)
    scans = exp.iv(post_master, "RTU_MIX_SCANS", 0)
    skipped = exp.iv(post_master, "RTU_PERIODS_SKIPPED", 0)

    rtu_req_s = (
        completed / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )
    scan_hz = (
        scans / (duration_ms / 1000.0)
        if duration_ms > 0
        else 0.0
    )
    rtu_target_pct = rtu_req_s / RTU_TARGET_REQ_S * 100.0

    mix_success = sum(
        exp.iv(post_master, key, 0)
        for key in (
            "RTU_MIX_DI_SUCCESS",
            "RTU_MIX_DO_SUCCESS",
            "RTU_MIX_AI_SUCCESS",
            "RTU_MIX_AO_SUCCESS",
        )
    )
    mix_failed = sum(
        exp.iv(post_master, key, 0)
        for key in (
            "RTU_MIX_DI_FAILED",
            "RTU_MIX_DO_FAILED",
            "RTU_MIX_AI_FAILED",
            "RTU_MIX_AO_FAILED",
        )
    )

    rtu_clean = (
        started == completed == success == mix_success
        and failed == 0
        and mix_failed == 0
        and rejected == 0
        and verify == 0
        and success == scans * 8
        and exp.iv(post_master, "RTU_MIX_DI_SUCCESS", 0) == scans * 2
        and exp.iv(post_master, "RTU_MIX_DO_SUCCESS", 0) == scans * 2
        and exp.iv(post_master, "RTU_MIX_AI_SUCCESS", 0) == scans * 2
        and exp.iv(post_master, "RTU_MIX_AO_SUCCESS", 0) == scans * 2
        and exp.iv(post_master, "RTU_CRC_ERRORS", 0) == 0
        and exp.iv(post_master, "RTU_MASTER_TIMEOUTS", 0) == 0
        and exp.iv(post_slave, "RTU_CRC_ERRORS", 0) == 0
        and exp.iv(post_slave, "RTU_EXCEPTIONS_SENT", 0) == 0
        and exp.iv(post_slave, "RTU_SERVER_DISCARDED_TAILS", 0) == 0
        and exp.iv(post_slave, "RTU_SERVER_DISCARDED_BYTES", 0) == 0
        and exp.iv(post_slave, "RTU_RX_FRAMES", 0) == success
        and exp.iv(post_slave, "RTU_TX_FRAMES", 0) == success
        and exp.iv(post_slave, "RTU_REQUESTS_OK", 0) == success
        and exp.output_maps_exact(post_master, post_slave)
    )

    tcp_clean = (
        int(tcp["timeouts"]) == 0
        and int(tcp["transport_errors"]) == 0
        and int(tcp["protocol_errors"]) == 0
        and exp.iv(post_master, "FRAME_TIMEOUTS", 0) == 0
        and exp.iv(post_master, "BUS_LOCK_TIMEOUTS", 0) == 0
        and exp.iv(post_master, "PROTOCOL_ERRORS", 0) == 0
        and exp.iv(post_master, "REQUESTS_OK", 0) == int(tcp["ok"])
    )

    peripheral_clean = (
        exp.sv(post_master, "FULL_RUNTIME_READY") == "YES"
        and exp.sv(post_master, "SERVER_READY") == "YES"
        and exp.sv(post_master, "ETH_READY") == "YES"
        and exp.sv(post_master, "ETH_LINK") == "UP"
        and exp.sv(post_master, "DISPLAY_READY") == "YES"
        and exp.sv(post_master, "FRAM_READY") == "YES"
        and exp.sv(post_master, "SD_READY") == "YES"
        and exp.sv(post_master, "RTC_PRESENT") == "YES"
        and exp.sv(post_master, "IO_INITIALIZED") == "YES"
        and exp.sv(post_master, "BUTTONS_READY") == "YES"
        and exp.iv(post_master, "PERIPHERAL_FAILURE_COUNT", 0) == 0
        and exp.iv(post_master, "SD_DATALOG_FAILED_COMMITS", 0) == 0
        and exp.iv(post_master, "SPI_PROBE_FAILS", 0) == 0
        and exp.sv(post_slave, "DISPLAY_READY") == "YES"
    )

    reset_clean = exp.no_reset(
        pre_master,
        post_master,
        pre_slave,
        post_slave,
        float(tcp["elapsed_s"]),
    )

    udp_sent_packets = int(udp_result.get("sent_packets", 0))
    udp_sent_bytes = int(udp_result.get("sent_bytes", 0))
    udp_send_errors = int(udp_result.get("send_errors", 0))
    udp_offered_mbps = float(udp_result.get("offered_mbps", 0.0))
    udp_host_pacing_skips = int(
        udp_result.get("pacing_deadlines_skipped", 0)
    )
    udp_host_max_late_us = float(
        udp_result.get("max_late_us", 0.0)
    )
    udp_host_min_interpacket_us = float(
        udp_result.get("min_interpacket_us", 0.0)
    )
    udp_host_target_interval_us = float(
        udp_result.get("target_interval_us", 0.0)
    )
    udp_host_min_gap_target_pct = float(
        udp_result.get("min_gap_target_pct", 0.0)
    )
    udp_host_too_close_packets = int(
        udp_result.get("too_close_packets", 0)
    )

    udp_rx_packets = exp.iv(post_master, "UDP_FAST_RX_PACKETS", 0)
    udp_rx_bytes = exp.iv(post_master, "UDP_FAST_RX_BYTES", 0)
    udp_transport_errors = exp.iv(
        post_master, "UDP_FAST_TRANSPORT_ERRORS", 0
    )
    udp_spi_errors = exp.iv(
        post_master, "UDP_FAST_SPI_LOCK_ERRORS", 0
    )
    udp_wrong_size = exp.iv(
        post_master, "UDP_FAST_WRONG_SIZE_PACKETS", 0
    )
    udp_decode_errors = exp.iv(
        post_master, "UDP_FAST_SEQUENCE_DECODE_ERRORS", 0
    )
    udp_duplicates = exp.iv(
        post_master, "UDP_FAST_SEQUENCE_DUPLICATES", 0
    )
    udp_reorders = exp.iv(
        post_master, "UDP_FAST_SEQUENCE_REORDERS", 0
    )
    udp_range_missing = exp.iv(
        post_master, "UDP_FAST_SEQUENCE_RANGE_MISSING", 0
    )

    tcp_elapsed = float(tcp["elapsed_s"])
    udp_delivered_mbps = (
        udp_rx_bytes * 8.0 / tcp_elapsed / 1_000_000.0
        if tcp_elapsed > 0.0
        else 0.0
    )
    udp_delivery_pct = (
        100.0 * udp_rx_packets / udp_sent_packets
        if udp_sent_packets > 0
        else 100.0
    )
    udp_offered_target_pct = (
        udp_offered_mbps / udp_target_mbps * 100.0
        if udp_target_mbps > 0.0
        else 100.0
    )
    udp_delivered_target_pct = (
        udp_delivered_mbps / udp_target_mbps * 100.0
        if udp_target_mbps > 0.0
        else 100.0
    )

    udp_integrity_clean = (
        udp_send_errors == 0
        and udp_transport_errors == 0
        and udp_spi_errors == 0
        and udp_wrong_size == 0
        and udp_decode_errors == 0
        and udp_duplicates == 0
        and udp_reorders == 0
    )

    udp_host_spacing_pass = (
        udp_target_mbps == 0.0
        or (
            udp_host_min_gap_target_pct
            >= UDP_HOST_MIN_GAP_RATIO * 100.0
            and udp_host_too_close_packets == 0
        )
    )
    udp_source_pass = (
        udp_target_mbps == 0.0
        or (
            udp_offered_target_pct >= 99.0
            and udp_host_spacing_pass
        )
    )
    udp_target_pass = (
        udp_target_mbps == 0.0
        or (
            udp_source_pass
            and udp_delivered_target_pct >= 99.0
            and udp_delivery_pct >= 99.0
        )
    )

    runtime_clean = (
        rtu_clean
        and tcp_clean
        and peripheral_clean
        and reset_clean
        and udp_integrity_clean
        and exp.fast_profile_pass(post_master, post_slave)
        and exp.sv(post_master, "RTU_RATE_MODE") == "SCAN_PACED"
        and exp.iv(post_master, "RTU_TARGET_HZ", -1)
        == RTU_TARGET_REQ_S
        and exp.iv(post_master, "RTU_SCAN_TARGET_HZ", -1) == 100
        and exp.iv(post_master, "RTU_SCAN_PERIOD_US", -1) == 10000
        and exp.sv(post_master, "UDP_FAST_READY") == "YES"
    )

    tcp_target_pct = float(tcp["target_pct"])
    tcp_target_pass = tcp_target_pct >= 99.9
    rtu_operational_pass = (
        rtu_target_pct >= 99.0
        and scan_hz >= 99.0
    )
    rtu_deterministic_pass = (
        rtu_operational_pass
        and skipped == 0
    )

    operational_pass = (
        runtime_clean
        and tcp_target_pass
        and rtu_operational_pass
        and udp_target_pass
    )
    deterministic_pass = (
        operational_pass
        and rtu_deterministic_pass
    )

    if not runtime_clean:
        classification = "PRODUCT_FAILURE"
    elif not tcp_target_pass:
        classification = "TCP_TARGET_FAIL_CLEAN"
    elif not rtu_operational_pass:
        classification = "RTU800_OPERATIONAL_TARGET_FAIL_CLEAN"
    elif not udp_source_pass:
        classification = "HOST_UDP_SOURCE_FAIL"
    elif not udp_target_pass:
        classification = "UDP_TARGET_FAIL_CLEAN"
    elif rtu_deterministic_pass:
        classification = "PASS_OPERATIONAL_RTU_DETERMINISTIC"
    else:
        classification = "PASS_OPERATIONAL_WITH_RTU_JITTER"

    row: dict[str, object] = {
        "case": label,
        "duration_s": tcp_elapsed,
        "tcp_target_req_s": TCP_TARGET_REQ_S,
        "tcp_req_s": float(tcp["achieved_req_s"]),
        "tcp_target_pct": tcp_target_pct,
        "tcp_avg_us": float(tcp["avg_us"]),
        "tcp_p95_us": float(tcp["p95_us"]),
        "tcp_p99_us": float(tcp["p99_us"]),
        "tcp_max_us": float(tcp["max_us"]),
        "tcp_timeouts": int(tcp["timeouts"]),
        "tcp_transport_errors": int(tcp["transport_errors"]),
        "tcp_protocol_errors": int(tcp["protocol_errors"]),
        "rtu_target_req_s": RTU_TARGET_REQ_S,
        "rtu_req_s": rtu_req_s,
        "rtu_target_pct": rtu_target_pct,
        "rtu_scans_s": scan_hz,
        "rtu_scan_period_ms": 1000.0 / scan_hz if scan_hz > 0 else 0.0,
        "rtu_periods_skipped": skipped,
        "rtu_success": success,
        "rtu_failed": failed,
        "rtu_rejected": rejected,
        "rtu_verify_fail": verify,
        "udp_target_mbps": udp_target_mbps,
        "udp_offered_mbps": udp_offered_mbps,
        "udp_offered_target_pct": udp_offered_target_pct,
        "udp_delivered_mbps": udp_delivered_mbps,
        "udp_delivered_target_pct": udp_delivered_target_pct,
        "udp_delivery_pct": udp_delivery_pct,
        "udp_sent_packets": udp_sent_packets,
        "udp_rx_packets": udp_rx_packets,
        "udp_host_pacing_mode": UDP_HOST_PACING_MODE,
        "udp_host_pacing_deadlines_skipped": udp_host_pacing_skips,
        "udp_host_max_late_us": udp_host_max_late_us,
        "udp_host_min_interpacket_us": udp_host_min_interpacket_us,
        "udp_host_target_interval_us": udp_host_target_interval_us,
        "udp_host_min_gap_target_pct": udp_host_min_gap_target_pct,
        "udp_host_too_close_packets": udp_host_too_close_packets,
        "udp_host_spacing_pass": udp_host_spacing_pass,
        "udp_range_missing": udp_range_missing,
        "udp_wrong_size_packets": udp_wrong_size,
        "udp_sequence_decode_errors": udp_decode_errors,
        "udp_sequence_duplicates": udp_duplicates,
        "udp_sequence_reorders": udp_reorders,
        "udp_transport_errors": udp_transport_errors,
        "udp_spi_lock_errors": udp_spi_errors,
        "udp_spi_hold_max_us": exp.iv(
            post_master, "UDP_FAST_SPI_HOLD_MAX_US", 0
        ),
        "loop_gap_max_us": exp.iv(post_master, "LOOP_GAP_MAX_US", 0),
        "rtu_service_gap_max_us": exp.iv(
            post_master, "RTU_SERVICE_GAP_MAX_US", 0
        ),
        "rtu_transaction_max_us": exp.iv(
            post_master, "RTU_TRANSACTION_MAX_US", 0
        ),
        "spi_probe_max_wait_us": exp.iv(
            post_master, "SPI_PROBE_MAX_WAIT_US", 0
        ),
        "spi_probe_over_1ms": exp.iv(
            post_master, "SPI_PROBE_OVER_1MS", 0
        ),
        "spi_probe_over_10ms": exp.iv(
            post_master, "SPI_PROBE_OVER_10MS", 0
        ),
        "peripheral_failures": exp.iv(
            post_master, "PERIPHERAL_FAILURE_COUNT", 0
        ),
        "sd_failed_commits": exp.iv(
            post_master, "SD_DATALOG_FAILED_COMMITS", 0
        ),
        "runtime_clean": runtime_clean,
        "tcp_target_pass": tcp_target_pass,
        "rtu_operational_pass": rtu_operational_pass,
        "rtu_deterministic_pass": rtu_deterministic_pass,
        "udp_source_pass": udp_source_pass,
        "udp_target_pass": udp_target_pass,
        "udp_integrity_clean": udp_integrity_clean,
        "operational_pass": operational_pass,
        "deterministic_pass": deterministic_pass,
        "classification": classification,
    }

    (case_root / "result.json").write_text(
        json.dumps(row, indent=2),
        encoding="utf-8",
    )

    print(
        f"CASE_END={label} "
        f"TCP={row['tcp_req_s']:.3f} "
        f"TCP_PCT={row['tcp_target_pct']:.3f} "
        f"RTU={row['rtu_req_s']:.3f} "
        f"RTU_PCT={row['rtu_target_pct']:.3f} "
        f"SCANS={row['rtu_scans_s']:.3f} "
        f"RTU_SKIPPED={row['rtu_periods_skipped']} "
        f"UDP_OFFERED={row['udp_offered_mbps']:.3f} "
        f"UDP_DUT={row['udp_delivered_mbps']:.3f} "
        f"UDP_DELIVERY={row['udp_delivery_pct']:.3f}% "
        f"HOST_MIN_GAP_PCT={row['udp_host_min_gap_target_pct']:.2f} "
        f"HOST_TOO_CLOSE={row['udp_host_too_close_packets']} "
        f"RUNTIME_CLEAN={row['runtime_clean']} "
        f"TCP_PASS={row['tcp_target_pass']} "
        f"RTU_OPERATIONAL_PASS={row['rtu_operational_pass']} "
        f"RTU_DETERMINISTIC_PASS={row['rtu_deterministic_pass']} "
        f"UDP_PASS={row['udp_target_pass']} "
        f"OPERATIONAL_PASS={row['operational_pass']} "
        f"DETERMINISTIC_PASS={row['deterministic_pass']} "
        f"CLASS={classification}",
        flush=True,
    )

    if classification == "PRODUCT_FAILURE":
        raise RuntimeError(f"PRODUCT_FAILURE_IN_{label}")

    return row


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--master-serial", default="COM14")
    parser.add_argument("--slave-serial", default="COM4")
    parser.add_argument("--output-root", default="")
    parser.add_argument(
        "--host-pacing-self-test",
        action="store_true",
    )
    parser.add_argument("--ladder-duration", type=float, default=300.0)
    parser.add_argument("--confirm-duration", type=float, default=600.0)
    parser.add_argument("--udp-ladder", default=DEFAULT_UDP_LADDER)
    args = parser.parse_args()

    if args.host_pacing_self_test:
        return run_host_pacing_self_test()

    if not args.output_root:
        raise ValueError("--output-root es requerido para la campaña")

    if args.ladder_duration < 300.0:
        raise ValueError("ladder-duration debe ser >=300 s")
    if args.confirm_duration < 600.0:
        raise ValueError("confirm-duration debe ser >=600 s")

    udp_ladder = parse_ladder(args.udp_ladder)
    root = Path(args.output_root)
    root.mkdir(parents=True, exist_ok=True)

    master = exp.p5b.open_serial_no_dtr(args.master_serial)
    slave = exp.p5b.open_serial_no_dtr(args.slave_serial)

    ladder_rows: list[dict[str, object]] = []
    confirmation: dict[str, object] | None = None
    baseline_not_operational = False
    selected_udp_mbps = 0.0

    try:
        time.sleep(1.0)
        host, boot_master, boot_slave = wait_udp_ready(master, slave)
        exp.write_snapshot(root / "master_boot.log", boot_master)
        exp.write_snapshot(root / "slave_boot.log", boot_slave)
        (root / "dut_ip.txt").write_text(host + "\n", encoding="utf-8")

        exp.configure_fast(master, slave)
        set_rtu_scan_100(master)
        set_udp_service(master, False)

        profile_master = exp.q.request_snapshot(master, echo=False)
        profile_slave = exp.p5b.request_slave_snapshot(slave, 5.0)
        exp.write_snapshot(root / "master_profile.log", profile_master)
        exp.write_snapshot(root / "slave_profile.log", profile_slave)

        if not profile_pass(profile_master, profile_slave, False):
            raise RuntimeError("TRIPLE_PROFILE_PREFLIGHT_FAIL")

        print("=" * 78)
        print(" A14 FINAL TRIPLE COEXISTENCE - TCP250 + RTU800 + UDP FAST")
        print("=" * 78)
        print(f"DUT_IP={host}")
        print("TCP_TARGET_REQ_S=250")
        print("RTU_TARGET_REQ_S=800")
        print("RTU_SCAN_TARGET_HZ=100")
        print("RTU_SCHEDULER_MODE=SCAN_PACED_8_PER_10MS")
        print("UDP_PAYLOAD_BYTES=1016")
        print(f"UDP_HOST_PACING_MODE={UDP_HOST_PACING_MODE}")
        print("UDP_HOST_SPACING_GUARD=MIN_GAP_GE_90PCT_TARGET")
        print("UDP_LADDER_MBPS=" + ",".join(f"{x:g}" for x in udp_ladder))
        print(f"LADDER_DURATION_S={args.ladder_duration:.0f}")
        print(f"CONFIRM_DURATION_S={args.confirm_duration:.0f}")
        print("TCP_STRICT_THRESHOLD_PCT=99.9")
        print("RTU_OPERATIONAL_THRESHOLD=REQ_GE_99PCT_AND_SCAN_GE_99HZ")
        print("RTU_DETERMINISTIC_THRESHOLD=OPERATIONAL_AND_ZERO_SKIPS")
        print("UDP_OPERATIONAL_THRESHOLD_PCT=99.0_DELIVERED")

        ladder_root = root / "UDP_LADDER"
        ladder_root.mkdir(parents=True, exist_ok=True)

        for target in udp_ladder:
            row = run_case(
                ladder_root,
                master,
                slave,
                host,
                target,
                args.ladder_duration,
                "LADDER",
            )
            ladder_rows.append(row)

            if target == 0.0 and not bool(row["operational_pass"]):
                baseline_not_operational = True
                print(
                    "BASELINE_RESULT=CHARACTERIZED_NOT_OPERATIONAL "
                    f"TCP_PCT={float(row['tcp_target_pct']):.3f} "
                    f"RTU_PCT={float(row['rtu_target_pct']):.3f} "
                    f"SCANS={float(row['rtu_scans_s']):.3f} "
                    f"RTU_SKIPPED={int(row['rtu_periods_skipped'])} "
                    f"RTU_DETERMINISTIC_PASS={bool(row['rtu_deterministic_pass'])} "
                    f"RUNTIME_CLEAN={bool(row['runtime_clean'])}",
                    flush=True,
                )
                break

        write_csv(root / "UDP_LADDER.csv", ladder_rows)

        operational_positive = [
            row
            for row in ladder_rows
            if float(row["udp_target_mbps"]) > 0.0
            and bool(row["operational_pass"])
        ]

        selected_udp_mbps = (
            max(float(row["udp_target_mbps"]) for row in operational_positive)
            if operational_positive
            else 0.0
        )

        selection = {
            "tcp_target_req_s": TCP_TARGET_REQ_S,
            "rtu_target_req_s": RTU_TARGET_REQ_S,
            "rtu_scan_target_hz": 100.0,
            "udp_selected_mbps": selected_udp_mbps,
            "operational_positive_points": [
                float(row["udp_target_mbps"])
                for row in operational_positive
            ],
        }
        (root / "SELECTION.json").write_text(
            json.dumps(selection, indent=2),
            encoding="utf-8",
        )

        if (not baseline_not_operational) and selected_udp_mbps > 0.0:
            confirm_root = root / "CONFIRMATION"
            confirm_root.mkdir(parents=True, exist_ok=True)
            confirmation = run_case(
                confirm_root,
                master,
                slave,
                host,
                selected_udp_mbps,
                args.confirm_duration,
                "CONFIRM",
            )

    finally:
        try:
            set_udp_service(master, False)
        except Exception:
            pass
        master.close()
        slave.close()

    if baseline_not_operational:
        status = "CHARACTERIZED_BASELINE_NOT_OPERATIONAL"
    elif selected_udp_mbps <= 0.0:
        status = "CHARACTERIZED_NO_POSITIVE_UDP_OPERATIONAL"
    elif confirmation is not None and bool(confirmation["operational_pass"]):
        status = "PASS_TRIPLE_COEXISTENCE_CONFIRMED"
    else:
        status = "REVIEW_CONFIRMATION_FAILED"

    summary = {
        "status": status,
        "tcp_target_req_s": TCP_TARGET_REQ_S,
        "rtu_target_req_s": RTU_TARGET_REQ_S,
        "rtu_scan_target_hz": 100.0,
        "udp_ladder_mbps": udp_ladder,
        "baseline_not_operational": baseline_not_operational,
        "selected_udp_mbps": selected_udp_mbps,
        "ladder": ladder_rows,
        "confirmation": confirmation,
    }

    (root / "FINAL_SUMMARY.json").write_text(
        json.dumps(summary, indent=2),
        encoding="utf-8",
    )

    final_lines = [
        f"A14_FINAL_TRIPLE_COEXISTENCE={status}",
        "TCP_TARGET_REQ_S=250",
        "RTU_TARGET_REQ_S=800",
        "RTU_SCAN_TARGET_HZ=100",
        f"BASELINE_NOT_OPERATIONAL={baseline_not_operational}",
        f"UDP_SELECTED_MBPS={selected_udp_mbps:g}",
        "UDP_LADDER_MBPS=" + ",".join(f"{x:g}" for x in udp_ladder),
    ]

    if confirmation is not None:
        final_lines.extend(
            [
                f"CONFIRM_TCP_REQ_S={float(confirmation['tcp_req_s']):.3f}",
                f"CONFIRM_RTU_REQ_S={float(confirmation['rtu_req_s']):.3f}",
                f"CONFIRM_RTU_SCANS_S={float(confirmation['rtu_scans_s']):.3f}",
                f"CONFIRM_RTU_SKIPPED={int(confirmation['rtu_periods_skipped'])}",
                f"CONFIRM_RTU_OPERATIONAL_PASS={bool(confirmation['rtu_operational_pass'])}",
                f"CONFIRM_RTU_DETERMINISTIC_PASS={bool(confirmation['rtu_deterministic_pass'])}",
                f"CONFIRM_UDP_DUT_MBPS={float(confirmation['udp_delivered_mbps']):.3f}",
                f"CONFIRM_UDP_DELIVERY_PCT={float(confirmation['udp_delivery_pct']):.3f}",
                f"CONFIRM_RUNTIME_CLEAN={bool(confirmation['runtime_clean'])}",
                f"CONFIRM_OPERATIONAL_PASS={bool(confirmation['operational_pass'])}",
                f"CONFIRM_DETERMINISTIC_PASS={bool(confirmation['deterministic_pass'])}",
            ]
        )

    (root / "FINAL_STATUS.txt").write_text(
        "\n".join(final_lines) + "\n",
        encoding="utf-8",
    )

    print("=" * 78)
    print(" A14 FINAL TRIPLE COEXISTENCE SUMMARY")
    print("=" * 78)
    for line in final_lines:
        print(line)
    print(f"RESULT_ROOT={root}")

    return 0 if status in (
        "PASS_TRIPLE_COEXISTENCE_CONFIRMED",
        "CHARACTERIZED_NO_POSITIVE_UDP_OPERATIONAL",
        "CHARACTERIZED_BASELINE_NOT_OPERATIONAL",
    ) else 2


if __name__ == "__main__":
    raise SystemExit(main())
