#!/usr/bin/env python3
"""G10-H N0: staging Windows/Linux byte-for-byte, no hardware or firmware edits.

Cubre el bug real G10-H: Path.write_text traduce LF a CRLF en Windows,
provocando STAGED_SOURCE_SHA_DIFFERS aunque G10-I CI Linux fuera PASS.
"""
from __future__ import annotations
import hashlib
import importlib.util
from pathlib import Path
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[3]
RUNNER=ROOT/"tools/alpha13/gates/a13_g10_integrated.py"

def require(condition,message):
    if not condition: raise RuntimeError(message)

def sha(value):
    return hashlib.sha256(value).hexdigest()

def main():
    spec=importlib.util.spec_from_file_location("g10h_portable_stager",RUNNER)
    require(spec is not None and spec.loader is not None,"INTEGRATOR_NOT_LOADABLE")
    runner=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=runner
    spec.loader.exec_module(runner)
    runner.preflight(True)
    files=runner.compose()
    require(len(files)>10,"EXPECTED_INTEGRATED_OVERLAY_SOURCES")
    hashes={rel:{"sha256":sha(body.encode("utf-8"))}
            for rel,body in files.items()}
    with tempfile.TemporaryDirectory(prefix="g10h-staging-selftest-") as d:
        scratch=Path(d)
        stage=scratch/"valid"
        manifest=runner.stage_platform(stage,files,hashes)
        require(set(manifest)==set(files),"STAGED_MANIFEST_INCOMPLETE")
        prefix="JWPLC/2.1.0/"
        for rel,body in files.items():
            require(rel.startswith(prefix),"WRONG_OVERLAY_PREFIX")
            real=(stage/rel[len(prefix):]).read_bytes()
            expected=body.encode("utf-8")
            require(real==expected,"STAGED_BYTES_CHANGED_"+rel)
            require(sha(real)==hashes[rel]["sha256"],
                    "STAGED_HASH_CHANGED_"+rel)
            require(manifest[rel]["sha256"]==hashes[rel]["sha256"],
                    "MANIFEST_DIGEST_CHANGED_"+rel)
        # Negative mutation: the integrity guard MUST reject an injected hash,
        # rather than hiding Windows line-ending drift by skipping validation.
        rel=next(iter(files))
        bad_hashes={rel:{"sha256":"0"*64}}
        failure=None
        try:
            runner.stage_platform(scratch/"negative",
                                  {rel:files[rel]},bad_hashes)
        except RuntimeError as exc:
            failure=str(exc)
        require(failure=="STAGED_SOURCE_SHA_DIFFERS_"+rel,
                "NEGATIVE_HASH_NOT_REJECTED_"+str(failure))
        for rel in ("boards.txt","platform.txt",
                    "precompiled/core/JWPLCBASIC/core.a"):
            source=(ROOT/"JWPLC/2.1.0"/rel).read_bytes()
            staged=(stage/rel).read_bytes()
            require(source==staged,"FROZEN_CORE_DIFFERENCE_"+rel)
    print("G10_H_STAGE_PORTABILITY=PASS_BYTES_IDENTICAL_"+str(len(files)))
    print("G10_H_STAGE_HASH_NEGATIVE=PASS_REJECTED")
    print("G10_H_STAGED_CORE_AND_BOARD=PASS_UNCHANGED")
    print("G10_H_HARDWARE=NOT_TOUCHED")
    return 0

if __name__=="__main__":
    sys.exit(main())
