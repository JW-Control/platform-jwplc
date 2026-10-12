#!/usr/bin/env python3
"""Alpha13-G8 P3: adopción local controlada de RTU/RS485 + archive, con rollback.

SOLO modifica exactamente dos .cpp productivos y un .a RTU tras respaldo.
NO hace flash, RS485 físico/COM, FRAM, git add/commit/push, merge ni release.
Requiere un candidato P1/P2 validado y artefactos de Windows intactos.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT=Path(__file__).resolve().parents[3]
GATES=ROOT/"tools/alpha13/gates"
ENGINE=GATES/"a13_g8_p1_p2_candidate.py"
PREVIOUS_ID="g8_p1p2_20261011_190500_d59b4022"
PREVIOUS_ARCHIVE_SHA="f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416"
REQUIRED_CI_COMMIT="faa2f913de8a11416da7f20cb764bfb6700032a0"
FRAM_SHA="df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299"


def require(value, reason):
    if not value:
        raise RuntimeError(reason)


def emit(name,value):
    print("G8_ADOPT_"+name+"="+str(value),flush=True)


def engine():
    spec=importlib.util.spec_from_file_location("g8_p1p2_engine",ENGINE)
    require(spec is not None and spec.loader is not None,"ENGINE_SPEC_INVALID")
    obj=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=obj
    spec.loader.exec_module(obj)
    return obj


def git_status(*args):
    cmd=["git","-c","core.quotepath=false",*args]
    proc=subprocess.run(cmd,cwd=ROOT,capture_output=True)
    return proc.returncode


def git_index_marks(paths):
    """Read-only: catch index flags that hide actual worktree changes."""
    p=subprocess.run(
        ["git","-c","core.quotepath=false","ls-files","-v","--",
         *sorted(str(x.relative_to(ROOT)) for x in paths)],
        cwd=ROOT,capture_output=True,text=True,encoding="utf-8",errors="replace")
    require(p.returncode==0,"GIT_LS_FILES_V_ERROR")
    result={}
    for line in p.stdout.splitlines():
        if len(line)>=3 and line[1]==" ":
            result[line[2:]]=line[0]
    require(set(result)=={str(x.relative_to(ROOT)) for x in paths},
            "INDEX_NOT_TRACKING_ALL_PRODUCT_FILES")
    return result


def audit_git_scope(g, expected, phase):
    """Never infer a correct Git change merely from successful copy/hash."""
    observed=g.git_paths("diff","--name-only")
    staged=g.git_paths("diff","--cached","--name-only")
    flags=git_index_marks(expected)
    desired={str(p.relative_to(ROOT)) for p in expected}
    changes_by_bytes={}
    for target in expected:
        relative=str(target.relative_to(ROOT))
        expected_blob=g.git("rev-parse","HEAD:"+relative)
        local_hash=subprocess.run(
            ["git","hash-object","--",str(target)],
            cwd=ROOT,capture_output=True,text=True)
        require(local_hash.returncode==0,"GIT_HASH_OBJECT_FAILED_"+relative)
        changes_by_bytes[relative]=local_hash.stdout.strip()!=expected_blob
    emit(phase+"_DIFF_PATHS",";".join(sorted(observed)) or "NONE")
    emit(phase+"_EXPECTED_PATHS",";".join(sorted(desired)))
    emit(phase+"_INDEX_FLAGS",";".join(
        k+":"+flags[k] for k in sorted(flags)))
    emit(phase+"_BYTES_CHANGED",";".join(
        k+":"+str(changes_by_bytes[k]) for k in sorted(changes_by_bytes)))
    g.M["git_scope_"+phase.lower()]={
        "observed":sorted(observed),"expected":sorted(desired),
        "staged":sorted(staged),"index_flags":flags,
        "head_differs_from_worktree":changes_by_bytes,
    }
    g.save()
    require(not staged,phase+"_UNEXPECTED_STAGED_FILES")
    # Skip-worktree (S) or assume-unchanged (lowercase) masks legitimate changes.
    require(all(c.isupper() and c!="S" for c in flags.values()),
            phase+"_GIT_INDEX_FLAGS_HIDE_PRODUCT")
    require(all(changes_by_bytes.values()),
            phase+"_CANDIDATE_SAME_AS_GIT_HEAD")
    require(observed==desired,phase+"_GIT_DIFF_SCOPE_MISMATCH")
    return True


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduino-cli",default=r"C:\Program Files\Arduino CLI\arduino-cli.exe")
    args=parser.parse_args()
    g=engine()
    run_id="g8_adopt_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    g.RES=ROOT/"tools/alpha13/results"/run_id
    g.TMP=Path(tempfile.gettempdir())/("jwplc_a13_"+run_id)
    g.RES.mkdir(parents=True,exist_ok=False)
    g.TMP.mkdir(parents=True,exist_ok=False)
    backup=g.TMP/"originals"

    # Archivos productivos permitidos exclusivamente.
    TARGETS={
        g.RTU/"src/JWPLC_ModbusRTU.cpp":g.CAND_RTU,
        g.RS/"src/JWPLC_RS485.cpp":g.CAND_RS,
        g.ARCHIVE:Path(tempfile.gettempdir())/
            ("jwplc_a13_"+PREVIOUS_ID)/
            "candidate/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a",
    }
    names={str(p.relative_to(ROOT)) for p in TARGETS}
    g.M.clear()
    g.M.update({
        "schema":"A13-G8-PRODUCT-ADOPTION-1","run_id":run_id,
        "source_run":PREVIOUS_ID,"status":"REVIEW_STOP",
        "phases":{},"product_adopted":False,
        "upload":False,"serial_access":False,"fram_write":False,
        "git_commit":False,"git_push":False,"hardware_runtime_test":False,
        "candidate_ci_run":38186574868,
        "candidate_native_20_cases":"PASS",
        "result_path":str(g.RES),
    })
    backup_ready=False
    try:
        g.stage("PREFLIGHT","RUNNING")
        cli=Path(args.arduino_cli).expanduser().resolve()
        require(Path.cwd().resolve()==ROOT,"MUST_RUN_FROM_REPO_ROOT")
        require(cli.is_absolute() and cli.is_file() and
                cli.name.lower()=="arduino-cli.exe","CLI_NOT_ABSOLUTE")
        require(g.sha(cli)==g.CLI_SHA,"OFFICIAL_CLI_SHA_MISMATCH")
        require(g.git("branch","--show-current")==g.BRANCH,"WRONG_BRANCH")
        require(Path(g.git("rev-parse","--show-toplevel")).resolve()==ROOT,
                "GIT_REPO_ROOT_CHANGED")
        head=g.git("rev-parse","HEAD")
        require(head==g.git("rev-parse","refs/remotes/origin/"+g.BRANCH),
                "REMOTE_HEAD_DIFFERS_FROM_LOCAL")
        require(git_status("merge-base","--is-ancestor",REQUIRED_CI_COMMIT,"HEAD")==0,
                "NATIVE_CI_COMMIT_NOT_IN_HISTORY")
        require(not g.git_paths("diff","--name-only") and
                not g.git_paths("diff","--cached","--name-only"),
                "WORKTREE_ALREADY_DIRTY_OR_STAGED")
        marks=git_index_marks(TARGETS)
        emit("PREFLIGHT_INDEX_FLAGS",";".join(
            k+":"+marks[k] for k in sorted(marks)))
        g.M["preflight_index_flags"]=marks
        require(all(x.isupper() and x!="S" for x in marks.values()),
                "PREFLIGHT_GIT_INDEX_FLAG_ASSUMED_OR_SKIPPED")
        require(not g.git_paths("ls-files","--unmerged"),
                "GIT_UNMERGED_PATHS_PRESENT")
        require(all(g.sha(f)==h for f,h in g.EXPECTED.items()),
                "CORE_TFT_FRAM_BASELINE_CHANGED")
        require(g.sha(g.FRAM)==FRAM_SHA,"FRAM_ARCHIVE_CHANGED")

        previous=ROOT/"tools/alpha13/results"/PREVIOUS_ID/"MANIFEST.json"
        require(previous.is_file(),"P1P2_MANIFEST_NOT_FOUND")
        old=json.loads(previous.read_text(encoding="utf-8-sig"))
        require(old.get("schema")=="A13-G8-P1P2-CANDIDATE-1" and
                old.get("run_id")==PREVIOUS_ID and
                old.get("status")=="CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED" and
                old.get("phases",{}).get("ARCHIVE_REBUILD")=="PASS" and
                old.get("phases",{}).get("CANDIDATE_REGRESSIONS")=="PASS" and
                old.get("phases",{}).get("FINAL_AUDIT")=="PASS" and
                old.get("candidate_regressions")==["SLAVE","MASTER","RS485_STATUS"],
                "P1P2_EVIDENCE_NOT_COMPLETE")
        require(old.get("archive_sha256")==PREVIOUS_ARCHIVE_SHA and
                old.get("archive_member_sha256")==old.get("source_object_sha256") and
                old.get("source_object_sha256"),
                "P1P2_ARCHIVE_OBJECT_PROVENANCE")
        require(old.get("source_sha256",{})==
                {str(p.relative_to(ROOT)):g.sha(p) for p in (g.CAND_RTU,g.CAND_RS)},
                "CANDIDATES_CHANGED_AFTER_P1P2")
        require(old.get("before_sha256",{})==
                {str(p.relative_to(ROOT)):g.sha(p) for p in g.PRODUCT},
                "BASELINE_PRODUCT_SHA_DIFFERS_P1P2")
        artifact=TARGETS[g.ARCHIVE]
        require(artifact.is_file() and
                g.sha(artifact)==PREVIOUS_ARCHIVE_SHA and
                Path(old.get("candidate_archive_path","")).resolve()==artifact.resolve(),
                "TEMP_ARCHIVE_NOT_EXACT_P1P2")
        require(all(f.is_file() for f in TARGETS),
                "SOURCE_OR_PRODUCT_TARGET_MISSING")
        # Ejecutar contrato de delta exacto ANTES de la adopción.
        check=g.run([sys.executable,"-B",g.SOURCE_TEST],"SOURCE_CONTRACT_PRE")
        require("G8_SOURCE_STATE=BASELINE_EXACT_DELTA" in check and
                "G8_SOURCE_CONTRACT=PASS" in check,
                "SOURCE_CONTRACT_PRE_NOT_EXACT")
        g.M["entry_head"]=head
        g.M["archive_sha256"]=PREVIOUS_ARCHIVE_SHA
        g.M["source_object_sha256"]=old["source_object_sha256"]
        g.M["candidate_sha256"]={
            str(dest.relative_to(ROOT)):g.sha(source)
            for dest,source in TARGETS.items()}
        g.M["before_sha256"]=old["before_sha256"]
        g.M["allowed_product_paths"]=sorted(names)
        g.M["cli_sha256"]=g.sha(cli)
        g.stage("PREFLIGHT","PASS")

        g.stage("BACKUP","RUNNING")
        for target in TARGETS:
            relative=target.relative_to(ROOT)
            destination=backup/relative
            destination.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(target,destination)
        backup_ready=True
        require(all(g.sha(target)==g.sha(backup/target.relative_to(ROOT))
                    for target in TARGETS),"BACKUP_PARITY_FAILED")
        g.M["backup_root"]=str(backup)
        g.stage("BACKUP","PASS")

        g.stage("PRODUCT_ADOPTION","RUNNING")
        for target,source in TARGETS.items():
            shutil.copy2(source,target)
        require(all(g.sha(target)==g.M["candidate_sha256"][str(target.relative_to(ROOT))]
                    for target in TARGETS),"ADOPTED_BYTES_NOT_CANDIDATE")
        audit_git_scope(g,TARGETS,"ADOPTION")
        check=g.run([sys.executable,"-B",g.SOURCE_TEST],"SOURCE_CONTRACT_POST")
        require("G8_SOURCE_STATE=ADOPTED_EXACT_CANDIDATE" in check and
                "G8_SOURCE_CONTRACT=PASS" in check,
                "ADOPTED_SOURCE_CONTRACT_FAILED")
        g.M["product_adopted"]=True
        g.stage("PRODUCT_ADOPTION","PASS")

        # Regresiones sin overlays ni --library explícito: seleccion normal por
        # --libraries del package. La compilación limpia evita caché antigua.
        g.stage("PRODUCT_REGRESSIONS","RUNNING")
        completed=[]
        for label,sketch in g.SKETCHES:
            build=g.TMP/("product_"+label.lower())
            output=g.run([cli,"compile","--fqbn",g.FQBN,
                          "-j","0","-v","--clean","--build-path",build,
                          "--libraries",g.LIBS,sketch],
                         "PRODUCT_COMPILE_"+label)
            g.assert_selected(output,label,"JWPLC_RS485",g.RS)
            objs=list(build.rglob("JWPLC_RS485.cpp.o"))
            require(len(objs)==1,"RS485_SOURCE_OBJECT_NOT_BUILT_"+label)
            require(not list(build.rglob("JWPLC_ModbusRTU.cpp.o")),
                    "RTU_COMPILED_FROM_CPP_INSTEAD_OF_ARCHIVE_"+label)
            if label!="RS485_STATUS":
                g.assert_selected(output,label,"JWPLC_ModbusRTU",g.RTU)
                g.link_check(output,g.RTU,label,g.ARCHIVE)
            completed.append(label)
            emit("PRODUCT_REGRESSION_"+label,"PASS")
        g.M["product_regressions"]=completed
        require(completed==["SLAVE","MASTER","RS485_STATUS"],"PRODUCT_CASE_MISSING")
        g.stage("PRODUCT_REGRESSIONS","PASS")

        g.stage("FINAL_AUDIT","RUNNING")
        for name,before_hash in old["before_sha256"].items():
            path=ROOT/name
            correct=(g.M["candidate_sha256"][name] if name in names else before_hash)
            require(g.sha(path)==correct,"FINAL_SHA_MISMATCH_"+name)
        audit_git_scope(g,TARGETS,"FINAL")
        require(g.git("rev-parse","HEAD")==head,"GIT_HEAD_CHANGED_DURING_RUN")
        require(g.sha(g.ARCHIVE)==PREVIOUS_ARCHIVE_SHA,
                "FINAL_RTU_ARCHIVE_HASH_MISMATCH")
        g.M["product_sha256"]={
            str(target.relative_to(ROOT)):g.sha(target)
            for target in TARGETS}
        g.stage("FINAL_AUDIT","PASS")
        g.M["status"]="PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_PHYSICAL_DECISION_AND_COMMIT"
        emit("STATUS",g.M["status"])
        emit("ARCHIVE_SHA256",g.sha(g.ARCHIVE))
        emit("BACKUP",backup)
        emit("RESULTS",g.RES)
        emit("GIT_COMMIT_OR_PUSH","NONE")
        emit("UPLOAD_OR_RS485_ACCESS","NONE")
        g.save()
        return 0
    except Exception as exc:
        g.M["reason"]=repr(exc)
        g.M["status"]="REVIEW_STOP_ROLLBACK"
        emit("STATUS",g.M["status"])
        emit("REASON",repr(exc))
        if backup_ready:
            try:
                for target in TARGETS:
                    shutil.copy2(backup/target.relative_to(ROOT),target)
                require(all(g.sha(target)==g.sha(backup/target.relative_to(ROOT))
                            for target in TARGETS),"ROLLBACK_SHA_MISMATCH")
                require(not g.git_paths("diff","--name-only") and
                        not g.git_paths("diff","--cached","--name-only"),
                        "ROLLBACK_GIT_NOT_CLEAN")
                g.M["rollback"]="PASS"
                g.M["product_adopted"]=False
                emit("ROLLBACK","PASS")
            except Exception as rollback_error:
                g.M["rollback"]="FAILED_"+repr(rollback_error)
                emit("ROLLBACK","REQUIRES_MANUAL_REVIEW")
        try:g.save()
        except Exception:pass
        emit("RESULTS",g.RES)
        return 2


if __name__=="__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        emit("FATAL",repr(exc))
        sys.exit(2)
