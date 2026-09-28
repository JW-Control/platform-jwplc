#!/usr/bin/env python3
import argparse
import hashlib
import re
import subprocess
from pathlib import Path

EXPECTED = {
    "tft": "5D860A131811DD9A7EB6FA55F5674B1D78B0DE7DFAF8748CE18A60CEED2D3738",
    "display": "52B9BC617FACB77705161B4F07E6D45571043E4473934EFE19A1F5444BB5D986",
    "modbus_rtu": "486BE38AE088B94898E516FFBC125855F22C2EC5EE8A6FA9E10F35D7CAC3A3BE",
    "core": "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566",
    "sd": "E75BDE36481BF621DB37300ADEA7CF0D4A89B73ECE442A218135BD3E8E8AA5C1",
}

EXPECTED_DIRTY = sorted([
    "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a",
    "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a",
])

REQUIRED_QUALIFICATION_MARKERS = [
    "TCP_FULL_RUNTIME_PASS=YES",
    "MASTER_RUNTIME_PASS=YES",
    "RTU_MASTER_PASS=YES",
    "RTU_SLAVE_PASS=YES",
    "RTU_CROSS_COUNT_PASS=YES",
    "RTU_SNAPSHOT_TAIL_TOLERANCE=0",
    "RTU_FINAL_SNAPSHOT_MODE=QUIESCED",
    "A14_P5B_AUTOMATED=PASS",
]

MASTER_EXPECTATIONS = {
    "FULL_RUNTIME_READY": "YES",
    "ETH_READY": "YES",
    "ETH_LINK": "UP",
    "DISPLAY_READY": "YES",
    "FRAM_READY": "YES",
    "FRAM_FAILS": "0",
    "SD_READY": "YES",
    "SD_DATALOG_ACTIVE": "YES",
    "SD_DATALOG_FAILED_COMMITS": "0",
    "RTC_PRESENT": "YES",
    "RTC_UNAVAILABLE": "0",
    "RTC_STALE": "0",
    "IO_INITIALIZED": "YES",
    "IO_STALE": "0",
    "BUTTONS_READY": "YES",
    "BUTTON_NOT_READY": "0",
    "SPI_PROBE_FAILS": "0",
    "PERIPHERAL_FAILURE_COUNT": "0",
    "RTU_READY": "YES",
    "RTU_REQUESTS_FAILED": "0",
    "RTU_VERIFY_FAILS": "0",
    "RTU_CRC_ERRORS": "0",
    "RTU_MASTER_TIMEOUTS": "0",
    "RTU_LAST_ERROR": "OK",
}

SLAVE_EXPECTATIONS = {
    "SLAVE_READY": "YES",
    "RTU_READY": "YES",
    "RTU_ROLE": "SLAVE",
    "RTU_SLAVE_ID": "2",
    "RTU_BAUD": "115200",
    "DISPLAY_READY": "YES",
    "RTU_CRC_ERRORS": "0",
    "RTU_EXCEPTIONS_SENT": "0",
    "RTU_LAST_ERROR": "OK",
    "SLAVE_HR1": "21930",
}


