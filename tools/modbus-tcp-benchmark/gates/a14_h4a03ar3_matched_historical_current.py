#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
CLOSURE_DOC = "docs/v2.1.0-alpha.14/A14_P3_UDP_RX_CLOSURE_20260924.md"
RAW_REL = Path(
    "tools/modbus-tcp-benchmark/firmware/"
    "eth14_raw_transport_server/eth14_raw_transport_server.ino"
)
EXPECTED_RAW_SHA256 = (
    "9A0C6CF27E44A61DC97686FB1A769EBBBB34D036CDF8D0CA0EEAB597E699AB6B"
)
MATCHED_DISPLAY_LIBRARIES = (
    "JWPLC_Display",
    "JWPLC_TFT",
)
DURATION_S = 15.0
RUNS_PER_VARIANT = 3
UDP_PAYLOAD = 1016
MATERIAL_THRESHOLD_PCT = 1.0
CONFIRM_THRESHOLD_PCT = 1.5
REPEATABILITY_SPREAD_MAX_PCT = 1.0
ORDER = ("HIST", "CURRENT", "CURRENT", "HIST", "HIST", "CURRENT")


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def decode(data: bytes) -> str:
    if data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
        return data.decode("utf-16")
    for enc in ("utf-8-sig", "utf-8", "cp1252"):
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace")


def run(
    cmd: list[str],
    cwd: Path | None = None,
) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def run_logged(
    cmd: list[str],
    log: Path,
    cwd: Path | None = None,
) -> int:
    p = run(cmd, cwd)
    log.write_bytes(p.stdout + p.stderr)
    return p.returncode


def git(repo: Path, *args: str) -> str:
    p = run(["git", "-C", str(repo), *args])
    if p.returncode != 0:
        raise RuntimeError(
            f"GIT_FAILED {' '.join(args)} :: "
            f"{decode(p.stdout + p.stderr)[-1600:]}"
        )
    return decode(p.stdout).strip()


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
        raise RuntimeError(f"H4A03AR3_KEY_COUNT_{key}={len(found)}")
    return found[0]


def number(text: str, key: str) -> float:
    return float(one(text, key))


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def normalize_eol(data: bytes) -> bytes:
    return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def semantic_fingerprint(paths: list[Path]) -> str:
    h = hashlib.sha256()
    for path in sorted(paths, key=lambda p: p.as_posix().lower()):
        h.update(path.name.encode("utf-8"))
        h.update(b"\0")
        h.update(normalize_eol(path.read_bytes()))
        h.update(b"\0")
    return h.hexdigest().upper()


def directory_fingerprint(root: Path) -> str:
    if not root.is_dir():
        raise RuntimeError(f"H4A03AR3_LIBRARY_DIR_MISSING={root}")

    h = hashlib.sha256()
    files = sorted(
        (p for p in root.rglob("*") if p.is_file()),
        key=lambda p: p.relative_to(root).as_posix().lower(),
    )
    if not files:
        raise RuntimeError(f"H4A03AR3_LIBRARY_DIR_EMPTY={root}")

    for path in files:
        rel = path.relative_to(root).as_posix()
        h.update(rel.encode("utf-8"))
        h.update(b"\0")
        h.update(path.read_bytes())
        h.update(b"\0")

    return h.hexdigest().upper()


def overlay_current_display_stack(
    historical_root: Path,
    current_root: Path,
) -> dict[str, str]:
    current_libs = current_root / "JWPLC" / "2.1.0" / "libraries"
    historical_libs = historical_root / "JWPLC" / "2.1.0" / "libraries"

    fingerprints: dict[str, str] = {}

    for library in MATCHED_DISPLAY_LIBRARIES:
        src = current_libs / library
        dst = historical_libs / library

        if not src.is_dir():
            raise RuntimeError(
                f"H4A03AR3_CURRENT_DISPLAY_LIBRARY_MISSING={src}"
            )

        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(src, dst)

        src_fp = directory_fingerprint(src)
        dst_fp = directory_fingerprint(dst)

        emit(f"H4A03AR3_CURRENT_{library}_FINGERPRINT", src_fp)
        emit(f"H4A03AR3_HIST_{library}_OVERLAY_FINGERPRINT", dst_fp)

        if src_fp != dst_fp:
            raise RuntimeError(
                f"H4A03AR3_{library}_OVERLAY_FINGERPRINT_MISMATCH"
            )

        fingerprints[library] = src_fp

    emit(
        "H4A03AR3_DISPLAY_STACK_MATCHED",
        "PASS",
    )
    emit(
        "H4A03AR3_DISPLAY_STACK_SOURCE",
        "CURRENT_HEAD_FOR_BOTH_VARIANTS",
    )
    emit(
        "H4A03AR3_DISPLAY_STACK_LIBRARIES",
        "+".join(MATCHED_DISPLAY_LIBRARIES),
    )
    return fingerprints


