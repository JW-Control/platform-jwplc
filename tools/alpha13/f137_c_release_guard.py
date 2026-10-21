#!/usr/bin/env python3
"""F137-C: guard de integridad del package candidato y release-source, sin build/USB.

Comprueba que el arbol JWPLC/2.1.0 es el mismo que ya aprobo N3 y H fisica;
el guard --review exige 14 rutas producto conocidas respecto de base Alpha13.
Nunca ejecuta merge, upload, publication, Git writes, ni modifica firmware.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
VALIDATED_N3_HEAD = "5e1010927a8c03c7ad46d38e4d5459b06bae543f"
PACKAGE_TREE_SHA = "cc17716f173509b1e0f3c0df5eb99549014c1d61"
EXPECTED_ARCHIVES = {
    "JWPLC_Ethernet": "f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4",
    "JWPLC_ModbusTCP": "d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d",
}
PRODUCT_DIFF_14 = frozenset({
    "JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/library.properties",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/esp32/libJWPLC_Ethernet.a",
    "JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/README.md",
    "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md",
    "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/README.md",
    "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/library.properties",
    "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/esp32/libJWPLC_ModbusTCP.a",
    "JWPLC/2.1.0/libraries/JWPLC_RS485/README.md",
    "JWPLC/2.1.0/libraries/JWPLC_TFT/README.md",
    "JWPLC/2.1.0/libraries/JW_FRAM/README.md",
    "JWPLC/2.1.0/libraries/JW_SD/README.md",
})


def stop(condition: bool, code: str) -> None:
    if not condition:
        raise RuntimeError("F137_C_REVIEW_STOP_" + code)


def git(*args: str) -> str:
    proc = subprocess.run(["git", *args], cwd=ROOT, capture_output=True,
                          text=True, errors="replace", timeout=35)
    stop(proc.returncode == 0, "GIT_" + args[0])
    return proc.stdout.strip()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def invariant_package() -> dict:
    package = ROOT / "JWPLC/2.1.0"
    tree = git("rev-parse", "HEAD:JWPLC/2.1.0")
    stop(tree == PACKAGE_TREE_SHA, "PACKAGE_TREE_NOT_N3")
    stop(git("rev-parse", VALIDATED_N3_HEAD + ":JWPLC/2.1.0") == tree,
         "N3_PROVENANCE_CHANGED")

    for lib, expected in EXPECTED_ARCHIVES.items():
        libdir = package / "libraries" / lib
        archive = libdir / "src" / "esp32" / ("lib" + lib + ".a")
        stop(archive.is_file(), "MISSING_ARCHIVE_" + lib)
        stop(sha256(archive) == expected, "ARCHIVE_SHA_" + lib)
        prop = (libdir / "library.properties").read_text(encoding="utf-8")
        stop(bool(re.search(r"(?m)^precompiled=full\s*$", prop)),
             "NOT_PRECOMPILED_FULL_" + lib)

    eth = (package / "libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp").read_text(
        encoding="utf-8")
    core = (package / "cores/jwcontrol/jwplc_peripherals.cpp").read_text(
        encoding="utf-8")
    stop('extern "C" bool __attribute__((weak, noinline, used))' in eth and
         'jwplcEthernetProfileAvailable(void)' in eth,
         "WEAK_ETHERNET_BASIC_POLICY_MISSING")
    stop('#if !JWPLC_HAS_ETHERNET' in core and
         'jwplcEthernetProfileAvailable(void)' in core and
         'return false;' in core,
         "STRONG_BASICCORE_DISABLE_POLICY_MISSING")

    return {
        "head": git("rev-parse", "HEAD"),
        "package_tree_sha1": tree,
        "validated_n3_head": VALIDATED_N3_HEAD,
        "archives_sha256": EXPECTED_ARCHIVES,
        "ethernet_profile_policy": "BASIC_WEAK_TRUE_BASICCORE_STRONG_FALSE",
    }


def review(base_ref: str) -> dict:
    stop(bool(base_ref), "BASE_REF_REQUIRED")
    old = git("merge-base", "HEAD", base_ref)
    stop(old != git("rev-parse", "HEAD"), "BASE_OR_CANDIDATE_REVERSED")
    diff = git("diff", "--name-only", base_ref, "HEAD", "--", "JWPLC/2.1.0")
    actual = frozenset(diff.splitlines())
    stop(actual == PRODUCT_DIFF_14,
         "PRODUCT_SCOPE_CHANGED_EXPECT_14_GOT_" + str(len(actual)))
    status = git("diff", "--name-status", base_ref, "HEAD", "--", "JWPLC/2.1.0")
    allowed = {"A", "M"}
    statuses = [line.split("\t")[0] for line in status.splitlines() if line]
    stop(all(s in allowed for s in statuses) and len(statuses) == 14,
         "PRODUCT_FILE_RENAME_DELETE_UNEXPECTED")
    result = invariant_package()
    result["base_ref"] = base_ref
    result["base_head"] = git("rev-parse", base_ref)
    result["product_files_14"] = sorted(PRODUCT_DIFF_14)
    result["merge_to_base"] = "NOT_PERFORMED"
    result["release"] = "NOT_PERFORMED"
    print("F137_C_REVIEW=PASS_EXACT_14_PRODUCT_PATHS")
    return result


def selftest() -> None:
    stop(len(PRODUCT_DIFF_14) == 14, "SELFTEST_COUNT")
    stop(len(EXPECTED_ARCHIVES) == 2, "SELFTEST_ARCHIVES")
    stop(all(re.fullmatch(r"[0-9a-f]{64}", sha) for sha in
             EXPECTED_ARCHIVES.values()), "SELFTEST_SHA256")
    stop(all(p.startswith("JWPLC/2.1.0/") for p in PRODUCT_DIFF_14),
         "SELFTEST_SCOPE")
    stop(PACKAGE_TREE_SHA != "", "SELFTEST_TREE")
    print("F137_C_SELFTEST=PASS_SCOPE_SHA_NEGATIVE_CONTRACT_NO_HARDWARE")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    modes = ap.add_mutually_exclusive_group(required=True)
    modes.add_argument("--selftest", action="store_true")
    modes.add_argument("--review", action="store_true")
    modes.add_argument("--verify-release-source", action="store_true")
    ap.add_argument("--base-ref")
    ap.add_argument("--expected-head")
    ap.add_argument("--output", type=Path)
    args = ap.parse_args()
    if args.selftest:
        selftest()
        return
    stop(bool(args.expected_head and re.fullmatch(r"[0-9a-f]{40}", args.expected_head)),
         "HEAD_REQUIRED")
    stop(git("rev-parse", "HEAD") == args.expected_head, "HEAD_CHANGED")
    stop(not git("status", "--porcelain", "--untracked-files=no"),
         "TRACKED_TREE_DIRTY")
    if args.review:
        data = review(args.base_ref or "")
    else:
        data = invariant_package()
        data["source_integrity_only"] = "PASS_MATCHES_N3"
        data["pre_release"] = "NOT_PUBLISHED"
        print("F137_C_RELEASE_SOURCE=PASS_PACKAGE_TREE_IDENTICAL_TO_N3")
    if args.output:
        output = args.output.expanduser().resolve()
        stop(output != ROOT and ROOT not in output.parents,
             "OUTPUT_INSIDE_GIT_WORKTREE")
        stop(not output.exists(), "OUTPUT_ALREADY_EXISTS")
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print("F137_C_ARCHIVES=PASS_2_SHA256")
    print("F137_C_PROFILE_POLICY=PASS_WEAK_BASIC_STRONG_BASICCORE")
    print("F137_C_NO_UPLOAD_NO_MERGE_NO_RELEASE=TRUE")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, subprocess.TimeoutExpired) as exc:
        print(str(exc) if isinstance(exc, RuntimeError)
              else "F137_C_REVIEW_STOP_LOCAL_IO", file=sys.stderr)
        sys.exit(2)
