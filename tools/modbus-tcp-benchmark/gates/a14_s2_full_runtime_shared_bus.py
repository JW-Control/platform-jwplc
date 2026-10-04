#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ast
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
DURATION_S = 600


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def decode(data: bytes) -> str:
    return data.decode("utf-8", errors="replace")


def run(cmd: list[str], cwd: Path) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def git(repo: Path, *args: str) -> str:
    result = run(["git", *args], repo)
    if result.returncode != 0:
        raise RuntimeError(
            f"S2_GIT_FAILED {' '.join(args)} {decode(result.stdout)[-1400:]}"
        )
    return decode(result.stdout).strip()


def find_cli(explicit: str | None) -> Path:
    candidates: list[Path] = []
    if explicit:
        candidates.append(Path(explicit))

    found = shutil.which("arduino-cli")
    if found:
        candidates.append(Path(found))

    candidates.append(
        Path(r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe")
    )

    for candidate in candidates:
        if candidate.is_file():
            return candidate.resolve()

    raise RuntimeError("S2_ARDUINO_CLI_NOT_FOUND")


def values(text: str, key: str) -> list[str]:
    return [
        m.strip()
        for m in re.findall(
            rf"(?m)^{re.escape(key)}=(.*)\r?$",
            text,
        )
    ]


def one(text: str, key: str) -> str:
    found = values(text, key)
    if len(found) != 1:
        raise RuntimeError(f"S2_KEY_COUNT_{key}={len(found)}")
    return found[0]


def integer(text: str, key: str) -> int:
    return int(one(text, key))


def compile_role(
    *,
    cli: Path,
    fqbn: str,
    repo: Path,
    sketch: Path,
    libraries: Path,
    build_dir: Path,
    role_master: bool,
    sd_mode_datalog: bool,
    fifo_mode: str,
    log: Path,
) -> None:
    role = "MASTER" if role_master else "SLAVE"
    role_value = "1" if role_master else "0"
    sd_mode_value = "1" if (role_master and sd_mode_datalog) else "0"

    extra_flag_parts = [
        f"-DJWPLC_S2_ROLE_MASTER={role_value}",
        f"-DJWPLC_S2_SD_MODE_DATALOG={sd_mode_value}",
        "-DJWPLC_W5500_RX_DIRECT_TRANSFER_BYTES=0",
    ]

    if fifo_mode == "force-on":
        extra_flag_parts.append("-DJWPLC_W5500_RX_FIFO_REUSE=1")
    elif fifo_mode != "default":
        raise RuntimeError(f"S2_UNKNOWN_FIFO_MODE={fifo_mode}")

    extra_flags = " ".join(extra_flag_parts)

    result = run(
        [
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
            f"compiler.cpp.extra_flags={extra_flags}",
            str(sketch),
        ],
        repo,
    )
    log.write_bytes(result.stdout)

    emit(f"S2_{role}_COMPILE_EXIT", result.returncode)

    if result.returncode != 0:
        print(decode(result.stdout)[-10000:])
        raise RuntimeError(f"S2_{role}_COMPILE_FAILED")

    normalized = decode(result.stdout).replace("\\", "/").lower()

    required = [
        f"-djwplc_s2_role_master={role_value}",
        f"-djwplc_s2_sd_mode_datalog={sd_mode_value}",
        "-djwplc_w5500_rx_direct_transfer_bytes=0",
        "ethernetclient.cpp",
        "socket.cpp",
        "w5100.cpp",
        "spi.cpp",
        "jwplc_idlescreen.cpp",
        "jwplc_tft.cpp",
    ]

    if fifo_mode == "force-on":
        required.append("-djwplc_w5500_rx_fifo_reuse=1")
    else:
        if "-djwplc_w5500_rx_fifo_reuse=" in normalized:
            raise RuntimeError(
                f"S2_{role}_FIFO_DEFAULT_WAS_OVERRIDDEN"
            )

    for token in required:
        ok = token in normalized
        emit(
            f"S2_{role}_TOKEN_{token.replace('=', '_')}",
            "PASS" if ok else "FAIL",
        )
        if not ok:
            raise RuntimeError(
                f"S2_{role}_COMPILE_TOKEN_MISSING={token}"
            )

    for archive in (
        "libjwplc_display.a",
        "libjwplc_tft.a",
        "libspi.a",
    ):
        if archive in normalized:
            raise RuntimeError(
                f"S2_{role}_UNEXPECTED_ARCHIVE={archive}"
            )


def upload_role(
    *,
    cli: Path,
    fqbn: str,
    repo: Path,
    sketch: Path,
    build_dir: Path,
    port: str,
    role: str,
    log: Path,
) -> None:
    result = run(
        [
            str(cli),
            "upload",
            "--fqbn",
            fqbn,
            "--port",
            port,
            "--input-dir",
            str(build_dir),
            str(sketch),
        ],
        repo,
    )
    log.write_bytes(result.stdout)
    emit(f"S2_{role}_UPLOAD_EXIT", result.returncode)

    if result.returncode != 0:
        print(decode(result.stdout)[-6000:])
        raise RuntimeError(f"S2_{role}_UPLOAD_FAILED")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 S2 full-runtime Master/Slave shared-bus gate"
    )
    parser.add_argument("--master", default="COM14")
    parser.add_argument("--slave", default="COM4")
    parser.add_argument("--duration", type=int, default=DURATION_S)
    parser.add_argument("--arduino-cli")
    parser.add_argument(
        "--fqbn",
        default="jwplc_local:esp32:jwplcbasic",
    )
    parser.add_argument(
        "--sd-mode",
        choices=("direct", "datalog"),
        default="direct",
        help="Política SD del Master; direct reproduce S2, datalog usa JWPLCDataLog.",
    )
    parser.add_argument(
        "--fifo-mode",
        choices=("force-on", "default"),
        default="force-on",
        help="force-on reproduce gates previos; default prueba el comportamiento promovido del package.",
    )
    parser.add_argument(
        "--promotion-short",
        action="store_true",
        help="Permite duración de 120 s para revalidación corta de promoción.",
    )
    parser.add_argument(
        "--defer-physical-review",
        action="store_true",
    )
    args = parser.parse_args()

    minimum_duration = 120 if args.promotion_short else 600
    if args.duration < minimum_duration:
        raise RuntimeError(
            f"S2_DURATION_MUST_BE_AT_LEAST_{minimum_duration}_SECONDS"
        )

    if args.promotion_short:
        if args.sd_mode != "datalog":
            raise RuntimeError("S2_PROMOTION_SHORT_REQUIRES_DATALOG")
        if args.fifo_mode != "default":
            raise RuntimeError("S2_PROMOTION_SHORT_REQUIRES_FIFO_DEFAULT")

    repo = Path(__file__).resolve().parents[3]
    sketch = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "firmware"
        / "a14_s2_full_runtime_shared_bus"
    )
    runner = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "pc"
        / "a14_s2_full_runtime_shared_bus_case.py"
    )
    libraries = repo / "JWPLC" / "2.1.0" / "libraries"
    contract = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "gates"
        / "a14_package_promotion_contract.py"
    )

    print("=" * 78)
    print(" A14 S2 - FULL RUNTIME MASTER/SLAVE SHARED-BUS SOAK")
    print("=" * 78)

    branch = git(repo, "branch", "--show-current")
    head = git(repo, "rev-parse", "HEAD")

    emit("S2_BRANCH", branch)
    emit("S2_HEAD", head)
    emit("S2_MASTER_PORT", args.master)
    emit("S2_SLAVE_PORT", args.slave)
    emit("S2_DURATION_S", args.duration)
    emit("S2_SD_MODE_REQUESTED", args.sd_mode.upper())
    emit("S2_FIFO_MODE_REQUESTED", args.fifo_mode.upper().replace("-", "_"))
    emit("S2_PROMOTION_SHORT", "YES" if args.promotion_short else "NO")
    if args.promotion_short:
        only_variable = "FIFO_REUSE_ACTIVATION_SOURCE_FORCED_VS_PACKAGE_DEFAULT"
    else:
        only_variable = (
            "SD_ACCESS_POLICY"
            if args.sd_mode == "datalog"
            else "BASELINE_DIRECT_SD"
        )
    emit("S2_ONLY_VARIABLE", only_variable)
    emit("S2_MODBUS_PROFILE", "115200_8N1_SLAVE_ID_2")
    emit("S2_ETH_SPI_HZ", 26000000)
    emit(
        "S2_FIFO_REUSE",
        "PACKAGE_DEFAULT" if args.fifo_mode == "default" else "ON_FOR_GATE",
    )
    emit("S2_DIRECT_RX", "OFF")
    emit("S2_RX_BATCH_POLICY", "FAIRNESS_CONSERVATIVE")
    emit("S2_MODBUS_FUNCTIONS", "FC15_FC01_FC02")
    emit("S2_PHYSICAL_OUTPUT_RELAYS", "NOT_DRIVEN")
    emit("S2_PRODUCT_SOURCE", "JWPLC/2.1.0_CANONICAL_PACKAGE")
    emit("S2_TEMP_PRODUCT_PATCHES", "NO")

    if branch != BRANCH:
        raise RuntimeError("S2_BRANCH_MISMATCH")

    tree_status = git(repo, "status", "--porcelain")
    if tree_status:
        emit("S2_TREE_CLEAN", "NO")
        print("S2_DIRTY_BEGIN")
        print(tree_status)
        print("S2_DIRTY_END")
        raise RuntimeError("S2_TREE_NOT_CLEAN")

    emit("S2_TREE_CLEAN", "YES")

    ast.parse(
        runner.read_text(encoding="utf-8"),
        filename=str(runner),
    )
    emit("S2_RUNNER_AST", "PASS")

    contract_result = run(
        [sys.executable, "-B", str(contract)],
        repo,
    )
    print(decode(contract_result.stdout))
    if contract_result.returncode != 0:
        raise RuntimeError("S2_PACKAGE_CONTRACT_FAILED")

    cli = find_cli(args.arduino_cli)
    emit("ARDUINO_CLI", cli)

    result_root = Path(
        tempfile.mkdtemp(prefix="jwplc_a14_s2_full_runtime_")
    )
    master_build = result_root / "build_master"
    slave_build = result_root / "build_slave"

    emit("S2_RESULT_ROOT", result_root)

    compile_role(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        libraries=libraries,
        build_dir=slave_build,
        role_master=False,
        sd_mode_datalog=False,
        fifo_mode=args.fifo_mode,
        log=result_root / "compile_slave.log",
    )
    compile_role(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        libraries=libraries,
        build_dir=master_build,
        role_master=True,
        sd_mode_datalog=(args.sd_mode == "datalog"),
        fifo_mode=args.fifo_mode,
        log=result_root / "compile_master.log",
    )

    upload_role(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        build_dir=slave_build,
        port=args.slave,
        role="SLAVE",
        log=result_root / "upload_slave.log",
    )
    upload_role(
        cli=cli,
        fqbn=args.fqbn,
        repo=repo,
        sketch=sketch,
        build_dir=master_build,
        port=args.master,
        role="MASTER",
        log=result_root / "upload_master.log",
    )

    print()
    print("Durante el soak: mantén pulsado OK ~1 s una vez en MASTER y SLAVE.")
    print("Observa también que ambas TFT sigan actualizando sin congelarse.")
    print()

    case_log = result_root / "case.log"
    case_result = run(
        [
            sys.executable,
            "-B",
            "-u",
            str(runner),
            "--master",
            args.master,
            "--slave",
            args.slave,
            "--duration",
            str(args.duration),
        ],
        repo,
    )
    case_log.write_bytes(case_result.stdout)
    case_text = decode(case_result.stdout)

    print(case_text)
    emit("S2_CASE_EXIT", case_result.returncode)
    emit("S2_CASE_LOG", case_log)

    if case_result.returncode != 0:
        raise RuntimeError("S2_RUNTIME_FAILED")

    if one(case_text, "S2_GATE_DATA") != "PASS":
        raise RuntimeError("S2_DATA_NOT_PASS")

    zero_keys = (
        "S2_ETH_CORRUPTION_ERRORS",
        "S2_ETH_TRANSPORT_ERRORS",
        "S2_ETH_SPI_LOCK_ERRORS",
        "S2_MODBUS_FAILURES",
        "S2_MODBUS_PATTERN_MISMATCHES",
        "S2_MODBUS_CRC_ERRORS",
        "S2_MODBUS_TIMEOUTS",
        "S2_FRAM_FAIL",
        "S2_RTC_FAIL",
        "S2_SD_FAIL",
        "S2_LONG_LOOP_CRITICAL",
        "S2_SLAVE_MODBUS_CRC_ERRORS",
        "S2_SLAVE_FRAM_FAIL",
        "S2_SLAVE_RTC_FAIL",
        "S2_SLAVE_LONG_LOOP_CRITICAL",
        "S2_MASTER_DEVICE_RESETS",
        "S2_SLAVE_DEVICE_RESETS",
    )

    for key in zero_keys:
        if integer(case_text, key) != 0:
            raise RuntimeError(
                f"S2_NONZERO_{key}={one(case_text, key)}"
            )

    positive_keys = (
        "S2_ETH_RX_BYTES",
        "S2_ETH_ECHO_BYTES",
        "S2_MODBUS_CYCLES_OK",
        "S2_FRAM_OK",
        "S2_RTC_OK",
        "S2_SD_OK",
        "S2_IO_SAMPLES",
        "S2_SLAVE_MODBUS_RX_FRAMES",
        "S2_SLAVE_MODBUS_TX_FRAMES",
        "S2_SLAVE_FRAM_OK",
        "S2_SLAVE_RTC_OK",
        "S2_SLAVE_IO_SAMPLES",
    )

    for key in positive_keys:
        if integer(case_text, key) <= 0:
            raise RuntimeError(f"S2_NOT_POSITIVE_{key}")

    if args.sd_mode == "datalog":
        if one(case_text, "S2_SD_MODE") != "DATALOG":
            raise RuntimeError("S2_DATALOG_MODE_NOT_ACTIVE")
        if one(case_text, "S2_DATALOG_ACTIVE") != "YES":
            raise RuntimeError("S2_DATALOG_NOT_ACTIVE")
        if one(case_text, "S2_DATALOG_MANUAL_SERVICE_CALL") != "NO":
            raise RuntimeError("S2_DATALOG_MANUAL_SERVICE_FORBIDDEN")
        for key in (
            "S2_DATALOG_ACCEPTED_WRITES",
            "S2_DATALOG_ACCEPTED_BYTES",
            "S2_DATALOG_COMMITTED_BYTES",
            "S2_DATALOG_COMMIT_COUNT",
        ):
            if integer(case_text, key) <= 0:
                raise RuntimeError(f"S2_DATALOG_NOT_POSITIVE_{key}")
        if integer(case_text, "S2_DATALOG_FAILED_COMMITS") != 0:
            raise RuntimeError(
                "S2_DATALOG_FAILED_COMMITS="
                f"{one(case_text, 'S2_DATALOG_FAILED_COMMITS')}"
            )
    else:
        if one(case_text, "S2_SD_MODE") != "DIRECT":
            raise RuntimeError("S2_DIRECT_SD_MODE_NOT_ACTIVE")

    if args.promotion_short:
        emit("S2_MASTER_BUTTON_PHYSICAL", "INHERITED_FROM_S2D_PASS")
        emit("S2_SLAVE_BUTTON_PHYSICAL", "INHERITED_FROM_S2D_PASS")
    else:
        if integer(case_text, "S2_BUTTON_DOWN_SAMPLES") <= 0:
            raise RuntimeError(
                "S2_MASTER_BUTTON_NOT_OBSERVED_HOLD_OK_FOR_1S"
            )

        if integer(case_text, "S2_SLAVE_BUTTON_DOWN_SAMPLES") <= 0:
            raise RuntimeError(
                "S2_SLAVE_BUTTON_NOT_OBSERVED_HOLD_OK_FOR_1S"
            )

    if one(case_text, "S2_LINK_FINAL") != "UP":
        raise RuntimeError("S2_ETH_LINK_NOT_UP")

    if integer(case_text, "S2_ETH_SPI_HOLD_MAX_US") > 10000:
        raise RuntimeError(
            "S2_ETH_SPI_HOLD_EXCEEDED_10MS "
            f"value={one(case_text, 'S2_ETH_SPI_HOLD_MAX_US')}"
        )

    if args.promotion_short:
        physical = "INHERITED_FROM_S2D_PASS"
        gate = "PASS"
    elif args.defer_physical_review:
        physical = "PENDING_USER"
        gate = "PASS_DATA_ONLY"
    else:
        answer = input(
            "¿Ambas TFT permanecieron activas/estables y sin congelarse? (S/N): "
        ).strip().upper()
        if answer != "S":
            raise RuntimeError("S2_TFT_PHYSICAL_FAILED")
        physical = "PASS"
        gate = "PASS"

    emit("S2_TFT_PHYSICAL", physical)
    emit("S2_BUTTONS_PHYSICAL", "PASS")
    emit("S2_PHYSICAL_STABILITY", physical)

    summary = result_root / "SUMMARY.log"
    summary.write_text(
        "\n".join(
            [
                f"A14_S2_FULL_RUNTIME_SHARED_BUS={gate}",
                f"HEAD={head}",
                f"MASTER_PORT={args.master}",
                f"SLAVE_PORT={args.slave}",
                f"DURATION_S={args.duration}",
                f"ETH_RX_MBPS={one(case_text, 'S2_ETH_RX_MBPS')}",
                f"ETH_ECHO_TX_MBPS={one(case_text, 'S2_ETH_ECHO_TX_MBPS')}",
                f"ETH_SPI_HOLD_MAX_US={one(case_text, 'S2_ETH_SPI_HOLD_MAX_US')}",
                f"MODBUS_CYCLES_OK={one(case_text, 'S2_MODBUS_CYCLES_OK')}",
                f"MODBUS_MAX_TRANSACTION_US={one(case_text, 'S2_MODBUS_MAX_TRANSACTION_US')}",
                f"FRAM_OK={one(case_text, 'S2_FRAM_OK')}",
                f"RTC_OK={one(case_text, 'S2_RTC_OK')}",
                f"SD_MODE={one(case_text, 'S2_SD_MODE')}",
                f"SD_OK={one(case_text, 'S2_SD_OK')}",
                f"DATALOG_ACCEPTED_WRITES={one(case_text, 'S2_DATALOG_ACCEPTED_WRITES')}",
                f"DATALOG_COMMITTED_BYTES={one(case_text, 'S2_DATALOG_COMMITTED_BYTES')}",
                f"DATALOG_COMMIT_COUNT={one(case_text, 'S2_DATALOG_COMMIT_COUNT')}",
                f"DATALOG_FAILED_COMMITS={one(case_text, 'S2_DATALOG_FAILED_COMMITS')}",
                f"MASTER_MAX_LOOP_US={one(case_text, 'S2_MAX_LOOP_US')}",
                f"SLAVE_MAX_LOOP_US={one(case_text, 'S2_SLAVE_MAX_LOOP_US')}",
                f"PHYSICAL_STABILITY={physical}",
                f"FIFO_REUSE_GATE={args.fifo_mode}",
                "FIFO_REUSE_DEFAULT=ON",
                "HARNESS_FAILURE=NO",
                "PRODUCT_FAILURE=NO_EVIDENCE",
                "HARDWARE_FAILURE=NO_EVIDENCE",
                (
                    "NEXT=RETURN_TO_CHAT_COMPARE_DATALOG_JITTER"
                    if args.sd_mode == "datalog"
                    else
                    "NEXT=RETURN_TO_CHAT_PROMOTION_DECISION_BEFORE_P4_1"
                ),
                "",
            ]
        ),
        encoding="utf-8",
    )

    emit("S2_SUMMARY_LOG", summary)
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit("A14_S2_FULL_RUNTIME_SHARED_BUS", gate)
    if args.promotion_short:
        emit("A14_FIFO_REUSE_DEFAULT_PROMOTION", gate)
    if args.sd_mode == "datalog":
        emit("A14_S2D_DATALOG_FULL_RUNTIME", gate)
        emit("NEXT", "RETURN_TO_CHAT_COMPARE_DATALOG_JITTER")
    else:
        emit("NEXT", "RETURN_TO_CHAT_PROMOTION_DECISION_BEFORE_P4_1")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:
        emit("HARNESS_FAILURE", "UNCLASSIFIED")
        emit("PRODUCT_FAILURE", "UNCLASSIFIED")
        emit("HARDWARE_FAILURE", "UNCLASSIFIED")
        emit("S2_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("S2_EXCEPTION", str(exc))
        raise SystemExit(1)