def median(values_: list[float]) -> float:
    if not values_:
        raise RuntimeError("H4A03AR3_MEDIAN_EMPTY")
    return float(statistics.median(values_))


def spread_pct(values_: list[float]) -> float:
    med = median(values_)
    if med <= 0.0:
        raise RuntimeError("H4A03AR3_SPREAD_MEDIAN_NONPOSITIVE")
    return (max(values_) - min(values_)) * 100.0 / med


def pct(a: float, b: float) -> float:
    return (a / b - 1.0) * 100.0


def python_ast_precheck(paths: list[Path]) -> None:
    import ast

    for path in paths:
        ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
        emit("H4A03AR3_AST_PASS", path)


def patch_cmd(
    python: str,
    script: Path,
    args: list[str],
    log: Path,
) -> None:
    code = run_logged([python, "-B", str(script), *args], log)
    emit(f"H4A03AR3_PATCH_{script.stem}_EXIT", code)
    if code != 0:
        print(decode(log.read_bytes())[-3000:])
        raise RuntimeError(f"H4A03AR3_PATCH_FAILED={script.name}")


def source_contract(work_root: Path, variant: str) -> str:
    sketch = (
        work_root
        / "sketch"
        / "eth14_raw_transport_server"
        / "eth14_raw_transport_server.ino"
    )
    eth = work_root / "libraries" / "JWPLC_Ethernet"
    wcpp = eth / "src" / "utility" / "w5100.cpp"
    wh = eth / "src" / "utility" / "w5100.h"
    hdr = eth / "src" / "JWPLC_W5x00_Ethernet.h"
    socket_cpp = eth / "src" / "socket.cpp"
    udp_cpp = eth / "src" / "EthernetUdp.cpp"

    required_paths = (sketch, wcpp, wh, hdr, socket_cpp, udp_cpp)
    for path in required_paths:
        if not path.is_file():
            raise RuntimeError(
                f"H4A03AR3_{variant}_SOURCE_MISSING={path}"
            )

    ino = sketch.read_text(encoding="utf-8")
    cpp = wcpp.read_text(encoding="utf-8")

    required = {
        "BATCH2": "static constexpr uint8_t UDP_RX_MAX_PACKETS_PER_HOLD = 2;",
        "INT": "static constexpr uint8_t ETH_INT_PIN = 15;",
        "FUSED": "udpSocket.jwplcDiagReadPacketFastDeferred(",
        "COMMIT2": "udpSocket.jwplcDiagCommitRxFast()",
        "R1": "uint16_t postCommitRsr = 0;",
        "R2": "ETH14_RAW_IDLE=PASS",
        "FREEZE": "ETH14_RAW_FREEZE=PASS",
    }
    for label, needle in required.items():
        count = ino.count(needle)
        emit(f"H4A03AR3_{variant}_CONTRACT_{label}_COUNT", count)
        if count != 1:
            raise RuntimeError(
                f"H4A03AR3_{variant}_CONTRACT_{label}_INVALID={count}"
            )

    forbidden = {
        "W5100_CALL_COUNTER": "++jwplcDiagReadCallsCounter;",
        "W5100_BYTE_COUNTER": "jwplcDiagReadBytesCounter += len;",
        "FAST_TIMER": "const uint32_t fastStartUs = micros();",
        "UDP_HOLD_TIMER": "const uint32_t udpRxHoldStartUs =",
        "TCP_HOLD_TIMER": "const uint32_t tcpSpiHoldStartUs = micros();",
        "LOOP_PROFILING": "    updateLoopTiming();",
        "ISR_COUNTER": "++ethIntIsrCount;",
        "INT_SKIP_COUNTER": "++udpRxIntSkipCount;",
        "INT_WAKE_COUNTER": "++udpRxIntWakeCount;",
        "INT_FALLBACK_COUNTER": "++udpRxIntLowFallbackCount;",
    }
    for label, needle in forbidden.items():
        haystack = cpp if label.startswith("W5100_") else ino
        count = haystack.count(needle)
        emit(f"H4A03AR3_{variant}_FORBID_{label}_COUNT", count)
        if count != 0:
            raise RuntimeError(
                f"H4A03AR3_{variant}_FORBID_{label}_PRESENT={count}"
            )

    freeze_pos = ino.index("c == 'F' || c == 'f'")
    freeze_end = ino.index("else if (c == 'S' || c == 's')", freeze_pos)
    freeze_body = ino[freeze_pos:freeze_end]
    if "resetCounters();" in freeze_body:
        raise RuntimeError(
            f"H4A03AR3_{variant}_FREEZE_RESETS_COUNTERS"
        )

    fingerprint = semantic_fingerprint(list(required_paths))
    emit(f"H4A03AR3_{variant}_FAST_SOURCE_FINGERPRINT", fingerprint)
    emit(f"H4A03AR3_{variant}_SOURCE_CONTRACT", "PASS")
    return fingerprint


