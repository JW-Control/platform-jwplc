#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import os
import re
import shutil
import statistics
import subprocess
import tempfile
from pathlib import Path

EXPECTED_BRANCH = "v2.1.0-alpha.14/feature/modbus-tcp"
CLOSURE_DOC = "docs/v2.1.0-alpha.14/A14_P3_UDP_RX_CLOSURE_20260924.md"
P3K_GATE = "tools/modbus-tcp-benchmark/gates/a14_p3k_tcp_udp_same_session_parity.ps1"
P3K_BRIDGE = "tools/modbus-tcp-benchmark/gates/a14_p3k_transport_parity_bridge.py"
RAW_RUNNER = "tools/modbus-tcp-benchmark/pc/eth14_raw_transport_benchmark.py"
RAW_FIRMWARE = "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
COMMON = "tools/modbus-tcp-benchmark/gates/common.ps1"
VALIDATOR = "tools/modbus-tcp-benchmark/gates/assert_ps1_syntax.ps1"

HIST = {
    "udp_b1": 13.860041,
    "udp_b2": 13.872847,
    "udp_all": 13.866349,
    "tcp_b1": 13.413063,
    "tcp_b2": 13.344293,
    "tcp_all": 13.412507,
}
R1_MINIMAL_TAIL0_MBPS = 12.501134
UDP_NOMINAL_TAIL_S = 0.40
TCP_NOMINAL_TAIL_S = 0.50


def emit(key: str, value: object) -> None:
    print(f"{key}={value}")


