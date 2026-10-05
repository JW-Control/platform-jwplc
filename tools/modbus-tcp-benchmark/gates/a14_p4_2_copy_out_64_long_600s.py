#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import importlib
import importlib.util
import math
import re
import socket
import statistics
import sys
import tempfile
import time
from pathlib import Path
from types import ModuleType

import a14_h4a04p3_w5500_fifo_reuse_ab as p3


BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
VERIFY_DURATION_S = 1.0
PERF_DURATION_S = 600.0
BUCKET_DURATION_S = 60.0
BUCKET_COUNT = 10
TCP_CHUNK = 4096
TCP_RX_MAX_CHUNKS = 8
TCP_PORT = 5001
SEND_SAMPLE_EVERY = 256
SPI_HZ = 26_000_000
BUCKET_SPREAD_MAX_PCT = 0.75
DRIFT_REVIEW_LIMIT_PCT = 0.75

ZERO_ARM_FUNCTIONAL_FIELDS = (
    "RX_BYTES",
    "RX_OPERATIONS",
    "TRANSPORT_ERRORS",
    "TCP_SPI_LOCK_ERRORS",
)
ZERO_ARM_HOLD_EVIDENCE_FIELDS = (
    "TCP_SPI_HOLD_COUNT",
    "TCP_SPI_HOLD_TOTAL_US",
    "TCP_SPI_HOLD_AVG_US",
    "TCP_SPI_HOLD_MAX_US",
)

P4_2_SHORT_PAYLOAD_MBPS = 17.113
P4_2_SHORT_US_PER_BYTE = 0.467481
P4_2_SHORT_TCP_SECONDARY_MBPS = 14.203710
PRE_P4_2_PAYLOAD_MBPS = 16.821
PRE_P4_2_US_PER_BYTE = 0.475601


class IntegrityFailure(RuntimeError):
    pass


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def delta_pct(value: float, reference: float) -> float:
    if reference <= 0.0:
        raise RuntimeError("P4_2_LR600_NONPOSITIVE_REFERENCE")
    return (value / reference - 1.0) * 100.0


def percentile(values: list[float], quantile: float) -> float:
    if not values:
        raise RuntimeError("P4_2_LR600_EMPTY_PERCENTILE")
    if not 0.0 <= quantile <= 1.0:
        raise RuntimeError("P4_2_LR600_INVALID_QUANTILE")

    ordered = sorted(values)
    position = (len(ordered) - 1) * quantile
    lower = math.floor(position)
    upper = math.ceil(position)

    if lower == upper:
        return float(ordered[lower])

    weight = position - lower
    return float(
        ordered[lower] * (1.0 - weight)
        + ordered[upper] * weight
    )


def require_define(text: str, macro: str, expected: str) -> None:
    pattern = re.compile(
        rf"^\s*#define\s+{re.escape(macro)}\s+"
        rf"{re.escape(expected)}\s*$",
        re.MULTILINE,
    )
    if not pattern.search(text):
        raise RuntimeError(
            f"P4_2_LR600_SOURCE_DEFAULT_MISMATCH={macro}:{expected}"
        )


def preflight_pyserial() -> ModuleType:
    emit("PYTHON_EXECUTABLE", sys.executable)

    try:
        serial_module = importlib.import_module("serial")
    except Exception as exc:
        emit("PYSERIAL_IMPORT", "FAIL")
        emit("PYSERIAL_VERSION", "UNAVAILABLE")
        raise RuntimeError(
            "P4_2_LR600_PYSERIAL_REQUIRED_BEFORE_BUILD"
        ) from exc

    emit("PYSERIAL_IMPORT", "PASS")
    emit(
        "PYSERIAL_VERSION",
        getattr(serial_module, "__version__", "UNKNOWN"),
    )
    return serial_module


def functional_zero_values(snapshot: dict[str, str]) -> dict[str, int]:
    try:
        return {
            field: int(snapshot[field])
            for field in ZERO_ARM_FUNCTIONAL_FIELDS
        }
    except (KeyError, TypeError, ValueError) as exc:
        raise RuntimeError("P4_2_LR600_ZERO_ARM_FIELD_INVALID") from exc


def functional_zero_arm_pass(snapshot: dict[str, str]) -> bool:
    return not any(functional_zero_values(snapshot).values())


def validate_zero_arm_regression() -> None:
    hold_activity_after_snapshot = {
        "RX_BYTES": "0",
        "RX_OPERATIONS": "0",
        "TRANSPORT_ERRORS": "0",
        "TCP_SPI_LOCK_ERRORS": "0",
        "TCP_SPI_HOLD_COUNT": "231",
        "TCP_SPI_HOLD_TOTAL_US": "16135",
        "TCP_SPI_HOLD_AVG_US": "69",
        "TCP_SPI_HOLD_MAX_US": "218",
    }
    if not functional_zero_arm_pass(hold_activity_after_snapshot):
        raise RuntimeError("P4_2_LR600_ZERO_ARM_HOLD_REGRESSION")

    for field in ZERO_ARM_FUNCTIONAL_FIELDS:
        invalid = dict(hold_activity_after_snapshot)
        invalid[field] = "1"
        if functional_zero_arm_pass(invalid):
            raise RuntimeError(
                f"P4_2_LR600_ZERO_ARM_FALSE_PASS={field}"
            )

    emit("P4_2_LR600_ZERO_ARM_REGRESSION", "PASS")
    emit("P4_2_LR600_ZERO_ARM_FIX", "PASS")
    emit("P4_2_LR600_FUNCTIONAL_ZERO_CONTRACT", "PASS")
    emit("P4_2_LR600_HOLD_COUNTERS_ZERO_REQUIRED", "NO")


