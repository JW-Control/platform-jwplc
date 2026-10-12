#!/usr/bin/env python3
"""G9 Git-close: autoprueba read-only + commit en repositorio sintetico temporal.

No ejecuta el gate productivo, no stage/push sobre repositorio real.
"""
from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile

def require(ok,reason):
    if not ok:raise RuntimeError(reason)

def git(root,*args):
    p=subprocess.run(["git",*args],cwd=root,capture_output=True)
    require(p.returncode==0,"SYNTHETIC_GIT_FAILED_"+repr(args)+":"+
            p.stderr.decode("utf-8","replace")[-200:])
    return p.stdout.decode("utf-8","replace").strip()

def main():
    path=Path(__file__).resolve().parent/"a13_g9_git_close.py"
    spec=importlib.util.spec_from_file_location("g9_git_close_runner",path)
    require(spec is not None and spec.loader is not None,"SCRIPT_IMPORT_FAIL")
    m=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=m
    spec.loader.exec_module(m)
    require(m.TARGETS=={m.SD_CPP,m.SD_A,m.GP_CPP},
            "G9_GIT_CLOSE_SCOPE_NOT_EXACT_THREE")
    require(m.ARCHIVE_SHA=="58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e",
            "NEW_ARCHIVE_NOT_PINNED")
    require(m.CAND_ID=="g9_p2p3_20261012_000449_23842b4f" and
            m.ADOPT_ID=="g9_adopt_20261012_101458_1e395a6e",
            "LOCAL_RUN_PROVENANCE_NOT_PINNED")
    require(m.manifest_map({r"a\b\file.cpp":"0"},"TEST")==
            {"a/b/file.cpp":"0"},"WINDOWS_MANIFEST_NOT_NORMALIZED")
    require(m.manifest_map({"a/b/file.cpp":"0"},"TEST")==
            {"a/b/file.cpp":"0"},"LINUX_MANIFEST_NOT_NORMALIZED")
    for bad in [{"a/b":"1",r"a\b":"2"},{"../danger":"0"}]:
        try:m.manifest_map(bad,"NEGATIVE")
        except RuntimeError:pass
        else:raise RuntimeError("UNSAFE_OR_COLLIDING_MANIFEST_ACCEPTED")
    source=path.read_text(encoding="utf-8")
    for required in [
        '"--verify-only"', '"--confirm"', '"A13_G9_COMMIT"',
        'call("add","--",*sorted(TARGETS))',
        'call("commit","--only","-m",MESSAGE,"--",*sorted(TARGETS))',
        'call("push","origin","HEAD:refs/heads/"+BRANCH)',
        'call("restore","--staged","--",*sorted(TARGETS))',
        'm["status"]="REVIEW_LOCAL_COMMIT_PUSH_PENDING" if committed',
        'gt("hash-object","--path",k,str(b))',
    ]:
        require(required in source,"GIT_CLOSE_REQUIRED_GUARD_MISSING_"+required)
    print("G9_GIT_SELFTEST_CONTRACT=PASS",flush=True)

    need=m.TARGETS
    require(m.gp("ls-tree","-r","--name-only","HEAD","--",*sorted(need))==need,
            "PRODUCT_HEAD_PATHS_NOT_PRESENT")
    require(m.gp("ls-files","--full-name","--",*sorted(need))==need,
            "PRODUCT_INDEX_PATHS_NOT_PRESENT")
    print("G9_GIT_SELFTEST_REAL_HEAD_READONLY=PASS",flush=True)

    original=m.ROOT
    with tempfile.TemporaryDirectory(prefix="jwplc_g9_git_close_") as tmp:
        m.ROOT=Path(tmp)
        git(m.ROOT,"init","-q")
        for name in sorted(need):
            target=m.ROOT/name
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(b"ORIGINAL\n")
        git(m.ROOT,"add","--",*sorted(need))
        git(m.ROOT,"-c","user.name=JWPLC CI",
            "-c","user.email=ci@example.invalid","commit","-qm","start")
        m.marks()
        require(not m.gp("diff","--name-only"),"CLEAN_REPO_DIRTY")
        for name in sorted(need):
            (m.ROOT/name).write_bytes(b"CHANGED\n")
        require(m.gp("diff","--name-only")==need,"THREE_DIFF_NOT_VISIBLE")
        m.call("add","--",*sorted(need))
        require(m.gp("diff","--cached","--name-only")==need and
                not m.gp("diff","--name-only"),"STAGED_SCOPE_WRONG")
        m.call("commit","-c","user.name=JWPLC CI") if False else None
        git(m.ROOT,"-c","user.name=JWPLC CI",
            "-c","user.email=ci@example.invalid","commit","-qm","test candidate")
        require(m.gp("diff-tree","--no-commit-id","--name-only",
                     "-r",m.gt("rev-parse","HEAD"))==need,
                "SYNTHETIC_COMMIT_DID_NOT_RETAIN_THREE_PATHS")
        m.marks()
    m.ROOT=original
    print("G9_GIT_SELFTEST_STAGE_COMMIT_EXACT_THREE=PASS",flush=True)
    print("G9_GIT_SELFTEST=PASS_NO_PRODUCT_MUTATION",flush=True)

if __name__=="__main__":
    main()
