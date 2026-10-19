#!/usr/bin/env python3
"""Alpha13 H0-F1: E1/E2, N2 y N3 sobre el PACKAGE real, nunca overlay H0/G10."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from a13_g10_r_b3_matrix import SKETCHES

ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/"JWPLC/2.1.0"
W5100="libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
BASE="a9c96273274170195498ff282decf93f0388a450"
FQBNS=("jwplc_local:esp32:jwplcbasic","jwplc_local:esp32:jwplcbasiccore")

def sha256(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def command(args, log=None, timeout=420, check=True):
    p=subprocess.run([str(x) for x in args], cwd=ROOT, capture_output=True,
                     text=True, errors="replace", timeout=timeout)
    output=(p.stdout or "")+"\n"+(p.stderr or "")
    if log:
        Path(log).write_text(output,encoding="utf-8")
    if check and p.returncode:
        raise RuntimeError("COMMAND_FAILED:"+repr(args[:4])+":"+output[-1700:])
    return p.returncode,output

def preflight():
    assert len(SKETCHES)==11 and len(set(SKETCHES))==11
    for sketch in SKETCHES:
        assert (ROOT/sketch).is_dir(),sketch
    assert "SS_PIN_DEFAULT" in (SOURCE/W5100).read_text(), "W5100_MISSING"
    print("H0_F1_MATRIX=PASS_11_SKETCHES_PACKAGE_ONLY_NO_OVERLAY")
    
def result_for(obj_dir,hardware,log_text):
    selected="in folder: "+str(hardware/"libraries/JWPLC_Ethernet")
    assert any(line.startswith("Using library JWPLC_Ethernet ") and selected in line
               for line in log_text.splitlines()),"WRONG_LIBRARY_SELECTED"
    found=list(obj_dir.rglob("w5100.cpp.o"))
    assert len(found)==1, "W5100_SOURCE_OBJECT_NOT_UNIQUE:"+str(found)
    target=hardware/W5100
    dep=found[0].with_suffix(".d")
    assert dep.is_file(), "W5100_DEPENDENCIES_MISSING"
    deps=dep.read_text(encoding="utf-8",errors="replace").replace("\\\n"," ")
    assert str(target) in deps, "W5100_SOURCE_TO_DEP_MISMATCH"
    elfs=list(obj_dir.glob("*.elf"))
    assert len(elfs)==1, "F1_ELF_NOT_UNIQUE"
    return {"object_sha256":sha256(found[0]),
            "source_sha256":sha256(target),
            "elf_sha256":sha256(elfs[0]),
            "elf":str(elfs[0]),"dependencies":str(dep)}

def compile_one(fqbn,sketch,hardware,tmp,out,label):
    code,properties=command(["arduino-cli","compile","--fqbn",fqbn,
                             "--show-properties",str(sketch)],
                            log=out/("props_"+label+".txt"))
    assert "runtime.platform.path="+str(hardware) in properties, "WRONG_PLATFORM:"+label
    build=tmp/("build-"+label)
    _,log=command(["arduino-cli","compile","--fqbn",fqbn,
                   "--warnings","default","--build-path",str(build),
                   "--clean","-v",str(sketch)],
                  log=out/("compile_"+label+".log"))
    prov=result_for(build,hardware,log)
    prov.pop("dependencies")
    print("H0_F1_COMPILE_"+label+"=PASS_SOURCE_TO_OBJECT_ELF",flush=True)
    return prov

def gdb_value(elf):
    choices=sorted((Path.home()/".arduino15/packages").glob("**/xtensa-esp32-elf-gdb"))
    choices+=sorted((Path.home()/".arduino15/packages").glob("**/xtensa-esp32-elf-gdb.exe"))
    tool=shutil.which("xtensa-esp32-elf-gdb") or (str(choices[-1]) if choices else None)
    if not tool: return {"state":"PENDING_GDB_UNAVAILABLE"}
    rc,txt=command([tool,"-batch","-ex","p/d 'W5100Class::ss_pin'",str(elf)],check=False)
    if rc or not re.search(r"\$\d+\s*=\s*(\d+)",txt):
        return {"state":"REVIEW_GDB_SYMBOL_UNAVAILABLE","log":txt[-600:]}
    number=int(re.search(r"\$\d+\s*=\s*(\d+)",txt).group(1))
    return {"state":"PASS_GDB","value":number}

def sdk_qio():
    paths=sorted((Path.home()/".arduino15/packages").glob(
        "**/esp32/qio_qspi/include/sdkconfig.h"))
    for p in paths:
        data=p.read_text(encoding="utf-8",errors="replace")
        found=re.search(r"^#define\s+CONFIG_ESPTOOLPY_FLASHMODE_QIO\s+(\d+)\s*$",data,re.M)
        if found:
            return {"state":"PASS_QIO" if found.group(1)=="1" else "REVIEW_QIO_NOT_1",
                    "value":int(found.group(1)),"file":str(p)}
    return {"state":"PENDING_SDKCONFIG_NOT_LOCATED"}

def make_probe(temp):
    folder=temp/"H0_F1_EarlyLink"
    folder.mkdir(parents=True)
    path=folder/"H0_F1_EarlyLink.ino"
    path.write_text("#include <JWPLC_Ethernet.h>\n"
                    "void setup(){ (void)JWPLC_Ethernet.linkStatus(); }\n"
                    "void loop(){}\n",encoding="utf-8")
    return folder

def build(output):
    preflight()
    output=Path(output)
    output.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="alpha13-h0-f1-package-") as t:
        tmp=Path(t)
        hardware=tmp/"sketchbook/hardware/jwplc_local/esp32"
        hardware.parent.mkdir(parents=True)
        shutil.copytree(SOURCE,hardware)
        for p in ("boards.txt","platform.txt"):
            assert sha256(SOURCE/p)==sha256(hardware/p)
        archives=sorted(SOURCE.rglob("*.a"))
        assert len(archives)>=5,"ARCHIVE_INVENTORY_REVIEW"
        for src in archives:
            assert sha256(src)==sha256(hardware/src.relative_to(SOURCE)), "FROZEN_ARCHIVE_CHANGED"
        command(["arduino-cli","config","set","directories.user",str(tmp/"sketchbook")])
        probe=make_probe(tmp)
        m={"status":"INCOMPLETE","baseline":BASE,"product_sha256":sha256(SOURCE/W5100),
           "archives":{str(x.relative_to(SOURCE)):sha256(x) for x in archives},
           "physical":"NOT_RUN","usb":"NOT_TOUCHED",
           "n2":{},"n3":[],"E1":{},"E2":{}}
        try:
            # E1: rebuild ORIGINAL unfixed source into isolated platform; no repo write.
            rc,baseline=command(["git","show",BASE+":JWPLC/2.1.0/"+W5100])
            candidate=(hardware/W5100).read_text(encoding="utf-8")
            assert "JWPLC F1:" not in baseline and "JWPLC F1:" in candidate
            (hardware/W5100).write_text(baseline,encoding="utf-8")
            old=compile_one(FQBNS[0],probe,hardware,tmp,output,"baseline_earlylink")
            m["E1"]=gdb_value(old["elf"])
            m["E1"]["expected"]=10
            m["E1"]["baseline_elf_sha256"]=old["elf_sha256"]
            if m["E1"]["state"]=="PASS_GDB":
                assert m["E1"]["value"]==10,"E1_OLD_ELF_VALUE_NOT_10"
            # Candidate = exact package source; no overlays applied.
            (hardware/W5100).write_text(candidate,encoding="utf-8")
            assert sha256(hardware/W5100)==sha256(SOURCE/W5100)
            for board in FQBNS:
                label="F1_"+board.split(":")[-1]
                m["n2"][board]=compile_one(board,probe,hardware,tmp,output,label)
                value=gdb_value(m["n2"][board]["elf"])
                m["n2"][board]["gdb"]=value
                if value["state"]=="PASS_GDB":
                    assert value["value"]==5,"F1_ELF_SS_NOT_5:"+board
            m["E2"]=sdk_qio()
            if m["E2"]["state"]=="REVIEW_QIO_NOT_1":
                raise RuntimeError("E2_SDKCONFIG_DISAGREES_QIO")
            for index,rel in enumerate(SKETCHES):
                label="N3_%02d_"%index+Path(rel).name
                entry=compile_one(FQBNS[0],ROOT/rel,hardware,tmp,output,label)
                m["n3"].append({"sketch":rel,"object":entry["object_sha256"],
                                 "elf":entry["elf_sha256"],"status":"PASS"})
            assert len(m["n3"])==11,"N3_SKETCH_INCOMPLETE"
            m["status"]="PASS_N2_BOTH_N3_11_PACKAGE_SOURCE_FIRST"
            print("H0_F1_N3=PASS_11_OF_11_PACKAGE_NO_OVERLAY",flush=True)
        finally:
            (output/"manifest.json").write_text(json.dumps(m,indent=2),encoding="utf-8")
            print("H0_F1_E1="+m["E1"].get("state","NOT_RUN"),flush=True)
            print("H0_F1_E2="+m["E2"].get("state","NOT_RUN"),flush=True)
            print("H0_F1_BUILD_STATUS="+m["status"],flush=True)

if __name__=="__main__":
    ap=argparse.ArgumentParser()
    g=ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--selftest",action="store_true")
    g.add_argument("--run",action="store_true")
    ap.add_argument("--output",type=Path)
    args=ap.parse_args()
    if args.selftest:preflight()
    else:
        assert args.output is not None,"OUTPUT_REQUIRED"
        build(args.output)