def audit_source_contract(repo: Path) -> None:
    spi_h = (
        repo / "JWPLC" / "2.1.0" / "libraries" / "SPI" / "src" / "SPI.h"
    ).read_text(encoding="utf-8")
    spi_cpp = (
        repo / "JWPLC" / "2.1.0" / "libraries" / "SPI" / "src" / "SPI.cpp"
    ).read_text(encoding="utf-8")
    w5100_h = (
        repo
        / "JWPLC"
        / "2.1.0"
        / "libraries"
        / "JWPLC_Ethernet"
        / "src"
        / "utility"
        / "w5100.h"
    ).read_text(encoding="utf-8")

    require_define(spi_h, "JWPLC_SPI_FIFO_REUSE_COPY_OUT_64", "1")
    require_define(spi_h, "JWPLC_SPI_FIFO_REUSE_DLEN_CACHE", "1")
    require_define(spi_h, "JWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS", "0")
    require_define(w5100_h, "JWPLC_W5500_RX_FIFO_REUSE", "1")
    require_define(w5100_h, "JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES", "0")

    if "SPISettings(26000000, MSBFIRST, SPI_MODE0)" not in w5100_h:
        raise RuntimeError("P4_2_LR600_W5500_SPI_HZ_MISMATCH")

    start_token = "#if JWPLC_SPI_FIFO_REUSE_COPY_OUT_64"
    start = spi_cpp.find(start_token)
    if start < 0:
        raise RuntimeError("P4_2_LR600_COPY_OUT_BLOCK_MISSING")
    end = spi_cpp.find("#endif", start)
    if end < 0:
        raise RuntimeError("P4_2_LR600_COPY_OUT_BLOCK_UNTERMINATED")
    block = spi_cpp[start:end]

    if "if (c_len == 64U)" not in block:
        raise RuntimeError("P4_2_LR600_64B_CONDITION_MISSING")
    if "memcpy" in block:
        raise RuntimeError("P4_2_LR600_MMIO_MEMCPY_FORBIDDEN")

    for index in range(16):
        line = f"result[{index}] = dev->data_buf[{index}];"
        if block.count(line) != 1:
            raise RuntimeError(
                f"P4_2_LR600_EXPLICIT_COPY_INVALID={index}"
            )

    normalized = spi_cpp.replace("\r\n", "\n")
    if "} else\n#endif\n    if (c_len & 3U) {" not in normalized:
        raise RuntimeError("P4_2_LR600_TAIL_PATH_CONTRACT_MISMATCH")

    emit("P4_2_LR600_COPY_OUT_DEFAULT", 1)
    emit("P4_2_LR600_COPY_OUT_SOURCE", "PACKAGE_DEFAULT")
    emit("P4_2_LR600_FIFO_REUSE", "ON_DEFAULT")
    emit("P4_2_LR600_DLEN_REUSE", "ON_DEFAULT")
    emit("P4_2_LR600_DIRECT_RX", "OFF")
    emit("P4_2_LR600_W5500_SPI_HZ", SPI_HZ)
    emit("P4_2_LR600_COPY_OUT_IMPLEMENTATION", "EXPLICIT_16_WORDS")
    emit("P4_2_LR600_TAIL_1_63", "PRESERVED")


def audit_firmware_contract(firmware: Path) -> None:
    source = firmware.read_text(encoding="utf-8")

    required_snapshot_fields = (
        "RX_BYTES",
        "RX_OPERATIONS",
        "TRANSPORT_ERRORS",
        "TCP_SPI_LOCK_ERRORS",
        "TCP_SPI_HOLD_COUNT",
        "TCP_SPI_HOLD_TOTAL_US",
        "TCP_SPI_HOLD_AVG_US",
        "TCP_SPI_HOLD_MAX_US",
        "TCP_PROF_PAYLOAD_READ_CALLS",
        "TCP_PROF_PAYLOAD_READ_TOTAL_US",
        "TCP_PROF_PAYLOAD_BYTES",
    )
    for field in required_snapshot_fields:
        if f'"{field}="' not in source:
            raise RuntimeError(
                f"P4_2_LR600_FIRMWARE_FIELD_MISSING={field}"
            )

    if not re.search(
        r"#define\s+JWPLC_H4A04P7_DEFER_TCP_COMMIT\s+0",
        source,
    ):
        raise RuntimeError("P4_2_LR600_RX_COMMIT_NOT_IMMEDIATE")

    emit("P4_2_LR600_DUT_COUNTER_CONTRACT", "PASS")
    emit("P4_2_LR600_RX_COMMIT", "IMMEDIATE")
    emit("P4_2_LR600_SPI_TELEMETRY", "FINAL_SNAPSHOT_ONLY")


