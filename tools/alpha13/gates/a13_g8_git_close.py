#!/usr/bin/env python3
"""Alpha13-G8: commit/push EXACTAMENTE dos fuentes productivas y archive RTU.

Requiere adopción local PASS ya terminada. NO restaura ni recompila producto,
no accede a COM/FRAM, no hace upload, merge, reset, rebase, release ni PR.
El BAT invoca --confirm A13_G8_COMMIT; --verify-only no muta Git.
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
BASE = "04bd61060f2bd5a91a4233714505942ff30f89c5"
CAND_ID = "g8_p1p2_20261011_190500_d59b4022"
ADOPT_ID = "g8_adopt_20261011_193839_57f9bb49"
ARCHIVE_SHA = "f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416"
CPP_RTU = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp"
CPP_RS = "JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.cpp"
ARCHIVE = "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a"
TARGETS = {CPP_RTU, CPP_RS, ARCHIVE}
CANDIDATES = {
    CPP_RTU: "tools/alpha13/candidates/g8/JWPLC_ModbusRTU.cpp",
    CPP_RS: "tools/alpha13/candidates/g8/JWPLC_RS485.cpp",
}
EXPECTED_TOOLING = {
    "tools/alpha13/gates/a13_g8_git_close.py",
    "tools/alpha13/gates/run_a13_g8_git_close.bat",
    "tools/alpha13/gates/a13_g8_git_close_selftest.py",
    ".github/workflows/alpha13-g8-git-close.yml",
    "docs/v2.1.0-alpha.13/ALPHA13_STATUS.md",
    "docs/v2.1.0-alpha.13/A13_G8_PRODUCT_ADOPTION_PASS_20261011.md",
    "docs/v2.1.0-alpha.13/A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md",
}
MESSAGE = "fix(alpha13): adoptar A13-010 y A13-011 en Modbus RTU y RS485"


def emit(key, value):
    print("G8_GIT_" + key + "=" + str(value), flush=True)


def require(ok, reason):
    if not ok:
        raise RuntimeError(reason)


def sha(path):
    value = hashlib.sha256()
    with Path(path).open("rb") as inp:
        for chunk in iter(lambda: inp.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def cmd(*args, check=True):
    p = subprocess.run(["git", "-c", "core.quotepath=false", *args],
                       cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if check and p.returncode:
        raise RuntimeError("GIT_FAILED_" + "_".join(args[:2]) + ": " +
                           p.stderr.decode("utf-8", "replace")[-600:])
    return p


def text_git(*args):
    return cmd(*args).stdout.decode("utf-8", "replace").strip()


def git_paths(*args):
    # -z debe anteceder al separador --, no convertirse en pathspec.
    argv = list(args)
    pos = argv.index("--") if "--" in argv else len(argv)
    argv.insert(pos, "-z")
    return set(filter(None, cmd(*argv).stdout.decode(
        "utf-8", "surrogateescape").split("\0")))


def remote_head():
    values = text_git("ls-remote", "--heads", "origin", "refs/heads/" + BRANCH).splitlines()
    require(len(values) == 1, "REMOTE_HEAD_UNAVAILABLE")
    return values[0].split()[0]


def manifest_paths(value, name):
    require(isinstance(value, dict) and
            all(isinstance(k, str) and isinstance(v, str) for k, v in value.items()),
            "MANIFEST_" + name + "_INVALID")
    out = {}
    for key, val in value.items():
        canonical = key.replace("\\", "/")
        require(canonical not in out, "MANIFEST_" + name + "_PATH_ALIAS_COLLISION")
        require(not canonical.startswith("/") and ".." not in canonical.split("/"),
                "MANIFEST_" + name + "_PATH_ESCAPE")
        out[canonical] = val
    return out


def expected_index_marks():
    raw = cmd("ls-files", "-v", "-z", "--", *sorted(TARGETS)).stdout
    observed = {}
    for entry in raw.split(b"\0"):
        if not entry:
            continue
        require(len(entry) >= 3 and entry[1:2] == b" ", "GIT_INDEX_RECORD_MALFORMED")
        path = entry[2:].decode("utf-8", "surrogateescape")
        flag = entry[:1].decode("ascii", "replace")
        require(path not in observed, "GIT_INDEX_DUPLICATE")
        observed[path] = flag
    emit("INDEX_FLAGS", ";".join(x + ":" + observed[x] for x in sorted(observed)))
    require(observed.keys() == TARGETS and all(f == "H" for f in observed.values()),
            "GIT_INDEX_TRACKING_OR_FLAGS_DIFFER")
    require(git_paths("ls-files", "--full-name", "--", *sorted(TARGETS)) == TARGETS,
            "GIT_INDEX_PATHS_DIFFER")
    require(git_paths("ls-tree", "-r", "--name-only", "HEAD", "--", *sorted(TARGETS))
            == TARGETS, "GIT_HEAD_PATHS_DIFFER")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--confirm")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    require(args.verify_only or args.confirm == "A13_G8_COMMIT",
            "EXPLICIT_AUTHORIZATION_REQUIRED")
    require(not args.verify_only or args.confirm is None,
            "MODES_ARE_MUTUALLY_EXCLUSIVE")
    runid = "g8_git_" + datetime.now().strftime("%Y%m%d_%H%M%S") + "_" + uuid.uuid4().hex[:8]
    output = ROOT/"tools/alpha13/results"/runid
    output.mkdir(parents=True, exist_ok=False)
    m = {
        "schema": "A13-G8-GIT-CLOSE-1", "run_id": runid,
        "candidate_run": CAND_ID, "adoption_run": ADOPT_ID,
        "status": "REVIEW_STOP_BEFORE_COMMIT", "product_commit": None,
        "git_push": False, "physical_rs485": "NOT_EXECUTED",
        "upload": False, "fram_write": False,
        "release_merge": False, "docs_closure": "PENDING",
    }
    did_stage = False
    committed = False
    try:
        require(Path.cwd().resolve() == ROOT and (ROOT/".git").exists(),
                "MUST_RUN_FROM_REPOSITORY_ROOT")
        require(Path(text_git("rev-parse", "--show-toplevel")).resolve() == ROOT,
                "ROOT_NOT_GIT_ROOT")
        require(text_git("branch", "--show-current") == BRANCH, "WRONG_BRANCH")
        require(not cmd("ls-files", "--unmerged").stdout, "UNMERGED_PATHS")
        require(not git_paths("diff", "--cached", "--name-only"), "FILES_ALREADY_STAGED")
        actual_diff = git_paths("diff", "--name-only")
        emit("UNSTAGED_PATHS", ";".join(sorted(actual_diff)) or "NONE")
        require(actual_diff == TARGETS, "EXACT_THREE_PRODUCT_FILES_NOT_DIRTY")
        head = text_git("rev-parse", "HEAD")
        m["entry_head"] = head
        require(cmd("merge-base", "--is-ancestor", BASE, head,
                    check=False).returncode == 0, "BASE_NOT_ANCESTOR")
        changed = git_paths("diff", "--name-only", BASE, head)
        require(changed <= EXPECTED_TOOLING and
                "tools/alpha13/gates/a13_g8_git_close.py" in changed and
                "tools/alpha13/gates/run_a13_g8_git_close.bat" in changed,
                "UNEXPECTED_REMOTE_PRODUCT_OR_TOOL_CHANGES")
        require(head == remote_head(), "LOCAL_REMOTE_HEAD_NOT_SYNCHRONIZED")
        expected_index_marks()

        result_dir = ROOT/"tools/alpha13/results"
        candidate_path = result_dir/CAND_ID/"MANIFEST.json"
        adoption_path = result_dir/ADOPT_ID/"MANIFEST.json"
        require(candidate_path.is_file() and adoption_path.is_file(),
                "LOCAL_MANIFESTS_MISSING")
        candidate = json.loads(candidate_path.read_text(encoding="utf-8-sig"))
        adopted = json.loads(adoption_path.read_text(encoding="utf-8-sig"))
        require(candidate.get("schema") == "A13-G8-P1P2-CANDIDATE-1" and
                candidate.get("run_id") == CAND_ID and
                candidate.get("status") == "CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED" and
                candidate.get("phases", {}).get("SOURCE_BUILD") == "PASS" and
                candidate.get("phases", {}).get("ARCHIVE_REBUILD") == "PASS" and
                candidate.get("phases", {}).get("CANDIDATE_REGRESSIONS") == "PASS" and
                candidate.get("phases", {}).get("FINAL_AUDIT") == "PASS" and
                candidate.get("candidate_regressions") == ["SLAVE", "MASTER", "RS485_STATUS"],
                "CANDIDATE_MANIFEST_NOT_PROVEN")
        require(candidate.get("archive_sha256") == ARCHIVE_SHA and
                candidate.get("source_object_sha256") and
                candidate.get("archive_member_sha256") ==
                candidate.get("source_object_sha256"),
                "ARCHIVE_OBJECT_PROVENANCE_NOT_PROVEN")
        require(adopted.get("schema") == "A13-G8-PRODUCT-ADOPTION-1" and
                adopted.get("run_id") == ADOPT_ID and
                adopted.get("source_run") == CAND_ID and
                adopted.get("status") ==
                "PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_PHYSICAL_DECISION_AND_COMMIT" and
                adopted.get("product_adopted") is True and
                adopted.get("entry_head") == BASE and
                adopted.get("archive_sha256") == ARCHIVE_SHA and
                adopted.get("source_object_sha256") ==
                candidate.get("source_object_sha256"),
                "PRODUCT_ADOPTION_PROVENANCE_NOT_PROVEN")
        phases = adopted.get("phases", {})
        require(all(phases.get(key) == "PASS" for key in [
            "PREFLIGHT", "BACKUP", "PRODUCT_ADOPTION",
            "PRODUCT_REGRESSIONS", "FINAL_AUDIT"]) and
                adopted.get("product_regressions") ==
                ["SLAVE", "MASTER", "RS485_STATUS"],
                "PRODUCT_ADOPTION_REGRESSIONS_INCOMPLETE")
        require(all(adopted.get(x) is False for x in [
            "upload", "serial_access", "fram_write", "git_commit", "git_push"]),
            "ADOPTION_MAY_HAVE_WRITTEN_HARDWARE_OR_GIT")

        old = manifest_paths(candidate.get("before_sha256"), "BEFORE")
        candidate_sources = manifest_paths(candidate.get("source_sha256"), "CANDIDATE_SOURCE")
        adopted_sources = manifest_paths(adopted.get("candidate_sha256"), "ADOPT_CANDIDATE")
        product_hashes = manifest_paths(adopted.get("product_sha256"), "ADOPT_PRODUCT")
        adopted_before = manifest_paths(adopted.get("before_sha256"), "ADOPT_BEFORE")
        require(old == adopted_before, "BEFORE_HASHES_DIFFER")
        require(set(product_hashes) == TARGETS and
                set(adopted_sources) == TARGETS and
                set(candidate_sources) == set(CANDIDATES.values()),
                "MANIFEST_EXPECTED_PATHS_NOT_EXACT")
        for dst, source in CANDIDATES.items():
            expected = candidate_sources[source]
            require(sha(ROOT/source) == sha(ROOT/dst) ==
                    product_hashes[dst] == adopted_sources[dst] == expected,
                    "SOURCE_SHA_MISMATCH_" + dst)
        require(sha(ROOT/ARCHIVE) == ARCHIVE_SHA ==
                product_hashes[ARCHIVE] == adopted_sources[ARCHIVE],
                "PRODUCT_ARCHIVE_NOT_EXPECTED")
        require(set(old) >= TARGETS, "BEFORE_PRODUCTS_NOT_IN_MANIFEST")
        backup = Path(adopted.get("backup_root", ""))
        require(backup.is_dir(), "ADOPTION_BACKUP_MISSING")
        for name, expected in old.items():
            if name in TARGETS:
                f = backup/Path(name)
                require(f.is_file() and sha(f) == expected,
                        "BACKUP_SHA_DIFFERS_" + name)
                # Prove HEAD baseline is exactly the saved original bytes
                # (respect Git's configured clean/EOL conversion).
                blob = text_git("rev-parse", "HEAD:" + name)
                current_blob = text_git("hash-object", "--path", name, str(f))
                require(blob == current_blob, "BACKUP_NOT_SAME_AS_HEAD_" + name)
            else:
                require((ROOT/name).is_file() and sha(ROOT/name) == expected,
                        "NON_TARGET_PRODUCT_BASELINE_CHANGED_" + name)
        saved_scope = adopted.get("git_scope_final", {})
        require(set(saved_scope.get("observed", [])) == TARGETS and
                set(saved_scope.get("expected", [])) == TARGETS and
                not saved_scope.get("staged", ["NOT_EMPTY"]) and
                all(saved_scope.get("head_differs_from_worktree", {}).get(x)
                    is True for x in TARGETS),
                "ADOPTION_FINAL_SCOPE_AUDIT_MISSING")
        m["product_sha256"] = product_hashes
        m["backups_verified"] = True
        m["candidate_archive_sha256"] = ARCHIVE_SHA
        emit("PREFLIGHT", "PASS")
        if args.verify_only:
            m["status"] = "VERIFY_ONLY_PASS_NO_GIT_MUTATION"
            emit("STATUS", m["status"])
            emit("RESULTS", output)
            return 0

        # Commit safety: exact scope, staged Git blobs and archive SHA.
        cmd("add", "--", *sorted(TARGETS))
        did_stage = True
        require(git_paths("diff", "--cached", "--name-only") == TARGETS,
                "STAGED_PATHS_DIFFER")
        require(not git_paths("diff", "--name-only"),
                "UNSTAGED_AFTER_ADD")
        require(cmd("diff", "--cached", "--check", check=False).returncode == 0,
                "STAGED_DIFF_CHECK_FAILED")
        for dest in CANDIDATES:
            require(text_git("rev-parse", ":" + dest) ==
                    text_git("hash-object", "--path", dest, dest),
                    "TEXT_STAGED_BLOB_DIFFER_" + dest)
        require(hashlib.sha256(cmd("show", ":" + ARCHIVE).stdout).hexdigest()
                == ARCHIVE_SHA, "STAGED_ARCHIVE_BYTES_DIFFER")
        (output/"STAGED_DIFF_STAT.log").write_bytes(cmd("diff", "--cached", "--stat").stdout)
        emit("STAGED_AUDIT", "PASS")

        require(head == remote_head(), "REMOTE_MOVED_BEFORE_COMMIT")
        cmd("commit", "--only", "-m", MESSAGE, "--", *sorted(TARGETS))
        committed = True
        did_stage = False
        newhead = text_git("rev-parse", "HEAD")
        require(text_git("rev-parse", "HEAD^") == head,
                "COMMIT_PARENT_CHANGED")
        require(git_paths("diff-tree", "--no-commit-id", "--name-only",
                          "-r", newhead) == TARGETS,
                "COMMITTED_SCOPE_NOT_EXACT_THREE")
        require(hashlib.sha256(cmd("show", "HEAD:" + ARCHIVE).stdout).hexdigest()
                == ARCHIVE_SHA, "COMMITTED_ARCHIVE_SHA_MISMATCH")
        for dest in CANDIDATES:
            require(text_git("rev-parse", "HEAD:" + dest) ==
                    text_git("hash-object", "--path", dest, dest),
                    "COMMITTED_SOURCE_BLOB_MISMATCH_" + dest)
        require(not git_paths("diff", "--name-only") and
                not git_paths("diff", "--cached", "--name-only"),
                "PRODUCT_WORKTREE_NOT_CLEAN_AFTER_COMMIT")
        m["product_commit"] = newhead
        emit("PRODUCT_COMMIT", newhead)
        cmd("push", "origin", "HEAD:refs/heads/" + BRANCH)
        require(remote_head() == newhead, "REMOTE_NOT_SAME_AS_COMMIT")
        m["git_push"] = True
        m["remote_head"] = newhead
        m["status"] = "PRODUCT_COMMIT_PUSH_PASS_PHYSICAL_DECISION_PENDING"
        emit("STATUS", m["status"])
        emit("COMMIT_PUSH", "PASS")
        emit("RESULTS", output)
        return 0
    except Exception as exc:
        m["reason"] = repr(exc)
        m["status"] = ("REVIEW_LOCAL_COMMIT_PUSH_PENDING" if committed else
                       "REVIEW_STOP_BEFORE_COMMIT")
        if did_stage and not committed:
            try:
                cmd("restore", "--staged", "--", *sorted(TARGETS))
                m["index_restored"] = True
                emit("STAGED_INDEX_RESTORE", "PASS")
            except Exception as restore_err:
                m["index_restored"] = False
                m["index_restore_error"] = repr(restore_err)
                emit("STAGED_INDEX_RESTORE", "REVIEW")
        emit("STATUS", m["status"])
        emit("REASON", repr(exc))
        emit("RESULTS", output)
        return 2
    finally:
        (output/"MANIFEST.json").write_text(
            json.dumps(m, ensure_ascii=False, indent=2)+"\n",
            encoding="utf-8")
        (output/"SUMMARY.log").write_text(
            "\n".join("G8_GIT_"+str(k).upper()+"="+str(v)
                      for k,v in m.items() if k != "product_sha256")+"\n",
            encoding="utf-8")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        emit("FATAL", repr(exc))
        sys.exit(2)
