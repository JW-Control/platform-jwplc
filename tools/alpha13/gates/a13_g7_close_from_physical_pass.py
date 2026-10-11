#!/usr/bin/env python3
"""Alpha13-G7: cerrar desde PHYSICAL_PASS ya obtenido. NO hay upload.

Valida hashes y trazabilidad del firmware/FRAM restaurada y del archive
existente, adopta sólo fuentes+archive, ejecuta regresiones Arduino IDE
normales, audita, y hace commits/push no-force tras todos los PASS.
Ante un fallo antes de Git restaura fuentes respaldadas; no repite test
FRAM, NO comunica con COM4 y NO llama a arduino-cli upload.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import sys
import tempfile
import uuid

ROOT=Path(__file__).resolve().parents[3]
RUN="g7_20261011_164228_e5e86ddc"
APP="30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a"
ARCHIVE_SHA="477ba0ccc3b20e638eccd7dda7d22b209d1ce73003df34917dc9ea7d476d83dd"
OBJECT_SHA="52b4b3f5154e7b0b45f6db5098830167c30db79fa03e12667d1f36fa5ad0501e"
CLI_SHA="1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571"
SOURCE_HEAD="62ad977583d193842719579cbffdf1c1b82e5817"
R3_TEMP=Path(tempfile.gettempdir())/("jwplc_a13_"+RUN)
R3_RESULT=ROOT/"tools/alpha13/results"/("g7_fram_"+RUN)
RECOVERY_SCRIPT=ROOT/"tools/alpha13/gates/a13_g7_resume_physical_only.py"
SERIAL_HOST=ROOT/"tools/alpha13/gates/a13_g7_fram_serial.py"
G7_SCRIPT=ROOT/"tools/alpha13/gates/a13_g7_integrated.py"
SRC=R3_TEMP/"candidate/JW_FRAM"
ARCHIVE=SRC/"src/esp32/libJW_FRAM.a"
PROOF=R3_RESULT/"G7_FRAM_PHYSICAL_RECOVERY.json"
EVIDENCE=R3_RESULT/"G7_FRAM_PHYSICAL_RECOVERY.log"
FQBN="jwplc_local:esp32:jwplcbasic"

def line(k,v):print("G7_CLOSE_"+str(k)+"="+str(v),flush=True)

def verify(ok,reason):
    if not ok:raise RuntimeError(reason)

def sha(file):
    digest=hashlib.sha256()
    with Path(file).open("rb") as fd:
        for b in iter(lambda:fd.read(1024*1024),b""):
            digest.update(b)
    return digest.hexdigest()

def read_json(path):
    verify(path.is_file(),"MISSING_"+path.name)
    return json.loads(path.read_text(encoding="utf-8-sig"))

def import_gate():
    spec=importlib.util.spec_from_file_location("a13_g7_integrated_for_closure",G7_SCRIPT)
    verify(spec is not None and spec.loader is not None,"GATE_IMPORT_SPEC")
    gate=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=gate
    spec.loader.exec_module(gate)
    return gate

def validate_evidence(g, cli, run_id):
    verify(run_id==RUN,"WRONG_PHYSICAL_RUN")
    verify(sha(cli)==CLI_SHA,"CLI_BINARY_IDENTITY_CHANGED")
    m=read_json(R3_RESULT/"MANIFEST.json")
    p=read_json(PROOF)
    verify(m.get("schema")=="A13-G7-INTEGRATED-1" and
           m.get("run_id")==RUN and m.get("entry_head")==SOURCE_HEAD,
           "R3_MANIFEST_PROVENANCE")
    verify(m.get("status")=="REVIEW_STOP" and
           m.get("reason")=="FRAM_PHYSICAL_SERIAL_FAILED" and
           m.get("physical_upload") is True and
           m.get("fram_restored") is False and
           m.get("product_adopted") is False and
           m.get("git_product_committed") is False and
           m.get("git_docs_committed") is False and
           m.get("git_pushed") is False,
           "R3_NOT_A_CLEAN_UNADOPTED_UPLOAD")
    phases=m.get("phase",{})
    for key in ("PREFLIGHT","CANDIDATE_LINK","CANDIDATE_REGRESSIONS",
                "PHYSICAL_BUILD","BENCH_SAFETY","PHYSICAL_UPLOAD"):
        verify(phases.get(key)=="PASS","ORIGINAL_GATE_PHASE_"+key)
    verify(phases.get("FRAM_PHYSICAL")=="RUNNING",
           "ORIGINAL_PHYSICAL_SHOULD_BE_REVIEW")
    verify(m.get("app_sha256")==APP and
           m.get("archive_sha256")==ARCHIVE_SHA and
           m.get("source_object_sha256")==OBJECT_SHA and
           m.get("cli_sha256")==CLI_SHA and
           m.get("port")=="COM4","ORIGINAL_G7_BINARIES_OR_PORT_DIFFER")
    verify(m.get("resume_from_run")=="g7_20261011_162621_6ca7668e",
           "R3_SOURCE_LINEAGE_CHANGED")
    verify(m.get("before_sha256")==g.MANIFEST["before_sha256"] and
           m.get("candidate_sha256")==g.MANIFEST["candidate_sha256"],
           "ORIGINAL_SOURCE_OR_CANDIDATE_CHANGED")
    verify(p.get("schema")=="A13-G7-PHYSICAL-RESUME-1" and
           p.get("status")=="PHYSICAL_PASS" and
           p.get("run_id")==RUN and p.get("upload_run")==RUN and
           p.get("app_sha256")==APP and
           p.get("original_manifest_app_sha256")==APP and
           p.get("serial_port")=="COM4" and
           p.get("run_command_sent") is True and
           p.get("serial_done_observed") is True and
           p.get("host_exit")==0 and
           p.get("fram_restored_verified") is True and
           p.get("no_new_upload") is True and
           p.get("product_mutated") is False and
           p.get("git_committed") is False,
           "RECOVERY_PROOF_NOT_PHYSICAL_PASS")
    verify(p.get("host_sha256")==sha(SERIAL_HOST) and
           p.get("log_sha256")==sha(EVIDENCE),
           "RECOVERY_HOST_OR_LOG_SHA_MISMATCH")
    log=EVIDENCE.read_text(encoding="utf-8-sig")
    for marker in (
         "G7_RECOVERY_PROTOCOL=A13-G7-PHYSICAL-1",
         "G7_SERIAL_DTR_RTS=FALSE_FALSE",
         "G7_SERIAL_READY_MATCH=PASS",
         "G7_SERIAL_COMMAND=G7_FRAM_RUN_SENT_ONCE",
         "G7_SERIAL_G7_FRAM_BACKUP_PASS=1",
         "G7_SERIAL_G7_FRAM_NULL_AND_BOUNDS_ERRORS=0",
         "G7_SERIAL_G7_FRAM_VALID_BLOCK_ERRORS=0",
         "G7_SERIAL_G7_FRAM_CORRUPT_BLOCK_ERRORS=0",
         "G7_SERIAL_G7_FRAM_VERSION_ERRORS=0",
         "G7_SERIAL_G7_FRAM_RESTORE_PASS=1",
         "G7_SERIAL_G7_FRAM_TOTAL_ERRORS=0",
         "G7_SERIAL_G7_FRAM_RESULT=PASS",
         "G7_SERIAL_G7_FRAM_DONE="+RUN,
         "G7_FRAM_PHYSICAL=PASS_BACKUP_RESTORE_CHECKSUM_NULL_BOUNDS",
         "RESTORATION_CONFIRMED=True",
         "HOST_EXIT=0",
    ):
        verify(marker in log,"RECOVERY_LOG_MISSING_"+marker)
    verify(log.count("G7_SERIAL_COMMAND=G7_FRAM_RUN_SENT_ONCE")==1,
           "FRAM_COMMAND_COUNT_NOT_ONE")
    verify(RECOVERY_SCRIPT.is_file() and
           sha(SERIAL_HOST)==p["host_sha256"],"SERIAL_HOST_CHANGED")

    uploaded=list((R3_TEMP/"build_G7_PHYSICAL").rglob("*.ino.bin"))
    verify(len(uploaded)==1 and sha(uploaded[0])==APP,
           "ORIGINAL_UPLOADED_APP_ARTIFACT_NOT_VERIFIED")
    verify(SRC.is_dir() and ARCHIVE.is_file() and sha(ARCHIVE)==ARCHIVE_SHA,
           "R3_ARCHIVE_MISSING_OR_SHA_CHANGED")
    verify(sha(SRC/"src/JW_FRAM.h")==g.MANIFEST["candidate_sha256"][
            "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.h"],
           "R3_HEADER_DOES_NOT_MATCH_CANDIDATE")
    g.MANIFEST["archive_sha256"]=ARCHIVE_SHA
    g.MANIFEST["source_object_sha256"]=OBJECT_SHA
    g.MANIFEST["app_sha256"]=APP
    g.MANIFEST["candidate_regressions"]=["GLOBAL_FRAM","CONFIG_BLOCK"]
    g.MANIFEST["physical_run_id"]=RUN
    g.MANIFEST["physical_recovery_proof_sha256"]=sha(PROOF)
    g.MANIFEST["physical_recovery_log_sha256"]=sha(EVIDENCE)
    g.MANIFEST["new_firmware_upload"]=False
    g.MANIFEST["no_physical_retest"]=True
    for label in ("CANDIDATE_LINK","CANDIDATE_REGRESSIONS_GLOBAL_FRAM",
                  "CANDIDATE_REGRESSIONS_CONFIG_BLOCK","G7_PHYSICAL"):
        path=R3_RESULT/("COMPILE_"+label+".log")
        verify(path.is_file(),"MISSING_BUILD_LOG_"+label)
        text=path.read_text(encoding="utf-8-sig")
        verify(re.search(r"(?m)^EXIT=0\s*$",text)!=None,
               "BUILD_LOG_FAILED_"+label)
        g.assert_fram_archive_link(text,SRC,"R3_"+label)
    for name in ("CANDIDATE_LINK","CANDIDATE_REGRESSIONS",
                 "PHYSICAL_BUILD","BENCH_SAFETY","PHYSICAL_UPLOAD"):
        g.MANIFEST["phase"][name]="PASS_REUSED_FROM_"+RUN
    g.MANIFEST["phase"]["SOURCE_REBUILD"]="PASS_REUSED_FROM_R2"
    g.MANIFEST["phase"]["ARCHIVE_REBUILD"]="PASS_REUSED_FROM_R2"
    g.MANIFEST["phase"]["FRAM_PHYSICAL"]="PASS"
    g.MANIFEST["physical_upload"]=True
    g.MANIFEST["fram_restored"]=True
    g.stage("PHYSICAL_EVIDENCE_REVIEW","PASS")
    line("PROOF_SHA256",g.MANIFEST["physical_recovery_proof_sha256"])
    line("PHYSICAL_RUN",RUN)
    line("FRAM_RESTORE_VERIFIED","PASS")
    line("ALREADY_UPLOADED_APP_SHA256",APP)
    line("ARCHIVE_SHA256",ARCHIVE_SHA)
    return m

def execute(cli,port,run):
    g=import_gate()
    g.RUN_ID="g7_close_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    g.RESULT=g.ROOT/"tools/alpha13/results"/("g7_fram_"+g.RUN_ID)
    g.TEMP=Path(tempfile.gettempdir())/("jwplc_a13_"+g.RUN_ID)
    g.BACKUP=g.TEMP/"product_backup"
    verify(not g.RESULT.exists() and not g.TEMP.exists(),
           "RUN_OR_TEMP_ALREADY_EXISTS")
    g.RESULT.mkdir(parents=True)
    g.TEMP.mkdir(parents=True)
    g.ADOPTED=False
    g.COMMITTED=False
    g.MANIFEST.clear()
    g.MANIFEST.update({
        "schema":"A13-G7-INTEGRATED-1","run_id":g.RUN_ID,
        "status":"REVIEW","phase":{},
        "physical_upload":False,"fram_restored":False,
        "product_adopted":False,"git_product_committed":False,
        "git_docs_committed":False,"git_pushed":False,
        "closure_from_prior_physical_pass":True,
    })
    entry=None
    try:
        g.preflight(cli,port)
        entry=g.MANIFEST["entry_head"]
        g.call([sys.executable,"-B",
               ROOT/"tools/alpha13/gates/a13_finalizer_portability_selftest.py"],
               "F112_PORTABILITY_SELFTEST")
        g.call([sys.executable,"-B",
               ROOT/"tools/alpha13/gates/a13_g7_contract_selftest.py"],
               "G7_CONTRACT_SELFTEST")
        validate_evidence(g,cli,run)
        candidate=g.TEMP/"verified_candidate"/"JW_FRAM"
        shutil.copytree(SRC,candidate)
        copied=candidate/"src/esp32/libJW_FRAM.a"
        verify(sha(copied)==ARCHIVE_SHA and
               sha(candidate/"src/JW_FRAM.h")==g.MANIFEST["candidate_sha256"][
                   "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.h"],
               "COPIED_ARCHIVE_OR_HEADER_DIFFERS")
        g.stage("ARCHIVE_REUSE","PASS")
        g.adopt(copied)
        g.regression(str(cli),g.FRAM,"PRODUCT_REGRESSIONS")
        g.audit(copied)
        # Original auto-closure is exact-path and non-force, as G6/G7 policy.
        g.auto_git_close()
        g.MANIFEST["status"]="CLOSED_PASS"
        g.log("STATUS","CLOSED_PASS")
        line("STATUS","CLOSED_PASS")
        line("NO_UPLOAD_NO_FRAM_WRITE_NO_SERIAL","YES")
        g.save()
        return 0
    except (Exception,KeyboardInterrupt) as exc:
        g.MANIFEST["status"]="REVIEW_STOP"
        g.MANIFEST["reason"]=str(exc)
        g.MANIFEST["category"]=(
            exc.kind if isinstance(exc,g.Review)
            else "USER_ABORT" if isinstance(exc,KeyboardInterrupt)
            else "HARNESS_OR_ENVIRONMENT")
        advanced=True
        try:
            advanced=(entry is None or g.git("rev-parse","HEAD")!=entry)
        except Exception:pass
        if not g.COMMITTED and not advanced:
            g.rollback()
        elif g.ADOPTED:
            g.log("ROLLBACK","SKIPPED_GIT_HEAD_ADVANCED_OR_UNVERIFIED")
        g.log("STATUS","REVIEW_STOP")
        g.log("REASON",str(exc))
        line("STATUS","REVIEW_STOP")
        line("REASON",str(exc))
        line("GIT_PARTIAL_COMMIT",bool(g.COMMITTED or advanced))
        line("NO_UPLOAD_NO_FRAM_WRITE_NO_SERIAL","YES")
        g.save()
        return 2

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--arduino-cli",required=True)
    p.add_argument("--serial-port",default="COM4")
    p.add_argument("--physical-run",required=True)
    args=p.parse_args()
    try:
        cli=Path(args.arduino_cli).expanduser()
        verify(cli.is_absolute() and cli.is_file() and
               "arduino plc ide tools" not in str(cli).lower(),
               "OFFICIAL_ARDUINO_CLI_ABSOLUTE_REQUIRED")
        verify(args.serial_port=="COM4","PORT_MUST_BE_COM4")
        verify(args.physical_run==RUN,"PHYSICAL_RUN_NOT_ALLOWLISTED")
        return execute(cli.resolve(),args.serial_port,args.physical_run)
    except Exception as exc:
        line("STATUS","REVIEW_STOP")
        line("ERROR",repr(exc))
        return 2

if __name__=="__main__":
    sys.exit(main())