def build_candidate(
    *,
    variant: str,
    source_root: Path,
    current_repo: Path,
    temp_root: Path,
    python: str,
    arduino_cli: Path,
    matched_display_fingerprints: dict[str, str],
) -> dict[str, Path | str]:
    gates = current_repo / "tools" / "modbus-tcp-benchmark" / "gates"

    scripts = {
        "instrument": gates / "a14_p3_udp_rx_instrument_patch.py",
        "batch2": gates / "a14_p3b_udp_rx_batch2_patch.py",
        "int": gates / "a14_p3g_udp_rx_int_guided_patch.py",
        "fused": gates / "a14_p3h_udp_rx_fused_fast_path_patch.py",
        "commit2": gates / "a14_p3j_udp_rx_coalesced_commit_patch.py",
        "r1": gates / "a14_p3j_r1_postcommit_rearm_patch.py",
        "r2": gates / "a14_p3j_r2_serial_idle_patch.py",
        "minimal": gates / "a14_h4a03a_minimal_instrumentation_patch.py",
        "freeze": gates / "a14_h4a03ar3_freeze_patch.py",
    }
    for script in scripts.values():
        if not script.is_file():
            raise RuntimeError(f"H4A03AR3_PATCH_SCRIPT_MISSING={script}")

    source_libraries = source_root / "JWPLC" / "2.1.0" / "libraries"
    for library, expected_fp in matched_display_fingerprints.items():
        actual_fp = directory_fingerprint(source_libraries / library)
        emit(
            f"H4A03AR3_{variant}_{library}_SOURCE_FINGERPRINT",
            actual_fp,
        )
        if actual_fp != expected_fp:
            raise RuntimeError(
                f"H4A03AR3_{variant}_{library}_SOURCE_NOT_MATCHED"
            )

    work_root = temp_root / f"{variant.lower()}_work"
    build_root = temp_root / f"{variant.lower()}_build"

    instrument_log = temp_root / f"{variant.lower()}_01_instrument.log"
    patch_cmd(
        python,
        scripts["instrument"],
        [
            "--repo-root",
            str(source_root),
            "--work-root",
            str(work_root),
        ],
        instrument_log,
    )

    sketch = (
        work_root
        / "sketch"
        / "eth14_raw_transport_server"
        / "eth14_raw_transport_server.ino"
    )
    eth = work_root / "libraries" / "JWPLC_Ethernet"

    steps: list[tuple[str, list[str]]] = [
        ("batch2", ["--instrumented-sketch", str(sketch)]),
        (
            "int",
            [
                "--ethernet-root",
                str(eth),
                "--instrumented-sketch",
                str(sketch),
            ],
        ),
        (
            "fused",
            [
                "--ethernet-root",
                str(eth),
                "--instrumented-sketch",
                str(sketch),
            ],
        ),
        (
            "commit2",
            [
                "--ethernet-root",
                str(eth),
                "--instrumented-sketch",
                str(sketch),
            ],
        ),
        ("r1", ["--instrumented-sketch", str(sketch)]),
        ("r2", ["--instrumented-sketch", str(sketch)]),
        (
            "minimal",
            [
                "--ethernet-root",
                str(eth),
                "--instrumented-sketch",
                str(sketch),
            ],
        ),
        ("freeze", ["--instrumented-sketch", str(sketch)]),
    ]

    for index, (name, args) in enumerate(steps, 2):
        patch_cmd(
            python,
            scripts[name],
            args,
            temp_root / f"{variant.lower()}_{index:02d}_{name}.log",
        )

    fingerprint = source_contract(work_root, variant)

    build_root.mkdir(parents=True, exist_ok=True)
    sketch_dir = sketch.parent
    diag_libraries = work_root / "libraries"
    repo_libraries = source_root / "JWPLC" / "2.1.0" / "libraries"
    compile_log = temp_root / f"{variant.lower()}_compile.log"

    compile_args = [
        str(arduino_cli),
        "compile",
        "--fqbn",
        "jwplc_local:esp32:jwplcbasic",
        "--build-path",
        str(build_root),
        "--libraries",
        str(diag_libraries),
        "--libraries",
        str(repo_libraries),
        str(sketch_dir),
    ]
    compile_exit = run_logged(compile_args, compile_log)
    emit(f"H4A03AR3_{variant}_COMPILE_EXIT", compile_exit)
    if compile_exit != 0:
        print(decode(compile_log.read_bytes())[-5000:])
        raise RuntimeError(f"H4A03AR3_{variant}_COMPILE_FAILED")

    compile_text = decode(compile_log.read_bytes())
    if str(eth).lower() not in compile_text.lower():
        raise RuntimeError(
            f"H4A03AR3_{variant}_DIAG_ETHERNET_NOT_SELECTED"
        )

    for library in MATCHED_DISPLAY_LIBRARIES:
        expected_library_path = (
            source_root / "JWPLC" / "2.1.0" / "libraries" / library
        )
        used = (
            str(expected_library_path).lower()
            in compile_text.lower()
        )
        emit(
            f"H4A03AR3_{variant}_{library}_COMPILE_PATH_USED",
            used,
        )
        if not used:
            raise RuntimeError(
                f"H4A03AR3_{variant}_{library}_NOT_SELECTED_BY_BUILD"
            )

    emit(f"H4A03AR3_{variant}_COMPILE", "PASS")

    return {
        "build": build_root,
        "sketch_dir": sketch_dir,
        "fingerprint": fingerprint,
    }


