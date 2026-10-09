#!/usr/bin/env python3
"""Alpha13 TFT-PRE6-P2A: guarded local product adoption; no flash or commit.

Three tracked files only: JWPLC_TFT.cpp, tft_setup.h, libJWPLC_TFT.a.
On any failure, restore exact original bytes and require a clean worktree.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime

BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
FQBN = "jwplc_local:esp32:jwplcbasic"
EXPECTED = {
    "old_tft_archive": "5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738",
    "new_tft_archive": "ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8",
    "old_cpp_gitblob": "2bdb55d504cfd2a1481ff561d33535d1740bb472",
    "old_setup_gitblob": "773f8123844f783bfc67c3123150c27f29f45d34",
    "new_cpp": "494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25",
    "new_setup": "8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1",
    "core_archive": "6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983",
    "display_archive": "c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02",
    "tft_obj": "03063a848b5d5e20d8010b7d0bc19189b7cc5be412489588c1b14ded66815adf",
    "backend_obj": "59b4050dd44b5f6db7a99b2050aee7ff076008eb28032d18d222f4cf57b06732",
}
NEW_BYTES = 1091990
REPO = Path(__file__).resolve().parents[3]
LIBS = REPO / "JWPLC" / "2.1.0" / "libraries"
TFT = LIBS / "JWPLC_TFT"
RELATIVE = (
    "JWPLC/2.1.0/libraries/JWPLC_TFT/src/JWPLC_TFT.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_TFT/src/tft_setup.h",
    "JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a",
)
RUNID = datetime.now().strftime("%Y%m%d_%H%M%S")
RUN_ROOT = REPO / "tools" / "alpha13" / "results" / ("tft_pre6_p2a_" + RUNID)
TEMP_ROOT = Path(tempfile.gettempdir()) / ("jwplc_a13_tft_pre6_p2a_" + RUNID)
BACKUP = TEMP_ROOT / "originals"
SUMMARY = RUN_ROOT / "SUMMARY.log"
phase = "PRECHECK"
changed = False
original_shas: dict[str, str] = {}


class GateError(Exception):
    pass


def emit(key: str, value: object) -> None:
    print(f"{key}={value}", flush=True)


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise GateError(reason)


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(args: list[str], *, cwd: Path | None = None, log: Path | None = None) -> tuple[int, str]:
    p = subprocess.run(args, cwd=str(cwd or REPO), stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT, text=True, errors="replace")
    output = p.stdout or ""
    if log:
        log.write_text(output, encoding="utf-8")
    return p.returncode, output


def git(*args: str) -> str:
    ret, output = run(["git", "-C", str(REPO), *args])
    require(ret == 0, "GIT_COMMAND_FAILED_" + args[0] + ":" + output[-350:])
    return output.strip()


def git_bytes(*args: str) -> bytes:
    p = subprocess.run(["git", "-C", str(REPO), *args], stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE)
    require(p.returncode == 0, "GIT_BYTES_FAILED_" + args[0])
    return p.stdout


def clean_worktree() -> bool:
    return git_bytes("status", "--porcelain=v1", "-z", "--untracked-files=normal") == b""


def tracked_bytes(relative: str) -> bytes:
    return git_bytes("show", "HEAD:" + relative)


def check_sha(path: Path, want: str, name: str) -> None:
    require(path.is_file(), name + "_NOT_FOUND")
    found = sha(path)
    require(found == want, name + "_SHA_MISMATCH:" + found)
    emit(name + "_SHA256", found)


def manifest_candidate() -> tuple[Path, Path, Path]:
    global phase
    phase = "P1B_MANIFEST"
    roots = sorted(Path(tempfile.gettempdir()).glob("jwplc_a13_tft_pre6_p1b_*"),
                   key=lambda p: p.name, reverse=True)
    emit("P1B_CANDIDATE_DIRECTORIES", len(roots))
    for root in roots:
        path = root / "P1B_MANIFEST.json"
        if not path.is_file():
            continue
        try:
            m = json.loads(path.read_text(encoding="utf-8-sig"))
        except (ValueError, OSError, UnicodeError):
            continue
        if not isinstance(m, dict):
            continue
        if (m.get("schema") != "a13-tft-pre6-p1b-archive-rebuild-v1"
            or m.get("status") != "PASS"
            or m.get("source_cpp_sha256") != EXPECTED["new_cpp"]
            or m.get("source_setup_sha256") != EXPECTED["new_setup"]
            or m.get("source_st7789_sha256") != "44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc"
            or m.get("tft_object_sha256") != EXPECTED["tft_obj"]
            or m.get("backend_object_sha256") != EXPECTED["backend_obj"]
            or m.get("archive_sha256") != EXPECTED["new_tft_archive"]
            or m.get("archive_bytes") != NEW_BYTES
            or m.get("member_parity") != "PASS"
            or m.get("normal_cases_pass") != 3
            or m.get("product_mutated") is not False
            or m.get("upload_executed") is not False):
            continue
        cand_archive = Path(str(m.get("archive_path", "")))
        candidate_root = (root / "libraries" / "JWPLC_TFT" / "src" / "esp32" / "libJWPLC_TFT.a")
        if cand_archive.resolve() != candidate_root.resolve():
            continue
        source_root = Path(str(m.get("source_p1a_root", "")))
        if not source_root.is_dir() or not source_root.name.startswith("jwplc_a13_tft_pre6_p1a_"):
            continue
        if source_root.parent.resolve() != Path(tempfile.gettempdir()).resolve():
            continue
        cpp = source_root / "libraries" / "JWPLC_TFT" / "src" / "JWPLC_TFT.cpp"
        setup = source_root / "libraries" / "JWPLC_TFT" / "src" / "tft_setup.h"
        if not (cpp.is_file() and setup.is_file() and candidate_root.is_file()):
            continue
        if sha(cpp) != EXPECTED["new_cpp"] or sha(setup) != EXPECTED["new_setup"]:
            continue
        if sha(candidate_root) != EXPECTED["new_tft_archive"]:
            continue
        if candidate_root.stat().st_size != NEW_BYTES:
            continue
        emit("P1B_MANIFEST_SELECTED", path)
        emit("P1A_CANONICAL_SOURCE_ROOT", source_root)
        return cpp, setup, candidate_root
    raise GateError("VERIFIED_P1B_MANIFEST_AND_BINARY_NOT_FOUND")


def check_initial() -> None:
    global phase
    phase = "PRECHECK"
    require(git("branch", "--show-current") == BRANCH, "BRANCH_MISMATCH")
    require(clean_worktree(), "WORKTREE_MUST_BE_CLEAN")
    require(git("diff", "--check") == "", "GIT_DIFF_CHECK_FAILED")
    for relative in RELATIVE:
        require(git("ls-files", "--error-unmatch", "--", relative) == relative,
                "PRODUCT_FILE_NOT_TRACKED:" + relative)
    for relative, blobkey in ((RELATIVE[0], "old_cpp_gitblob"), (RELATIVE[1], "old_setup_gitblob")):
        data = tracked_bytes(relative)
        blob = hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()
        require(blob == EXPECTED[blobkey], "OLD_GIT_BLOB_MISMATCH:" + relative)
        original_shas[relative] = sha(REPO / relative)
    check_sha(REPO / RELATIVE[2], EXPECTED["old_tft_archive"], "ORIGINAL_TFT_ARCHIVE")
    original_shas[RELATIVE[2]] = EXPECTED["old_tft_archive"]
    check_sha(REPO / "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a",
              EXPECTED["core_archive"], "CORE_ARCHIVE")
    check_sha(LIBS / "JWPLC_Display/src/esp32/libJWPLC_Display.a",
              EXPECTED["display_archive"], "DISPLAY_ARCHIVE")
    props = (TFT / "library.properties").read_text(encoding="utf-8")
    require("precompiled=full" in props and "dot_a_linkage=true" in props,
            "PACKAGE_PRECOMPILED_CONTRACT_INVALID")


def rollback() -> bool:
    global phase
    phase = "ROLLBACK"
    valid = True
    for relative in RELATIVE:
        src = BACKUP / relative
        dst = REPO / relative
        try:
            if src.is_file():
                shutil.copy2(src, dst)
                valid &= sha(dst) == original_shas[relative]
            else:
                valid = False
        except Exception:
            valid = False
    valid &= clean_worktree()
    emit("ROLLBACK_VERIFIED", "YES" if valid else "NO")
    return valid


def adopt(cpp: Path, setup: Path, archive: Path) -> None:
    global phase, changed
    phase = "BACKUP"
    for relative in RELATIVE:
        src = REPO / relative
        dest = BACKUP / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        require(sha(src) == sha(dest), "BACKUP_BYTE_PARITY_FAILED:" + relative)
    emit("BACKUP_ROOT", BACKUP)

    phase = "PRODUCT_ADOPTION"
    for relative, src in zip(RELATIVE, (cpp, setup, archive)):
        changed = True
        dest = REPO / relative
        shutil.copy2(src, dest)
    for relative, target in zip(RELATIVE, ("new_cpp", "new_setup", "new_tft_archive")):
        check_sha(REPO / relative, EXPECTED[target], "ADOPTED_" + target.upper())
    require(not clean_worktree(), "ADOPTION_DID_NOT_MODIFY_TRACKED_FILES")
    changedpaths = set(git("diff", "--name-only", "--").splitlines())
    require(changedpaths == set(RELATIVE), "UNEXPECTED_PRODUCT_DIFF:" + repr(sorted(changedpaths)))
    require(git("diff", "--check") == "", "PRODUCT_DIFF_CHECK_FAILED")
    emit("TRACKED_PRODUCT_FILES_CHANGED", len(changedpaths))
    emit("PRODUCT_FILES_UNCOMMITTED", "YES")


def compile_cases(cli: str) -> None:
    global phase
    cases = (
        ("DIRECT_TFT", LIBS / "JWPLC_Display/examples/04.Display_TFT_Direct"),
        ("DISPLAY_INTEGRATION", LIBS / "JWPLC_Display/examples/Display_UserUI_Callbacks"),
        ("NORMAL_AUTOLOAD", REPO / "tools/build-speed-benchmark/sketches/01_empty"),
        ("STARTUP_PROBE", REPO / "tools/alpha13/firmware/a13_tft_pre1_startup_baseline_probe"),
    )
    phase = "ARDUINO_COMPILE"
    for label, sketch in cases:
        require(sketch.is_dir(), "SKETCH_PATH_NOT_FOUND:" + label)
        build = TEMP_ROOT / ("build_" + label)
        args = [cli, "compile", "--fqbn", FQBN, "-j", "0", "-v",
                "--clean", "--build-path", str(build),
                "--library", str(TFT), "--libraries", str(LIBS), str(sketch)]
        rc, output = run(args, log=RUN_ROOT / ("compile_" + label + ".log"))
        emit(label + "_COMPILE_EXIT", rc)
        if rc:
            print("\n".join(output.splitlines()[-35:]), flush=True)
            raise GateError("NORMAL_PACKAGE_COMPILE_FAILED:" + label)
        found = re.findall(r"^Using library JWPLC_TFT at version .+ in folder: (.+)\s*$",
                           output, re.MULTILINE)
        require(len(found) == 1, "TFT_SELECTION_CARDINALITY:" + label)
        actual = Path(found[0].strip()).resolve()
        require(os.path.normcase(str(actual)).casefold() ==
                os.path.normcase(str(TFT.resolve())).casefold(),
                "TFT_SELECTED_NOT_PRODUCT_REPO:" + label)
        require(re.search(r"Using precompiled library .*JWPLC_TFT", output) is not None,
                "TFT_ARCHIVE_NOT_PRECOMPILED:" + label)
        require(re.search(r"^Using library TFT_eSPI at version", output, re.MULTILINE) is None,
                "EXTERNAL_TFT_ESPI_SELECTED:" + label)
        objects = list(build.rglob("JWPLC_TFT.cpp.o")) + list(build.rglob("TFT_eSPI.cpp.o"))
        require(len(objects) == 0, "NORMAL_SOURCE_OBJECT_FOUND:" + label)
        require("Using core 'jwcontrol_precompiled_stub'" in output,
                "PRECOMPILED_STUB_CORE_MISSING:" + label)
        require(re.search(r"[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a", output) is not None,
                "CORE_ARCHIVE_NOT_LINKED:" + label)
        emit(label + "_PRODUCT_TFT_PRECOMPILED", "True")
        emit(label + "_EXTERNAL_TFT_ESPI", "False")
        emit(label + "_CORE_ARCHIVE_LINKED", "True")


def main() -> int:
    global phase, changed
    ap = argparse.ArgumentParser()
    ap.add_argument("--arduino-cli", default=r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe")
    args = ap.parse_args()
    RUN_ROOT.mkdir(parents=True, exist_ok=True)
    BACKUP.mkdir(parents=True, exist_ok=True)
    emit("GATE", "A13-TFT-PRE6-P2A")
    emit("PHASE", "PRECHECK")
    try:
        check_initial()
        phase = "VERIFY_CANDIDATE"
        cpp, setup, archive = manifest_candidate()
        cli = args.arduino_cli
        if not Path(cli).is_file():
            binary = shutil.which("arduino-cli")
            require(binary is not None, "ARDUINO_CLI_NOT_FOUND")
            cli = binary
        rc, ver = run([cli, "version"])
        require(rc == 0 and re.search(r"Version:\s*1\.0\.2\b", ver),
                "ARDUINO_CLI_VERSION_MISMATCH")
        emit("ARDUINO_CLI", ver.strip())
        adopt(cpp, setup, archive)
        compile_cases(cli)
        phase = "FINAL_AUDIT"
        require(set(git("diff", "--name-only", "--").splitlines()) == set(RELATIVE),
                "FINAL_PRODUCT_DIFF_INVALID")
        require(git("diff", "--check") == "", "FINAL_DIFF_CHECK_FAILED")
        check_sha(REPO / RELATIVE[0], EXPECTED["new_cpp"], "FINAL_CPP")
        check_sha(REPO / RELATIVE[1], EXPECTED["new_setup"], "FINAL_SETUP")
        check_sha(REPO / RELATIVE[2], EXPECTED["new_tft_archive"], "FINAL_ARCHIVE")
        check_sha(REPO / "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a",
                  EXPECTED["core_archive"], "FINAL_CORE")
        check_sha(LIBS / "JWPLC_Display/src/esp32/libJWPLC_Display.a",
                  EXPECTED["display_archive"], "FINAL_DISPLAY")
        report = [
            "GATE=A13-TFT-PRE6-P2A",
            "STATUS=PASS",
            "REASON=PRODUCT_WORKTREE_ADOPTED_AND_FOUR_NORMAL_BUILDS_PASS",
            "PRODUCT_REPO_MODIFIED=YES_UNCOMMITTED",
            "UPLOAD_EXECUTED=NO",
            "PRODUCT_COMMIT_EXECUTED=NO",
            "PRODUCT_CHANGED_FILE_COUNT=3",
            "NORMAL_COMPILE_CASES_PASS=4",
            "TFT_ARCHIVE_SHA256=" + EXPECTED["new_tft_archive"],
            "TFT_ARCHIVE_BYTES=" + str(NEW_BYTES),
            "CORE_AND_DISPLAY_ARCHIVES_PRESERVED=YES",
            "GLOBAL_TFT_ESPI_REQUIRED=NO",
            "BACKUP_ROOT=" + str(BACKUP),
            "WORKTREE=EXPECTED_THREE_TRACKED_FILES_MODIFIED",
            "NEXT=TFT_PRE6_P2B_PHYSICAL_NORMAL_PACKAGE_TEST",
        ]
        SUMMARY.write_text("\n".join(report) + "\n", encoding="utf-8")
        for item in report:
            print(item, flush=True)
        return 0
    except (GateError, OSError, ValueError, subprocess.SubprocessError) as exc:
        emit("PHASE", phase)
        emit("ERROR", str(exc))
        recovered = True
        if changed:
            recovered = rollback()
        report = [
            "GATE=A13-TFT-PRE6-P2A", "STATUS=REVIEW",
            "REASON=PRODUCT_ADOPTION_OR_BUILD_FAILED",
            "PRODUCT_FAILURE=UNDETERMINED",
            "HARNESS_FAILURE=UNDETERMINED",
            "UPLOAD_EXECUTED=NO",
            "PRODUCT_COMMIT_EXECUTED=NO",
            "ROLLBACK_REQUIRED=" + ("YES" if changed else "NO"),
            "ROLLBACK_VERIFIED=" + ("YES" if recovered else "NO"),
            "PHASE=" + phase,
            "ERROR=" + str(exc),
            "BACKUP_ROOT=" + str(BACKUP),
        ]
        SUMMARY.write_text("\n".join(report) + "\n", encoding="utf-8")
        for item in report:
            print(item, flush=True)
        return 3 if recovered else 4


if __name__ == "__main__":
    sys.exit(main())
