#!/usr/bin/env python3
"""H0 E1/E2 complementario sin repetir N3: comprobar bytes de SS en ELF y SDK."""
from __future__ import annotations
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).parent))
import a13_h0_f1_arduino as a

def elf_ss_value(elf):
    elf=Path(elf)
    p=subprocess.run(["readelf","-Ws",str(elf)],capture_output=True,text=True,check=True)
    symbols=[]
    for line in p.stdout.splitlines():
        parts=line.split()
        if len(parts)>=8 and "ss_pin" in parts[-1] and parts[3]=="OBJECT" and parts[6].isdigit():
            symbols.append((int(parts[1],16),int(parts[2]),int(parts[6]),parts[-1]))
    symbols=[s for s in symbols if "W5100Class" in s[3] or "_ZN10W5100Class6ss_pinE" in s[3]]
    assert len(symbols)==1, "E1_ELF_SS_SYMBOL_NOT_UNIQUE:"+repr(symbols)
    addr,size,section,name=symbols[0]
    assert size==1,"E1_SS_NOT_UINT8"
    p=subprocess.run(["readelf","-WS",str(elf)],capture_output=True,text=True,check=True)
    entry=None
    for line in p.stdout.splitlines():
        m=re.match(r"^\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)",line)
        if m and int(m.group(1))==section:
            entry=(m.group(2),m.group(3),int(m.group(4),16),
                   int(m.group(5),16),int(m.group(6),16))
            break
    assert entry is not None,"E1_SECTION_NOT_FOUND"
    secname,sectype,vma,offset,length=entry
    assert sectype=="PROGBITS" and vma<=addr<vma+length
    at=offset+(addr-vma)
    val=elf.read_bytes()[at]
    return {"value":val,"symbol":name,"section":secname,
            "symbol_address_hex":hex(addr),"section_offset_hex":hex(offset),
            "ELF_sha256":a.sha256(elf)}

def sdk_config(properties):
    fields={}
    for line in properties.splitlines():
        if "=" in line:
            k,v=line.split("=",1)
            if k in ("compiler.sdk.path","build.memory_type","build.boot",
                     "build.flash_mode","build.flash_freq"):
                fields[k]=v
    sdk=Path(fields.get("compiler.sdk.path","/DOES_NOT_EXIST"))
    paths=[]
    if sdk.is_dir():
        paths=list(sdk.rglob("sdkconfig.h"))
    outcome={"state":"PENDING_NO_SDKCONFIG_QIO_MACRO","properties":fields,
             "sdkconfig_files_found":len(paths)}
    positives=[]
    for p in paths:
        try: t=p.read_text(encoding="utf-8",errors="replace")
        except OSError: continue
        m=re.search(r"^#define\s+CONFIG_ESPTOOLPY_FLASHMODE_QIO\s+(\d+)",t,re.M)
        if m:
            positives.append({"value":int(m.group(1)),
                              "path_relative_to_sdk":str(p.relative_to(sdk))})
    outcome["matches"]=positives
    if len(positives)==1 and positives[0]["value"]==1:
        outcome["state"]="PASS_QIO_SDKCONFIG_EFFECTIVE"
    elif any(x["value"]!=1 for x in positives):
        outcome["state"]="REVIEW_QIO_NOT_ENABLED"
    elif fields.get("build.memory_type")=="qio_qspi" and fields.get("build.boot")=="qio":
        outcome["state"]="QIO_BUILD_PROPERTIES_PASS_SDK_MACRO_PENDING"
    return outcome

def main(output):
    output.mkdir(parents=True,exist_ok=True)
    evidence={"state":"INCOMPLETE","E1":{},"E2":{},"hardware":"NOT_TOUCHED"}
    with tempfile.TemporaryDirectory(prefix="a13-h0-e1e2-") as td:
        temp=Path(td)
        hardware=temp/"sketchbook/hardware/jwplc_local/esp32"
        hardware.parent.mkdir(parents=True)
        shutil.copytree(a.SOURCE,hardware)
        a.command(["arduino-cli","config","set","directories.user",str(temp/"sketchbook")])
        probe=a.make_probe(temp)
        _,original=a.command(["git","show",a.BASE+":JWPLC/2.1.0/"+a.W5100])
        candidate=(hardware/a.W5100).read_text(encoding="utf-8")
        try:
            (hardware/a.W5100).write_text(original,encoding="utf-8")
            old=a.compile_one(a.FQBNS[0],probe,hardware,temp,output,"E1_original")
            evidence["E1"]["before"]=elf_ss_value(old["elf"])
            assert evidence["E1"]["before"]["value"]==10,"E1_ELF_ORIGINAL_NOT_10"
            (hardware/a.W5100).write_text(candidate,encoding="utf-8")
            assert a.sha256(hardware/a.W5100)==a.sha256(a.SOURCE/a.W5100)
            new=a.compile_one(a.FQBNS[0],probe,hardware,temp,output,"E1_F1")
            evidence["E1"]["after"]=elf_ss_value(new["elf"])
            assert evidence["E1"]["after"]["value"]==5,"E1_ELF_FIXED_NOT_5"
            evidence["E1"]["state"]="PASS_ELF_BYTES_10_TO_5_GDB_NOT_NEEDED"
            _,props=a.command(["arduino-cli","compile","--fqbn",a.FQBNS[0],
                                "--show-properties",str(probe)])
            evidence["E2"]=sdk_config(props)
            if evidence["E2"]["state"]=="REVIEW_QIO_NOT_ENABLED":
                raise RuntimeError("E2_QIO_CONFLICT")
            evidence["state"]="PASS_ELF_E1_N3_REUSED_SHA_IDENTICAL"
            print("H0_E1=PASS_ELF_10_TO_5",flush=True)
            print("H0_E2="+evidence["E2"]["state"],flush=True)
        finally:
            (output/"E1_E2_MANIFEST.json").write_text(json.dumps(evidence,indent=2))
            print("H0_E1_E2_EVIDENCE="+evidence["state"],flush=True)

if __name__=="__main__":
    import argparse
    p=argparse.ArgumentParser()
    p.add_argument("--output",type=Path,required=True)
    args=p.parse_args()
    main(args.output)
