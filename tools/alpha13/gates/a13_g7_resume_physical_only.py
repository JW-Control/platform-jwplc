#!/usr/bin/env python3
"""G7 físico SIN re-upload: ejecutar una sola vez con token existente.

Requiere MANIFEST del upload G7, READY pasivo previo probado, FRAM aislada.
Abre COM4 via a13_g7_fram_serial.py (DTR=RTS=False), envía una sola
orden G7_FRAM_RUN y verifica restauración. No recompila, flashea,
modifica producto o Git; no completa G7 ni hace commits.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]
RUN="g7_20261011_164228_e5e86ddc"
APP_SHA="30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a"
FOLDER=ROOT/"tools/alpha13/results"/("g7_fram_"+RUN)
PASSIVE=ROOT/"tools/alpha13/results/g7_passive_uart_20261011_165410_c8f47164.log"
HOST=ROOT/"tools/alpha13/gates/a13_g7_fram_serial.py"
EVIDENCE=FOLDER/"G7_FRAM_PHYSICAL_RECOVERY.log"
PROOF=FOLDER/"G7_FRAM_PHYSICAL_RECOVERY.json"

def say(key,value):print(f"G7_RECOVER_{key}={value}",flush=True)

def verify(ok,msg):
    if not ok:raise RuntimeError(msg)

def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as fh:
        for block in iter(lambda:fh.read(1024*1024),b""):h.update(block)
    return h.hexdigest()

def check(args):
    verify(args.serial_port.upper()=="COM4","ONLY_COM4_ALLOWED")
    verify(args.run_id==RUN,"RUN_ID_MUST_MATCH_FLASHED_FIRMWARE")
    verify(EVIDENCE.exists() is False and PROOF.exists() is False,
           "RECOVERY_ALREADY_ATTEMPTED_DO_NOT_REPEAT")
    verify(HOST.is_file() and PASSIVE.is_file(),"HOST_OR_PASSIVE_EVIDENCE_MISSING")
    m=json.loads((FOLDER/"MANIFEST.json").read_text(encoding="utf-8-sig"))
    verify(m.get("schema")=="A13-G7-INTEGRATED-1" and
           m.get("run_id")==RUN and
           m.get("app_sha256")==APP_SHA and
           m.get("port","").upper()=="COM4",
           "ORIGINAL_UPLOAD_IDENTITY_MISMATCH")
    verify(m.get("physical_upload") is True and
           m.get("status")=="REVIEW_STOP" and
           m.get("reason")=="FRAM_PHYSICAL_SERIAL_FAILED" and
           m.get("product_adopted") is False and
           m.get("fram_restored") is False and
           m.get("git_product_committed") is False and
           m.get("git_docs_committed") is False and
           m.get("git_pushed") is False,
           "ORIGINAL_UPLOAD_NOT_ELIGIBLE")
    verify(m.get("phase",{}).get("PHYSICAL_UPLOAD")=="PASS" and
           m.get("phase",{}).get("FRAM_PHYSICAL")=="RUNNING",
           "ORIGINAL_PHASES_NOT_SUITABLE")
    oldlog=(FOLDER/"FRAM_PHYSICAL_SERIAL.log").read_text(encoding="utf-8-sig")
    verify("G7_SERIAL_ERROR=READY_TIMEOUT" in oldlog and
           "G7_SERIAL_COMMAND=G7_FRAM_RUN_SENT_ONCE" not in oldlog,
           "ORIGINAL_TEST_NOT_CLEAN_READY_TIMEOUT")
    ready="G7_FRAM_READY="+RUN
    oldready=PASSIVE.read_text(encoding="utf-8-sig")
    verify(oldready.count(ready)>=10,
           "PASSIVE_READY_EVIDENCE_DOES_NOT_MATCH_CURRENT_FIRMWARE")
    say("RUN_ID",RUN)
    say("APP_SHA256",APP_SHA)
    say("PASSIVE_READY_COUNT",oldready.count(ready))
    say("SOURCE","ALREADY_FLASHED_NO_NEW_UPLOAD")
    say("SERIAL_RESET_LINES","DTR_FALSE_RTS_FALSE")
    say("FRAM_WINDOW","0x1F00..0x1F7F_BACKUP_NVS_RESTORE")
    return m

def execute(args):
    m=check(args)
    params=[sys.executable,"-B",str(HOST),
            "--serial-port",args.serial_port,"--token",RUN]
    cmd_sent=False
    serial_done=False
    success=False
    code=None
    # x/unique evidence prevents unintentional duplicate FRAM writes
    # after failure, including interruption during backup/restoration.
    with EVIDENCE.open("x",encoding="utf-8",buffering=1) as log:
        log.write("G7_RECOVERY_PROTOCOL=A13-G7-PHYSICAL-1\n")
        log.write("ORIGINAL_APP_SHA256="+APP_SHA+"\n")
        log.write("ORIGINAL_RUN_ID="+RUN+"\n")
        log.write("ORIGINAL_UPLOAD=PASS\nNEW_UPLOAD=NO\n")
        log.write("ORIGINAL_FRAM_RUN_SENT=NO\n")
        p=subprocess.Popen(params,cwd=ROOT,stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT,text=True,bufsize=1,
                           encoding="utf-8",errors="replace")
        lines=[]
        for line in p.stdout:
            line=line.rstrip("\r\n")
            lines.append(line)
            log.write(line+"\n")
            print(line,flush=True)
            if line=="G7_SERIAL_COMMAND=G7_FRAM_RUN_SENT_ONCE":cmd_sent=True
            if line=="G7_SERIAL_G7_FRAM_DONE="+RUN:serial_done=True
        code=p.wait()
        success=(code==0 and
                 "G7_FRAM_PHYSICAL=PASS_BACKUP_RESTORE_CHECKSUM_NULL_BOUNDS"
                  in lines and
                 "G7_CHECK_G7_FRAM_RESTORE_PASS=PASS" in lines and
                 "G7_CHECK_G7_FRAM_TOTAL_ERRORS=PASS" in lines and
                 serial_done and cmd_sent)
        log.write("HOST_EXIT="+str(code)+"\n")
        log.write("RESTORATION_CONFIRMED="+str(success)+"\n")
    proof={
       "schema":"A13-G7-PHYSICAL-RESUME-1",
       "run_id":RUN,"upload_run":RUN,
       "app_sha256":APP_SHA,
       "host_sha256":sha(HOST),
       "log_sha256":sha(EVIDENCE),
       "original_manifest_app_sha256":m["app_sha256"],
       "serial_port":args.serial_port,
       "run_command_sent":cmd_sent,
       "serial_done_observed":serial_done,
       "host_exit":code,
       "fram_restored_verified":success,
       "status":"PHYSICAL_PASS" if success else "REVIEW_STOP",
       "no_new_upload":True,"product_mutated":False,"git_committed":False
    }
    PROOF.write_text(json.dumps(proof,indent=2,ensure_ascii=False)+"\n",
                     encoding="utf-8")
    say("SERIAL_EXIT",code)
    say("RUN_COMMAND_SENT",cmd_sent)
    say("SERIAL_DONE",serial_done)
    say("FRAM_RESTORE_VERIFIED",success)
    say("EVIDENCE",EVIDENCE)
    say("PROOF",PROOF)
    say("STATUS",proof["status"])
    if not success:
        say("SAFETY","NO_RETRY_NO_NEW_FLASH_IF_FRAM_BACKUP_MAY_BE_PENDING")
        return 3
    return 0

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--serial-port",required=True)
    p.add_argument("--run-id",required=True)
    args=p.parse_args()
    try:return execute(args)
    except Exception as exc:
        say("STATUS","REVIEW_STOP")
        say("ERROR",repr(exc))
        say("SAFETY","DO_NOT_RETRY_OR_REFLASH_BEFORE_REVIEW")
        return 2

if __name__=="__main__":
    sys.exit(main())