def build_flags(*, verify: bool) -> tuple[str, ...]:
    profile = "0" if verify else "1"
    verify_flag = "1" if verify else "0"
    flags = (
        f"-DJWPLC_ETHERNET_ENABLE_PROFILE_HOOKS={profile}",
        "-DJWPLC_W5500_RX_DIRECT_TRANSFER_BYTES=0",
        "-DJWPLC_W5500_RX_FIFO_REUSE=1",
        f"-DJWPLC_H4A04P3_VERIFY_PAYLOAD={verify_flag}",
        "-DJWPLC_SPI_PROFILE_FIFO_REUSE_CHUNKS=0",
    )

    forbidden = "-D" + "JWPLC_SPI_FIFO_REUSE_COPY_OUT_64="
    dlen_override = "-D" + "JWPLC_SPI_FIFO_REUSE_DLEN_CACHE="
    if any(flag.startswith(forbidden) for flag in flags):
        raise RuntimeError("P4_2_LR600_COPY_OUT_OVERRIDE_FORBIDDEN")
    if any(flag.startswith(dlen_override) for flag in flags):
        raise RuntimeError("P4_2_LR600_DLEN_OVERRIDE_FORBIDDEN")

    return flags


def compile_default(
    *,
    cli: Path,
    fqbn: str,
    repo: Path,
    sketch: Path,
    libraries: Path,
    build_dir: Path,
    log: Path,
    verify: bool,
) -> None:
    flags = build_flags(verify=verify)
    mode = "VERIFY" if verify else "PERF"
    cmd = [
        str(cli),
        "compile",
        "--verbose",
        "--fqbn",
        fqbn,
        "--build-path",
        str(build_dir),
        "--libraries",
        str(libraries),
        "--build-property",
        f"compiler.cpp.extra_flags={' '.join(flags)}",
        str(sketch),
    ]

    exit_code = p3.run_logged(cmd, log, repo)
    emit(f"P4_2_LR600_{mode}_COMPILE_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-9000:])
        raise RuntimeError(f"P4_2_LR600_{mode}_COMPILE_FAILED")

    normalized = p3.decode(log.read_bytes()).replace("\\", "/").lower()
    forbidden = ("-d" + "jwplc_spi_fifo_reuse_copy_out_64=").lower()
    dlen_override = ("-d" + "jwplc_spi_fifo_reuse_dlen_cache=").lower()

    if forbidden in normalized:
        raise RuntimeError("P4_2_LR600_COPY_OUT_OVERRIDE_DETECTED")
    if dlen_override in normalized:
        raise RuntimeError("P4_2_LR600_DLEN_OVERRIDE_DETECTED")

    for flag in flags:
        if flag.lower() not in normalized:
            raise RuntimeError(
                f"P4_2_LR600_{mode}_FLAG_MISSING={flag}"
            )

    for source in (
        "w5100.cpp",
        "socket.cpp",
        "spi.cpp",
        "jwplc_idlescreen.cpp",
        "jwplc_tft.cpp",
    ):
        if source not in normalized:
            raise RuntimeError(
                f"P4_2_LR600_{mode}_SOURCE_MISSING={source}"
            )

    for archive in (
        "libjwplc_display.a",
        "libjwplc_tft.a",
        "libspi.a",
    ):
        if archive in normalized:
            raise RuntimeError(
                f"P4_2_LR600_{mode}_UNEXPECTED_ARCHIVE={archive}"
            )

    emit(f"P4_2_LR600_{mode}_SOURCE_FIRST", "PASS")
    emit(f"P4_2_LR600_{mode}_COPY_OUT_OVERRIDE", "NO")


def upload(
    *,
    cli: Path,
    fqbn: str,
    serial_port: str,
    build_dir: Path,
    sketch: Path,
    repo: Path,
    log: Path,
    label: str,
) -> None:
    exit_code = p3.run_logged(
        [
            str(cli),
            "upload",
            "--fqbn",
            fqbn,
            "--port",
            serial_port,
            "--input-dir",
            str(build_dir),
            str(sketch),
        ],
        log,
        repo,
    )
    emit(f"P4_2_LR600_{label}_UPLOAD_EXIT", exit_code)
    if exit_code != 0:
        print(p3.decode(log.read_bytes())[-7000:])
        raise RuntimeError(f"P4_2_LR600_{label}_UPLOAD_FAILED")


def run_verify_case(
    *,
    runner: Path,
    repo: Path,
    serial_port: str,
    log: Path,
) -> str:
    exit_code = p3.run_logged(
        [
            sys.executable,
            "-B",
            "-u",
            str(runner),
            "--serial",
            serial_port,
            "--duration",
            f"{VERIFY_DURATION_S:.1f}",
            "--chunk",
            str(TCP_CHUNK),
            "--max-chunks",
            str(TCP_RX_MAX_CHUNKS),
            "--variant",
            "BASE",
        ],
        log,
        repo,
    )
    emit("P4_2_LR600_VERIFY_CASE_EXIT", exit_code)
    text = p3.decode(log.read_bytes())

    if exit_code not in (0, 2):
        print(text[-9000:])
        raise RuntimeError("P4_2_LR600_VERIFY_RUNTIME_FAILED")

    functional = p3.one(text, "H4A04P1_FUNCTIONAL_PASS")
    transport_errors = p3.integer(text, "H4A04P1_TRANSPORT_ERRORS")
    lock_errors = p3.integer(text, "H4A04P1_TCP_SPI_LOCK_ERRORS")

    if (
        exit_code != 0
        or functional != "YES"
        or transport_errors != 0
        or lock_errors != 0
    ):
        raise IntegrityFailure("P4_2_LR600_VERIFY_FUNCTIONAL_FAIL")

    return text


def load_rx_runner(runner: Path) -> ModuleType:
    spec = importlib.util.spec_from_file_location(
        "a14_p4_2_lr600_rx_runner",
        runner,
    )
    if spec is None or spec.loader is None:
        raise RuntimeError("P4_2_LR600_RUNNER_LOAD_FAILED")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_long_case(
    *,
    rx: ModuleType,
    serial_port: str,
) -> dict[str, object]:
    dut = rx.DutSerial(serial_port)
    sock: socket.socket | None = None

    bucket_bytes = [0] * BUCKET_COUNT
    bucket_operations = [0] * BUCKET_COUNT
    bucket_samples: list[list[float]] = [
        [] for _ in range(BUCKET_COUNT)
    ]
    send_samples_us: list[float] = []

    try:
        dut.open()
        host, ready = rx.wait_ready(dut)
        emit("P4_2_LR600_DUT_IP", host)
        emit("P4_2_LR600_TCP_PORT", TCP_PORT)

        if rx.intval(ready, "TCP_RX_MAX_CHUNKS_PER_LOCK") != TCP_RX_MAX_CHUNKS:
            raise RuntimeError("P4_2_LR600_MAX_CHUNKS_MISMATCH")
        if ready.get("TCP_PROFILE_ENABLED") != "YES":
            raise RuntimeError("P4_2_LR600_ETHERNET_PROFILE_REQUIRED")
        if ready.get("PAYLOAD_VERIFY_ENABLED") != "NO":
            raise RuntimeError("P4_2_LR600_PERF_FNV_MUST_BE_OFF")
        if ready.get("SPI_CHUNK_PROFILE_ENABLED") != "NO":
            raise RuntimeError("P4_2_LR600_SPI_CHUNK_PROFILE_MUST_BE_OFF")

        sock = socket.create_connection((host, TCP_PORT), timeout=3.0)
        sock.settimeout(3.0)
        sock.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_SNDBUF,
            4 * 1024 * 1024,
        )

        sock.sendall(b"R")
        time.sleep(0.08)

        armed = dut.snapshot()
        if armed.get("MODE") != "TCP_RX":
            raise RuntimeError("P4_2_LR600_ARM_FAILED")

        dut.command_ack(b"R", b"H4A04P1_RESET=PASS")
        time.sleep(0.02)

        zero = dut.snapshot()
        zero_values = functional_zero_values(zero)
        hold_evidence = {
            field: zero.get(field, "UNAVAILABLE")
            for field in ZERO_ARM_HOLD_EVIDENCE_FIELDS
        }
        if (
            zero.get("MODE") != "TCP_RX"
            or not functional_zero_arm_pass(zero)
        ):
            raise RuntimeError(
                f"P4_2_LR600_ZERO_ARM_FAILED={zero_values}"
            )

        emit("P4_2_LR600_PREFLIGHT", "PASS")
        emit("P4_2_LR600_ZERO_ARM", "PASS")
        emit("P4_2_LR600_ZERO_ARM_HOLD_EVIDENCE", hold_evidence)

        # Prepare every host-side object before the final DUT reset.
        payload = bytes((index & 0xFF) for index in range(TCP_CHUNK))
        pc_bytes = 0
        pc_operations = 0

        # Final reset: after its ACK, start timing immediately. No snapshot,
        # readiness polling, Serial access or artificial wait is permitted.
        dut.command_ack(b"R", b"H4A04P1_RESET=PASS")
        start = time.perf_counter()
        deadline = start + PERF_DURATION_S

        while True:
            before_send = time.perf_counter()
            if before_send >= deadline:
                break

            operation_number = pc_operations + 1
            sampled = operation_number % SEND_SAMPLE_EVERY == 0
            sample_start_ns = time.perf_counter_ns() if sampled else 0

            try:
                sock.sendall(payload)
            except (socket.timeout, ConnectionError, OSError) as exc:
                raise IntegrityFailure(
                    "P4_2_LR600_TRANSPORT_RUNTIME_ERROR"
                ) from exc

            sample_end_ns = time.perf_counter_ns() if sampled else 0
            after_send = time.perf_counter()
            bucket_index = min(
                int((after_send - start) // BUCKET_DURATION_S),
                BUCKET_COUNT - 1,
            )

            pc_bytes += len(payload)
            pc_operations += 1
            bucket_bytes[bucket_index] += len(payload)
            bucket_operations[bucket_index] += 1

            if sampled:
                duration_us = (sample_end_ns - sample_start_ns) / 1000.0
                send_samples_us.append(duration_us)
                bucket_samples[bucket_index].append(duration_us)

        send_end = time.perf_counter()
        send_elapsed = send_end - start

        freeze_request, freeze_ack = dut.command_ack(
            b"F",
            b"H4A04P1_FREEZE=PASS",
        )
        freeze_request_tail_us = (freeze_request - send_end) * 1_000_000.0
        freeze_ack_tail_us = (freeze_ack - send_end) * 1_000_000.0

        measured = dut.snapshot()

        if measured.get("MODE") != "TCP_RX":
            raise IntegrityFailure("P4_2_LR600_FROZEN_MODE_INVALID")
        if measured.get("TCP_RX_FROZEN") != "YES":
            raise IntegrityFailure("P4_2_LR600_FROZEN_FLAG_MISSING")
        if measured.get("TCP_PROFILE_ENABLED") != "YES":
            raise IntegrityFailure("P4_2_LR600_PROFILE_SNAPSHOT_MISMATCH")
        if measured.get("PAYLOAD_VERIFY_ENABLED") != "NO":
            raise IntegrityFailure("P4_2_LR600_PERF_VERIFY_UNEXPECTED")
        if measured.get("SPI_CHUNK_PROFILE_ENABLED") != "NO":
            raise IntegrityFailure("P4_2_LR600_SPI_PROFILE_UNEXPECTED")

        dut_bytes = rx.intval(measured, "RX_BYTES")
        dut_operations = rx.intval(measured, "RX_OPERATIONS")
        transport_errors = rx.intval(measured, "TRANSPORT_ERRORS")
        lock_errors = rx.intval(measured, "TCP_SPI_LOCK_ERRORS")
        hold_count = rx.intval(measured, "TCP_SPI_HOLD_COUNT")
        hold_total_us = rx.intval(measured, "TCP_SPI_HOLD_TOTAL_US")
        hold_avg_us = rx.intval(measured, "TCP_SPI_HOLD_AVG_US")
        hold_max_us = rx.intval(measured, "TCP_SPI_HOLD_MAX_US")
        payload_calls = rx.intval(measured, "TCP_PROF_PAYLOAD_READ_CALLS")
        payload_total_us = rx.intval(
            measured,
            "TCP_PROF_PAYLOAD_READ_TOTAL_US",
        )
        payload_bytes = rx.intval(measured, "TCP_PROF_PAYLOAD_BYTES")

        if dut_bytes <= 0 or dut_operations <= 0:
            raise IntegrityFailure("P4_2_LR600_EMPTY_DUT_ACCOUNTING")
        if hold_count <= 0 or hold_total_us <= 0:
            raise IntegrityFailure("P4_2_LR600_EMPTY_SPI_ACCOUNTING")
        if payload_calls <= 0 or payload_total_us <= 0 or payload_bytes <= 0:
            raise IntegrityFailure("P4_2_LR600_EMPTY_PAYLOAD_PROFILE")
        if payload_bytes != dut_bytes:
            raise IntegrityFailure(
                "P4_2_LR600_PAYLOAD_RX_BYTE_MISMATCH "
                f"PAYLOAD={payload_bytes} RX={dut_bytes}"
            )
        if not send_samples_us:
            raise IntegrityFailure("P4_2_LR600_NO_SEND_SAMPLES")

        measured_window_us = send_elapsed * 1_000_000.0
        bucket_elapsed: list[float] = []
        bucket_mbps: list[float] = []

        for index in range(BUCKET_COUNT):
            interval_start = start + index * BUCKET_DURATION_S
            interval_end = min(
                send_end,
                start + (index + 1) * BUCKET_DURATION_S,
            )
            elapsed = max(0.0, interval_end - interval_start)
            bucket_elapsed.append(elapsed)
            bucket_mbps.append(
                rx.mbps(bucket_bytes[index], elapsed)
                if elapsed > 0.0
                else 0.0
            )

        valid_buckets = sum(
            1
            for index in range(BUCKET_COUNT)
            if (
                bucket_elapsed[index] >= BUCKET_DURATION_S - 0.001
                and bucket_operations[index] > 0
            )
        )

        bucket_median = float(statistics.median(bucket_mbps))
        if bucket_median <= 0.0:
            raise IntegrityFailure("P4_2_LR600_BUCKET_MEDIAN_INVALID")
        bucket_spread = (
            (max(bucket_mbps) - min(bucket_mbps))
            * 100.0
            / bucket_median
        )
        drift = delta_pct(bucket_mbps[-1], bucket_mbps[0])

        payload_mbps = payload_bytes * 8.0 / payload_total_us
        us_per_byte = payload_total_us / payload_bytes
        pc_mbps = rx.mbps(pc_bytes, send_elapsed)
        dut_window_mbps = rx.mbps(dut_bytes, send_elapsed)
        occupancy = hold_total_us * 100.0 / measured_window_us
        bytes_per_operation = dut_bytes / dut_operations
        holds_per_operation = hold_count / dut_operations
        hold_us_per_byte = hold_total_us / dut_bytes

        unexpected_resets = 0
        integrity_ok = (
            transport_errors == 0
            and lock_errors == 0
            and unexpected_resets == 0
        )
        complete = valid_buckets == BUCKET_COUNT

        if not integrity_ok or not complete:
            classification = "FAIL"
        elif (
            bucket_spread > BUCKET_SPREAD_MAX_PCT
            or abs(drift) > DRIFT_REVIEW_LIMIT_PCT
        ):
            classification = "REVIEW"
        else:
            classification = "PASS"

        return {
            "host": host,
            "duration_s": send_elapsed,
            "pc_bytes": pc_bytes,
            "pc_operations": pc_operations,
            "dut_bytes": dut_bytes,
            "dut_operations": dut_operations,
            "transport_errors": transport_errors,
            "lock_errors": lock_errors,
            "unexpected_resets": unexpected_resets,
            "hold_count": hold_count,
            "hold_total_us": hold_total_us,
            "hold_avg_us": hold_avg_us,
            "hold_max_us": hold_max_us,
            "payload_calls": payload_calls,
            "payload_bytes": payload_bytes,
            "payload_total_us": payload_total_us,
            "payload_mbps": payload_mbps,
            "us_per_byte": us_per_byte,
            "pc_mbps": pc_mbps,
            "dut_window_mbps": dut_window_mbps,
            "occupancy": occupancy,
            "bytes_per_operation": bytes_per_operation,
            "holds_per_operation": holds_per_operation,
            "hold_us_per_byte": hold_us_per_byte,
            "valid_buckets": valid_buckets,
            "bucket_bytes": bucket_bytes,
            "bucket_operations": bucket_operations,
            "bucket_elapsed": bucket_elapsed,
            "bucket_mbps": bucket_mbps,
            "bucket_samples": bucket_samples,
            "bucket_spread": bucket_spread,
            "drift": drift,
            "send_samples_us": send_samples_us,
            "freeze_request_tail_us": freeze_request_tail_us,
            "freeze_ack_tail_us": freeze_ack_tail_us,
            "classification": classification,
        }
    finally:
        if sock is not None:
            try:
                sock.close()
            except Exception:
                pass
        dut.close()


def report_result(result: dict[str, object]) -> None:
    bucket_elapsed = list(result["bucket_elapsed"])
    bucket_bytes = list(result["bucket_bytes"])
    bucket_operations = list(result["bucket_operations"])
    bucket_mbps = list(result["bucket_mbps"])
    bucket_samples = list(result["bucket_samples"])
    send_samples = list(result["send_samples_us"])

    for index in range(BUCKET_COUNT):
        prefix = f"P4_2_LR600_BUCKET_{index + 1:02d}"
        emit(f"{prefix}_ELAPSED_S", f"{bucket_elapsed[index]:.6f}")
        emit(f"{prefix}_PC_BYTES", bucket_bytes[index])
        emit(f"{prefix}_PC_OPERATIONS", bucket_operations[index])
        emit(f"{prefix}_PC_OFFERED_MBPS", f"{bucket_mbps[index]:.6f}")
        emit(f"{prefix}_PC_SEND_SAMPLES", len(bucket_samples[index]))

    emit("P4_2_LR600_DURATION_S", f"{float(result['duration_s']):.6f}")
    emit("P4_2_LR600_VALID_BUCKETS", result["valid_buckets"])
    emit("P4_2_LR600_TOTAL_PC_BYTES", result["pc_bytes"])
    emit("P4_2_LR600_TOTAL_DUT_RX_BYTES", result["dut_bytes"])
    emit("P4_2_LR600_RX_OPERATIONS", result["dut_operations"])
    emit("P4_2_LR600_PC_OPERATIONS", result["pc_operations"])

    emit("P4_2_LR600_PAYLOAD_MBPS", f"{float(result['payload_mbps']):.6f}")
    emit("P4_2_LR600_PC_MBPS", f"{float(result['pc_mbps']):.6f}")
    emit(
        "P4_2_LR600_DUT_WINDOW_MBPS",
        f"{float(result['dut_window_mbps']):.6f}",
    )
    emit("P4_2_LR600_BUCKET_METRIC", "PC_OFFERED_THROUGHPUT")
    emit("P4_2_LR600_BUCKET_DUT_PAYLOAD", "NOT_DERIVED_NO_PERIODIC_SNAPSHOT")
    emit("P4_2_LR600_BUCKET_MIN_MBPS", f"{min(bucket_mbps):.6f}")
    emit(
        "P4_2_LR600_BUCKET_MEDIAN_MBPS",
        f"{statistics.median(bucket_mbps):.6f}",
    )
    emit("P4_2_LR600_BUCKET_MAX_MBPS", f"{max(bucket_mbps):.6f}")
    emit(
        "P4_2_LR600_BUCKET_SPREAD_PCT",
        f"{float(result['bucket_spread']):.6f}",
    )
    emit("P4_2_LR600_FIRST_MINUTE_MBPS", f"{bucket_mbps[0]:.6f}")
    emit("P4_2_LR600_LAST_MINUTE_MBPS", f"{bucket_mbps[-1]:.6f}")
    emit("P4_2_LR600_DRIFT_PCT", f"{float(result['drift']):.6f}")

    emit("P4_2_LR600_US_PER_BYTE", f"{float(result['us_per_byte']):.9f}")
    emit(
        "P4_2_LR600_RX_BYTES_PER_OPERATION",
        f"{float(result['bytes_per_operation']):.6f}",
    )
    emit("P4_2_LR600_PAYLOAD_PROFILE_CALLS", result["payload_calls"])
    emit("P4_2_LR600_PAYLOAD_PROFILE_BYTES", result["payload_bytes"])
    emit("P4_2_LR600_PAYLOAD_PROFILE_TOTAL_US", result["payload_total_us"])

    emit("P4_2_LR600_SPI_HOLD_COUNT", result["hold_count"])
    emit("P4_2_LR600_SPI_HOLD_TOTAL_US", result["hold_total_us"])
    emit("P4_2_LR600_SPI_HOLD_AVG_US", result["hold_avg_us"])
    emit("P4_2_LR600_SPI_HOLD_MAX_US", result["hold_max_us"])
    emit(
        "P4_2_LR600_SPI_HOLD_OCCUPANCY_PCT",
        f"{float(result['occupancy']):.6f}",
    )
    emit(
        "P4_2_LR600_SPI_HOLDS_PER_RX_OPERATION",
        f"{float(result['holds_per_operation']):.6f}",
    )
    emit(
        "P4_2_LR600_SPI_HOLD_US_PER_RX_BYTE",
        f"{float(result['hold_us_per_byte']):.9f}",
    )
    emit("P4_2_LR600_SPI_HOLD_P95_US", "NOT_AVAILABLE")
    emit("P4_2_LR600_SPI_HOLD_P99_US", "NOT_AVAILABLE")

    emit("P4_2_LR600_PC_SEND_METRIC", "HOST_SEND_BACKPRESSURE")
    emit("P4_2_LR600_PC_SEND_SAMPLE_EVERY", SEND_SAMPLE_EVERY)
    emit("P4_2_LR600_PC_SEND_SAMPLES", len(send_samples))
    emit(
        "P4_2_LR600_PC_SEND_AVG_US",
        f"{statistics.fmean(send_samples):.6f}",
    )
    emit("P4_2_LR600_PC_SEND_P50_US", f"{percentile(send_samples, 0.50):.6f}")
    emit("P4_2_LR600_PC_SEND_P95_US", f"{percentile(send_samples, 0.95):.6f}")
    emit("P4_2_LR600_PC_SEND_P99_US", f"{percentile(send_samples, 0.99):.6f}")
    emit("P4_2_LR600_PC_SEND_MAX_US", f"{max(send_samples):.6f}")

    emit("P4_2_LR600_TRANSPORT_ERRORS", result["transport_errors"])
    emit("P4_2_LR600_TCP_SPI_LOCK_ERRORS", result["lock_errors"])
    emit("P4_2_LR600_UNEXPECTED_RESETS", result["unexpected_resets"])
    emit(
        "P4_2_LR600_UNEXPECTED_RESET_EVIDENCE",
        "CONTINUOUS_SOCKET_AND_FINAL_FROZEN_SNAPSHOT",
    )
    emit(
        "P4_2_LR600_FREEZE_REQUEST_TAIL_US",
        f"{float(result['freeze_request_tail_us']):.3f}",
    )
    emit(
        "P4_2_LR600_FREEZE_ACK_TAIL_US",
        f"{float(result['freeze_ack_tail_us']):.3f}",
    )

    emit(
        "DELTA_VS_P4_2_SHORT_PAYLOAD_PCT",
        f"{delta_pct(float(result['payload_mbps']), P4_2_SHORT_PAYLOAD_MBPS):.6f}",
    )
    emit(
        "DELTA_VS_PRE_P4_2_PAYLOAD_PCT",
        f"{delta_pct(float(result['payload_mbps']), PRE_P4_2_PAYLOAD_MBPS):.6f}",
    )
    emit(
        "DELTA_VS_P4_2_SHORT_US_PER_BYTE_PCT",
        f"{delta_pct(float(result['us_per_byte']), P4_2_SHORT_US_PER_BYTE):.6f}",
    )
    emit(
        "DELTA_VS_PRE_P4_2_US_PER_BYTE_PCT",
        f"{delta_pct(float(result['us_per_byte']), PRE_P4_2_US_PER_BYTE):.6f}",
    )
    emit("P4_2_LR600_HISTORICAL_TCP_SECONDARY_MBPS", P4_2_SHORT_TCP_SECONDARY_MBPS)
    emit("P4_2_LR600_CLASSIFICATION", result["classification"])


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 P4.2 COPY_OUT 64 B default RAW TCP RX 600 s soak"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--arduino-cli")
    parser.add_argument(
        "--fqbn",
        default="jwplc_local:esp32:jwplcbasic",
    )
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[3]
    sketch = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "firmware"
        / "a14_h4a04p1_tcp_rx_profiler"
    )
    firmware = sketch / "a14_h4a04p1_tcp_rx_profiler.ino"
    runner = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "pc"
        / "a14_h4a04p1_tcp_rx_case.py"
    )
    contract = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "gates"
        / "a14_package_promotion_contract.py"
    )
    libraries = repo / "JWPLC" / "2.1.0" / "libraries"

    print("=" * 78)
    print(" A14 P4.2 - COPY_OUT 64 B DEFAULT - RAW LONG RUN 600 S")
    print("=" * 78)

    emit("P4_2_LR600_SERIAL", args.serial)
    emit("P4_2_LR600_DURATION_TARGET_S", PERF_DURATION_S)
    emit("P4_2_LR600_BUCKET_COUNT", BUCKET_COUNT)
    emit("P4_2_LR600_BUCKET_DURATION_S", BUCKET_DURATION_S)
    emit("P4_2_LR600_CONNECTION_POLICY", "SINGLE_CONTINUOUS")
    emit("P4_2_LR600_SERIAL_POLICY", "QUIET")
    emit("P4_2_LR600_BUCKET_POLICY", "10X60S_PC_SIDE")
    emit("P4_2_LR600_LATENCY_POLICY", "SAMPLED_PC_SEND_BACKPRESSURE")
    emit("P4_2_LR600_TCP_CHUNK", TCP_CHUNK)
    emit("P4_2_LR600_PAYLOAD_PATTERN", "4096B_INCREMENTING_00_FF")
    emit("P4_2_LR600_SPI_CHUNK_PROFILE", "OFF")
    emit(
        "P4_2_LR600_ETHERNET_PROFILE_HOOKS",
        "ON_STRICTLY_FOR_COMPARABLE_AGGREGATE_PAYLOAD_COST",
    )
    emit("P4_2_LR600_RAW_PC_SEND_IS_H3ER_LATENCY", "NO")

    validate_zero_arm_regression()

    # Mandatory environment preflight precedes CLI discovery and every build.
    preflight_pyserial()

    branch = p3.git(repo, "branch", "--show-current")
    head = p3.git(repo, "rev-parse", "HEAD")
    emit("P4_2_LR600_BRANCH", branch)
    emit("P4_2_LR600_HEAD", head)

    if branch != BRANCH:
        raise RuntimeError("P4_2_LR600_BRANCH_MISMATCH")
    if p3.git(repo, "status", "--porcelain"):
        raise RuntimeError("P4_2_LR600_TREE_NOT_CLEAN")

    audit_source_contract(repo)
    audit_firmware_contract(firmware)

    ast.parse(runner.read_text(encoding="utf-8"), filename=str(runner))
    emit("P4_2_LR600_RUNNER_AST", "PASS")

    contract_run = p3.run([sys.executable, "-B", str(contract)], repo)
    print(p3.decode(contract_run.stdout))
    if contract_run.returncode != 0:
        raise RuntimeError("P4_2_LR600_PACKAGE_CONTRACT_FAILED")

    cli = p3.find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_p4_2_lr600_")
    )
    emit("P4_2_LR600_RESULT_ROOT", result_root)

    verify_build = result_root / "build_verify_default"
    perf_build = result_root / "build_perf_default"

    compile_default(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        libraries=libraries,
        build_dir=verify_build,
        log=result_root / "compile_verify_default.log",
        verify=True,
    )
    compile_default(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        libraries=libraries,
        build_dir=perf_build,
        log=result_root / "compile_perf_default.log",
        verify=False,
    )

    upload(
        cli=cli,
        fqbn=args.fqbn,
        serial_port=args.serial,
        build_dir=verify_build,
        sketch=sketch,
        repo=repo,
        log=result_root / "verify_upload.log",
        label="VERIFY",
    )
    time.sleep(3.0)

    verify_text = run_verify_case(
        runner=runner,
        repo=repo,
        serial_port=args.serial,
        log=result_root / "verify_case.log",
    )
    if p3.one(verify_text, "H4A04P1_PAYLOAD_VERIFY_ENABLED") != "YES":
        raise RuntimeError("P4_2_LR600_VERIFY_NOT_ACTIVE")

    verify_bytes = p3.integer(verify_text, "H4A04P1_DUT_RX_BYTES")
    fnv_actual = p3.integer(verify_text, "H4A04P1_RX_FNV1A32")
    fnv_expected = p3.fnv1a_expected(verify_bytes)
    emit("P4_2_LR600_VERIFY_RX_BYTES", verify_bytes)
    emit("P4_2_LR600_FNV_ACTUAL", fnv_actual)
    emit("P4_2_LR600_FNV_EXPECTED", fnv_expected)

    if fnv_actual != fnv_expected:
        raise IntegrityFailure("P4_2_LR600_FNV_MISMATCH")
    emit("P4_2_LR600_VERIFY_FNV", "PASS")

    upload(
        cli=cli,
        fqbn=args.fqbn,
        serial_port=args.serial,
        build_dir=perf_build,
        sketch=sketch,
        repo=repo,
        log=result_root / "perf_upload.log",
        label="PERF",
    )
    time.sleep(3.0)

    rx = load_rx_runner(runner)
    result = run_long_case(rx=rx, serial_port=args.serial)
    report_result(result)
    emit("P4_2_LR600_PHYSICAL_RUN", "COMPLETE")

    summary_log = result_root / "SUMMARY.log"
    summary_log.write_text(
        "\n".join(
            (
                f"P4_2_LR600_CLASSIFICATION={result['classification']}",
                f"HEAD={head}",
                "COPY_OUT_SOURCE=PACKAGE_DEFAULT",
                "COPY_OUT_DEFAULT=1",
                "SERIAL_POLICY=QUIET_DURING_600S",
                "BUCKET_POLICY=10X60S_PC_SIDE",
                "SPI_TELEMETRY=FINAL_SNAPSHOT_ONLY",
                f"VERIFY_FNV_ACTUAL={fnv_actual}",
                f"VERIFY_FNV_EXPECTED={fnv_expected}",
                f"VALID_BUCKETS={result['valid_buckets']}",
                f"TRANSPORT_ERRORS={result['transport_errors']}",
                f"TCP_SPI_LOCK_ERRORS={result['lock_errors']}",
                f"UNEXPECTED_RESETS={result['unexpected_resets']}",
            )
        )
        + "\n",
        encoding="utf-8",
    )
    emit("P4_2_LR600_SUMMARY_LOG", summary_log)
    return 0 if result["classification"] in ("PASS", "REVIEW") else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except IntegrityFailure as exc:
        emit("P4_2_LR600_CLASSIFICATION", "FAIL")
        emit("P4_2_LR600_INTEGRITY_FAILURE", str(exc))
        raise SystemExit(2) from exc
