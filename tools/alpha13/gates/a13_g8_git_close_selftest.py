#!/usr/bin/env python3
"""A13 G8: autoprueba de solo lectura del helper Git + repo temporal sintético."""
from __future__ import annotations
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import sys


def need(ok, why):
    if not ok: raise RuntimeError(why)


def repo_git(root, *args):
    p = subprocess.run(["git",*args],cwd=root,capture_output=True)
    need(p.returncode == 0,"SELFTEST_GIT_"+repr(args)+":"+
         p.stderr.decode("utf-8","replace")[-600:])
    return p.stdout.decode("utf-8","replace").strip()


def main():
    p=Path(__file__).resolve().parent/"a13_g8_git_close.py"
    spec=importlib.util.spec_from_file_location("g8_close",p)
    need(spec is not None and spec.loader is not None,"IMPORT_FAILED")
    g=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=g
    spec.loader.exec_module(g)
    original_root=g.ROOT
    # El parser de manifests debe poder leer Windows y Unix con el MISMO canon.
    need(g.manifest_paths({r"JWPLC\2.1.0\test.cpp":"123"},
        "TEST")=={"JWPLC/2.1.0/test.cpp":"123"},"WINDOWS_PATH_NORMALIZATION")
    need(g.manifest_paths({"JWPLC/2.1.0/test.cpp":"123"},
        "TEST")=={"JWPLC/2.1.0/test.cpp":"123"},"POSIX_PATH_NORMALIZATION")
    for sample in [
      {r"x\y":"1","x/y":"2"},
      {"../outside":"0"}
    ]:
        try:
            g.manifest_paths(sample,"NEGATIVE")
        except RuntimeError:
            pass
        else:
            raise RuntimeError("BAD_PATH_ACCEPTED")
    print("G8_GIT_SELFTEST_MANIFEST_WINDOWS_POSIX=PASS",flush=True)

    need(g.git_paths("ls-tree","-r","--name-only","HEAD","--",*sorted(g.TARGETS))
         == g.TARGETS,"REAL_REPO_HEAD_PATHS_DIFFERS")
    need(g.git_paths("ls-files","--full-name","--",*sorted(g.TARGETS))
         == g.TARGETS,"REAL_REPO_INDEX_PATHS_DIFFERS")
    print("G8_GIT_SELFTEST_REAL_HEAD_INDEX_READONLY=PASS",flush=True)

    with tempfile.TemporaryDirectory(prefix="jwplc_g8_git_selftest_") as tmp:
        g.ROOT=Path(tmp)
        repo_git(g.ROOT,"init","-q")
        for name in sorted(g.TARGETS):
            target=g.ROOT/name
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(b"INITIAL\n")
        repo_git(g.ROOT,"add","--",*sorted(g.TARGETS))
        repo_git(g.ROOT,"-c","user.email=ci@example.invalid",
                 "-c","user.name=JWPLC CI","commit","-qm","synthetic baseline")
        g.expected_index_marks()
        need(not g.git_paths("diff","--name-only"),"INITIAL_DIRTY")
        for name in sorted(g.TARGETS):
            (g.ROOT/name).write_bytes(b"MODIFIED\n")
        need(g.git_paths("diff","--name-only")==g.TARGETS,
             "SYNTHETIC_GIT_DIFF_SET_INVALID")
        g.cmd("add","--",*sorted(g.TARGETS))
        need(g.git_paths("diff","--cached","--name-only")==g.TARGETS,
             "SYNTHETIC_GIT_STAGE_INVALID")
        need(not g.git_paths("diff","--name-only"),"SYNTHETIC_UNSTAGED")
        repo_git(g.ROOT,"-c","user.email=ci@example.invalid",
                 "-c","user.name=JWPLC CI","commit","-qm","synthetic changed")
        need(g.git_paths("diff-tree","--no-commit-id","--name-only","-r",
                         g.text_git("rev-parse","HEAD")) == g.TARGETS,
             "SYNTHETIC_COMMIT_PATHS_INVALID")
    g.ROOT=original_root
    print("G8_GIT_SELFTEST_THREE_PATHS_STAGE_COMMIT=PASS",flush=True)
    print("G8_GIT_SELFTEST_STATUS=PASS_NO_PRODUCT_MUTATION",flush=True)


if __name__=="__main__":
    main()
