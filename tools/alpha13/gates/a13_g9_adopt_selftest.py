#!/usr/bin/env python3
"""G9: contrato del adopter + auditoria Git de tres rutas en repo efimero.

No ejecuta adopcion, upload, compilacion ni comandos Git contra producto.
"""
from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import sys
import tempfile


def require(ok,reason):
    if not ok:raise RuntimeError(reason)


def run_git(root,*args):
    p=subprocess.run(["git",*args],cwd=root,capture_output=True)
    require(p.returncode==0,"GIT_SELFTEST_FAILED_"+repr(args)+":"+
            p.stderr.decode("utf-8","replace")[-300:])
    return p.stdout


def main():
    script=Path(__file__).resolve().parent/"a13_g9_adopt_local.py"
    spec=importlib.util.spec_from_file_location("g9_adopter",script)
    require(spec is not None and spec.loader is not None,"ADOPTER_IMPORT_MISSING")
    g=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=g
    spec.loader.exec_module(g)
    require(set(g.CANDIDATES)==g.FILES and len(g.FILES)==3,
            "EXPECTED_SCOPE_NOT_THREE")
    require(g.NEW_ARCHIVE=="58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e",
            "CANDIDATE_ARCHIVE_HASH_NOT_PINNED")
    require(g.CAND_ID=="g9_p2p3_20261012_000449_23842b4f" and
            g.P0_ID=="g9_p0_20261011_234245_6727f0b0",
            "EVIDENCE_RUN_ID_NOT_PINNED")
    require(g.CI_COMMIT=="712a3510fee3764aa4384ac784c9fd112ba0154e",
            "CI_RUN_SOURCE_NOT_PINNED")
    require(g.FQBN=="jwplc_local:esp32:jwplcbasic",
            "FQBN_NOT_PINNED")
    code=script.read_text(encoding="utf-8")
    for marker in [
        '"--libraries",LIBS,sketch', '"--build-path",build',
        'verify_archive(log,label)', 'sha(ROOT/name)==sha(backup/name)',
        'phase("FINAL_AUDIT","PASS")','M["rollback"]="PASS"',
        '"git_commit":False','"git_push":False',
        '"upload":False','"serial_access":False',
    ]:
        require(marker in code,"ADOPTER_REQUIRED_GUARD_MISSING_"+marker)
    print("G9_ADOPT_SELFTEST_STATIC_CONTRACT=PASS",flush=True)

    with tempfile.TemporaryDirectory(prefix="jwplc_g9_adopt_selftest_") as tmp:
        root=Path(tmp)
        run_git(root,"init","-q")
        for name in g.FILES:
            file=root/name
            file.parent.mkdir(parents=True,exist_ok=True)
            file.write_bytes(b"INITIAL\n")
        run_git(root,"add","--",*sorted(g.FILES))
        run_git(root,"-c","user.name=JWPLC CI",
                "-c","user.email=ci@example.invalid","commit","-qm","baseline")
        g.ROOT=root
        g.OUT=root/"results"
        g.OUT.mkdir()
        g.M.clear()
        require(g.index_marks() and not g.git_paths("diff","--name-only"),
                "SYNTHETIC_CLEAN_REPO_BAD")
        only=sorted(g.FILES)[0]
        (root/only).write_bytes(b"UPDATED\n")
        try:
            g.git_scope("NEGATIVE_ONLY_ONE")
        except RuntimeError as e:
            require("EXACT_THREE_DIRTY_PATHS_FAILED" in str(e),
                    "UNEXPECTED_NEGATIVE_ERROR")
        else:
            raise RuntimeError("BAD_ONE_PATH_SCOPE_ACCEPTED")
        for name in g.FILES-{only}:
            (root/name).write_bytes(b"UPDATED\n")
        g.git_scope("POSITIVE_ALL_THREE")
        for name in g.FILES:
            require(g.sha(root/name)!=g.sha(root/"results/MANIFEST.json"),
                    "DEGENERATE_HASH_TEST")
        run_git(root,"add","--",*sorted(g.FILES))
        try:
            g.git_scope("NEGATIVE_STAGED")
        except RuntimeError as e:
            require("EXACT_THREE_DIRTY_PATHS_FAILED" in str(e),
                    "UNEXPECTED_STAGED_ERROR")
        else:
            raise RuntimeError("BAD_STAGED_SCOPE_ACCEPTED")
    print("G9_ADOPT_SELFTEST_GIT_PATHS_WINDOWS_POSIX=PASS",flush=True)
    print("G9_ADOPT_SELFTEST_NEGATIVE_ONE_PATH_AND_STAGED=PASS",flush=True)
    print("G9_ADOPT_SELFTEST=PASS_NO_PRODUCT_MUTATION",flush=True)


if __name__=="__main__":
    main()