def fail(message: str) -> None:
    print("A14_H3E5_EXISTING_RUN_REVALIDATION=FAIL")
    print(f"H3E5R_FAILURE={message}")
    raise SystemExit(1)


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def git(repo: Path, *args: str) -> str:
    proc = subprocess.run(
        ["git", "-C", str(repo), *args],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode != 0:
        fail(f"GIT_FAILED={' '.join(args)}:{proc.stderr.strip()}")
    return proc.stdout.strip()


def find_repo_root(start: Path) -> Path:
    probe = start.resolve()
    while True:
        if (probe / ".git").exists():
            return probe
        if probe.parent == probe:
            fail("REPO_ROOT_NOT_FOUND")
        probe = probe.parent


def read_text(path: Path) -> str:
    if not path.is_file():
        fail(f"MISSING_FILE={path}")
    return path.read_text(encoding="utf-8", errors="replace")


def library_selected(lines, name: str) -> bool:
    verbose = re.compile(rf"^Using library {re.escape(name)} at version ")
    table = re.compile(rf"^\s*{re.escape(name)}\s+\S+\s+.+[\\/]{re.escape(name)}\s*$")
    return any(verbose.search(line) or table.search(line) for line in lines)


def library_precompiled(lines, name: str) -> bool:
    marker = re.compile(
        r"Using precompiled library|Usando libreria precompilada|Usando biblioteca precompilada"
    )
    return any(name in line and marker.search(line) for line in lines)


def source_object_counts(build_path: Path):
    if not build_path.is_dir():
        fail(f"MISSING_BUILD_PATH={build_path}")

    counts = {
        "display": 0,
        "tft": 0,
        "tft_espi": 0,
        "modbus_rtu": 0,
    }
    for path in build_path.rglob("*.o"):
        if path.name == "JWPLC_TFT.cpp.o":
            counts["tft"] += 1
        if path.name == "TFT_eSPI.cpp.o":
            counts["tft_espi"] += 1
        if path.name == "JWPLC_ModbusRTU.cpp.o":
            counts["modbus_rtu"] += 1
        normalized = path.as_posix()
        if "/libraries/JWPLC_Display/" in normalized and path.name.endswith(".cpp.o"):
            counts["display"] += 1
    return counts


def assert_build(label: str, compile_log: Path, build_path: Path) -> None:
    text = read_text(compile_log)
    lines = text.splitlines()

    display_selected = library_selected(lines, "JWPLC_Display")
    tft_selected = library_selected(lines, "JWPLC_TFT")
    modbus_selected = library_selected(lines, "JWPLC_ModbusRTU")

    display_precompiled = library_precompiled(lines, "JWPLC_Display")
    tft_precompiled = library_precompiled(lines, "JWPLC_TFT")
    modbus_precompiled = library_precompiled(lines, "JWPLC_ModbusRTU")

    external_tft_espi = library_selected(lines, "TFT_eSPI")
    counts = source_object_counts(build_path)

    print(f"H3E5R_{label}_DISPLAY_SELECTED={display_selected}")
    print(f"H3E5R_{label}_DISPLAY_PRECOMPILED={display_precompiled}")
    print(f"H3E5R_{label}_JWPLC_TFT_SELECTED={tft_selected}")
    print(f"H3E5R_{label}_JWPLC_TFT_PRECOMPILED={tft_precompiled}")
    print(f"H3E5R_{label}_MODBUS_RTU_SELECTED={modbus_selected}")
    print(f"H3E5R_{label}_MODBUS_RTU_PRECOMPILED={modbus_precompiled}")
    print(f"H3E5R_{label}_EXTERNAL_TFT_ESPI_SELECTED={external_tft_espi}")
    print(f"H3E5R_{label}_DISPLAY_SOURCE_OBJECT_COUNT={counts['display']}")
    print(f"H3E5R_{label}_JWPLC_TFT_SOURCE_OBJECT_COUNT={counts['tft']}")
    print(f"H3E5R_{label}_TFT_ESPI_SOURCE_OBJECT_COUNT={counts['tft_espi']}")
    print(f"H3E5R_{label}_MODBUS_RTU_SOURCE_OBJECT_COUNT={counts['modbus_rtu']}")

    if not all([
        display_selected,
        tft_selected,
        modbus_selected,
        display_precompiled,
        tft_precompiled,
        modbus_precompiled,
    ]):
        fail(f"{label}_BUILD_SELECTION_OR_PRECOMPILED_POLICY")
    if external_tft_espi:
        fail(f"{label}_EXTERNAL_TFT_ESPI_SELECTED")
    if any(counts.values()):
        fail(f"{label}_SOURCE_OBJECT_POLICY")

    print(f"H3E5R_{label}_BUILD_POLICY=PASS")


def parse_snapshot(path: Path):
    values = {}
    for line in read_text(path).splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if key in values:
            fail(f"SNAPSHOT_DUPLICATE_KEY={path.name}:{key}")
        values[key] = value
    return values


def assert_snapshot(label: str, path: Path, expectations) -> None:
    values = parse_snapshot(path)
    for key, expected in expectations.items():
        actual = values.get(key)
        print(f"H3E5R_{label}_{key}={actual}")
        if actual != expected:
            fail(f"{label}_{key}_EXPECTED_{expected}_GOT_{actual}")
    print(f"H3E5R_{label}_SNAPSHOT=PASS")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Revalida offline artifacts existentes de A14 H3E.5 sin compilar ni tocar hardware."
    )
    parser.add_argument("--temp-root", required=True)
    args = parser.parse_args()

    temp_root = Path(args.temp_root).resolve()
    if not temp_root.is_dir():
        fail(f"TEMP_ROOT_NOT_FOUND={temp_root}")

    repo_root = find_repo_root(Path(__file__).parent)
    branch = git(repo_root, "branch", "--show-current")
    head = git(repo_root, "rev-parse", "HEAD")

    print("============================================================")
    print(" A14 H3E.5R - EXISTING RUN OFFLINE REVALIDATION")
    print(" NO COMPILE / NO UPLOAD / NO HARDWARE EXECUTION")
    print("============================================================")
    print(f"H3E5R_REPO_ROOT={repo_root}")
    print(f"H3E5R_BRANCH={branch}")
    print(f"H3E5R_HEAD={head}")
    print(f"H3E5R_TEMP_ROOT={temp_root}")

    if branch != "v2.1.0-alpha.14/feature/modbus-tcp":
        fail(f"WRONG_BRANCH={branch}")

    paths = {
        "master_compile": temp_root / "compile_master.log",
        "slave_compile": temp_root / "compile_slave.log",
        "qualification": temp_root / "qualification.log",
        "master_snapshot": temp_root / "master_final.txt",
        "slave_snapshot": temp_root / "slave_final.txt",
        "master_build": temp_root / "build_master",
        "slave_build": temp_root / "build_slave",
    }
    for key, path in paths.items():
        exists = path.is_dir() if key.endswith("_build") else path.is_file()
        print(f"H3E5R_ARTIFACT_{key.upper()}={exists}")
        if not exists:
            fail(f"MISSING_ARTIFACT={key}:{path}")

    assert_build("MASTER", paths["master_compile"], paths["master_build"])
    assert_build("SLAVE", paths["slave_compile"], paths["slave_build"])

    qualification = read_text(paths["qualification"])
    for marker in REQUIRED_QUALIFICATION_MARKERS:
        present = marker in qualification
        print(f"H3E5R_QUALIFICATION_MARKER={marker}:{present}")
        if not present:
            fail(f"QUALIFICATION_MARKER_MISSING={marker}")
    print("H3E5R_QUALIFICATION=PASS")

    assert_snapshot("MASTER", paths["master_snapshot"], MASTER_EXPECTATIONS)
    assert_snapshot("SLAVE", paths["slave_snapshot"], SLAVE_EXPECTATIONS)

    archive_paths = {
        "tft": repo_root / "JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a",
        "display": repo_root / "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a",
        "modbus_rtu": repo_root / "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a",
        "core": repo_root / "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a",
        "sd": repo_root / "JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a",
    }
    for key, path in archive_paths.items():
        if not path.is_file():
            fail(f"ARCHIVE_MISSING={key}:{path}")
        actual = sha256(path)
        print(f"H3E5R_{key.upper()}_SHA256={actual}")
        if actual != EXPECTED[key]:
            fail(f"{key.upper()}_SHA_MISMATCH")
    print("H3E5R_ARCHIVE_INVARIANTS=PASS")

    dirty = sorted(
        line.replace("\\", "/")
        for line in git(repo_root, "diff", "--name-only").splitlines()
        if line.strip()
    )
    staged = [
        line for line in git(repo_root, "diff", "--cached", "--name-only").splitlines()
        if line.strip()
    ]
    print(f"H3E5R_DIRTY_COUNT={len(dirty)}")
    for path in dirty:
        print(f"H3E5R_DIRTY={path}")
    print(f"H3E5R_STAGED_COUNT={len(staged)}")

    if dirty != EXPECTED_DIRTY:
        fail("DIRTY_SCOPE_INVALID")
    if staged:
        fail("INDEX_NOT_CLEAN")
    print("H3E5R_DIRTY_SCOPE=PASS")

    print("H3E5R_COMPILES=NO")
    print("H3E5R_UPLOADS=NO")
    print("H3E5R_HARDWARE_EXECUTION=NO")
    print("H3E5R_REPOSITORY_MUTATION=NO")
    print("A14_H3E5_EXISTING_RUN_REVALIDATION=PASS")
    print("NEXT=RETURN_OUTPUT_TO_CHAT_FOR_H3E_CLOSURE")


if __name__ == "__main__":
    main()
