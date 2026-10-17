#!/usr/bin/env python3
"""N3 candidato G10-R: 11 sketches en plataforma aislada, sin puertos.

Baseline N3 se reutiliza SOLO tras comparar JWPLC/ contra a954cabd;
los cambios del overlay se recompilan en 11 sketches candidatos.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/"JWPLC/2.1.0"
FQBN="jwplc_local:esp32:jwplcbasic"
G10I="a954cabd9c6e37876b59995a41ff0a449308ccd1"
SKETCHES=(
 "JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/examples/01.DigitalIO_Basic",
 "JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/examples/02.Buttons_Basic",
 "JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/examples/03.RTC_Basic",
 "JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/examples/04.FRAM_Basic",
 "JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/examples/05.microSD_Basic",
 "JWPLC/2.1.0/libraries/JWPLC_RS485/examples/03.RS485_Status",
 "JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/examples/01.ModbusRTU_Slave_Holding",
 "JWPLC/2.1.0/libraries/JWPLC_Display/examples/01.Display_IDLE_Status",
 "JWPLC/2.1.0/libraries/JWPLC_Ethernet/examples/01.Ethernet_DHCP_Basic",
 "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/examples/01.ModbusTCP_Server",
 "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/examples/02.ModbusTCP_Client",
)
def execute(args,*,output=None):
    p=subprocess.run([str(x) for x in args],cwd=ROOT,
        capture_output=True,text=True,errors="replace",timeout=360)
    content=(p.stdout or "")+"\n"+(p.stderr or "")
    if output is not None: output.write_text(content,encoding="utf-8")
    if p.returncode:raise RuntimeError(f"COMMAND_FAILED_{args[:3]}_{content[-1400:]}")
    return content

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def check_baseline():
    execute(["git","diff","--quiet",G10I,"HEAD","--","JWPLC/"])
    return execute(["git","rev-parse","HEAD"]).strip()

def test():
    assert len(SKETCHES)==11 and len(set(SKETCHES))==11
    assert all((ROOT/x).is_dir() for x in SKETCHES)
    assert all(x.startswith("JWPLC/2.1.0/") for x in SKETCHES)
    assert (ROOT/"tools/alpha13/gates/a13_g10_r_build_candidate.py").is_file()
    print("B3_N3_MATRIX=PASS_11_SKETCHES_OFFLINE")
    print("B3_N3_HARDWARE=NOT_RUN")

def build(output):
    head=check_baseline()
    with tempfile.TemporaryDirectory(prefix="jwplc-b3-n3-") as temp:
        tmp=Path(temp)
        staged=tmp/"staged"
        execute([sys.executable,"-B",
            "tools/alpha13/gates/a13_g10_r_build_candidate.py",
            "--stage-only","--stage-output",str(staged)],output=output/"stage.log")
        hardware=tmp/"sketchbook/hardware/jwplc_local/esp32"
        hardware.parent.mkdir(parents=True)
        shutil.copytree(staged,hardware)
        for filename in ("boards.txt","platform.txt"):
            assert sha(SOURCE/filename)==sha(hardware/filename),filename
        archives=sorted(SOURCE.rglob("*.a"))
        if len(archives)<5:raise RuntimeError("ARCHIVE_INVENTORY_TOO_SMALL")
        for original in archives:
            if sha(original)!=sha(hardware/original.relative_to(SOURCE)):
                raise RuntimeError("FROZEN_ARCHIVE_CONTENT_DRIFT")
        execute(["arduino-cli","config","set","directories.user",
                 str(tmp/"sketchbook")])
        result={"status":"NOT_COMPLETE","head":head,
                "baseline_n3":"REUSED_11_OF_11_RUN_38843599043",
                "baseline_source_equal_to":G10I,
                "candidate_compiled":0,"total":11,
                "archives":{str(x.relative_to(SOURCE)):sha(x) for x in archives},
                "physical":"NOT_RUN","upload":"NOT_RUN","sketches":[]}
        for rel in SKETCHES:
            sketch=ROOT/rel
            label=sketch.name
            prop=execute(["arduino-cli","compile","--fqbn",FQBN,
                "--show-properties",str(sketch)],output=output/("properties-"+label+".txt"))
            if "runtime.platform.path="+str(hardware) not in prop:
                raise RuntimeError("WRONG_PLATFORM_"+label)
            target=tmp/("build-"+label)
            log=output/("compile-"+label+".log")
            txt=execute(["arduino-cli","compile","--fqbn",FQBN,"--warnings",
                "default","--build-path",str(target),"--clean","-v",
                str(sketch)],output=log)
            require_ethernet="in folder: "+str(hardware/"libraries/JWPLC_Ethernet")
            if not any(line.startswith("Using library JWPLC_Ethernet ") and
                require_ethernet in line for line in txt.splitlines()):
                raise RuntimeError("WRONG_ETHERNET_LIBRARY_"+label)
            if label in ("01.ModbusTCP_Server","02.ModbusTCP_Client"):
                expected="in folder: "+str(hardware/"libraries/JWPLC_ModbusTCP")
                if not any(line.startswith("Using library JWPLC_ModbusTCP ") and
                    expected in line for line in txt.splitlines()):
                    raise RuntimeError("WRONG_MODBUSTCP_LIBRARY_"+label)
            objs=list(target.rglob("JWPLC_W5500_SafeAccess.cpp.o"))
            if len(objs)!=1:raise RuntimeError("SAFEACCESS_OBJECT_NOT_UNIQUE_"+label)
            dep=Path(str(objs[0])+".d")
            if not dep.is_file():dep=objs[0].with_suffix(".d")
            if not dep.is_file() or str(hardware/"libraries/JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.cpp") not in dep.read_text(encoding="utf-8",errors="replace"):
                raise RuntimeError("SOURCE_DEP_NOT_PROVEN_"+label)
            elfs=list(target.glob("*.elf"))
            if len(elfs)!=1:raise RuntimeError("ELF_NOT_UNIQUE_"+label)
            result["sketches"].append({"name":label,"status":"PASS_SOURCE_FIRST",
                "source_object_sha256":sha(objs[0]),"elf_sha256":sha(elfs[0])})
            result["candidate_compiled"]+=1
            print("B3_N3_CANDIDATE_"+label+"=PASS_SOURCE_FIRST",flush=True)
        if result["candidate_compiled"]!=11:raise RuntimeError("N3_INCOMPLETE")
        result["status"]="PASS_11_OF_11_CANDIDATE"
        (output/"manifest.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
        print("B3_N3_CANDIDATE=PASS_11_OF_11")
        print("B3_N3_BASELINE=REUSED_11_OF_11_SHA_EQUIVALENT")
        print("B3_N3_HARDWARE=NOT_RUN")
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    q=ap.add_mutually_exclusive_group(required=True)
    q.add_argument("--selftest",action="store_true")
    q.add_argument("--run",action="store_true")
    ap.add_argument("--output",type=Path)
    a=ap.parse_args()
    if a.selftest:test();return
    if a.output is None:raise SystemExit("B3_OUTPUT_REQUIRED")
    a.output.mkdir(parents=True,exist_ok=True)
    build(a.output)
if __name__=="__main__":main()
