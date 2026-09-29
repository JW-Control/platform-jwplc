#!/usr/bin/env python3
from __future__ import annotations

import argparse
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
    p = run(["git", "-C", str(repo), *args])
    text = decode(p.stdout)
    if p.returncode != 0:
        raise RuntimeError(f"GIT_FAILED {' '.join(args)} :: {text[-1200:]}")
    return text.strip()


def unique(text: str, key: str) -> str:
    matches = re.findall(rf"(?m)^{re.escape(key)}=(.*)\r?$", text)
    if len(matches) != 1:
        raise RuntimeError(f"H4A03AR2_KEY_COUNT_{key}={len(matches)}")
    return matches[0].strip()


def as_float(text: str, key: str) -> float:
    return float(unique(text, key))


def median(values: list[float]) -> float:
    if not values:
        raise RuntimeError("H4A03AR2_EMPTY_MEDIAN")
    return float(statistics.median(values))


def pct(current: float, reference: float) -> float:
    return (current / reference - 1.0) * 100.0


def source_contract(worktree: Path) -> None:
    gate_text = (worktree / P3K_GATE).read_text(encoding="utf-8")
    bridge_text = (worktree / P3K_BRIDGE).read_text(encoding="utf-8")
    raw_text = (worktree / RAW_RUNNER).read_text(encoding="utf-8")
    closure_text = (worktree / CLOSURE_DOC).read_text(encoding="utf-8")

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

    duration = as_float(text, "DURATION_S")
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
    emit("H4A03AR2_REPLAY_SCOPE", "HISTORICAL_P3_CLOSURE_REPO_TREE_CURRENT_HOST_TOOLCHAIN")
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

    result_root = Path(tempfile.mkdtemp(prefix="jwplc_a14_h4a03ar2_"))
    worktree = result_root / "historical_worktree"
    temp_branch = f"h4a03ar2-p3k-{closure_commit[:8]}-{os.getpid()}"
    emit("H4A03AR2_RESULT_ROOT", result_root)
    emit("H4A03AR2_TEMP_BRANCH", temp_branch)

    worktree_added = False
    temp_branch_created = False

    try:
        add = run([
            "git", "-C", str(repo), "worktree", "add",
            "-b", temp_branch, str(worktree), closure_commit
        ])
        if add.returncode != 0:
            raise RuntimeError(
                "H4A03AR2_WORKTREE_ADD_FAILED=" + decode(add.stdout)[-1200:]
            )
        worktree_added = True
        temp_branch_created = True

        historical_head = git(worktree, "rev-parse", "HEAD")
        emit("H4A03AR2_HISTORICAL_WORKTREE_HEAD", historical_head)
        if historical_head != closure_commit:
            raise RuntimeError("H4A03AR2_HISTORICAL_HEAD_MISMATCH")

        for relative in (CLOSURE_DOC, P3K_GATE, P3K_BRIDGE, RAW_RUNNER, COMMON, VALIDATOR):
            if not (worktree / relative).is_file():
                raise RuntimeError(f"H4A03AR2_HISTORICAL_FILE_MISSING={relative}")

        source_contract(worktree)

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
                "REPLAY_SCOPE=HISTORICAL_P3_CLOSURE_REPO_TREE_CURRENT_HOST_TOOLCHAIN",
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
