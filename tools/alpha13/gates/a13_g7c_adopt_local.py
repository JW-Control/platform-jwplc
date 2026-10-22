#!/usr/bin/env python3
"""A13-G7C: adoptar candidato FRAM prevalidado; dos regresiones; rollback.

No flash, no FRAM write, no serial, no git add/commit/push.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools/alpha13/gates"))
from a13_g7c_integrated_offline import (  # noqa: E402
    PRODUCT, ARCHIVE, BRANCH, import_gate, sha, git, git_paths,
)

PRIOR_ID = "g7c_20261011_175217_3b6759cd"
PRIOR_ARCHIVE_SHA = "df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299"
PRIOR_OBJECT_SHA = "2d635936aeb0096a7e3dd8051e2c4a23e56dc863922f0a114cc3b2b56323366c"
OFFICIAL_CLI_SHA = "1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571"
CI_COMMIT = "cd5172d99fb9d0ade76e3b36f106436cc4c08434"
TARGETS = [*PRODUCT.keys(), ARCHIVE]


def need(condition, reason):
    if not condition:
        raise RuntimeError(reason)


def line(name, value):
    print("G7C_ADOPT_" + name + "=" + str(value), flush=True)


def repo_command(*args):
    r = subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True)
    return r.returncode


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--arduino-cli", default=r"C:\Program Files\Arduino CLI\arduino-cli.exe")
    args = p.parse_args()
    g = import_gate()
    run_id = "g7c_adopt_" + datetime.now().strftime("%Y%m%d_%H%M%S") + "_" + uuid.uuid4().hex[:8]
    g.RUN_ID = run_id
    g.RESULT = ROOT / "tools/alpha13/results" / run_id
    g.TEMP = Path(tempfile.gettempdir()) / ("jwplc_a13_" + run_id)
    g.RESULT.mkdir(parents=True, exist_ok=False)
    g.TEMP.mkdir(parents=True, exist_ok=False)
    backup = g.TEMP / "originals"
    g.MANIFEST.clear()
    g.MANIFEST.update({
        "schema": "A13-G7C-ADOPTION-1", "run_id": run_id,
        "status": "REVIEW", "phase": {}, "product_adopted": False,
        "physical_upload": False, "fram_writes": False,
        "git_committed": False, "git_pushed": False,
        "ci_native_run": 38183543776, "ci_native_11_cases": "PASS",
        "source_run": PRIOR_ID,
    })
    backup_ready = False
    try:
        g.stage("PREFLIGHT", "RUNNING")
        cli = Path(args.arduino_cli).expanduser().resolve()
        need(cli.is_file() and cli.name.lower() == "arduino-cli.exe", "ARDUINO_CLI_MISSING")
        need(sha(cli) == OFFICIAL_CLI_SHA, "ARDUINO_CLI_SHA_CHANGED")
        need(git("branch", "--show-current") == BRANCH, "WRONG_BRANCH")
        need(git("rev-parse", "HEAD") == git("rev-parse", "refs/remotes/origin/"+BRANCH),
             "LOCAL_REMOTE_HEAD_NOT_EQUAL")
        need(repo_command("merge-base", "--is-ancestor", CI_COMMIT, "HEAD") == 0,
             "NATIVE_CI_COMMIT_NOT_IN_HISTORY")
        need(not git_paths("diff", "--name-only") and
             not git_paths("diff", "--cached", "--name-only"),
             "WORKTREE_TRACKED_DIRTY_OR_STAGED")
        need(sha(g.CORE) == g.CORE_SHA and sha(g.TFT) == g.TFT_SHA,
             "CORE_OR_TFT_SHA_CHANGED")
        result = ROOT / "tools/alpha13/results" / ("g7c_offline_" + PRIOR_ID) / "MANIFEST.json"
        oldtemp = Path(tempfile.gettempdir()) / ("jwplc_a13_" + PRIOR_ID)
        archive = oldtemp / "candidate/JW_FRAM/src/esp32/libJW_FRAM.a"
        header = oldtemp / "candidate/JW_FRAM/src/JW_FRAM.h"
        need(result.is_file() and archive.is_file() and header.is_file(),
             "PRIOR_G7C_ARTIFACTS_NOT_AVAILABLE_STOP_NO_REBUILD")
        m = json.loads(result.read_text(encoding="utf-8-sig"))
        need(m.get("schema") == "A13-G7C-OFFLINE-1" and m.get("run_id") == PRIOR_ID,
             "WRONG_OFFLINE_MANIFEST")
        need(m.get("status") == "CANDIDATE_BUILD_PASS_NATIVE_PENDING" and
             m.get("phase", {}).get("FINAL_AUDIT") == "PASS" and
             m.get("phase", {}).get("CANDIDATE_REGRESSIONS") == "PASS" and
             m.get("candidate_regressions") == ["GLOBAL_FRAM", "CONFIG_BLOCK"],
             "OFFLINE_EVIDENCE_INCOMPLETE")
        need(m.get("source_object_sha256") == PRIOR_OBJECT_SHA and
             m.get("archive_sha256") == PRIOR_ARCHIVE_SHA and
             sha(archive) == PRIOR_ARCHIVE_SHA,
             "PRIOR_SOURCE_OR_ARCHIVE_SHA_MISMATCH")
        need(set(m.get("before_sha256", {})) == set(TARGETS) and
             all(sha(ROOT / path) == m["before_sha256"][path] for path in TARGETS),
             "PRODUCT_BASELINE_DIFFERS_FROM_OFFLINE_RUN")
        need(set(m.get("candidate_sha256", {})) == set(PRODUCT) and
             all(sha(ROOT / path) == m["candidate_sha256"][prod]
                 for prod, path in PRODUCT.items()) and
             sha(header) == m["candidate_sha256"][
                 "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.h"],
             "CANDIDATE_SOURCES_OR_HEADER_CHANGED")
        g.MANIFEST["archive_sha256"] = PRIOR_ARCHIVE_SHA
        g.MANIFEST["before_sha256"] = m["before_sha256"]
        g.MANIFEST["candidate_sha256"] = m["candidate_sha256"]
        g.MANIFEST["entry_head"] = git("rev-parse", "HEAD")
        g.MANIFEST["cli_path"] = str(cli)
        g.MANIFEST["cli_sha256"] = sha(cli)
        g.stage("PREFLIGHT", "PASS")

        g.stage("BACKUP", "RUNNING")
        for dest in TARGETS:
            out = backup / dest
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / dest, out)
        backup_ready = True
        need(all(sha(ROOT/p) == sha(backup/p) for p in TARGETS), "BACKUP_SHA_MISMATCH")
        g.MANIFEST["backup_root"] = str(backup)
        g.stage("BACKUP", "PASS")

        g.stage("PRODUCT_ADOPTION", "RUNNING")
        for dest, candidate in PRODUCT.items():
            shutil.copy2(ROOT / candidate, ROOT / dest)
        shutil.copy2(archive, ROOT / ARCHIVE)
        need(git_paths("diff", "--name-only") == set(TARGETS), "DIRTY_SCOPE_NOT_THREE")
        need(not git("diff", "--check"), "GIT_DIFF_CHECK_FAILED")
        need(all(sha(ROOT/p) == m["candidate_sha256"][p] for p in PRODUCT) and
             sha(ROOT / ARCHIVE) == PRIOR_ARCHIVE_SHA,
             "ADOPTION_FILE_SHA_INVALID")
        g.MANIFEST["product_adopted"] = True
        g.stage("PRODUCT_ADOPTION", "PASS")

        g.regression(cli, g.FRAM, "PRODUCT_REGRESSIONS")
        g.stage("FINAL_AUDIT", "RUNNING")
        need(g.MANIFEST.get("product_regressions") == ["GLOBAL_FRAM", "CONFIG_BLOCK"],
             "MISSING_PRODUCT_REGRESSION_PROOF")
        need(git_paths("diff", "--name-only") == set(TARGETS) and
             not git_paths("diff", "--cached", "--name-only"),
             "FINAL_GIT_SCOPE_OR_STAGING_CHANGED")
        need(sha(ROOT/ARCHIVE) == PRIOR_ARCHIVE_SHA and
             all(sha(ROOT/p) == m["candidate_sha256"][p] for p in PRODUCT),
             "FINAL_PRODUCT_SHA_MISMATCH")
        g.MANIFEST["product_sha256"] = {p: sha(ROOT/p) for p in TARGETS}
        g.stage("FINAL_AUDIT", "PASS")
        g.MANIFEST["status"] = "PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_COMMIT"
        line("STATUS", g.MANIFEST["status"])
        line("ARCHIVE_SHA256", sha(ROOT/ARCHIVE))
        line("RESULTS", g.RESULT)
        line("BACKUP", backup)
        line("GIT_COMMIT_OR_PUSH", "NONE")
        line("UPLOAD_OR_FRAM_WRITE", "NONE")
        g.save()
        return 0
    except Exception as exc:
        g.MANIFEST["status"] = "REVIEW_STOP_ROLLBACK"
        g.MANIFEST["reason"] = repr(exc)
        line("STATUS", g.MANIFEST["status"])
        line("REASON", repr(exc))
        if backup_ready:
            try:
                for target in TARGETS:
                    shutil.copy2(backup/target, ROOT/target)
                need(all(sha(ROOT/p) == sha(backup/p) for p in TARGETS),
                     "ROLLBACK_SHA_INVALID")
                g.MANIFEST["rollback"] = "PASS"
                line("ROLLBACK", "PASS")
            except Exception as rexc:
                g.MANIFEST["rollback"] = "FAIL_" + repr(rexc)
                line("ROLLBACK", "FAIL_REQUIRES_MANUAL_REVIEW")
        g.save()
        return 2


if __name__ == "__main__":
    sys.exit(main())