def run(cmd: list[str], cwd: Path | None = None) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def decode(data: bytes) -> str:
    if data.startswith(b"\xff\xfe") or data.startswith(b"\xfe\xff"):
        return data.decode("utf-16")
    for encoding in ("utf-8-sig", "cp1252"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            pass
    return data.decode("utf-8", errors="replace")


def read_auto(path: Path) -> str:
    return decode(path.read_bytes())


def git(repo: Path, *args: str) -> str:
    # Git machine-readable data belongs to stdout. Keep stderr separate so
    # Windows line-ending warnings (for example LF -> CRLF) cannot be parsed
    # as paths or hashes by source-contract checks.
    p = subprocess.run(
        ["git", "-C", str(repo), *args],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    stdout_text = decode(p.stdout)
    stderr_text = decode(p.stderr)
    if p.returncode != 0:
        detail = (stdout_text + "\n" + stderr_text).strip()
        raise RuntimeError(f"GIT_FAILED {' '.join(args)} :: {detail[-1200:]}")
    return stdout_text.strip()


def git_show_bytes(repo: Path, commit: str, path: str) -> bytes | None:
    p = subprocess.run(
        ["git", "-C", str(repo), "show", f"{commit}:{path}"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if p.returncode != 0:
        return None
    return p.stdout


def sha256_git_path_variants(
    repo: Path,
    commit: str,
    path: str,
) -> dict[str, str] | None:
    data = git_show_bytes(repo, commit, path)
    if data is None:
        return None

    # Historical P3K froze hashes with Get-FileHash over a Windows checkout.
    # Git blobs are stored with repository bytes (normally LF for text), while
    # the checkout used for the physical gate can be CRLF via core.autocrlf.
    # Compare both representations during historical discovery; the original
    # P3K gate will later validate the actual checked-out working-tree hash.
    lf = data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    crlf = lf.replace(b"\n", b"\r\n")

    return {
        "BLOB": hashlib.sha256(data).hexdigest().upper(),
        "LF": hashlib.sha256(lf).hexdigest().upper(),
        "CRLF": hashlib.sha256(crlf).hexdigest().upper(),
    }


def hash_variant_match(
    variants: dict[str, str] | None,
    expected: str,
) -> str | None:
    if variants is None:
        return None
    for representation in ("BLOB", "LF", "CRLF"):
        if variants[representation] == expected:
            return representation
    return None


def parse_p3k_expected_hashes(gate_text: str) -> dict[str, str]:
    patterns = {
        "raw": r'\$expectedRawHash\s*=\s*"([0-9A-F]{64})"',
        "w5100_cpp": r'\$expectedW5100CppHash\s*=\s*"([0-9A-F]{64})"',
        "w5100_h": r'\$expectedW5100HHash\s*=\s*"([0-9A-F]{64})"',
    }
    values: dict[str, str] = {}
    for key, pattern in patterns.items():
        match = re.search(pattern, gate_text)
        if not match:
            raise RuntimeError(f"H4A03AR2_EXPECTED_HASH_NOT_FOUND={key}")
        values[key] = match.group(1)
    return values


def collect_historical_commit_candidates(
    repo: Path,
    closure_commit: str,
) -> list[tuple[str, str]]:
    ordered: list[tuple[str, str]] = []
    seen: set[str] = set()

    def add(source: str, values: list[str]) -> None:
        for value in values:
            commit = value.strip()
            if not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
                continue
            commit = commit.lower()
            if commit in seen:
                continue
            seen.add(commit)
            ordered.append((source, commit))

    # 1) First-parent/ancestor history around the documented closure.
    add(
        "CLOSURE_ANCESTRY",
        git(repo, "rev-list", closure_commit).splitlines(),
    )

    # 2) Every currently reachable ref in the local clone.
    add(
        "ALL_REFS",
        git(repo, "rev-list", "--all").splitlines(),
    )

    # 3) Reflogs preserve force-pushed/rebased commits for a retention window.
    reflog = subprocess.run(
        [
            "git", "-C", str(repo), "reflog", "show", "--all",
            "--format=%H",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if reflog.returncode == 0:
        add("REFLOG", decode(reflog.stdout).splitlines())
        emit("H4A03AR2_REFLOG_SCAN", "AVAILABLE")
    else:
        emit("H4A03AR2_REFLOG_SCAN", "UNAVAILABLE")

    # 4) Last resort: commits still present in .git but unreachable from refs
    # and reflogs. These are exactly the objects commonly left by rebases.
    fsck = subprocess.run(
        [
            "git", "-C", str(repo), "fsck", "--full", "--no-reflogs",
            "--unreachable",
        ],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if fsck.returncode in (0, 1):
        unreachable: list[str] = []
        for line in decode(fsck.stdout).splitlines():
            match = re.match(
                r"^unreachable commit ([0-9a-fA-F]{40})$",
                line.strip(),
            )
            if match:
                unreachable.append(match.group(1))
        add("UNREACHABLE", unreachable)
        emit("H4A03AR2_UNREACHABLE_COMMIT_COUNT", len(unreachable))
    else:
        emit("H4A03AR2_UNREACHABLE_SCAN", "UNAVAILABLE")

    return ordered


def find_exact_p3k_snapshot(
    repo: Path,
    closure_commit: str,
    expected: dict[str, str],
) -> tuple[str, int, str, dict[str, int]]:
    raw_path = "tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino"
    wcpp_path = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
    wh_path = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h"

    candidates = collect_historical_commit_candidates(repo, closure_commit)
    source_counts: dict[str, int] = {}
    for source, _ in candidates:
        source_counts[source] = source_counts.get(source, 0) + 1

    emit("H4A03AR2_HISTORICAL_CANDIDATE_COUNT", len(candidates))
    for source in ("CLOSURE_ANCESTRY", "ALL_REFS", "REFLOG", "UNREACHABLE"):
        emit(
            f"H4A03AR2_CANDIDATES_{source}",
            source_counts.get(source, 0),
        )

    for checked, (source, commit) in enumerate(candidates, 1):
        raw_variants = sha256_git_path_variants(repo, commit, raw_path)
        raw_repr = hash_variant_match(raw_variants, expected["raw"])
        if raw_repr is None:
            continue

        wcpp_variants = sha256_git_path_variants(repo, commit, wcpp_path)
        wcpp_repr = hash_variant_match(
            wcpp_variants,
            expected["w5100_cpp"],
        )
        if wcpp_repr is None:
            continue

        wh_variants = sha256_git_path_variants(repo, commit, wh_path)
        wh_repr = hash_variant_match(wh_variants, expected["w5100_h"])
        if wh_repr is None:
            continue

        gate_bytes = git_show_bytes(repo, commit, P3K_GATE)
        bridge_bytes = git_show_bytes(repo, commit, P3K_BRIDGE)
        common_bytes = git_show_bytes(repo, commit, COMMON)
        if gate_bytes is None or bridge_bytes is None or common_bytes is None:
            continue

        gate_text = decode(gate_bytes)
        if parse_p3k_expected_hashes(gate_text) != expected:
            continue

        # The original gate had a clean-tree invariant and protected-artifact
        # checks. Keep the complete historical gate/common pair from this same
        # commit; do not synthesize a hybrid snapshot.
        emit("H4A03AR2_MATCH_RAW_REPRESENTATION", raw_repr)
        emit("H4A03AR2_MATCH_W5100_CPP_REPRESENTATION", wcpp_repr)
        emit("H4A03AR2_MATCH_W5100_H_REPRESENTATION", wh_repr)
        return commit, checked, source, source_counts

    raise RuntimeError(
        "H4A03AR2_EXACT_P3K_SNAPSHOT_NOT_FOUND_IN_"
        "ANCESTRY_REFS_REFLOG_OR_UNREACHABLE_OBJECTS"
    )


def values(text: str, key: str) -> list[str]:
    return [
        match.strip()
        for match in re.findall(
            rf"(?m)^{re.escape(key)}=(.*)\r?$",
            text,
        )
    ]


def unique(text: str, key: str) -> str:
    matches = values(text, key)
    if len(matches) != 1:
        raise RuntimeError(f"H4A03AR2_KEY_COUNT_{key}={len(matches)}")
    return matches[0]


def as_float(text: str, key: str) -> float:
    return float(unique(text, key))


def median(values: list[float]) -> float:
    if not values:
        raise RuntimeError("H4A03AR2_EMPTY_MEDIAN")
    return float(statistics.median(values))


def pct(current: float, reference: float) -> float:
    return (current / reference - 1.0) * 100.0


def source_contract(worktree: Path, closure_text: str) -> None:
    gate_text = (worktree / P3K_GATE).read_text(encoding="utf-8")
    bridge_text = (worktree / P3K_BRIDGE).read_text(encoding="utf-8")
    raw_text = (worktree / RAW_RUNNER).read_text(encoding="utf-8")

    checks = {
        "CLOSURE_UDP_13P866349": "UDP median = 13.866349 Mbps" in closure_text,
        "CLOSURE_TCP_13P412507": "TCP median = 13.412507 Mbps" in closure_text,
        "DURATION_DEFAULT_5S": '[double]$DurationSeconds = 5.0' in gate_text,
        "RUNS_DEFAULT_3": '[int]$Runs = 3' in gate_text,
        "UDP_PAYLOAD_1016": '$payloads = @(1016)' in gate_text,
        "VARIANT_COMMIT2_R1": '$variants = @("INT_COMMIT2_R1")' in gate_text,
        "BLOCK1_ORDER": '1 = @("udp-rx", "tcp-rx", "udp-rx", "tcp-rx", "udp-rx", "tcp-rx")' in gate_text,
        "BLOCK2_ORDER": '2 = @("tcp-rx", "udp-rx", "tcp-rx", "udp-rx", "tcp-rx", "udp-rx")' in gate_text,
        "TCP_CHUNK_4096": '"--tcp-chunk", "4096"' in gate_text,
        "RUN_UDP_PAYLOAD_1016": '"--udp-payload", "1016"' in gate_text,
        "UDP_TAIL_400MS": "time.sleep(0.40)" in bridge_text,
        "TCP_WRAPPED_ORIGINAL": "_original_tcp_rx_bench = runner.tcp_rx_bench" in bridge_text,
    }

    tcp_start = raw_text.find("def tcp_rx_bench(")
    tcp_end = raw_text.find("\ndef tcp_tx_bench(", tcp_start)
    if tcp_start < 0 or tcp_end < 0:
        raise RuntimeError("H4A03AR2_TCP_RX_SOURCE_SPAN_NOT_FOUND")
    tcp_body = raw_text[tcp_start:tcp_end]
    checks["TCP_TAIL_300MS"] = "time.sleep(0.30)" in tcp_body
    checks["TCP_TAIL_EXTRA_200MS"] = "time.sleep(0.20)" in tcp_body

    for name, ok in checks.items():
        emit(f"H4A03AR2_CONTRACT_{name}", "PASS" if ok else "FAIL")

    failed = [name for name, ok in checks.items() if not ok]
    if failed:
        raise RuntimeError("H4A03AR2_SOURCE_CONTRACT_FAILED=" + ",".join(failed))

    emit("H4A03AR2_SOURCE_CONTRACT", "PASS")


def inspect_run(path: Path, mode: str) -> dict[str, float]:
    text = read_auto(path)
    if unique(text, "RAW_BENCH_FUNCTIONAL_PASS") != "YES":
        raise RuntimeError(f"H4A03AR2_FUNCTIONAL_FAIL={path}")

    duration_values = values(text, "DURATION_S")
    if len(duration_values) != 2:
        raise RuntimeError(
            f"H4A03AR2_DURATION_KEY_COUNT={len(duration_values)} PATH={path}"
        )

    requested_duration = float(duration_values[0])
    duration = float(duration_values[1])
    if requested_duration <= 0.0 or duration <= 0.0:
        raise RuntimeError(f"H4A03AR2_INVALID_DURATION={path}")

    if abs(requested_duration - 5.0) > 0.000001:
        raise RuntimeError(
            f"H4A03AR2_UNEXPECTED_REQUESTED_DURATION="
            f"{requested_duration:.6f} PATH={path}"
        )

    dut_bytes = as_float(text, "DUT_BYTES")
    summary_key = (
        "SUMMARY_UDP_RX_DUT_MBPS"
        if mode == "UDP"
        else "SUMMARY_TCP_RX_DUT_MBPS"
    )
    reported = as_float(text, summary_key)
    recomputed = dut_bytes * 8.0 / duration / 1_000_000.0

    if abs(recomputed - reported) > 0.00001:
        raise RuntimeError(
            f"H4A03AR2_REPORTED_RECOMPUTE_MISMATCH={path} "
            f"REPORTED={reported:.6f} RECOMPUTED={recomputed:.6f}"
        )

    tail = UDP_NOMINAL_TAIL_S if mode == "UDP" else TCP_NOMINAL_TAIL_S
    corrected = dut_bytes * 8.0 / (duration + tail) / 1_000_000.0

    return {
        "requested_duration": requested_duration,
        "duration": duration,
        "bytes": dut_bytes,
        "reported": reported,
        "corrected": corrected,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.3A-R2 historical P3K source-tree replay + accounting audit"
    )
    parser.add_argument("--serial", default="COM14")
    args = parser.parse_args()

    script = Path(__file__).resolve()
    repo = script.parents[3]

    print("============================================================")
    print(" A14 H4A0.3A-R2 - HISTORICAL P3K REPLAY + ACCOUNTING AUDIT")
    print("============================================================")

    branch = git(repo, "branch", "--show-current")
    current_head = git(repo, "rev-parse", "HEAD")
    emit("BRANCH", branch)
    emit("HEAD", current_head)
    emit("SERIAL_PORT", args.serial)
    emit("H4A03AR2_REPLAY_SCOPE", "P3_PRE_CLOSURE_TREE_PLUS_VERIFIED_RAW_WORKTREE_BYTES_CURRENT_HOST_TOOLCHAIN")
    emit("H4A03AR2_PRODUCT_SOURCE_MUTATION", "NO")
    emit("H4A03AR2_COMPONENT_ABLATION", "NO")

    if branch != EXPECTED_BRANCH:
        raise RuntimeError("H4A03AR2_BRANCH_MISMATCH")
    if git(repo, "diff", "--name-only") or git(repo, "diff", "--cached", "--name-only"):
        raise RuntimeError("H4A03AR2_CURRENT_TREE_NOT_CLEAN")

    product_untracked = [
        p.replace("\\", "/")
        for p in git(repo, "ls-files", "--others", "--exclude-standard").splitlines()
        if p.replace("\\", "/").startswith(
            ("JWPLC/2.1.0/", "tools/modbus-tcp-benchmark/firmware/")
        )
    ]
    emit("H4A03AR2_UNTRACKED_PRODUCT_COUNT", len(product_untracked))
    if product_untracked:
        raise RuntimeError("H4A03AR2_UNTRACKED_PRODUCT_SOURCE_FOUND")

    closure_candidates = [
        x.strip()
        for x in git(
            repo,
            "log",
            "--format=%H",
            "--diff-filter=A",
            "--",
            CLOSURE_DOC,
        ).splitlines()
        if x.strip()
    ]
    emit("H4A03AR2_CLOSURE_COMMIT_CANDIDATE_COUNT", len(closure_candidates))
    if len(closure_candidates) != 1:
        raise RuntimeError(
            f"H4A03AR2_CLOSURE_COMMIT_NOT_UNIQUE={len(closure_candidates)}"
        )

    closure_commit = closure_candidates[0]
    emit("H4A03AR2_P3_CLOSURE_COMMIT", closure_commit)

    closure_doc_bytes = git_show_bytes(repo, closure_commit, CLOSURE_DOC)
    closure_gate_bytes = git_show_bytes(repo, closure_commit, P3K_GATE)
    if closure_doc_bytes is None or closure_gate_bytes is None:
        raise RuntimeError("H4A03AR2_CLOSURE_EVIDENCE_MISSING")

    closure_text = decode(closure_doc_bytes)
    expected_hashes = parse_p3k_expected_hashes(decode(closure_gate_bytes))
    emit("H4A03AR2_EXPECTED_RAW_SHA256", expected_hashes["raw"])
    emit("H4A03AR2_EXPECTED_W5100_CPP_SHA256", expected_hashes["w5100_cpp"])
    emit("H4A03AR2_EXPECTED_W5100_H_SHA256", expected_hashes["w5100_h"])

    replay_commit = git(repo, "rev-parse", f"{closure_commit}^")
    replay_source = "P3_CLOSURE_PARENT"
    emit("H4A03AR2_REPLAY_SOURCE", replay_source)
    emit("H4A03AR2_REPLAY_COMMIT", replay_commit)

    closure_delta = [
        line.strip().replace("\\", "/")
        for line in git(
            repo,
            "diff",
            "--name-only",
            replay_commit,
            closure_commit,
        ).splitlines()
        if line.strip()
    ]
    emit("H4A03AR2_CLOSURE_DELTA_COUNT", len(closure_delta))
    emit("H4A03AR2_CLOSURE_DELTA", ",".join(closure_delta))
    if closure_delta != [CLOSURE_DOC]:
        raise RuntimeError(
            "H4A03AR2_CLOSURE_PARENT_NOT_PURE_DOC_ADD="
            + ",".join(closure_delta)
        )

    current_raw_path = repo / RAW_FIRMWARE
    if not current_raw_path.is_file():
        raise RuntimeError("H4A03AR2_CURRENT_RAW_FILE_MISSING")

    current_raw_bytes = current_raw_path.read_bytes()
    current_raw_sha = hashlib.sha256(current_raw_bytes).hexdigest().upper()
    emit("H4A03AR2_CURRENT_RAW_WORKTREE_SHA256", current_raw_sha)
    if current_raw_sha != expected_hashes["raw"]:
        raise RuntimeError(
            "H4A03AR2_CURRENT_RAW_NO_LONGER_MATCHES_P3K_EXPECTED"
        )

    replay_raw_blob = git_show_bytes(repo, replay_commit, RAW_FIRMWARE)
    if replay_raw_blob is None:
        raise RuntimeError("H4A03AR2_REPLAY_RAW_BLOB_MISSING")

    def normalize_eol(data: bytes) -> bytes:
        return data.replace(b"\r\n", b"\n").replace(b"\r", b"\n")

    raw_semantic_match = (
        normalize_eol(current_raw_bytes)
        == normalize_eol(replay_raw_blob)
    )
    emit("H4A03AR2_RAW_SEMANTIC_MATCH_TO_REPLAY_TREE", raw_semantic_match)
    if not raw_semantic_match:
        raise RuntimeError(
            "H4A03AR2_CURRENT_RAW_SEMANTICS_DIFFER_FROM_REPLAY_TREE"
        )

    emit(
        "H4A03AR2_RAW_RESTORATION_REASON",
        "PRESERVE_HISTORICAL_GET_FILE_HASH_EOL_REPRESENTATION_ONLY",
    )

    result_root = Path(tempfile.mkdtemp(prefix="jwplc_a14_h4a03ar2_"))
    worktree = result_root / "historical_worktree"
    temp_branch = f"h4a03ar2-p3k-{replay_commit[:8]}-{os.getpid()}"
    emit("H4A03AR2_RESULT_ROOT", result_root)
    emit("H4A03AR2_TEMP_BRANCH", temp_branch)

    worktree_added = False
    temp_branch_created = False

    try:
        add = run([
            "git", "-C", str(repo), "worktree", "add",
            "-b", temp_branch, str(worktree), replay_commit
        ])
        if add.returncode != 0:
            raise RuntimeError(
                "H4A03AR2_WORKTREE_ADD_FAILED=" + decode(add.stdout)[-1200:]
            )
        worktree_added = True
        temp_branch_created = True

        historical_head = git(worktree, "rev-parse", "HEAD")
        emit("H4A03AR2_HISTORICAL_WORKTREE_HEAD", historical_head)
        if historical_head != replay_commit:
            raise RuntimeError("H4A03AR2_HISTORICAL_HEAD_MISMATCH")

        for relative in (P3K_GATE, P3K_BRIDGE, RAW_RUNNER, COMMON, VALIDATOR):
            if not (worktree / relative).is_file():
                raise RuntimeError(f"H4A03AR2_HISTORICAL_FILE_MISSING={relative}")

        source_contract(worktree, closure_text)

        replay_gate_text = (worktree / P3K_GATE).read_text(encoding="utf-8")
        replay_expected = parse_p3k_expected_hashes(replay_gate_text)
        if replay_expected != expected_hashes:
            raise RuntimeError("H4A03AR2_REPLAY_EXPECTED_HASH_CONTRACT_DRIFT")

        historical_raw_path = worktree / RAW_FIRMWARE
        historical_raw_before = historical_raw_path.read_bytes()
        semantic_before = (
            normalize_eol(historical_raw_before)
            == normalize_eol(current_raw_bytes)
        )
        emit(
            "H4A03AR2_WORKTREE_RAW_SEMANTIC_MATCH_BEFORE_RESTORE",
            semantic_before,
        )
        if not semantic_before:
            raise RuntimeError(
                "H4A03AR2_WORKTREE_RAW_SEMANTICS_NOT_EQUIVALENT"
            )

        historical_raw_path.write_bytes(current_raw_bytes)
        restored_raw_sha = hashlib.sha256(
            historical_raw_path.read_bytes()
        ).hexdigest().upper()
        emit(
            "H4A03AR2_WORKTREE_RAW_RESTORED_SHA256",
            restored_raw_sha,
        )
        if restored_raw_sha != expected_hashes["raw"]:
            raise RuntimeError(
                "H4A03AR2_WORKTREE_RAW_RESTORE_HASH_MISMATCH"
            )

        raw_visible_diff = git(
            worktree,
            "diff",
            "--name-only",
            "--",
            RAW_FIRMWARE,
        )
        emit(
            "H4A03AR2_RAW_RESTORE_GIT_DIFF_VISIBLE",
            bool(raw_visible_diff),
        )
        if raw_visible_diff:
            raise RuntimeError(
                "H4A03AR2_RAW_EOL_RESTORE_CHANGED_GIT_SEMANTICS"
            )

        emit(
            "H4A03AR2_REPLAY_HASH_CONTRACT",
            "DEFERRED_TO_ORIGINAL_P3K_GET_FILE_HASH_AFTER_EOL_RESTORE",
        )

        common_path = worktree / COMMON
        common_text = common_path.read_text(encoding="utf-8")
        old_branch_line = (
            '$script:G2ExpectedBranch = '
            '"v2.1.0-alpha.14/feature/modbus-tcp"'
        )
        new_branch_line = f'$script:G2ExpectedBranch = "{temp_branch}"'

        if common_text.count(old_branch_line) != 1:
            raise RuntimeError(
                "H4A03AR2_COMMON_BRANCH_GUARD_MATCH_COUNT="
                f"{common_text.count(old_branch_line)}"
            )

        patched_common = common_text.replace(old_branch_line, new_branch_line, 1)
        common_path.write_text(patched_common, encoding="utf-8", newline="\n")

        changed = git(worktree, "diff", "--name-only")
        if changed.replace("\\", "/") != COMMON:
            raise RuntimeError(f"H4A03AR2_TEMP_PATCH_SCOPE_INVALID={changed}")

        git(worktree, "add", "--", COMMON)
        commit = run([
            "git", "-C", str(worktree),
            "-c", "user.name=JWPLC Historical Replay",
            "-c", "user.email=replay@local.invalid",
            "commit", "-m", "test(local): habilitar replay P3K historico",
        ])
        if commit.returncode != 0:
            raise RuntimeError(
                "H4A03AR2_TEMP_GUARD_COMMIT_FAILED=" + decode(commit.stdout)[-1200:]
            )

        replay_head = git(worktree, "rev-parse", "HEAD")
        emit("H4A03AR2_TEMP_REPLAY_HEAD", replay_head)
        emit("H4A03AR2_TEMP_PATCH_SCOPE", "COMMON_EXPECTED_BRANCH_ONLY")
        if git(worktree, "diff", "--name-only") or git(
            worktree, "diff", "--cached", "--name-only"
        ):
            raise RuntimeError("H4A03AR2_HISTORICAL_WORKTREE_NOT_CLEAN_AFTER_GUARD_COMMIT")

        powershell = shutil.which("powershell.exe") or shutil.which("powershell")
        if not powershell:
            raise RuntimeError("H4A03AR2_POWERSHELL_NOT_FOUND")

        gate = worktree / P3K_GATE
        validator = worktree / VALIDATOR
        syntax = run([
            powershell, "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", str(validator), "-Path", str(gate)
        ])
        syntax_text = decode(syntax.stdout)
        print(syntax_text, end="" if syntax_text.endswith("\n") else "\n")
        emit("H4A03AR2_HISTORICAL_PS1_SYNTAX_EXIT", syntax.returncode)
        if syntax.returncode != 0:
            raise RuntimeError("H4A03AR2_HISTORICAL_PS1_SYNTAX_FAILED")

        child = run([
            powershell, "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass",
            "-File", str(gate),
            "-SerialPort", args.serial,
            "-DurationSeconds", "5.0",
            "-Runs", "3",
        ])
        child_text = decode(child.stdout)
        child_log = result_root / "p3k_historical_exact_replay.log"
        child_log.write_text(child_text, encoding="utf-8")
        emit("H4A03AR2_CHILD_EXIT", child.returncode)
        emit("H4A03AR2_CHILD_LOG", child_log)

        if child.returncode != 0:
            print("=== H4A0.3A-R2 CHILD FAILURE TAIL ===")
            for line in child_text.splitlines()[-140:]:
                print(line)
            raise RuntimeError("H4A03AR2_HISTORICAL_P3K_CHILD_FAILED")

        if unique(child_text, "A14_P3K_SAME_SESSION_TCP_UDP_PARITY") != "PASS":
            raise RuntimeError("H4A03AR2_P3K_FINAL_MARKER_MISSING")
        if unique(child_text, "A14_P3K_PRODUCT_SOURCE_MUTATION") != "NO":
            raise RuntimeError("H4A03AR2_P3K_PRODUCT_MUTATION_CONTRACT_FAILED")
        if unique(child_text, "A14_P3K_DIAGNOSTIC_COPY_ONLY") != "YES":
            raise RuntimeError("H4A03AR2_P3K_DIAGNOSTIC_COPY_CONTRACT_FAILED")

        original_temp = Path(unique(child_text, "TEMP_ROOT"))
        if not original_temp.is_dir():
            raise RuntimeError(
                f"H4A03AR2_ORIGINAL_TEMP_ROOT_MISSING={original_temp}"
            )
        emit("H4A03AR2_ORIGINAL_P3K_TEMP_ROOT", original_temp)

        raw_values = {"UDP": {1: [], 2: []}, "TCP": {1: [], 2: []}}
        corrected = {"UDP": {1: [], 2: []}, "TCP": {1: [], 2: []}}

        variant_root = original_temp / "INT_COMMIT2_R1"
        for block in (1, 2):
            for mode in ("UDP", "TCP"):
                for run_no in (1, 2, 3):
                    run_log = (
                        variant_root
                        / f"p3k_block_{block}_{mode}_RX_run_{run_no}.log"
                    )
                    if not run_log.is_file():
                        raise RuntimeError(
                            f"H4A03AR2_RUN_LOG_MISSING={run_log}"
                        )
                    row = inspect_run(run_log, mode)
                    raw_values[mode][block].append(row["reported"])
                    corrected[mode][block].append(row["corrected"])
                    emit(
                        f"H4A03AR2_B{block}_{mode}_R{run_no}",
                        f"RAW={row['reported']:.6f},"
                        f"CORRECTED={row['corrected']:.6f},"
                        f"DURATION={row['duration']:.6f},"
                        f"BYTES={int(row['bytes'])}",
                    )

        raw_udp_b1 = median(raw_values["UDP"][1])
        raw_udp_b2 = median(raw_values["UDP"][2])
        raw_tcp_b1 = median(raw_values["TCP"][1])
        raw_tcp_b2 = median(raw_values["TCP"][2])
        raw_udp_all = median(raw_values["UDP"][1] + raw_values["UDP"][2])
        raw_tcp_all = median(raw_values["TCP"][1] + raw_values["TCP"][2])

        cor_udp_b1 = median(corrected["UDP"][1])
        cor_udp_b2 = median(corrected["UDP"][2])
        cor_tcp_b1 = median(corrected["TCP"][1])
        cor_tcp_b2 = median(corrected["TCP"][2])
        cor_udp_all = median(corrected["UDP"][1] + corrected["UDP"][2])
        cor_tcp_all = median(corrected["TCP"][1] + corrected["TCP"][2])

        child_udp = as_float(child_text, "P3K_UDP_MEDIAN_MBPS")
        child_tcp = as_float(child_text, "P3K_TCP_MEDIAN_MBPS")
        if (
            abs(child_udp - raw_udp_all) > 0.00001
            or abs(child_tcp - raw_tcp_all) > 0.00001
        ):
            raise RuntimeError("H4A03AR2_ORIGINAL_SUMMARY_RECOMPUTE_MISMATCH")

        print("============================================================")
        print(" H4A0.3A-R2 HISTORICAL REPLAY + ACCOUNTING SUMMARY")
        print("============================================================")
        emit("H4A03AR2_HISTORICAL_REPORTED_UDP_MBPS", f"{HIST['udp_all']:.6f}")
        emit("H4A03AR2_HISTORICAL_REPORTED_TCP_MBPS", f"{HIST['tcp_all']:.6f}")
        emit("H4A03AR2_REPLAY_RAW_UDP_B1_MBPS", f"{raw_udp_b1:.6f}")
        emit("H4A03AR2_REPLAY_RAW_TCP_B1_MBPS", f"{raw_tcp_b1:.6f}")
        emit("H4A03AR2_REPLAY_RAW_UDP_B2_MBPS", f"{raw_udp_b2:.6f}")
        emit("H4A03AR2_REPLAY_RAW_TCP_B2_MBPS", f"{raw_tcp_b2:.6f}")
        emit("H4A03AR2_REPLAY_RAW_UDP_MEDIAN_MBPS", f"{raw_udp_all:.6f}")
        emit("H4A03AR2_REPLAY_RAW_TCP_MEDIAN_MBPS", f"{raw_tcp_all:.6f}")
        emit(
            "H4A03AR2_REPLAY_RAW_UDP_VS_TCP_PCT",
            f"{pct(raw_udp_all, raw_tcp_all):.2f}",
        )
        emit(
            "H4A03AR2_RAW_UDP_VS_HISTORICAL_PCT",
            f"{pct(raw_udp_all, HIST['udp_all']):.2f}",
        )
        emit(
            "H4A03AR2_RAW_TCP_VS_HISTORICAL_PCT",
            f"{pct(raw_tcp_all, HIST['tcp_all']):.2f}",
        )
        emit(
            "H4A03AR2_RAW_UDP_WITHIN_0P5PCT_HISTORICAL",
            abs(pct(raw_udp_all, HIST["udp_all"])) <= 0.5,
        )
        emit(
            "H4A03AR2_RAW_TCP_WITHIN_0P5PCT_HISTORICAL",
            abs(pct(raw_tcp_all, HIST["tcp_all"])) <= 0.5,
        )

        emit("H4A03AR2_CORRECTED_UDP_B1_MBPS", f"{cor_udp_b1:.6f}")
        emit("H4A03AR2_CORRECTED_TCP_B1_MBPS", f"{cor_tcp_b1:.6f}")
        emit("H4A03AR2_CORRECTED_UDP_B2_MBPS", f"{cor_udp_b2:.6f}")
        emit("H4A03AR2_CORRECTED_TCP_B2_MBPS", f"{cor_tcp_b2:.6f}")
        emit("H4A03AR2_CORRECTED_UDP_MEDIAN_MBPS", f"{cor_udp_all:.6f}")
        emit("H4A03AR2_CORRECTED_TCP_MEDIAN_MBPS", f"{cor_tcp_all:.6f}")
        emit(
            "H4A03AR2_CORRECTED_UDP_VS_TCP_PCT",
            f"{pct(cor_udp_all, cor_tcp_all):.2f}",
        )
        emit(
            "H4A03AR2_UDP_ACCOUNTING_INFLATION_PCT",
            f"{pct(raw_udp_all, cor_udp_all):.2f}",
        )
        emit(
            "H4A03AR2_TCP_ACCOUNTING_INFLATION_PCT",
            f"{pct(raw_tcp_all, cor_tcp_all):.2f}",
        )
        emit(
            "H4A03AR2_CORRECTED_UDP_VS_R1_MINIMAL_TAIL0_PCT",
            f"{pct(cor_udp_all, R1_MINIMAL_TAIL0_MBPS):.2f}",
        )
        emit(
            "H4A03AR2_R1_MINIMAL_TAIL0_MBPS",
            f"{R1_MINIMAL_TAIL0_MBPS:.6f}",
        )
        emit(
            "H4A03AR2_CORRECTED_METRIC_SCOPE",
            "NOMINAL_TAIL_ACCOUNTING_NOT_EXACT_CAPTURE_TIMESTAMP",
        )
        emit("H4A03AR2_PERFORMANCE_VERDICT_IS_AUTOMATIC", "NO")

        answer = input(
            "TFT COM14 estable y operativo durante H4A0.3A-R2? (S/N): "
        ).strip().upper()
        emit("H4A03AR2_TFT_PHYSICAL_PASS", answer == "S")
        if answer != "S":
            raise RuntimeError("H4A03AR2_TFT_PHYSICAL_REVIEW")

        summary = result_root / "SUMMARY.log"
        summary.write_text(
            "\n".join([
                "A14_H4A03AR2_HISTORICAL_P3K_REPLAY_AUDIT=PASS",
                f"CURRENT_HEAD={current_head}",
                f"P3_CLOSURE_COMMIT={closure_commit}",
                f"P3_REPLAY_SOURCE={replay_source}",
                f"P3_REPLAY_COMMIT={replay_commit}",
                "REPLAY_SCOPE=P3_PRE_CLOSURE_TREE_PLUS_VERIFIED_RAW_WORKTREE_BYTES_CURRENT_HOST_TOOLCHAIN",
                f"H4A03AR2_HISTORICAL_REPORTED_UDP_MBPS={HIST['udp_all']:.6f}",
                f"H4A03AR2_HISTORICAL_REPORTED_TCP_MBPS={HIST['tcp_all']:.6f}",
                f"H4A03AR2_REPLAY_RAW_UDP_MEDIAN_MBPS={raw_udp_all:.6f}",
                f"H4A03AR2_REPLAY_RAW_TCP_MEDIAN_MBPS={raw_tcp_all:.6f}",
                f"H4A03AR2_RAW_UDP_VS_HISTORICAL_PCT={pct(raw_udp_all, HIST['udp_all']):.2f}",
                f"H4A03AR2_RAW_TCP_VS_HISTORICAL_PCT={pct(raw_tcp_all, HIST['tcp_all']):.2f}",
                f"H4A03AR2_CORRECTED_UDP_MEDIAN_MBPS={cor_udp_all:.6f}",
                f"H4A03AR2_CORRECTED_TCP_MEDIAN_MBPS={cor_tcp_all:.6f}",
                f"H4A03AR2_CORRECTED_UDP_VS_TCP_PCT={pct(cor_udp_all, cor_tcp_all):.2f}",
                f"H4A03AR2_UDP_ACCOUNTING_INFLATION_PCT={pct(raw_udp_all, cor_udp_all):.2f}",
                f"H4A03AR2_TCP_ACCOUNTING_INFLATION_PCT={pct(raw_tcp_all, cor_tcp_all):.2f}",
                f"H4A03AR2_CORRECTED_UDP_VS_R1_MINIMAL_TAIL0_PCT={pct(cor_udp_all, R1_MINIMAL_TAIL0_MBPS):.2f}",
                "H4A03AR2_PRODUCT_SOURCE_MUTATION=NO",
                "HARNESS_FAILURE=NO",
                "PRODUCT_FAILURE=NO_EVIDENCE",
                "HARDWARE_FAILURE=NO_EVIDENCE",
                "NEXT=RETURN_TO_CHAT_INTERPRET_HISTORICAL_REPLAY_DO_NOT_ABLATE",
            ]) + "\n",
            encoding="utf-8",
        )
        emit("H4A03AR2_SUMMARY_LOG", summary)

    finally:
        if worktree_added:
            remove = run([
                "git", "-C", str(repo), "worktree", "remove", "--force", str(worktree)
            ])
            if remove.returncode != 0:
                emit(
                    "H4A03AR2_WORKTREE_CLEANUP_WARNING",
                    decode(remove.stdout)[-800:].replace("\n", " "),
                )
        if temp_branch_created:
            delete = run(["git", "-C", str(repo), "branch", "-D", temp_branch])
            if delete.returncode != 0:
                emit(
                    "H4A03AR2_TEMP_BRANCH_CLEANUP_WARNING",
                    decode(delete.stdout)[-800:].replace("\n", " "),
                )
        run(["git", "-C", str(repo), "worktree", "prune"])

    if git(repo, "diff", "--name-only") or git(repo, "diff", "--cached", "--name-only"):
        raise RuntimeError("H4A03AR2_CURRENT_REPOSITORY_MUTATED")

    emit("H4A03AR2_PRODUCT_SOURCE_MUTATION", "NO")
    emit("HARNESS_FAILURE", "NO")
    emit("PRODUCT_FAILURE", "NO_EVIDENCE")
    emit("HARDWARE_FAILURE", "NO_EVIDENCE")
    emit("A14_H4A03AR2_HISTORICAL_P3K_REPLAY_AUDIT", "PASS")
    emit("NEXT", "RETURN_TO_CHAT_INTERPRET_HISTORICAL_REPLAY_DO_NOT_ABLATE")
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
        emit("H4A03AR2_FAILURE_REQUIRES_CLASSIFICATION", "YES")
        emit("H4A03AR2_EXCEPTION", str(exc))
        raise SystemExit(1)
