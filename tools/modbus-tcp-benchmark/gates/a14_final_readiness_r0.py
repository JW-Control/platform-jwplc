#!/usr/bin/env python3
import hashlib
import re
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

EXPECTED_BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
FQBN = "jwplc_local:esp32:jwplcbasic"

EXPECTED_ARTIFACTS = {
    "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a":
        "4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566",
    "JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a":
        "E75BDE36481BF621DB37300ADEA7CF0D4A89B73ECE442A218135BD3E8E8AA5C1",
    "JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a":
        "52B9BC617FACB77705161B4F07E6D45571043E4473934EFE19A1F5444BB5D986",
    "JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a":
        "5D860A131811DD9A7EB6FA55F5674B1D78B0DE7DFAF8748CE18A60CEED2D3738",
    "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a":
        "486BE38AE088B94898E516FFBC125855F22C2EC5EE8A6FA9E10F35D7CAC3A3BE",
}

# Frozen by A14 NB3 global closure. R0 verifies these product files did not
# change after the physical/functional NB3 evidence was collected.
EXPECTED_NB3_ETHERNET = {
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp":
        "5D67E2FE8F2333A9A8AAD2A290A26DBAE92761DF11661C4305F8EC9D09604A60",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h":
        "B92718E94D549D60A7F2E648DAAD2A18DF4FAB94936BFEB279025911949AAFED",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp":
        "6ADD2904893DA8C3A61C19FC91BF146C40038834458BC9F3084339462C9A9770",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp":
        "F5AE79FA9E9642A670D0AF23E029621711D2DD857B776A5208CF2866EDB496E5",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp":
        "CC8EEE779FC932C5A8FA0175ABBF0421AA8C95279B4260FADAC1276E7BD74E1C",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h":
        "C1C5615DFF167F3572FBEA85CFFE1A7F5051732E2BD446387F1025A1D981FB47",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h":
        "7A5923105CFEEF07CD4BACEB72854397B9E9389772F97DF6B9DF3AB5A4D0F15A",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp":
        "6E55F494F5E43B6BCF327F40C268AB6FF8F739331C96C328E9A0BEC0DA38654A",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp":
        "9F94AAC1BB25966C18EDFD6A5C5D5908A9DBD4CF11B6E9BC099E10B28CEF272F",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h":
        "9A833532C44E0CFCD66429A764E8BDBF62A8871838B0355565BC0A63043455FC",
}

SKETCHES = {
    "MODBUS_TCP_SERVER": "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/examples/01.ModbusTCP_Server",
    "MODBUS_TCP_CLIENT": "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/examples/02.ModbusTCP_Client",
    "FULL_RUNTIME_MASTER": "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master",
    "RTU_SLAVE": "tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave",
}


def fail(message: str) -> None:
    print(f"A14_R0_FAILURE={message}")
    print("A14_FINAL_READINESS_R0=FAIL")
    raise SystemExit(1)


