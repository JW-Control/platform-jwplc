#!/usr/bin/env python3
"""Alpha13 G8-P0: baseline read-only de Modbus RTU y RS-485.

Sin cambios de producto, sin COM/upload, sin Git add/commit/push, sin FRAM.
Genera manifest, summary y logs de las compilaciones normales.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[3]
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
FQBN = "jwplc_local:esp32:jwplcbasic"
LIBRARIES = ROOT / "JWPLC/2.1.0/libraries"
RTU = LIBRARIES / "JWPLC_ModbusRTU"
RS485 = LIBRARIES / "JWPLC_RS485"
RTU_CPP = RTU / "src/JWPLC_ModbusRTU.cpp"
RS485_CPP = RS485 / "src/JWPLC_RS485.cpp"
RTU_A = RTU / "src/esp32/libJWPLC_ModbusRTU.a"
CORE_A = ROOT / "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
TFT_A = LIBRARIES / "JWPLC_TFT/src/esp32/libJWPLC_TFT.a"
FRAM_A = LIBRARIES / "JW_FRAM/src/esp32/libJW_FRAM.a"

EXPECTED = {
    CORE_A: "a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d",
    TFT_A: "4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc",
    FRAM_A: "df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299",
}
OFFICIAL_CLI_SHA = "1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571"
INPUTS = [
    RTU_CPP, RTU / "src/JWPLC_ModbusRTU.h",
    RS485_CPP, RS485 / "src/JWPLC_RS485.h",
    RTU_A, RTU / "library.properties", RS485 / "library.properties",
    CORE_A, TFT_A, FRAM_A,
]
CASES = [
    ("SLAVE", RTU / "examples/01.ModbusRTU_Slave_Holding"),
    ("MASTER", RTU / "examples/02.ModbusRTU_Master_Read"),
    ("RS485_STATUS", RS485 / "examples/03.RS485_Status"),
]
MANIFEST = {}
RESULTS = None
TEMP = None


def emit(key, val):
    print("G8_P0_" + key + "=" + str(val), flush=True)


def require(condition, reason):
    if not condition:
        raise RuntimeError(reason)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(part)
    return h.hexdigest()


def git(*args):
    proc = subprocess.run(["git", "-c", "core.quotepath=false", *args],
                          cwd=ROOT, capture_output=True)
    require(proc.returncode == 0, "GIT_" + proc.stderr.decode("utf-8", "replace")[-200:])
    return proc.stdout.decode("utf-8", "replace").strip()


def git_paths(*args):
    proc = subprocess.run(["git", "-c", "core.quotepath=false", *args, "-z"],
                          cwd=ROOT, capture_output=True)
    require(proc.returncode == 0, "GIT_PATHS_FAILED")
    return set(filter(None, proc.stdout.decode("utf-8", "replace").split("\0")))


def record():
    RESULTS.mkdir(parents=True, exist_ok=True)
    (RESULTS / "MANIFEST.json").write_text(
        json.dumps(MANIFEST, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    rows = ["G8_P0_RUN_ID=" + MANIFEST["run_id"],
            "G8_P0_STATUS=" + MANIFEST["status"]]
    rows += ["G8_P0_PHASE_" + k + "=" + v
             for k, v in MANIFEST["phases"].items()]
    for k in ("reason", "entry_head", "cli_sha256", "result_dir"):
        if k in MANIFEST:
            rows.append("G8_P0_" + k.upper() + "=" + str(MANIFEST[k]))
    (RESULTS / "SUMMARY.log").write_text("\n".join(rows) + "\n", encoding="utf-8")


def stage(name, state):
    MANIFEST["phases"][name] = state
    emit("PHASE", name + "_" + state)
    record()


def run(args, label, cwd=ROOT):
    proc = subprocess.run([str(a) for a in args], cwd=cwd,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          encoding="utf-8", errors="replace")
    log = proc.stdout or ""
    (RESULTS / (label + ".log")).write_text(
        "ARGS=" + repr([str(a) for a in args]) +
        "\nEXIT=" + str(proc.returncode) + "\n" + log, encoding="utf-8")
    emit(label + "_EXIT", proc.returncode)
    if proc.returncode:
        errors = [line for line in log.splitlines() if re.search(
            r"(error:|fatal error:|undefined reference|failed)", line, re.I)]
        for err in errors[-6:]:
            emit("DIAGNOSTIC", err[-250:])
    require(proc.returncode == 0, label + "_FAILED")
    return log


def preflight(cli):
    stage("PREFLIGHT", "RUNNING")
    require(Path.cwd().resolve() == ROOT, "RUN_FROM_REPOSITORY_ROOT")
    require(cli.is_absolute() and cli.is_file() and cli.name.lower() == "arduino-cli.exe",
            "ABSOLUTE_ARDUINO_CLI_REQUIRED")
    require("arduino plc ide tools" not in str(cli).lower(), "WRONG_CLI")
    require(sha(cli) == OFFICIAL_CLI_SHA, "CLI_SHA_CHANGED")
    require(git("branch", "--show-current") == BRANCH, "BRANCH_CHANGED")
    require(Path(git("rev-parse", "--show-toplevel")).resolve() == ROOT, "WRONG_ROOT")
    require(not git_paths("diff", "--name-only"), "PRODUCT_WORKTREE_DIRTY")
    require(not git_paths("diff", "--cached", "--name-only"), "STAGED_FILES_PRESENT")
    local = git("rev-parse", "HEAD")
    remote = git("rev-parse", "refs/remotes/origin/" + BRANCH)
    require(local == remote, "LOCAL_REMOTE_HEAD_NOT_EQUAL")
    for p in INPUTS:
        require(p.is_file(), "MISSING_FILE_" + str(p))
    for p, expected in EXPECTED.items():
        require(sha(p) == expected, "CLOSED_BASELINE_CHANGED_" + p.name)
    MANIFEST["entry_head"] = local
    MANIFEST["cli_sha256"] = sha(cli)
    MANIFEST["before_sha256"] = {str(p.relative_to(ROOT)): sha(p) for p in INPUTS}
    MANIFEST["normal_sources"] = "UNCHANGED"
    version = run([cli, "version"], "CLI_VERSION")
    require("Version: 1.5.1" in version, "CLI_VERSION_NOT_OFFICIAL_151")
    cores = run([cli, "core", "list"], "CLI_CORES")
    require(re.search(r"(?m)^\s*jwplc_local:esp32\s+2\.1\.0-dev(?:\s|$)",
                      cores), "LOCAL_JWPLC_CORE_NOT_FOUND")
    props_rtu = (RTU / "library.properties").read_text(encoding="utf-8")
    props_rs485 = (RS485 / "library.properties").read_text(encoding="utf-8")
    require(re.search(r"(?m)^precompiled=full\s*$", props_rtu),
            "MODBUS_RTU_NOT_PRECOMPILED_FULL")
    require(re.search(r"(?m)^precompiled\s*=", props_rs485) is None,
            "RS485_COMPILATION_POLICY_CHANGED")
    stage("PREFLIGHT", "PASS")


def source_diagnostics():
    stage("SOURCE_CONTRACT", "RUNNING")
    rtu = RTU_CPP.read_text(encoding="utf-8")
    rs485 = RS485_CPP.read_text(encoding="utf-8")
    # Solo read-only: a P0 le corresponde caracterizar el defecto existente.
    duplicate = bool(re.search(
        r"_stats\.rxBytes\+\+;\s*_stats\.rxBytes\+\+;\s*_rxBuffer\[_rxLength\+\+\]",
        rtu))
    # Limitar la inspección al último bucle no-bulk justo antes del
    # decodificador master. Evita que el scanner abarque el Slave por error.
    end_master_rx = rtu.find("uint16_t expectedLength = 0;")
    loops = list(re.finditer(
        r"else\s*\{\s*while \(JWPLC_RS485\.available\(\) > 0\)",
        rtu[:end_master_rx] if end_master_rx >= 0 else ""))
    master_block = (rtu[loops[-1].start():end_master_rx]
                    if loops and end_master_rx >= 0 else "")
    master_missing = bool(
        "_rxBuffer[_rxLength++] = (uint8_t)value;" in master_block and
        "_stats.rxBytes" not in master_block)
    unconditional_ready = bool(re.search(
        r"_serial->begin\(_baud,\s*_config,\s*_rxPin,\s*_txPin\);\s*"
        r"_ready\s*=\s*true\s*;", rs485))
    results = {
        "slave_nonbulk_rx_counter_double": duplicate,
        "master_nonbulk_rx_counter_missing": master_missing,
        "rs485_ready_without_uart_validation": unconditional_ready,
    }
    MANIFEST["source_observations"] = results
    for key, present in results.items():
        emit(key.upper(), "CONFIRMED" if present else "NOT_CONFIRMED")
    require(all(results.values()), "G8_SOURCE_BASELINE_CHANGED_REVIEW_SCOPE")
    stage("SOURCE_CONTRACT", "PASS")


def assert_selection(output, library, path, label):
    hits = re.findall(
        r"(?m)^Using library " + re.escape(library) +
        r" at version .+ in folder: (.+)$", output)
    require(len(hits) == 1 and
            Path(hits[0].strip()).resolve() == path.resolve(),
            "SELECTED_LIBRARY_NOT_PACKAGE_" + label + "_" + library)


def assert_rtu_archive_link(output, label):
    lines = [line.replace("\\\\", "\\") for line in output.splitlines()
             if "-lJWPLC_ModbusRTU" in line and ".elf" in line]
    require(len(lines) == 1, "RTU_LINK_COMMAND_NOT_UNIQUE_" + label)
    line = lines[0]
    require(re.search(r'(?<!\S)["\']?-lJWPLC_ModbusRTU(?=\s|["\']|$)', line),
            "RTU_LINK_FLAG_MISSING_" + label)
    found = re.findall(
        r'(?<!\S)["\']?-L["\']?([A-Za-z]:[\\/][^\s"\']+)', line)
    expected = RTU_A.parent.resolve()
    require(expected in [Path(v).resolve() for v in found],
            "RTU_LINK_PATH_MISMATCH_" + label)
    require(sha(RTU_A) == MANIFEST["before_sha256"][str(RTU_A.relative_to(ROOT))],
            "RTU_ARCHIVE_SHA_CHANGED_IN_BUILD")
    emit("RTU_ARCHIVE_LINK_" + label, "PASS_EXACT_L_PATH_L_FLAG_SHA")


def compilation(cli):
    stage("NORMAL_COMPILES", "RUNNING")
    results = []
    for label, sketch in CASES:
        require(sketch.is_dir(), "EXAMPLE_MISSING_" + label)
        output, build_path = None, TEMP / ("build_" + label.lower())
        output = run([cli, "compile", "--fqbn", FQBN, "-j", "0", "-v",
                      "--clean", "--build-path", build_path,
                      "--libraries", LIBRARIES, sketch], "COMPILE_" + label)
        assert_selection(output, "JWPLC_RS485", RS485, label)
        require(len(list(build_path.rglob("JWPLC_RS485.cpp.o"))) == 1,
                "RS485_NOT_COMPILED_FROM_SOURCE_" + label)
        require(not list(build_path.rglob("JWPLC_ModbusRTU.cpp.o")),
                "RTU_SOURCE_COMPILED_INSTEAD_OF_PRECOMPILED_" + label)
        if label != "RS485_STATUS":
            assert_selection(output, "JWPLC_ModbusRTU", RTU, label)
            assert_rtu_archive_link(output, label)
        results.append(label)
        emit("NORMAL_COMPILE_" + label, "PASS")
    MANIFEST["normal_compilations"] = results
    stage("NORMAL_COMPILES", "PASS")


def final_audit():
    stage("FINAL_AUDIT", "RUNNING")
    for path, original in MANIFEST["before_sha256"].items():
        require(sha(ROOT/path) == original, "PRODUCT_FILE_CHANGED_" + path)
    require(git("rev-parse", "HEAD") == MANIFEST["entry_head"],
            "GIT_HEAD_CHANGED")
    require(not git_paths("diff", "--name-only") and
            not git_paths("diff", "--cached", "--name-only"),
            "GIT_CHANGED_IN_P0")
    require(MANIFEST.get("normal_compilations") ==
            ["SLAVE", "MASTER", "RS485_STATUS"], "MISSING_COMPILATIONS")
    stage("FINAL_AUDIT", "PASS")


def main():
    global RESULTS, TEMP
    args = argparse.ArgumentParser(description=__doc__)
    args.add_argument("--arduino-cli", default=r"C:\Program Files\Arduino CLI\arduino-cli.exe")
    options = args.parse_args()
    rid = "g8_p0_" + datetime.now().strftime("%Y%m%d_%H%M%S") + "_" + uuid.uuid4().hex[:8]
    RESULTS = ROOT/"tools/alpha13/results"/rid
    TEMP = Path(tempfile.gettempdir())/("jwplc_a13_" + rid)
    require(not RESULTS.exists() and not TEMP.exists(), "RUN_ID_COLLISION")
    RESULTS.mkdir(parents=True)
    TEMP.mkdir(parents=True)
    MANIFEST.update({
        "schema": "A13-G8-P0-READONLY-1", "run_id": rid,
        "status": "REVIEW_STOP", "phases": {},
        "product_changed": False, "physical_upload": False,
        "serial_access": False, "fram_write": False,
        "git_commit": False, "git_push": False,
        "result_dir": str(RESULTS),
    })
    try:
        preflight(Path(options.arduino_cli).expanduser().resolve())
        source_diagnostics()
        compilation(Path(options.arduino_cli).expanduser().resolve())
        final_audit()
        MANIFEST["status"] = "P0_BASELINE_PASS_NO_PRODUCT_CHANGE"
        emit("STATUS", MANIFEST["status"])
        emit("RUN_ID", rid)
        emit("RESULTS", RESULTS)
        emit("PRODUCT_OR_HARDWARE_CHANGE", "NONE")
        record()
        return 0
    except Exception as exc:
        MANIFEST["status"] = "REVIEW_STOP"
        MANIFEST["reason"] = repr(exc)
        emit("STATUS", "REVIEW_STOP")
        emit("REASON", repr(exc))
        emit("RESULTS", RESULTS)
        try:
            record()
        except Exception:
            pass
        return 2


if __name__ == "__main__":
    sys.exit(main())