def inspect_case(text: str, variant: str) -> dict[str, float]:
    if one(text, "H4A03AR3_FUNCTIONAL_PASS") != "YES":
        raise RuntimeError(f"H4A03AR3_{variant}_FUNCTIONAL_FAIL")

    for key in (
        "H4A03AR3_TRANSPORT_ERRORS",
        "H4A03AR3_UDP_SPI_LOCK_ERRORS",
        "H4A03AR3_TCP_SPI_LOCK_ERRORS",
        "H4A03AR3_SPI_LOCK_ERRORS_TOTAL",
    ):
        if number(text, key) != 0.0:
            raise RuntimeError(f"H4A03AR3_{variant}_{key}_NONZERO")

    lower = number(text, "H4A03AR3_DUT_MBPS_LOWER")
    upper = number(text, "H4A03AR3_DUT_MBPS_UPPER")
    mid = number(text, "H4A03AR3_DUT_MBPS_MID")
    freeze_ack = number(text, "H4A03AR3_FREEZE_ACK_TAIL_S")
    freeze_request = number(text, "H4A03AR3_FREEZE_REQUEST_TAIL_S")

    if not (0.0 < lower <= mid <= upper):
        raise RuntimeError(f"H4A03AR3_{variant}_THROUGHPUT_BOUNDS_INVALID")
    if freeze_request < 0.0 or freeze_ack < freeze_request:
        raise RuntimeError(f"H4A03AR3_{variant}_FREEZE_TIMING_INVALID")

    return {
        "lower": lower,
        "upper": upper,
        "mid": mid,
        "freeze_ack": freeze_ack,
        "freeze_request": freeze_request,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.3A-R3 matched historical-vs-current UDP RX gate"
    )
    parser.add_argument("--serial", default="COM14")
    args = parser.parse_args()

    script = Path(__file__).resolve()
    repo = script.parents[3]
    runner = (
        repo
        / "tools"
        / "modbus-tcp-benchmark"
        / "pc"
        / "a14_h4a03ar3_udp_freeze_case.py"
    )

    print("=" * 72)
    print(" A14 H4A0.3A-R3 - MATCHED HISTORICAL VS CURRENT FAST UDP RX")
    print("=" * 72)

    branch = git(repo, "branch", "--show-current")
    head = git(repo, "rev-parse", "HEAD")
    emit("BRANCH", branch)
    emit("HEAD", head)
    emit("SERIAL_PORT", args.serial)
    emit("H4A03AR3_DURATION_S", DURATION_S)
    emit("H4A03AR3_RUNS_PER_VARIANT", RUNS_PER_VARIANT)
    emit("H4A03AR3_UDP_PAYLOAD", UDP_PAYLOAD)
    emit("H4A03AR3_ORDER", ",".join(ORDER))
    emit("H4A03AR3_FRESH_UPLOAD_PER_CASE", "YES")
    emit("H4A03AR3_COMPONENT_ABLATION", "NO")
    emit("H4A03AR3_PRODUCT_SOURCE_MUTATION", "NO")
    emit(
        "H4A03AR3_FAST_CHAIN",
        "BATCH2+INT+FUSED+COMMIT2+R1+R2+MINIMAL+FREEZE",
    )
    emit(
        "H4A03AR3_ACCOUNTING",
        "FREEZE_REQUEST_TO_ACK_BOUNDED_NO_POST_FREEZE_RX_ACCOUNTING",
    )
    emit(
        "H4A03AR3_DISPLAY_STACK_POLICY",
        "CURRENT_JWPLC_DISPLAY_PLUS_JWPLC_TFT_FOR_BOTH_VARIANTS",
    )

    if branch != BRANCH:
        raise RuntimeError("H4A03AR3_BRANCH_MISMATCH")

    if git(repo, "diff", "--name-only") or git(
        repo, "diff", "--cached", "--name-only"
    ):
        raise RuntimeError("H4A03AR3_CURRENT_TREE_NOT_CLEAN")

    untracked = [
        line.replace("\\", "/")
        for line in git(
            repo, "ls-files", "--others", "--exclude-standard"
        ).splitlines()
        if line.strip()
    ]
    product_untracked = [
        line
        for line in untracked
        if line.startswith(
            ("JWPLC/2.1.0/", "tools/modbus-tcp-benchmark/firmware/")
        )
    ]
    emit("H4A03AR3_UNTRACKED_PRODUCT_COUNT", len(product_untracked))
    if product_untracked:
        raise RuntimeError("H4A03AR3_UNTRACKED_PRODUCT_SOURCE_FOUND")

    if not runner.is_file():
        raise RuntimeError("H4A03AR3_RUNNER_MISSING")

    python_ast_precheck(
        [
            runner,
            repo
            / "tools"
            / "modbus-tcp-benchmark"
            / "gates"
            / "a14_h4a03ar3_freeze_patch.py",
        ]
    )

    python = sys.executable
    arduino_cli = Path(
        r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe"
    )
    if not arduino_cli.is_file():
        raise RuntimeError("H4A03AR3_ARDUINO_CLI_NOT_FOUND")

    closure_candidates = [
        line.strip()
        for line in git(
            repo,
            "log",
            "--format=%H",
            "--diff-filter=A",
            "--",
            CLOSURE_DOC,
        ).splitlines()
        if line.strip()
    ]
    emit(
        "H4A03AR3_CLOSURE_COMMIT_CANDIDATE_COUNT",
        len(closure_candidates),
    )
    if len(closure_candidates) != 1:
        raise RuntimeError("H4A03AR3_CLOSURE_COMMIT_NOT_UNIQUE")

    closure_commit = closure_candidates[0]
    historical_commit = git(repo, "rev-parse", f"{closure_commit}^")
    emit("H4A03AR3_P3_CLOSURE_COMMIT", closure_commit)
    emit("H4A03AR3_HISTORICAL_COMMIT", historical_commit)

    closure_delta = [
        line.strip().replace("\\", "/")
        for line in git(
            repo,
            "diff",
            "--name-only",
            historical_commit,
            closure_commit,
        ).splitlines()
        if line.strip()
    ]
    emit("H4A03AR3_CLOSURE_DELTA_COUNT", len(closure_delta))
    emit("H4A03AR3_CLOSURE_DELTA", ",".join(closure_delta))
    if closure_delta != [CLOSURE_DOC]:
        raise RuntimeError("H4A03AR3_CLOSURE_PARENT_NOT_PURE_DOC_ADD")

    current_raw = repo / RAW_REL
    if not current_raw.is_file():
        raise RuntimeError("H4A03AR3_CURRENT_RAW_MISSING")
    current_raw_bytes = current_raw.read_bytes()
    current_raw_hash = hashlib.sha256(current_raw_bytes).hexdigest().upper()
    emit("H4A03AR3_CURRENT_RAW_SHA256", current_raw_hash)
    if current_raw_hash != EXPECTED_RAW_SHA256:
        raise RuntimeError("H4A03AR3_CURRENT_RAW_HASH_DRIFT")

    temp_root = Path(tempfile.mkdtemp(prefix="jwplc_a14_h4a03ar3_"))
    historical_worktree = temp_root / "historical_repo"
    emit("H4A03AR3_RESULT_ROOT", temp_root)

    worktree_added = False

    try:
        add = run(
            [
                "git",
                "-C",
                str(repo),
                "worktree",
                "add",
                "--detach",
                str(historical_worktree),
                historical_commit,
            ]
        )
        if add.returncode != 0:
            raise RuntimeError(
                "H4A03AR3_HISTORICAL_WORKTREE_ADD_FAILED="
                + decode(add.stdout + add.stderr)[-1800:]
            )
        worktree_added = True

        hist_raw = historical_worktree / RAW_REL
        if not hist_raw.is_file():
            raise RuntimeError("H4A03AR3_HIST_RAW_MISSING")

        semantic_match = (
            normalize_eol(hist_raw.read_bytes())
            == normalize_eol(current_raw_bytes)
        )
        emit("H4A03AR3_HIST_RAW_SEMANTIC_MATCH", semantic_match)
        if not semantic_match:
            raise RuntimeError("H4A03AR3_HIST_RAW_SEMANTIC_DRIFT")

        # Restore only the historical Windows byte representation required by
        # the frozen P3 instrumentation precondition. Git semantics stay equal.
        hist_raw.write_bytes(current_raw_bytes)
        emit("H4A03AR3_HIST_RAW_RESTORED_SHA256", sha256(hist_raw))
        if sha256(hist_raw) != EXPECTED_RAW_SHA256:
            raise RuntimeError("H4A03AR3_HIST_RAW_RESTORE_FAILED")
        if git(
            historical_worktree,
            "diff",
            "--name-only",
            "--",
            RAW_REL.as_posix(),
        ):
            raise RuntimeError("H4A03AR3_HIST_RAW_RESTORE_GIT_DIFF_VISIBLE")

        matched_display_fingerprints = overlay_current_display_stack(
            historical_worktree,
            repo,
        )

        # The historical worktree is intentionally dirty only in the matched
        # display overlay. Those files are not product mutations in the
        # canonical repository and exist solely inside the disposable worktree.
        hist_overlay_dirty = [
            line.strip().replace("\\", "/")
            for line in git(
                historical_worktree,
                "diff",
                "--name-only",
            ).splitlines()
            if line.strip()
        ]
        allowed_prefixes = tuple(
            f"JWPLC/2.1.0/libraries/{library}/"
            for library in MATCHED_DISPLAY_LIBRARIES
        )
        unexpected_overlay_dirty = [
            line
            for line in hist_overlay_dirty
            if not line.startswith(allowed_prefixes)
        ]
        emit(
            "H4A03AR3_HIST_DISPLAY_OVERLAY_DIRTY_COUNT",
            len(hist_overlay_dirty),
        )
        if unexpected_overlay_dirty:
            raise RuntimeError(
                "H4A03AR3_HIST_OVERLAY_SCOPE_INVALID="
                + ",".join(unexpected_overlay_dirty)
            )
        emit(
            "H4A03AR3_HIST_DISPLAY_OVERLAY_SCOPE",
            "JWPLC_Display+JWPLC_TFT_ONLY",
        )

        hist_candidate = build_candidate(
            variant="HIST",
            source_root=historical_worktree,
            current_repo=repo,
            temp_root=temp_root,
            python=python,
            arduino_cli=arduino_cli,
            matched_display_fingerprints=matched_display_fingerprints,
        )
        current_candidate = build_candidate(
            variant="CURRENT",
            source_root=repo,
            current_repo=repo,
            temp_root=temp_root,
            python=python,
            arduino_cli=arduino_cli,
            matched_display_fingerprints=matched_display_fingerprints,
        )

        hist_fp = str(hist_candidate["fingerprint"])
        current_fp = str(current_candidate["fingerprint"])
        emit("H4A03AR3_FAST_SOURCE_FINGERPRINT_MATCH", hist_fp == current_fp)

        buckets: dict[str, dict[str, list[float]]] = {
            "HIST": {
                "lower": [],
                "upper": [],
                "mid": [],
                "freeze_ack": [],
            },
            "CURRENT": {
                "lower": [],
                "upper": [],
                "mid": [],
                "freeze_ack": [],
            },
        }
        counts = {"HIST": 0, "CURRENT": 0}

        for case_index, variant in enumerate(ORDER, 1):
            counts[variant] += 1
            run_number = counts[variant]
            candidate = (
                hist_candidate
                if variant == "HIST"
                else current_candidate
            )

            print()
            print("=" * 72)
            print(
                f" H4A0.3A-R3 CASE {case_index}/6 "
                f"{variant} RUN {run_number}/3"
            )
            print("=" * 72)

            upload_log = (
                temp_root
                / f"case_{case_index}_{variant.lower()}_upload.log"
            )
            upload_exit = run_logged(
                [
                    str(arduino_cli),
                    "upload",
                    "--fqbn",
                    "jwplc_local:esp32:jwplcbasic",
                    "--port",
                    args.serial,
                    "--input-dir",
                    str(candidate["build"]),
                    str(candidate["sketch_dir"]),
                ],
                upload_log,
            )
            emit(
                f"H4A03AR3_{variant}_RUN{run_number}_UPLOAD_EXIT",
                upload_exit,
            )
            if upload_exit != 0:
                print(decode(upload_log.read_bytes())[-4000:])
                raise RuntimeError(
                    f"H4A03AR3_{variant}_RUN{run_number}_UPLOAD_FAILED"
                )

            time.sleep(3.0)

            case_log = (
                temp_root
                / f"case_{case_index}_{variant.lower()}_run{run_number}.log"
            )
            case_exit = run_logged(
                [
                    python,
                    "-B",
                    "-u",
                    str(runner),
                    "--serial",
                    args.serial,
                    "--duration",
                    f"{DURATION_S:.1f}",
                    "--udp-payload",
                    str(UDP_PAYLOAD),
                    "--variant",
                    variant,
                ],
                case_log,
            )
            emit(
                f"H4A03AR3_{variant}_RUN{run_number}_CASE_EXIT",
                case_exit,
            )
            emit(
                f"H4A03AR3_{variant}_RUN{run_number}_LOG",
                case_log,
            )
            case_text = decode(case_log.read_bytes())
            print(case_text)
            if case_exit != 0:
                raise RuntimeError(
                    f"H4A03AR3_{variant}_RUN{run_number}_CASE_FAILED"
                )

            row = inspect_case(case_text, variant)
            for key in ("lower", "upper", "mid", "freeze_ack"):
                buckets[variant][key].append(row[key])

            emit(
                "H4A03AR3_RUN_RESULT",
                (
                    f"VARIANT={variant} RUN={run_number} "
                    f"LOWER={row['lower']:.6f} "
                    f"UPPER={row['upper']:.6f} "
                    f"MID={row['mid']:.6f} "
                    f"FREEZE_ACK_MS={row['freeze_ack'] * 1000.0:.3f}"
                ),
            )

        if counts != {"HIST": 3, "CURRENT": 3}:
            raise RuntimeError(f"H4A03AR3_RUN_COUNTS_INVALID={counts}")

        summary: dict[str, dict[str, float]] = {}
        for variant in ("HIST", "CURRENT"):
            summary[variant] = {
                "lower": median(buckets[variant]["lower"]),
                "upper": median(buckets[variant]["upper"]),
                "mid": median(buckets[variant]["mid"]),
                "spread": spread_pct(buckets[variant]["mid"]),
                "freeze_ack_ms": (
                    median(buckets[variant]["freeze_ack"]) * 1000.0
                ),
            }

        hist = summary["HIST"]
        cur = summary["CURRENT"]
        delta_mid = pct(hist["mid"], cur["mid"])
        intervals_overlap = not (
            hist["lower"] > cur["upper"]
            or cur["lower"] > hist["upper"]
        )

        if hist["lower"] > cur["upper"]:
            guaranteed_gap = pct(hist["lower"], cur["upper"])
            direction = "HIST_FASTER"
        elif cur["lower"] > hist["upper"]:
            guaranteed_gap = pct(cur["lower"], hist["upper"])
            direction = "CURRENT_FASTER"
        else:
            guaranteed_gap = 0.0
            direction = "BOUNDS_OVERLAP"

        repeatability_ok = (
            hist["spread"] <= REPEATABILITY_SPREAD_MAX_PCT
            and cur["spread"] <= REPEATABILITY_SPREAD_MAX_PCT
        )

        if not repeatability_ok:
            interpretation = "REPEATABILITY_INSUFFICIENT"
        elif abs(delta_mid) <= MATERIAL_THRESHOLD_PCT:
            interpretation = "NO_MATERIAL_HISTORICAL_CURRENT_RESIDUAL"
        elif (
            direction == "HIST_FASTER"
            and delta_mid >= CONFIRM_THRESHOLD_PCT
        ):
            interpretation = "HISTORICAL_FASTER_RESIDUAL_CONFIRMED"
        elif (
            direction == "CURRENT_FASTER"
            and delta_mid <= -CONFIRM_THRESHOLD_PCT
        ):
            interpretation = "CURRENT_FASTER_RESIDUAL_REVERSED"
        else:
            interpretation = "RESIDUAL_INCONCLUSIVE"

        print()
        print("=" * 72)
        print(" H4A0.3A-R3 SUMMARY")
        print("=" * 72)
        for variant in ("HIST", "CURRENT"):
            s = summary[variant]
            emit(
                f"H4A03AR3_{variant}_LOWER_MEDIAN_MBPS",
                f"{s['lower']:.6f}",
            )
            emit(
                f"H4A03AR3_{variant}_UPPER_MEDIAN_MBPS",
                f"{s['upper']:.6f}",
            )
            emit(
                f"H4A03AR3_{variant}_MID_MEDIAN_MBPS",
                f"{s['mid']:.6f}",
            )
            emit(
                f"H4A03AR3_{variant}_MID_SPREAD_PCT",
                f"{s['spread']:.3f}",
            )
            emit(
                f"H4A03AR3_{variant}_FREEZE_ACK_MEDIAN_MS",
                f"{s['freeze_ack_ms']:.3f}",
            )

        emit(
            "H4A03AR3_HIST_VS_CURRENT_MID_PCT",
            f"{delta_mid:.3f}",
        )
        emit(
            "H4A03AR3_BOUNDS_OVERLAP",
            intervals_overlap,
        )
        emit(
            "H4A03AR3_GUARANTEED_DIRECTION",
            direction,
        )
        emit(
            "H4A03AR3_GUARANTEED_GAP_PCT",
            f"{guaranteed_gap:.3f}",
        )
        emit(
            "H4A03AR3_REPEATABILITY_OK",
            repeatability_ok,
        )
        emit(
            "H4A03AR3_MATERIAL_THRESHOLD_PCT",
            MATERIAL_THRESHOLD_PCT,
        )
        emit(
            "H4A03AR3_CONFIRM_THRESHOLD_PCT",
            CONFIRM_THRESHOLD_PCT,
        )
        emit(
            "H4A03AR3_INTERPRETATION",
            interpretation,
        )

        answer = input(
            "TFT COM14 estable y operativo durante H4A0.3A-R3? (S/N): "
        ).strip().upper()
        if answer != "S":
            raise RuntimeError("H4A03AR3_TFT_PHYSICAL_REVIEW_FAILED")
        emit("H4A03AR3_TFT_PHYSICAL", "PASS")

        if git(repo, "diff", "--name-only") or git(
            repo, "diff", "--cached", "--name-only"
        ):
            raise RuntimeError("H4A03AR3_REPOSITORY_MUTATED")

        summary_log = temp_root / "SUMMARY.log"
        summary_log.write_text(
            "\n".join(
                [
                    "A14_H4A03AR3_MATCHED_HISTORICAL_CURRENT=PASS",
                    f"HEAD={head}",
                    f"HISTORICAL_COMMIT={historical_commit}",
                    f"H4A03AR3_HIST_MID_MEDIAN_MBPS={hist['mid']:.6f}",
                    f"H4A03AR3_CURRENT_MID_MEDIAN_MBPS={cur['mid']:.6f}",
                    f"H4A03AR3_HIST_VS_CURRENT_MID_PCT={delta_mid:.3f}",
                    f"H4A03AR3_BOUNDS_OVERLAP={intervals_overlap}",
                    f"H4A03AR3_GUARANTEED_DIRECTION={direction}",
                    f"H4A03AR3_GUARANTEED_GAP_PCT={guaranteed_gap:.3f}",
                    f"H4A03AR3_INTERPRETATION={interpretation}",
                    "H4A03AR3_DISPLAY_STACK_MATCHED=PASS",
                    "H4A03AR3_DISPLAY_STACK_SOURCE=CURRENT_HEAD_FOR_BOTH_VARIANTS",
                    "H4A03AR3_DISPLAY_STACK_LIBRARIES=JWPLC_Display+JWPLC_TFT",
                    "HARNESS_FAILURE=NO",
                    "PRODUCT_FAILURE=NO_EVIDENCE",
                    "HARDWARE_FAILURE=NO_EVIDENCE",
                    "COMPONENT_ABLATION=NO",
                    "NEXT=RETURN_TO_CHAT_INTERPRET_R3_DO_NOT_ABLATE",
                    "",
                ]
            ),
            encoding="utf-8",
        )

        emit("H4A03AR3_SUMMARY_LOG", summary_log)
        emit("HARNESS_FAILURE", "NO")
        emit("PRODUCT_FAILURE", "NO_EVIDENCE")
        emit("HARDWARE_FAILURE", "NO_EVIDENCE")
        emit("A14_H4A03AR3_MATCHED_HISTORICAL_CURRENT", "PASS")
        emit("NEXT", "RETURN_TO_CHAT_INTERPRET_R3_DO_NOT_ABLATE")
        return 0

    finally:
        if worktree_added:
            remove = run(
                [
                    "git",
                    "-C",
                    str(repo),
                    "worktree",
                    "remove",
                    "--force",
                    str(historical_worktree),
                ]
            )
            if remove.returncode != 0:
                emit(
                    "H4A03AR3_WORKTREE_CLEANUP_WARNING",
                    decode(remove.stdout + remove.stderr)[-1200:],
                )
            run(["git", "-C", str(repo), "worktree", "prune"])


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:
        emit("HARNESS_FAILURE", "UNCLASSIFIED")
        emit("PRODUCT_FAILURE", "UNCLASSIFIED")
        emit("HARDWARE_FAILURE", "UNCLASSIFIED")
        emit("H4A03AR3_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("H4A03AR3_EXCEPTION", str(exc))
        raise SystemExit(1)
