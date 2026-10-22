#!/usr/bin/env python3
"""F121 regression: manifest->closure document contract, NO COMPILE OR GIT.

Runs gate.regression() with mocked compiler/linker and calls the real
close_documents(). Catches missing/misspelled manifest keys BEFORE adoption.
"""
from __future__ import annotations
import importlib.util
from pathlib import Path
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[3]
GATE=ROOT/"tools/alpha13/gates/a13_g7_integrated.py"

def must(ok,label):
    if not ok:raise RuntimeError("G7_CLOSE_SELFTEST_"+label)
    print("G7_CLOSE_SELFTEST_"+label+"=PASS",flush=True)

def main():
    spec=importlib.util.spec_from_file_location("a13_g7_doc_fixture",GATE)
    must(spec is not None and spec.loader is not None,"IMPORT")
    gate=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=gate
    spec.loader.exec_module(gate)
    m=gate.MANIFEST
    m.clear()
    # Replace only the compiler, linker checker and logging. The manifest
    # producer and document generator remain the real code under test.
    with tempfile.TemporaryDirectory(prefix="jwplc_a13_g7_doc_fixture_") as tmp:
        folder=Path(tmp)
        invoked=[]
        gate.compile=lambda cli,sketch,label,selected: (
            invoked.append((str(label),str(sketch))) or "FAKE_VERIFIED_ELF",
            folder)
        gate.assert_fram_archive_link=lambda verbose,selected,label: True
        gate.stage=lambda prefix,state: invoked.append(("PHASE_"+prefix,state))
        gate.log=lambda key,value: None

        gate.regression("NO_EXECUTION",gate.FRAM,"CANDIDATE_REGRESSIONS")
        gate.regression("NO_EXECUTION",gate.FRAM,"PRODUCT_REGRESSIONS")
        expected=["GLOBAL_FRAM","CONFIG_BLOCK"]
        must(m.get("candidate_regressions")==expected,"CANDIDATE_KEY")
        must(m.get("product_regressions")==expected,"PRODUCT_KEY")
        must("candidate_regressions_regressions" not in m and
             "product_regressions_regressions" not in m,
             "NO_DUPLICATED_SUFFIX")
        must(len([x for x in invoked if x[0].startswith(("CANDIDATE_","PRODUCT_"))])==4,
             "TWO_REGRESSIONS_EACH")

        m.update({
            "cli_path":r"C:\Program Files\Arduino CLI\arduino-cli.exe",
            "cli_sha256":"1"*64,
            "app_sha256":"2"*64,
            "port":"COM4",
            "source_object_sha256":"3"*64,
            "archive_sha256":"4"*64,
            "physical_run_id":"g7_20261011_164228_e5e86ddc",
            "new_firmware_upload":False,
            "physical_recovery_log_sha256":"5"*64,
            "physical_recovery_proof_sha256":"6"*64,
            "product_sha256":{x:"7"*64 for x in [*gate.PRODUCT,gate.ARCHIVE]},
        })
        docs=gate.close_documents()
        must(set(docs)=={gate.CLOSE_DOC,gate.CHECK_DOC,gate.STATUS_DOC},
             "DOCUMENT_SCOPE")
        text=docs[gate.CLOSE_DOC]
        must("Regresiones producto: ['GLOBAL_FRAM', 'CONFIG_BLOCK']" in text,
             "DOCUMENT_USES_REAL_PRODUCT_REGRESSIONS")
        must("Regresiones candidato: ['GLOBAL_FRAM', 'CONFIG_BLOCK']" in text,
             "DOCUMENT_USES_REAL_CANDIDATE_REGRESSIONS")
        must("g7_20261011_164228_e5e86ddc" in text and
             "PRUEBA_FISICA_PROOF_SHA256=" in text,
             "PHYSICAL_LINEAGE_AND_PROOF")
        must("G7_FRAM=CLOSED_PASS" in docs[gate.STATUS_DOC],
             "STATUS_DOCUMENT_CLOSURE")
        # Regression test: absent key reports a labeled harness error,
        # not an unhandled KeyError after product adoption.
        m.pop("product_regressions")
        try:
            gate.close_documents()
        except gate.Review as exc:
            must("DOCUMENT_MANIFEST_REGRESSIONS_INVALID_product_regressions"
                 in str(exc),"REJECTS_MISSING_KEY")
        else:
            must(False,"REJECTS_MISSING_KEY")
    print("G7_CLOSURE_DOCUMENT_SELFTEST=PASS",flush=True)

if __name__=="__main__":
    try:main()
    except Exception as exc:
        print("G7_CLOSURE_DOCUMENT_SELFTEST=FAIL",flush=True)
        print("G7_CLOSURE_DOCUMENT_REASON="+repr(exc),flush=True)
        sys.exit(2)
