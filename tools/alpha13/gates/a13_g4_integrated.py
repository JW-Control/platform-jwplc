#!/usr/bin/env python3
"""Alpha13-G4: hito integrado de propiedad de batch TFT y archive precompilado.

No commits, pushes, resets, ni alteración del TFT_eSPI instalado.
Reconstruye un archive temporal desde los fuentes candidatos, prueba enlace
normal, ejecuta test físico supervisado y solo entonces adopta 3 archivos de
producto de forma reversible. Un fallo tras la adopción restaura los bytes.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
BASE = "d9fd7cb0675b757804a8b748c79bf1e9e9aeeb95"
FQBN = "jwplc_local:esp32:jwplcbasic"
LIBS = ROOT / "JWPLC/2.1.0/libraries"
TFT = LIBS / "JWPLC_TFT"
TFT_REL = "JWPLC/2.1.0/libraries/JWPLC_TFT/src/"
CORE = ROOT / "JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
DISPLAY_A = LIBS / "JWPLC_Display/src/esp32/libJWPLC_Display.a"
CORE_SHA = "a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d"
DISPLAY_SHA = "c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02"
OLD_TFT_SHA = "ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8"
OLD_CPP_SHA = "494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25"
PRODUCT = (TFT_REL+"JWPLC_TFT.cpp", TFT_REL+"JWPLC_TFT.h",
           TFT_REL+"esp32/libJWPLC_TFT.a")
CANDIDATES = ("tools/alpha13/candidates/g4/JWPLC_TFT.cpp",
              "tools/alpha13/candidates/g4/JWPLC_TFT.h")
BACKEND_SHA = {
    "TFT_eSPI.cpp":"01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1",
    "TFT_eSPI.h":"b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462",
    "TFT_Drivers/ST7789_Init.h":"e21cae2ac84285dc0e77648eca753f41f7da3c271594ede750b725136c10",
}
PATCHED_INIT_SHA = "44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc"
RUN_ID = datetime.now().strftime("%Y%m%d_%H%M%S") + "_" + uuid.uuid4().hex[:8]
RESULTS = ROOT/"tools/alpha13/results"/("g4_integrated_"+RUN_ID)
TEMP = Path(tempfile.gettempdir())/("jwplc_a13_g4_"+RUN_ID)
BACKUP = TEMP/"product_backup"
MANIFEST = {"hito":"A13-G4","run_id":RUN_ID,"status":"REVIEW",
            "phases":{},"product_commit":False,"release_publish":False,
            "product_changed_by_gate":False,"paths":{"results":str(RESULTS),"temp":str(TEMP)}}
PHASE = "INIT"
ADOPTED = False

class GateStop(RuntimeError):
    def __init__(self, message, category="PRECONDITION"):
        super().__init__(message)
        self.category = category

def check(yes, msg, cat="PRECONDITION"):
    if not yes: raise GateStop(msg, cat)

def sha(path):
    hash_obj=hashlib.sha256()
    with Path(path).open("rb") as file:
        for part in iter(lambda:file.read(1048576),b""): hash_obj.update(part)
    return hash_obj.hexdigest()

def git(*args, ok=True):
    p=subprocess.run(["git",*args],cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if ok and p.returncode!=0:
        raise GateStop("GIT_ERROR:"+repr(args)+":"+p.stderr.decode("utf-8","replace")[-400:])
    return p.stdout.decode("utf-8","replace").strip()

def emit(key,val):
    print(str(key)+"="+str(val),flush=True)

def persist():
    RESULTS.mkdir(parents=True,exist_ok=True)
    (RESULTS/"MANIFEST.json").write_text(json.dumps(MANIFEST,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    summary=["HITO=A13-G4","STATUS="+MANIFEST["status"],"RUN_ID="+RUN_ID,
             "CURRENT_PHASE="+PHASE,"RESULTS="+str(RESULTS)]
    summary += [k+"="+str(v) for k,v in MANIFEST["phases"].items()]
    for key in ("reason","category","archive_sha256","app_sha256","selected_port",
                "visual_operator","rollback","source_sha256"):
        if key in MANIFEST: summary.append(key.upper()+"="+str(MANIFEST[key]))
    summary += ["PRODUCT_COMMIT=NO","GIT_PUSH=NO","RELEASE_MERGE=NO"]
    (RESULTS/"SUMMARY.log").write_text("\n".join(summary)+"\n",encoding="utf-8")

def phase(name,state):
    global PHASE
    PHASE=name
    MANIFEST["phases"][name]=state
    emit("PHASE",name+"_"+state)
    persist()

def cmd(argv,label,cwd=None):
    args=[str(arg) for arg in argv]
    p=subprocess.run(args,cwd=cwd or ROOT,stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT,encoding="utf-8",errors="backslashreplace")
    log=(p.stdout or "")
    (RESULTS/(label+".log")).write_text("ARGS="+repr(args)+"\nEXIT="+str(p.returncode)+"\n"+log,encoding="utf-8")
    emit(label+"_EXIT",p.returncode)
    if p.returncode:
        errors=[x.strip() for x in log.splitlines()
                if re.search(r"(?:fatal error:|[ ]error:|undefined reference|Error:)",x)]
        for error in list(dict.fromkeys(errors))[:8]: emit("COMPILER_ERROR",error[-350:])
        if not errors: emit(label+"_TAIL","\\n".join(log.splitlines()[-7:])[-1100:])
        raise GateStop(label+"_EXIT_"+str(p.returncode),"PRODUCT_OR_ENVIRONMENT")
    return log

def program_cli(explicit):
    if explicit:
        check(Path(explicit).is_file() or shutil.which(explicit), "ARDUINO_CLI_UNAVAILABLE","ENVIRONMENT")
        return explicit
    first=Path(r"C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe")
    found=str(first) if first.is_file() else shutil.which("arduino-cli")
    check(found,"ARDUINO_CLI_UNAVAILABLE","ENVIRONMENT")
    return found

def backend_path(explicit):
    if explicit: return Path(explicit).expanduser()
    env=os.environ.get("JWPLC_TFT_ESPI_BACKEND","")
    if env: return Path(env).expanduser()
    return Path.home()/"Documentos/Programacion/Arduino/libraries/TFT_eSPI"

def program_ar():
    base=Path(os.environ.get("LOCALAPPDATA",""))/"Arduino15/packages/jwplc_local/tools/esp-x32"
    prefer=base/"2601/bin/xtensa-esp32-elf-gcc-ar.exe"
    if prefer.is_file(): return str(prefer)
    found=list(base.rglob("xtensa-esp32-elf-gcc-ar.exe")) if base.is_dir() else []
    check(len(found)==1,"ARCHIVER_NOT_FOUND_OR_AMBIGUOUS","ENVIRONMENT")
    return str(found[0])

def structural_test():
    src=(ROOT/CANDIDATES[0]).read_text(encoding="utf-8")
    hdr=(ROOT/CANDIDATES[1]).read_text(encoding="utf-8")
    check("std::atomic<void *> _batchOwner;" in hdr, "OWNER_ATOMIC_MISSING","HARNESS")
    check("_batchActive" not in src and "_batchActive" not in hdr,"OLD_BATCH_FLAG_PRESENT","HARNESS")
    check(src.count("_batchOwner.load(std::memory_order_acquire)")==5,
          "EXPECTED_OWNER_READ_COUNT_CHANGED","HARNESS")
    check(src.count("_batchOwner.store(")==2,"EXPECTED_OWNER_WRITE_COUNT_CHANGED","HARNESS")
    for name in ("beginBatch","endBatch","batchActive","acquireForOperation",
                 "releaseAfterOperation"):
        check(src.count("JWPLC_TFTClass::"+name+"(")==1,"FUNCTION_COUNT_"+name,"HARNESS")
    check(src.count("jwplcSPI_release()")==3,"SPI_RELEASE_COUNT_CHANGED","HARNESS")
    check("g_backend.writecommand(ST7789_CMD_DISPON);" in src,
          "PRESERVE_TFT_GRAM_FIX","HARNESS")
    check("#include <atomic>" in hdr,"HEADER_ATOMIC_REQUIRED","HARNESS")

def preflight():
    phase("PREFLIGHT","RUNNING")
    check(git("branch","--show-current")==BRANCH,"BRANCH_MISMATCH")
    head=git("rev-parse","HEAD")
    check(subprocess.run(["git","merge-base","--is-ancestor",BASE,head],cwd=ROOT).returncode==0,
          "BASE_NOT_ANCESTOR")
    check(not git("diff","--name-only") and not git("diff","--cached","--name-only"),
          "TRACKED_WORKTREE_NOT_CLEAN")
    check(not git("ls-files","--others","--exclude-standard"),
          "UNTRACKED_WORKTREE_FILES_PRESENT")
    check(sha(CORE)==CORE_SHA,"G3_CORE_CHANGED")
    check(sha(DISPLAY_A)==DISPLAY_SHA,"DISPLAY_ARCHIVE_CHANGED")
    check(sha(ROOT/PRODUCT[2])==OLD_TFT_SHA,"TFT_ARCHIVE_BASELINE_MISMATCH")
    check(sha(ROOT/PRODUCT[0])==OLD_CPP_SHA,"TFT_CPP_BASELINE_MISMATCH")
    check((ROOT/CANDIDATES[0]).is_file() and (ROOT/CANDIDATES[1]).is_file(),
          "CANDIDATE_MISSING")
    check("precompiled=full" in (TFT/"library.properties").read_text(encoding="utf-8"),
          "NORMAL_ARCHIVE_CONTRACT_CHANGED")
    structural_test()
    MANIFEST["head"]=head
    MANIFEST["source_sha256"]={path:sha(ROOT/path) for path in CANDIDATES}
    MANIFEST["product_before_sha256"]={p:sha(ROOT/p) for p in PRODUCT}
    phase("PREFLIGHT","PASS")

def make_backend(source_root):
    phase("BACKEND_RECIPE","RUNNING")
    source_root=source_root.resolve()
    check(source_root.is_dir(),"BACKEND_MISSING:"+str(source_root),"ENVIRONMENT")
    for rel,expected in BACKEND_SHA.items():
        check((source_root/rel).is_file() and sha(source_root/rel)==expected,
              "BACKEND_SHA_MISMATCH:"+rel,"ENVIRONMENT")
    check('#define TFT_ESPI_VERSION "2.5.43"' in
          (source_root/"TFT_eSPI.h").read_text(encoding="utf-8"),
          "BACKEND_NOT_2_5_43","ENVIRONMENT")
    backend=TEMP/"source"/"TFT_eSPI"
    shutil.copytree(source_root,backend)
    init=backend/"TFT_Drivers/ST7789_Init.h"
    raw=init.read_text(encoding="utf-8")
    pattern=re.compile(r"(?m)^(?P<indent>\s*)writecommand\(ST7789_DISPON\);\s*//\s*Display on\s*\r?\n(?P=indent)delay\(120\);")
    check(len(pattern.findall(raw))==2,"PATCH_BACKEND_ANCHORS_NOT_2","HARNESS")
    patched=None
    for eol in ("\n","\r\n"):
        def modify(m):
            ind=m.group("indent")
            return eol.join((ind+"#ifndef JWPLC_TFT_DEFER_DISPON",
                             ind+"writecommand(ST7789_DISPON);    // Display on",
                             ind+"delay(120);",ind+"#endif"))
        candidate=pattern.sub(modify,raw).encode("utf-8")
        if hashlib.sha256(candidate).hexdigest()==PATCHED_INIT_SHA:
            patched=candidate
            break
    check(patched is not None,"BACKEND_PATCH_NOT_REPRODUCED","HARNESS")
    init.write_bytes(patched)
    cpp=backend/"TFT_eSPI.cpp"
    source=cpp.read_bytes()
    include=b'#include "TFT_eSPI.h"'
    check(source.count(include)==1,"BACKEND_INCLUDE_ANCHOR_CHANGED","HARNESS")
    eol=os.linesep.encode("ascii")
    guard=eol.join((b"#if !defined(ST7789_DRIVER) || !defined(JWPLC_TFT_DEFER_DISPON)",
                    b"#error A13_G4_BACKEND_CONFIG_NOT_PROPAGATED",b"#endif"))
    cpp.write_bytes(source.replace(include,include+eol+guard,1))
    phase("BACKEND_RECIPE","PASS")
    return backend

def ar_archive(cli,ar,backend):
    phase("SOURCE_REBUILD","RUNNING")
    lib=TEMP/"source"/"JWPLC_TFT"
    src=lib/"src"
    src.mkdir(parents=True,exist_ok=True)
    for srcName,candidate in (("JWPLC_TFT.cpp",CANDIDATES[0]),
                              ("JWPLC_TFT.h",CANDIDATES[1])):
        shutil.copy2(ROOT/candidate,src/srcName)
    shutil.copy2(TFT/"src/tft_setup.h",src/"tft_setup.h")
    props=("name=JWPLC_TFT\nversion=0.1.0-alpha13-g4-source\n"
           "author=JW Control\nmaintainer=JW Control\n"
           "sentence=Fuente temporal para mantener backend ST7789.\n"
           "paragraph=No distribuir.\ncategory=Display\narchitectures=esp32\n"
           "includes=JWPLC_TFT.h\ndepends=TFT_eSPI,SPI\n")
    (lib/"library.properties").write_text(props,encoding="utf-8")
    sketch=TEMP/"source"/"G4_Source_Probe"
    sketch.mkdir()
    probe=ROOT/"tools/alpha13/firmware/a13_tft_pre1_startup_baseline_probe/a13_tft_pre1_startup_baseline_probe.ino"
    check(probe.is_file(),"SOURCE_PROBE_MISSING")
    shutil.copy2(probe,sketch/"G4_Source_Probe.ino")
    shutil.copy2(src/"tft_setup.h",sketch/"tft_setup.h")
    build=TEMP/"source_compile"
    output=cmd([cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
                "--build-path",build,"--library",lib,"--library",backend,
                "--libraries",LIBS,sketch],"COMPILE_SOURCE_REBUILD")
    for name,selected in (("JWPLC_TFT",lib),("TFT_eSPI",backend)):
        hits=re.findall(r"^Using library "+re.escape(name)+r" at version .+ in folder: (.+)\s*$",output,re.M)
        check(len(hits)==1 and Path(hits[0].strip()).resolve()==selected.resolve(),
              "WRONG_SOURCE_BACKEND_SELECTED:"+name,"HARNESS")
    check("Using core 'jwcontrol_precompiled_stub'" in output,"SOURCE_BUILD_USED_WRONG_CORE","HARNESS")
    objs={}
    for name in ("JWPLC_TFT.cpp.o","TFT_eSPI.cpp.o"):
        match=list(build.rglob(name))
        check(len(match)==1,"SOURCE_OBJECT_COUNT_"+name,"HARNESS")
        objs[name]=match[0]
    phase("SOURCE_REBUILD","PASS")

    phase("ARCHIVE_CREATE","RUNNING")
    (src/"esp32").mkdir()
    archive=src/"esp32/libJWPLC_TFT.a"
    cmd([ar,"crs",archive,objs["JWPLC_TFT.cpp.o"],objs["TFT_eSPI.cpp.o"]],"ARCHIVE_CREATE")
    members=cmd([ar,"t",archive],"ARCHIVE_MEMBERS").splitlines()
    check(len(members)==2 and set(members)==set(objs),"ARCHIVE_MEMBERS_MISMATCH","HARNESS")
    extract=TEMP/"extracted"
    extract.mkdir()
    cmd([ar,"x",archive],"ARCHIVE_EXTRACT",cwd=extract)
    for name,obj in objs.items():
        check(sha(extract/name)==sha(obj),"ARCHIVE_MEMBER_NOT_BYTE_EQUAL_"+name,"HARNESS")
    MANIFEST["archive_sha256"]=sha(archive)
    MANIFEST["member_sha256"]={name:sha(path) for name,path in objs.items()}
    MANIFEST["archive_bytes"]=archive.stat().st_size
    check(MANIFEST["archive_sha256"]!=OLD_TFT_SHA,"NEW_ARCHIVE_SAME_AS_OLD","HARNESS")
    # Pasar a ruta de consumidor normal, sin compilar el backend como source.
    shutil.copy2(TFT/"library.properties",lib/"library.properties")
    phase("ARCHIVE_CREATE","PASS")
    return lib,archive

def normal_compile(cli,sketch,label,temp_library):
    build=TEMP/("build_"+label)
    args=[cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
          "--build-path",build,"--library",temp_library,"--libraries",LIBS,sketch]
    output=cmd(args,"COMPILE_"+label)
    selected=re.findall(r"^Using library JWPLC_TFT at version .+ in folder: (.+)\s*$",output,re.M)
    check(len(selected)==1 and Path(selected[0].strip()).resolve()==temp_library.resolve(),
          "WRONG_TFT_LIBRARY_"+label,"HARNESS")
    check("Using precompiled library" in output and
          re.search(r"Using precompiled library .+JWPLC_TFT",output) is not None,
          "NOT_PRECOMPILED_"+label,"HARNESS")
    check(re.search(r"^Using library TFT_eSPI at version",output,re.M) is None,
          "EXTERNAL_BACKEND_SELECTED_"+label,"HARNESS")
    check("Using core 'jwcontrol_precompiled_stub'" in output and
          re.search(r"[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a",output) is not None,
          "CORE_ARCHIVE_NOT_SELECTED_"+label,"HARNESS")
    check(not list(build.rglob("JWPLC_TFT.cpp.o")) and
          not list(build.rglob("TFT_eSPI.cpp.o")),"SOURCE_FALLBACK_"+label,"HARNESS")
    return build

def serial_port(requested):
    from serial.tools import list_ports
    ps=list(list_ports.comports())
    matches=[p for p in ps if p.device.upper()==requested.upper()]
    check(len(matches)==1,"SERIAL_PORT_NOT_FOUND:"+requested,"ENVIRONMENT")
    p=matches[0]
    MANIFEST["selected_port"]=p.device
    MANIFEST["vid_pid"]=(f"{p.vid:04X}:{p.pid:04X}"
                         if p.vid is not None and p.pid is not None else "UNKNOWN")
    emit("SELECTED_PORT",p.device)
    emit("USB_VID_PID",MANIFEST["vid_pid"])
    return p.device

def physical(cli,lib,port):
    phase("PHYSICAL_BUILD","RUNNING")
    sketch=TEMP/"G4_Batch_Owner_Probe"
    sketch.mkdir()
    src=ROOT/"tools/alpha13/firmware/a13_g4_batch_owner_probe/a13_g4_batch_owner_probe.ino"
    text=src.read_text(encoding="utf-8")
    check(text.count("__G4_TOKEN__")==1,"PHYSICAL_PROBE_TOKEN_ANCHOR_CHANGED","HARNESS")
    token=RUN_ID+"_"+uuid.uuid4().hex
    (sketch/"G4_Batch_Owner_Probe.ino").write_text(text.replace("__G4_TOKEN__",token),encoding="utf-8")
    build=normal_compile(cli,sketch,"PHYSICAL",lib)
    apps=list(build.rglob("*.ino.bin"))
    check(len(apps)==1,"PHYSICAL_APP_BINARY_COUNT","HARNESS")
    MANIFEST["app_sha256"]=sha(apps[0])
    MANIFEST["runtime_token"]=token
    phase("PHYSICAL_BUILD","PASS")
    emit("APP_SHA256",MANIFEST["app_sha256"])
    phase("PHYSICAL_APPROVAL","RUNNING")
    emit("SAFETY","Firmware de prueba reemplaza sketch actual; no probar en maquina productiva.")
    consent=input("Confirma JWPLC en banco, sin actuadores/cargas conectadas (BANCO): ").strip()
    check(consent=="BANCO","NO_OPERATOR_PERMISSION_FOR_FLASH","PRECONDITION")
    MANIFEST["operator_bench_confirmed"]=True
    phase("PHYSICAL_APPROVAL","PASS")
    phase("PHYSICAL_UPLOAD","RUNNING")
    cmd([cli,"upload","--fqbn",FQBN,"--port",port,"--input-dir",build],"UPLOAD_G4")
    phase("PHYSICAL_UPLOAD","PASS")
    phase("PHYSICAL_SERIAL","RUNNING")
    import serial
    log=[]
    ready=False
    try:
        deadline=time.monotonic()+24
        session=None
        while time.monotonic()<deadline:
            try:
                session=serial.Serial(port,115200,timeout=.3)
                break
            except (OSError,serial.SerialException):
                time.sleep(.25)
        check(session is not None,"SERIAL_REOPEN_TIMEOUT","ENVIRONMENT")
        with session as ser:
            while time.monotonic()<deadline:
                raw=ser.readline().decode("utf-8","backslashreplace").strip()
                if raw: log.append(raw)
                if raw=="G4_READY="+token:
                    ready=True
                    break
            check(ready,"FRESH_TOKEN_NOT_SEEN","HARDWARE_OR_ENVIRONMENT")
            ser.write(("G4_RUN:"+token+"\n").encode("ascii"))
            done=False
            deadline=time.monotonic()+100
            while time.monotonic()<deadline:
                raw=ser.readline().decode("utf-8","backslashreplace").strip()
                if raw: log.append(raw)
                if raw=="G4_DONE="+token:
                    done=True
                    break
            check(done,"SERIAL_DONE_TIMEOUT","HARDWARE_OR_PRODUCT")
    finally:
        (RESULTS/"serial_g4.log").write_text("\n".join(log)+"\n",encoding="utf-8")
    def one(key):
        vals=[s.split("=",1)[1] for s in log if s.startswith(key+"=")]
        check(len(vals)==1,"SERIAL_KEY_COUNT_"+key+":"+str(len(vals)),"HARNESS")
        return vals[0]
    for key,value in (("G4_RESULT","PASS"),("G4_ERRORS","0"),("G4_TRIALS","50"),
                      ("G4_BATCH_FINAL","RELEASED"),("G4_DONE",token)):
        check(one(key)==value,"SERIAL_ASSERT_FAIL_"+key,"PRODUCT_OR_HARDWARE")
    MANIFEST["serial_result"]={"trials":50,"errors":0,"batch_final":"RELEASED",
                               "ready_token":token,"done_token":token}
    phase("PHYSICAL_SERIAL","PASS")

    phase("VISUAL_OPERATOR","RUNNING")
    white=input("¿Apareció fondo blanco irregular persistente/flicker? (NO/SI): ").strip().upper()
    idle=input("¿TFT limpia y estable, sin boot-loop? (SI/NO): ").strip().upper()
    MANIFEST["visual_operator"]={"white_problem":white,"tft_stable":idle}
    check(white=="NO" and idle=="SI","TFT_VISUAL_REGRESSION_REPORTED","PRODUCT_OR_HARDWARE")
    phase("VISUAL_OPERATOR","PASS")

def adopt():
    global ADOPTED
    phase("PRODUCT_ADOPTION","RUNNING")
    BACKUP.mkdir(parents=True,exist_ok=False)
    for path in PRODUCT:
        target=BACKUP/path
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/path,target)
    ADOPTED=True
    for path,candidate in zip(PRODUCT[:2],CANDIDATES):
        shutil.copy2(ROOT/candidate,ROOT/path)
    shutil.copy2(MANIFEST["temp_archive"],ROOT/PRODUCT[2])
    check(git("diff","--check")=="","DIFF_CHECK_FAILED","HARNESS")
    dirty=set(git("diff","--name-only").splitlines())
    check(dirty==set(PRODUCT),"PRODUCT_DIRTY_SCOPE_INVALID","HARNESS")
    check(sha(ROOT/PRODUCT[2])==MANIFEST["archive_sha256"],
          "PRODUCT_ARCHIVE_COPY_SHA_MISMATCH","HARNESS")
    MANIFEST["product_changed_by_gate"]=True
    phase("PRODUCT_ADOPTION","PASS")

def restore():
    if not ADOPTED:return
    for path in PRODUCT:
        original=BACKUP/path
        if original.is_file():
            shutil.copy2(original,ROOT/path)
    good=all((BACKUP/p).is_file() and sha(ROOT/p)==sha(BACKUP/p) for p in PRODUCT)
    MANIFEST["rollback"]="PASS" if good else "FAIL"
    emit("ROLLBACK",MANIFEST["rollback"])

def final_audit():
    phase("FINAL_AUDIT","RUNNING")
    check(set(git("diff","--name-only").splitlines())==set(PRODUCT),
          "FINAL_SCOPE_CHANGED","HARNESS")
    check(not git("diff","--cached","--name-only"),"INDEX_CHANGED","HARNESS")
    check(not git("diff","--check"),"FINAL_DIFF_CHECK","HARNESS")
    for path,candidate in zip(PRODUCT[:2],CANDIDATES):
        check(sha(ROOT/path)==sha(ROOT/candidate),"FINAL_SOURCE_SHA_MISMATCH","HARNESS")
    check(sha(ROOT/PRODUCT[2])==MANIFEST["archive_sha256"],
          "FINAL_ARCHIVE_SHA_CHANGED","HARNESS")
    check(sha(CORE)==CORE_SHA and sha(DISPLAY_A)==DISPLAY_SHA,
          "G3_CORE_OR_DISPLAY_ARCHIVE_CHANGED","HARNESS")
    MANIFEST["product_sha256"]={p:sha(ROOT/p) for p in PRODUCT}
    phase("FINAL_AUDIT","PASS")

def run():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--serial-port",default="COM4")
    ap.add_argument("--arduino-cli",default="")
    ap.add_argument("--backend",default="")
    ap.add_argument("--skip-physical",action="store_true",
                    help="Compilar y probar archive temporal sin modificar producto; no cerrar G4")
    args=ap.parse_args()
    RESULTS.mkdir(parents=True,exist_ok=False)
    TEMP.mkdir(parents=True,exist_ok=False)
    try:
        preflight()
        cli=program_cli(args.arduino_cli)
        cmd([cli,"version"],"CLI_VERSION")
        ar=program_ar()
        if not args.skip_physical:
            try: import serial
            except ImportError: raise GateStop("PYSERIAL_REQUIRED","ENVIRONMENT")
            port=serial_port(args.serial_port)
        backend=make_backend(backend_path(args.backend))
        lib,archive=ar_archive(cli,ar,backend)
        MANIFEST["temp_archive"]=str(archive)
        phase("TEMP_ARCHIVE_NORMAL_LINK","RUNNING")
        normal_compile(cli,LIBS/"JWPLC_Display/examples/01.Display_IDLE_Status",
                       "TEMP_ARCHIVE",lib)
        phase("TEMP_ARCHIVE_NORMAL_LINK","PASS")
        if args.skip_physical:
            MANIFEST["status"]="PASS_TEMP_BUILD_PHYSICAL_PENDING"
            emit("STATUS",MANIFEST["status"])
            persist()
            return 0
        physical(cli,lib,port)
        adopt()
        phase("PRODUCT_NORMAL_REGRESSION","RUNNING")
        cases=(("IDLE_STATUS",LIBS/"JWPLC_Display/examples/01.Display_IDLE_Status"),
               ("HMI_FIELDS",LIBS/"JWPLC_Display/examples/02.Display_HMI_Fields"),
               ("IDLE_MODES",LIBS/"JWPLC_Display/examples/Display_Idle_Return_Modes"),
               ("LOGIC_UI",LIBS/"JWPLC_LogicRuntime_UI/examples/JWPLC_LogicRuntime_UI_Home"))
        for label,sketch in cases:
            check(sketch.is_dir(),"MISSING_REGRESSION_"+label,"HARNESS")
            normal_compile(cli,sketch,"REG_"+label,TFT)
        MANIFEST["regressions"]=[k for k,_ in cases]
        phase("PRODUCT_NORMAL_REGRESSION","PASS")
        final_audit()
        MANIFEST["status"]="PASS_PHYSICAL_AND_REBUILT_NORMAL_ARCHIVE"
        emit("STATUS",MANIFEST["status"])
        emit("PRODUCT_DIRTY_FILES",len(PRODUCT))
        emit("NEW_TFT_ARCHIVE_SHA256",MANIFEST["archive_sha256"])
        emit("NEXT_GATE","A13_G4_CLOSURE_AFTER_REVIEW")
        persist()
        return 0
    except (GateStop,Exception,KeyboardInterrupt) as exc:
        MANIFEST["reason"]=str(exc)
        MANIFEST["category"]=exc.category if isinstance(exc,GateStop) else (
            "USER_ABORT" if isinstance(exc,KeyboardInterrupt) else "HARNESS")
        restore()
        MANIFEST["status"]="REVIEW" if MANIFEST["category"] not in (
            "PRODUCT_OR_HARDWARE","PRODUCT_OR_ENVIRONMENT") else "FAIL"
        emit("STATUS",MANIFEST["status"])
        emit("REASON",MANIFEST["reason"])
        emit("CATEGORY",MANIFEST["category"])
        persist()
        return 2

if __name__=="__main__":
    sys.exit(run())
