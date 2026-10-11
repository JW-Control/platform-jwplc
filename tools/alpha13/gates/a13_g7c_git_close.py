#!/usr/bin/env python3
"""Alpha13 G7C: cerrar EN GIT los tres archivos productivos FRAM ya adoptados.

No descarga archivos, no edita producto, no compila, no ejecuta upload,
no toca FRAM, no hace merge/rebase/reset/clean, ni publica release.
Requiere --confirm A13_G7C_COMMIT tras verificacion completa.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[3]
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
BASE = "e975dbc3180784b4a7d8c9d47f359f608df7b333"
TOOL_FILES = {
    "tools/alpha13/gates/a13_g7c_git_close.py",
    "tools/alpha13/gates/run_a13_g7c_git_close.bat",
}
CPP = "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.cpp"
HDR = "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.h"
ARCHIVE = "JWPLC/2.1.0/libraries/JW_FRAM/src/esp32/libJW_FRAM.a"
TARGETS = {CPP, HDR, ARCHIVE}
CANDIDATES = {
    CPP: "tools/alpha13/candidates/g7c/JW_FRAM.cpp",
    HDR: "tools/alpha13/candidates/g7c/JW_FRAM.h",
}
OFFLINE_ID = "g7c_20261011_175217_3b6759cd"
ADOPTION_ID = "g7c_adopt_20261011_182119_efba4050"
ARCHIVE_SHA = "df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299"
OBJECT_SHA = "2d635936aeb0096a7e3dd8051e2c4a23e56dc863922f0a114cc3b2b56323366c"
MESSAGE = "fix(alpha13): completar A13-009 y A13-013 en FRAM"


class GateStop(RuntimeError):
    pass


def need(condition, message):
    if not condition:
        raise GateStop(message)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as source:
        for part in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(part)
    return digest.hexdigest()


def cmd(*args, check=True):
    p = subprocess.run(["git", "-c", "core.quotepath=false", *args],
                       cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and p.returncode:
        raise GateStop("GIT_" + "_".join(args[:2]) + ": " +
                       p.stderr.decode("utf-8", "replace")[-700:])
    return p


def txt(*args):
    return cmd(*args).stdout.decode("utf-8", "replace").strip()


def paths(*args):
    return set(filter(None,
        cmd(*args, "-z").stdout.decode("utf-8", "replace").split("\0")))


def remote_sha():
    lines = txt("ls-remote", "--heads", "origin", "refs/heads/" + BRANCH).splitlines()
    need(len(lines) == 1, "REMOTE_HEAD_UNAVAILABLE")
    return lines[0].split()[0]


def log(key, value):
    print("G7C_GIT_" + key + "=" + str(value), flush=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--confirm", required=True)
    args = ap.parse_args()
    need(args.confirm == "A13_G7C_COMMIT", "AUTHORIZATION_REQUIRED")
    run_id = "g7c_git_" + datetime.now().strftime("%Y%m%d_%H%M%S") + "_" + uuid.uuid4().hex[:8]
    folder = ROOT / "tools/alpha13/results" / run_id
    folder.mkdir(parents=True, exist_ok=False)
    m = {
        "schema": "A13-G7C-GIT-CLOSURE-1", "run_id": run_id,
        "status": "REVIEW_STOP", "product_commit": None,
        "git_pushed": False, "release_merged": False,
        "upload": False, "fram_written": False,
        "docs_closure": "PENDING",
        "result_path": str(folder),
    }
    did_stage = False
    committed = False
    try:
        need(Path.cwd().resolve() == ROOT, "MUST_RUN_FROM_REPO_ROOT")
        need((ROOT / ".git").exists(), "NOT_A_GIT_WORKTREE_ROOT")
        need(Path(txt("rev-parse", "--show-toplevel")).resolve() == ROOT, "ROOT_MISMATCH")
        need(txt("branch", "--show-current") == BRANCH, "WRONG_BRANCH")
        need(not cmd("ls-files", "--unmerged").stdout, "MERGE_CONFLICT_PRESENT")
        need(not paths("diff", "--cached", "--name-only"), "STAGED_FILES_EXIST")
        need(paths("diff", "--name-only") == TARGETS, "PRODUCT_DIRTY_SCOPE_NOT_THREE_FRAM_FILES")
        head = txt("rev-parse", "HEAD")
        need(cmd("merge-base", "--is-ancestor", BASE, head, check=False).returncode == 0,
             "G7C_BASELINE_NOT_ANCESTOR")
        need(paths("diff", "--name-only", BASE, head) == TOOL_FILES,
             "UNEXPECTED_COMMITS_AFTER_G7C_BASE")
        need(head == remote_sha(), "LOCAL_REMOTE_HEAD_DIFFER_NO_PUSH_OR_PULL")
        m["entry_head"] = head

        offline_path = ROOT/"tools/alpha13/results"/("g7c_offline_" + OFFLINE_ID)/"MANIFEST.json"
        adopted_path = ROOT/"tools/alpha13/results"/ADOPTION_ID/"MANIFEST.json"
        need(offline_path.is_file() and adopted_path.is_file(), "G7C_MANIFEST_MISSING")
        offline = json.loads(offline_path.read_text(encoding="utf-8-sig"))
        adopted = json.loads(adopted_path.read_text(encoding="utf-8-sig"))
        need(offline.get("schema") == "A13-G7C-OFFLINE-1" and
             offline.get("run_id") == OFFLINE_ID and
             offline.get("status") == "CANDIDATE_BUILD_PASS_NATIVE_PENDING",
             "OFFLINE_PROVENANCE_MISMATCH")
        need(offline.get("phase", {}).get("FINAL_AUDIT") == "PASS" and
             offline.get("phase", {}).get("CANDIDATE_REGRESSIONS") == "PASS" and
             offline.get("candidate_regressions") == ["GLOBAL_FRAM", "CONFIG_BLOCK"],
             "OFFLINE_BUILD_AND_REGRESSIONS_NOT_PROVEN")
        need(offline.get("source_object_sha256") == OBJECT_SHA and
             offline.get("archive_sha256") == ARCHIVE_SHA,
             "OFFLINE_SOURCE_OR_ARCHIVE_SHA_MISMATCH")
        need(adopted.get("schema") == "A13-G7C-ADOPTION-1" and
             adopted.get("run_id") == ADOPTION_ID and
             adopted.get("source_run") == OFFLINE_ID and
             adopted.get("status") == "PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_COMMIT",
             "ADOPTION_PROVENANCE_MISMATCH")
        need(adopted.get("phase", {}).get("PRODUCT_REGRESSIONS") == "PASS" and
             adopted.get("phase", {}).get("FINAL_AUDIT") == "PASS" and
             adopted.get("product_regressions") == ["GLOBAL_FRAM", "CONFIG_BLOCK"],
             "PRODUCT_REGRESSIONS_NOT_PROVEN")
        need(adopted.get("archive_sha256") == ARCHIVE_SHA and
             set(adopted.get("product_sha256", {})) == TARGETS and
             set(offline.get("candidate_sha256", {})) == set(CANDIDATES),
             "MANIFEST_HASH_SCOPE_INVALID")
        for product, candidate in CANDIDATES.items():
            expected = offline["candidate_sha256"][product]
            need(sha(ROOT/product) == sha(ROOT/candidate) ==
                 adopted["product_sha256"][product] == expected,
                 "PRODUCT_SOURCE_NOT_CANDIDATE_" + product)
        need(sha(ROOT/ARCHIVE) == adopted["product_sha256"][ARCHIVE] == ARCHIVE_SHA,
             "PRODUCT_ARCHIVE_SHA_MISMATCH")
        backup = Path(adopted.get("backup_root", ""))
        need(backup.is_dir(), "G7C_BACKUP_MISSING")
        for product in TARGETS:
            need((backup/product).is_file() and
                 sha(backup/product) == offline.get("before_sha256", {}).get(product),
                 "BACKUP_SHA_MISMATCH_" + product)
        m["backup_root"] = str(backup)
        m["product_sha256"] = {p: sha(ROOT/p) for p in sorted(TARGETS)}
        log("PREFLIGHT", "PASS")

        cmd("add", "--", *sorted(TARGETS))
        did_stage = True
        need(paths("diff", "--cached", "--name-only") == TARGETS,
             "INDEX_STAGED_SCOPE_INVALID")
        need(not paths("diff", "--name-only"), "UNSTAGED_CHANGES_AFTER_ADD")
        need(cmd("diff", "--cached", "--check", check=False).returncode == 0,
             "STAGED_DIFF_WHITESPACE_ERROR")
        need(hashlib.sha256(cmd("show", ":" + ARCHIVE).stdout).hexdigest() == ARCHIVE_SHA,
             "STAGED_ARCHIVE_NOT_CORRECT_BYTES")
        for product in CANDIDATES:
            need(txt("rev-parse", ":" + product) ==
                 txt("hash-object", "--path", product, product),
                 "STAGED_TEXT_NOT_LOCAL_SOURCE_" + product)
        (folder/"STAGED_DIFF_STAT.log").write_bytes(cmd("diff", "--cached", "--stat").stdout)
        log("STAGED_AUDIT", "PASS")

        need(head == remote_sha(), "REMOTE_ADVANCED_BEFORE_COMMIT")
        cmd("commit", "--only", "-m", MESSAGE, "--", *sorted(TARGETS))
        committed = True
        did_stage = False
        new_head = txt("rev-parse", "HEAD")
        need(txt("rev-parse", "HEAD^") == head, "COMMIT_PARENT_INVALID")
        need(paths("diff-tree", "--no-commit-id", "--name-only", "-r", new_head) == TARGETS,
             "COMMITTED_PATHS_NOT_EXACT")
        need(not paths("diff", "--name-only") and
             not paths("diff", "--cached", "--name-only"), "PRODUCT_WORKTREE_NOT_CLEAN")
        need(hashlib.sha256(cmd("show", "HEAD:" + ARCHIVE).stdout).hexdigest() == ARCHIVE_SHA,
             "COMMITTED_ARCHIVE_NOT_EXPECTED")
        m["product_commit"] = new_head
        log("PRODUCT_COMMIT", new_head)
        cmd("push", "origin", "HEAD:refs/heads/" + BRANCH)
        need(remote_sha() == new_head, "REMOTE_NOT_SAME_COMMIT")
        m["remote_head"] = new_head
        m["git_pushed"] = True
        m["status"] = "PRODUCT_COMMIT_PUSH_PASS_DOC_PENDING"
        log("STATUS", m["status"])
        log("GIT_COMMIT_OR_PUSH", "PASS")
        log("RESULTS", folder)
        return 0
    except Exception as exc:
        m["reason"] = repr(exc)
        m["status"] = ("REVIEW_COMMIT_LOCAL_PUSH_PENDING" if committed else
                       "REVIEW_STOP_BEFORE_COMMIT")
        if did_stage and not committed:
            # Devuelve exclusivamente el INDEX; los tres archivos del disco no se tocan.
            try:
                cmd("restore", "--staged", "--", *sorted(TARGETS))
                m["index_restored"] = True
            except Exception as index_error:
                m["index_restored"] = False
                m["index_error"] = repr(index_error)
        log("STATUS", m["status"])
        log("REASON", repr(exc))
        log("RESULTS", folder)
        return 2
    finally:
        (folder/"MANIFEST.json").write_text(
            json.dumps(m, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        (folder/"SUMMARY.log").write_text(
            "\n".join("G7C_GIT_" + str(k).upper() + "=" + str(v)
                      for k,v in m.items() if k != "product_sha256") + "\n",
            encoding="utf-8")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        log("FATAL", repr(exc))
        sys.exit(2)