def run_git(repo: Path, *args: str, allowed=(0,)) -> subprocess.CompletedProcess:
    proc = subprocess.run(
        ["git", "-C", str(repo), *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode not in allowed:
        stderr = decode_output(proc.stderr).strip()
        fail(f"GIT_{'_'.join(args[:2]).upper()}_EXIT_{proc.returncode}:{stderr}")
    return proc


def decode_output(data: bytes) -> str:
    if not data:
        return ""
    if data.startswith(b"\xef\xbb\xbf"):
        return data.decode("utf-8-sig", errors="replace")
    if data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
        return data.decode("utf-16", errors="replace")

    sample = data[:4096]
    if sample:
        pairs = max(1, len(sample) // 2)
        even_nuls = sample[0::2].count(0)
        odd_nuls = sample[1::2].count(0)
        if odd_nuls / pairs > 0.30 and even_nuls / pairs < 0.05:
            return data.decode("utf-16-le", errors="replace")
        if even_nuls / pairs > 0.30 and odd_nuls / pairs < 0.05:
            return data.decode("utf-16-be", errors="replace")

    for encoding in ("utf-8", "cp1252"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace")


def sha256(path: Path) -> str:
    if not path.is_file():
        fail(f"MISSING_FILE={path}")
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest().upper()


def find_repo_root() -> Path:
    proc = subprocess.run(
        ["git", "-C", str(Path(__file__).resolve().parent), "rev-parse", "--show-toplevel"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode != 0:
        fail("REPO_ROOT_NOT_FOUND")
    return Path(decode_output(proc.stdout).strip()).resolve()


def read_text(path: Path) -> str:
    if not path.is_file():
        fail(f"MISSING_TEXT_FILE={path}")
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


def source_object_count(build_path: Path, filename: str) -> int:
    return sum(1 for p in build_path.rglob(filename) if p.is_file())


def compile_sketch(
    cli: Path,
    repo: Path,
    temp_root: Path,
    label: str,
    relative_dir: str,
) -> dict:
    sketch_dir = repo / relative_dir
    if not sketch_dir.is_dir():
        fail(f"{label}_SKETCH_DIR_MISSING={sketch_dir}")

    build_path = temp_root / f"build_{label.lower()}"
    log_path = temp_root / f"compile_{label.lower()}.log"
    build_path.mkdir(parents=True, exist_ok=True)

    args = [
        str(cli),
        "compile",
        "--fqbn", FQBN,
        "--build-path", str(build_path),
        "--libraries", str(repo / "JWPLC/2.1.0/libraries"),
        str(sketch_dir),
    ]

    start = time.monotonic()
    proc = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    elapsed = time.monotonic() - start
    text = decode_output(proc.stdout)
    log_path.write_text(text, encoding="utf-8")

    print(f"A14_R0_{label}_COMPILE_EXIT={proc.returncode}")
    print(f"A14_R0_{label}_COMPILE_SECONDS={elapsed:.3f}")
    print(f"A14_R0_{label}_COMPILE_LOG={log_path}")

    if proc.returncode != 0:
        tail = "\n".join(text.splitlines()[-120:])
        print(tail)
        fail(f"{label}_COMPILE_FAILED")

    bins = list(build_path.rglob("*.bin"))
    print(f"A14_R0_{label}_BIN_COUNT={len(bins)}")
    if not bins:
        fail(f"{label}_BIN_MISSING")

    return {
        "text": text,
        "lines": text.splitlines(),
        "build": build_path,
        "log": log_path,
    }


def assert_repo_library_used(repo: Path, result: dict, label: str, library: str) -> None:
    selected = library_selected(result["lines"], library)
    expected_path = str(repo / "JWPLC/2.1.0/libraries" / library)
    repo_path_seen = expected_path.lower() in result["text"].lower()

    print(f"A14_R0_{label}_{library}_SELECTED={selected}")
    print(f"A14_R0_{label}_{library}_REPO_PATH={repo_path_seen}")

    if not selected or not repo_path_seen:
        fail(f"{label}_{library}_SELECTION_INVALID")


def assert_full_runtime_precompiled(result: dict, label: str) -> None:
    for library, obj in (
        ("JWPLC_Display", "JWPLC_Display.cpp.o"),
        ("JWPLC_TFT", "JWPLC_TFT.cpp.o"),
        ("JWPLC_ModbusRTU", "JWPLC_ModbusRTU.cpp.o"),
    ):
        selected = library_selected(result["lines"], library)
        precompiled = library_precompiled(result["lines"], library)
        obj_count = source_object_count(result["build"], obj)

        print(f"A14_R0_{label}_{library}_SELECTED={selected}")
        print(f"A14_R0_{label}_{library}_PRECOMPILED={precompiled}")
        print(f"A14_R0_{label}_{library}_SOURCE_OBJECT_COUNT={obj_count}")

        if not selected or not precompiled or obj_count != 0:
            fail(f"{label}_{library}_PRECOMPILED_POLICY")

    external_tft = library_selected(result["lines"], "TFT_eSPI")
    external_obj_count = source_object_count(result["build"], "TFT_eSPI.cpp.o")
    print(f"A14_R0_{label}_EXTERNAL_TFT_ESPI_SELECTED={external_tft}")
    print(f"A14_R0_{label}_EXTERNAL_TFT_ESPI_OBJECT_COUNT={external_obj_count}")

    if external_tft or external_obj_count != 0:
        fail(f"{label}_EXTERNAL_TFT_ESPI_SELECTED")


def main() -> None:
    repo = find_repo_root()

    print("============================================================")
    print(" A14 R0 - FINAL READINESS STATIC + ARDUINO CLI")
    print(" NO UPLOAD / NO HARDWARE / NO PRODUCT MUTATION")
    print("============================================================")

    branch = decode_output(run_git(repo, "branch", "--show-current").stdout).strip()
    head = decode_output(run_git(repo, "rev-parse", "HEAD").stdout).strip()
    print(f"A14_R0_BRANCH={branch}")
    print(f"A14_R0_HEAD={head}")

    if branch != EXPECTED_BRANCH:
        fail(f"WRONG_BRANCH={branch}")

    unstaged = [
        x for x in decode_output(run_git(repo, "diff", "--name-only").stdout).splitlines()
        if x.strip()
    ]
    staged = [
        x for x in decode_output(run_git(repo, "diff", "--cached", "--name-only").stdout).splitlines()
        if x.strip()
    ]
    untracked = [
        x for x in decode_output(
            run_git(repo, "ls-files", "--others", "--exclude-standard").stdout
        ).splitlines() if x.strip()
    ]

    print(f"A14_R0_TRACKED_DIRTY_COUNT={len(unstaged)}")
    print(f"A14_R0_STAGED_COUNT={len(staged)}")
    print(f"A14_R0_UNTRACKED_COUNT={len(untracked)}")

    if unstaged or staged:
        fail("TRACKED_WORKTREE_NOT_CLEAN")

    diff_check = run_git(repo, "diff", "--check")
    cached_check = run_git(repo, "diff", "--cached", "--check")
    if decode_output(diff_check.stdout).strip() or decode_output(cached_check.stdout).strip():
        fail("GIT_DIFF_CHECK_OUTPUT_NOT_EMPTY")
    print("A14_R0_GIT_DIFF_CHECK=PASS")

    conflicts = run_git(
        repo,
        "grep", "-n", "-E", r"^(<<<<<<< |>>>>>>> )", "--",
        "JWPLC/2.1.0",
        "tools/modbus-tcp-benchmark",
        "docs/v2.1.0-alpha.14",
        allowed=(0, 1),
    )
    conflict_text = decode_output(conflicts.stdout).strip()
    print(f"A14_R0_CONFLICT_MARKER_MATCHES={0 if not conflict_text else len(conflict_text.splitlines())}")
    if conflicts.returncode == 0 and conflict_text:
        print(conflict_text)
        fail("CONFLICT_MARKERS_FOUND")

    upstream_ref = f"origin/{EXPECTED_BRANCH}"
    ahead_behind = decode_output(
        run_git(repo, "rev-list", "--left-right", "--count", f"HEAD...{upstream_ref}").stdout
    ).strip().split()
    if len(ahead_behind) != 2:
        fail("UPSTREAM_COUNT_PARSE_FAILED")
    ahead, behind = map(int, ahead_behind)
    print(f"A14_R0_LOCAL_AHEAD_OF_REMOTE={ahead}")
    print(f"A14_R0_LOCAL_BEHIND_REMOTE={behind}")
    if ahead != 0 or behind != 0:
        fail("LOCAL_REMOTE_NOT_SYNCHRONIZED")

    base_check = subprocess.run(
        ["git", "-C", str(repo), "merge-base", "--is-ancestor", "origin/release/v2.1.x", "HEAD"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    print(f"A14_R0_RELEASE_BASE_IS_ANCESTOR={base_check.returncode == 0}")
    if base_check.returncode != 0:
        fail("RELEASE_BASE_NOT_ANCESTOR")

    print("A14_R0_GIT_STATE=PASS")

    for relative, expected in EXPECTED_ARTIFACTS.items():
        actual = sha256(repo / relative)
        key = Path(relative).name.upper().replace(".", "_")
        print(f"A14_R0_ARTIFACT_{key}_SHA256={actual}")
        if actual != expected:
            fail(f"ARTIFACT_SHA_MISMATCH={relative}")
    print("A14_R0_ARTIFACT_INVARIANTS=PASS")

    for relative, expected in EXPECTED_NB3_ETHERNET.items():
        actual = sha256(repo / relative)
        if actual != expected:
            print(f"A14_R0_NB3_HASH_EXPECTED={relative}={expected}")
            print(f"A14_R0_NB3_HASH_ACTUAL={relative}={actual}")
            fail(f"NB3_ETHERNET_PRODUCT_CHANGED={relative}")
    print("A14_R0_ETHERNET_NB3_PRODUCT_HASHES=PASS")

    w5100_h = read_text(repo / "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h")
    boundary = w5100_h.find("#if defined(ARDUINO_ARCH_ARC32)")
    if boundary < 0:
        fail("SPI_BASE_BOUNDARY_NOT_FOUND")
    base_region = w5100_h[:boundary]
    spi_matches = re.findall(
        r"(?m)^[ \t]*#define[ \t]+SPI_ETHERNET_SETTINGS[ \t]+"
        r"SPISettings\((\d+),[ \t]*MSBFIRST,[ \t]*SPI_MODE0\)[ \t]*$",
        base_region,
    )
    print(f"A14_R0_SPI_BASE_SETTINGS_COUNT={len(spi_matches)}")
    if spi_matches != ["26000000"]:
        fail(f"W5500_SPI_NOT_FROZEN_26MHZ={spi_matches}")
    print("A14_R0_W5500_SPI_HZ=26000000")

    modbus_tcp_cpp = read_text(
        repo / "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
    )
    core_main = read_text(repo / "JWPLC/2.1.0/cores/jwcontrol/main.cpp")
    master_sketch = read_text(
        repo / "tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master/a14_p5_full_runtime_master.ino"
    )

    provider_signature = 'extern "C" void jwplcModbusTCPLoopServiceCallback(void)'
    provider_count = modbus_tcp_cpp.count(provider_signature)
    provider_task_count = modbus_tcp_cpp.count("JWPLC_ModbusTCP.task();")
    core_hook_calls = core_main.count("jwplcModbusTCPLoopServiceCallback();")
    master_manual_calls = master_sketch.count("JWPLC_ModbusTCP.task();")

    print(f"A14_R0_MODBUS_TCP_PROVIDER_COUNT={provider_count}")
    print(f"A14_R0_MODBUS_TCP_PROVIDER_TASK_CALL_COUNT={provider_task_count}")
    print(f"A14_R0_CORE_AUTOSERVICE_HOOK_CALL_COUNT={core_hook_calls}")
    print(f"A14_R0_MASTER_MANUAL_TCP_TASK_CALL_COUNT={master_manual_calls}")

    if provider_count != 1:
        fail("MODBUS_TCP_AUTOSERVICE_PROVIDER_COUNT_INVALID")
    if provider_task_count < 1:
        fail("MODBUS_TCP_AUTOSERVICE_PROVIDER_TASK_MISSING")
    if core_hook_calls != 2:
        fail("CORE_AUTOSERVICE_HOOK_CALL_COUNT_INVALID")
    if master_manual_calls != 0:
        fail("MASTER_STILL_REQUIRES_MANUAL_MODBUS_TCP_TASK")

    print("A14_R0_MODBUS_TCP_AUTOSERVICE_CONTRACT=PASS")

    closure_doc = repo / "docs/v2.1.0-alpha.14/A14_H3E_CLOSURE_20260928.md"
    status_doc = repo / "docs/v2.1.0-alpha.14/ALPHA14_STATUS.md"
    if not closure_doc.is_file() or not status_doc.is_file():
        fail("FINAL_DOCUMENTATION_MISSING")
    print("A14_R0_DOCUMENTATION_PRESENT=PASS")

    cli_candidates = [
        Path(r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"),
    ]
    which_cli = shutil.which("arduino-cli")
    if which_cli:
        cli_candidates.append(Path(which_cli))
    cli = next((p for p in cli_candidates if p.is_file()), None)
    if cli is None:
        fail("ARDUINO_CLI_NOT_FOUND")

    print(f"A14_R0_ARDUINO_CLI={cli}")
    version_proc = subprocess.run(
        [str(cli), "version"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    version_text = decode_output(version_proc.stdout).strip().replace("\r", " ").replace("\n", " ")
    print(f"A14_R0_ARDUINO_CLI_VERSION={version_text}")
    if version_proc.returncode != 0:
        fail("ARDUINO_CLI_VERSION_FAILED")

    stamp = time.strftime("%Y%m%d_%H%M%S")
    temp_root = Path(tempfile.gettempdir()) / f"jwplc_a14_r0_{stamp}"
    temp_root.mkdir(parents=True, exist_ok=True)
    print(f"A14_R0_TEMP_ROOT={temp_root}")

    results = {}
    for label, relative in SKETCHES.items():
        results[label] = compile_sketch(cli, repo, temp_root, label, relative)

    assert_repo_library_used(repo, results["MODBUS_TCP_SERVER"], "MODBUS_TCP_SERVER", "JWPLC_ModbusTCP")
    assert_repo_library_used(repo, results["MODBUS_TCP_CLIENT"], "MODBUS_TCP_CLIENT", "JWPLC_ModbusTCP")
    assert_repo_library_used(repo, results["FULL_RUNTIME_MASTER"], "FULL_RUNTIME_MASTER", "JWPLC_ModbusTCP")
    assert_repo_library_used(repo, results["FULL_RUNTIME_MASTER"], "FULL_RUNTIME_MASTER", "JWPLC_ModbusRTU")
    assert_repo_library_used(repo, results["RTU_SLAVE"], "RTU_SLAVE", "JWPLC_ModbusRTU")

    server_tcp_objects = source_object_count(
        results["MODBUS_TCP_SERVER"]["build"], "JWPLC_ModbusTCP.cpp.o"
    )
    client_tcp_objects = source_object_count(
        results["MODBUS_TCP_CLIENT"]["build"], "JWPLC_ModbusTCP.cpp.o"
    )
    master_tcp_objects = source_object_count(
        results["FULL_RUNTIME_MASTER"]["build"], "JWPLC_ModbusTCP.cpp.o"
    )

    print(f"A14_R0_MODBUS_TCP_SERVER_SOURCE_OBJECT_COUNT={server_tcp_objects}")
    print(f"A14_R0_MODBUS_TCP_CLIENT_SOURCE_OBJECT_COUNT={client_tcp_objects}")
    print(f"A14_R0_FULL_RUNTIME_MASTER_MODBUS_TCP_SOURCE_OBJECT_COUNT={master_tcp_objects}")

    if server_tcp_objects != 1 or client_tcp_objects != 1 or master_tcp_objects != 1:
        fail("MODBUS_TCP_SOURCE_OBJECT_COUNT_INVALID")

    assert_full_runtime_precompiled(results["FULL_RUNTIME_MASTER"], "FULL_RUNTIME_MASTER")
    assert_full_runtime_precompiled(results["RTU_SLAVE"], "RTU_SLAVE")

    print("A14_R0_MODBUS_TCP_SERVER_CLI=PASS")
    print("A14_R0_MODBUS_TCP_CLIENT_CLI=PASS")
    print("A14_R0_FULL_RUNTIME_MASTER_CLI=PASS")
    print("A14_R0_RTU_SLAVE_CLI=PASS")

    # Recheck product identity and worktree after all compiles.
    for relative, expected in EXPECTED_ARTIFACTS.items():
        if sha256(repo / relative) != expected:
            fail(f"ARTIFACT_CHANGED_DURING_R0={relative}")

    final_unstaged = [
        x for x in decode_output(run_git(repo, "diff", "--name-only").stdout).splitlines()
        if x.strip()
    ]
    final_staged = [
        x for x in decode_output(run_git(repo, "diff", "--cached", "--name-only").stdout).splitlines()
        if x.strip()
    ]
    print(f"A14_R0_FINAL_TRACKED_DIRTY_COUNT={len(final_unstaged)}")
    print(f"A14_R0_FINAL_STAGED_COUNT={len(final_staged)}")
    if final_unstaged or final_staged:
        fail("REPOSITORY_MUTATED_DURING_R0")

    print("A14_R0_REPOSITORY_MUTATION=NO")
    print("A14_R0_UPLOADS=NO")
    print("A14_R0_HARDWARE_EXECUTION=NO")
    print("A14_R0_RESULT=PASS")
    print("A14_FINAL_READINESS_R0=PASS")
    print("NEXT=ARDUINO_IDE_FINAL_COMPILE_UPLOAD_GATE")


if __name__ == "__main__":
    main()
