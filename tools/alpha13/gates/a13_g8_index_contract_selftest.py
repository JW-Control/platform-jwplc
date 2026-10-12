#!/usr/bin/env python3
"""CI ONLY: probo el mismo helper Git de adopción G8 en checkout efímero.

Este test modifica tres archivos en el RUNNER de GitHub Actions.
No ejecutar directamente sobre la PC/producto del operador.
"""
from __future__ import annotations
import importlib.util
import os
from pathlib import Path
import shutil
import tempfile
import sys


def require(ok, msg):
    if not ok:
        raise RuntimeError(msg)


def main():
    require(os.environ.get("GITHUB_ACTIONS") == "true",
            "CI_ONLY_DO_NOT_RUN_ON_OPERATOR_PRODUCT")
    path = Path(__file__).resolve().parent/"a13_g8_adopt_local.py"
    spec=importlib.util.spec_from_file_location("a13_g8_adopt_ci",path)
    require(spec is not None and spec.loader is not None,"IMPORT_FAILED")
    gate=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=gate
    spec.loader.exec_module(gate)
    engine=gate.engine()
    products = {
        engine.RTU/"src/JWPLC_ModbusRTU.cpp":engine.CAND_RTU,
        engine.RS/"src/JWPLC_RS485.cpp":engine.CAND_RS,
        engine.ARCHIVE:None,
    }
    before=gate.git_index_marks(products)
    require(set(before)=={str(p.relative_to(gate.ROOT)) for p in products},
            "BASELINE_INDEX_PATHS_INVALID")
    require(all(value=="H" for value in before.values()),
            "BASELINE_GIT_FLAGS_NOT_H")
    require(not engine.git_paths("diff","--name-only") and
            not engine.git_paths("diff","--cached","--name-only"),
            "NOT_CLEAN_CI_CHECKOUT")
    print("G8_CI_INDEX_FLAGS_ALL_H=PASS",flush=True)
    for target,source in products.items():
        if source is not None:
            shutil.copy2(source,target)
        else:
            with target.open("ab") as binary:
                binary.write(b"G8_CI_SIMULATED_ARCHIVE_DIFF_20261011")
    with tempfile.TemporaryDirectory(prefix="jwplc_g8_index_ci_") as tmp:
        engine.RES=Path(tmp)
        engine.M={
            "schema":"G8-INDEX-CI-1",
            "run_id":"ci_git_index_exact_three",
            "status":"IN_PROGRESS",
            "phases":{},
        }
        gate.audit_git_scope(engine,products,"CI_ADOPTED")
    print("G8_CI_SAME_ADOPTION_HELPER_EXACT_THREE=PASS",flush=True)
    print("G8_CI_PRODUCT_ALTERATION=EPHEMERAL_RUNNER_ONLY",flush=True)


if __name__=="__main__":
    main()
