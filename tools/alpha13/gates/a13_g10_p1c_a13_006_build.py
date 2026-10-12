#!/usr/bin/env python3
"""Alpha13 G10-P1C: build Arduino real del candidato A13-006 en overlay.

4 builds (Client/Server baseline y Client/Server candidato), sin tocar producto.
No flash COM4/COM14, Git stage/commit/push, SD/FRAM ni cambios de autoload.
No concluye que reconexion/reset Ethernet esten validados.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
G10_P0_HEAD="bd6787711c1480f9abf2fe79ca16352e03ad4e02"
P0_ID="g10_p0_20261012_114921_34102c6b"
P1A_ID="g10_p1a_20261012_120529_a174356e"
P1B_ID="g10_p1b_20261012_124640_8458554e"
CLI_SHA="1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571"
FQBN="jwplc_local:esp32:jwplcbasic"
LIBS=ROOT/"JWPLC/2.1.0/libraries"
TCP=LIBS/"JWPLC_ModbusTCP"
TARGETS=[
"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp",
"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.h",
"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp",
"JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h",
]
FROZEN={
"JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a":
"58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e",
"JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a":
"a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d",
"JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a":
"4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc",
"JWPLC/2.1.0/libraries/JW_FRAM/src/esp32/libJW_FRAM.a":
"df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299",
"JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a":
"f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416",
}
SKETCHES=[
("SERVER",TCP/"examples/01.ModbusTCP_Server"),
("CLIENT",TCP/"examples/02.ModbusTCP_Client"),
]
M={}
OUT=None
TMP=None

def emit(k,v):print("G10_P1C_"+str(k)+"="+str(v),flush=True)
def need(v,reason):
    if not v:raise RuntimeError(reason)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open("rb") as f:
        for data in iter(lambda:f.read(1024*1024),b""):h.update(data)
    return h.hexdigest()
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                     stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    need(p.returncode==0,"GIT_FAILED_"+repr(args)+":"+
         p.stderr.decode("utf-8","replace")[-160:])
    return p.stdout.decode("utf-8","replace").strip()
def save():
    if OUT is None:return
    (OUT/"MANIFEST.json").write_text(
        json.dumps(M,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    (OUT/"SUMMARY.log").write_text(
        "\n".join("G10_P1C_"+k.upper()+"="+str(v)
                  for k,v in M.items() if not isinstance(v,dict))+"\n",
        encoding="utf-8")
def phase(n,v):
    M.setdefault("phases",{})[n]=v
    emit("PHASE",n+"_"+v)
    save()
def call(argv,name):
    args=[str(x) for x in argv]
    p=subprocess.run(args,cwd=ROOT,stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT,encoding="utf-8",errors="replace")
    s=p.stdout or ""
    (OUT/(name+".log")).write_text(
        "COMMAND="+repr(args)+"\nEXIT="+str(p.returncode)+"\n"+s,
        encoding="utf-8")
    emit(name+"_EXIT",p.returncode)
    if p.returncode:
        bad=[line for line in s.splitlines()
             if re.search(r"error:|fatal error|undefined reference|no such file|cannot",line,re.I)]
        for line in bad[-10:]:emit("COMPILER_DIAGNOSTIC",line[:300])
    need(p.returncode==0,name+"_FAILED_SEE_LOG")
    return s
def read_manifest(run_id,schema,status):
    p=ROOT/"tools/alpha13/results"/run_id/"MANIFEST.json"
    need(p.is_file(),"LOCAL_EVIDENCE_MISSING_"+run_id)
    m=json.loads(p.read_text(encoding="utf-8-sig"))
    need(m.get("run_id")==run_id and m.get("schema")==schema and
         m.get("status")==status,"LOCAL_EVIDENCE_NOT_QUALIFIED_"+run_id)
    return m
def verify_candidate_manifest(p0,p1a,p1b):
    need(p0.get("head")==G10_P0_HEAD and
         p1b.get("provenance")=="LOCAL_G10_P0_AND_P1A" and
         p1b.get("mode")=="source-only",
         "LOCAL_CANDIDATE_PROVENANCE_NOT_VERIFIED")
    need(p1b.get("product_mutated") is False and
         p1b.get("git_mutated") is False and
         p1b.get("hardware_access") is False,
         "LOCAL_CANDIDATE_REPORTED_SIDE_EFFECTS")
    sources=p1b.get("candidate_paths",{})
    need(set(sources)==set(TARGETS),"LOCAL_CANDIDATE_PATHS_NOT_EXACT_FOUR")
    original={}
    candidate={}
    for name in TARGETS:
        info=sources[name]
        old=ROOT/name
        new=ROOT/"tools/alpha13/results"/P1B_ID/"candidate_sources"/name
        need(info.get("old_sha256")==sha(old) and
             info.get("candidate_sha256")==sha(new) and
             info.get("path") and
             Path(info["path"]).resolve()==new.resolve() and
             sha(new)!=sha(old),"CANDIDATE_SOURCE_SHA_OR_PATH_BAD_"+name)
        if name.endswith(".cpp"):
            need(p0["sources"][name]["sha256"]==sha(old) and
                 p1a["source_cpp_sha256"][name]==sha(old),
                 "P0_P1A_SOURCE_PROVENANCE_MISMATCH_"+name)
        original[name]=sha(old)
        candidate[name]=sha(new)
    return original,candidate
def verify_lib(log,folder,label):
    paths=re.findall(r"(?m)^Using library JWPLC_ModbusTCP at version .+ in folder: (.+)$",log)
    need(len(paths)==1 and Path(paths[0].strip()).resolve()==folder.resolve(),
         "WRONG_TCP_LIBRARY_SELECTED_"+label)
    need("JWPLC_Ethernet" in log and
         re.search(r"(?m)^Using library JWPLC_Ethernet at version",log),
         "JWPLC_ETHERNET_DEPENDENCY_NOT_SELECTED_"+label)
def verify_core(log,label):
    need(re.search(r"[/\\]precompiled[/\\]core[/\\]JWPLCBASIC[/\\]core\.a",log),
         "PRECOMPILED_JWPLC_CORE_NOT_LINKED_"+label)
def normalize_compiler_path_text(value):
    """Normalizar texto de Make .d / arduino-cli -v, no rutas para acceso FS.

    Soporta rutas GCC Win con separadores slash/backslash, espacios escapados,
    colon escapado y CRLF. No eliminar nombres para hacer match espurio.
    """
    return (value.replace("\\\\\\\\", "\\\\")
                 .replace("\\\\ ", " ")
                 .replace("\\\\:", ":")
                 .replace("\\\\r\\\\n", " ")
                 .replace("\\\\n", " ")
                 .replace("\\\\", "/").casefold())


def verify_source_build(log,build,folder,label):
    """Probar objeto REAL y entrada de compilación con .d generado por -MMD.

    No depender de que Arduino CLI imprima la ruta absoluta compilada: la
    selección efectiva de librería se comprueba por separado en verify_lib.
    """
    objects={}
    for name in ["JWPLC_ModbusTCP_Client.cpp","JWPLC_ModbusTCP.cpp"]:
        object_name=name+".o"
        found=list(build.rglob(object_name))
        need(len(found)==1,
             "TCP_SOURCE_OBJECT_MISSING_OR_DUPLICATED_"+label+"_"+object_name)
        obj=found[0]
        objects[object_name]=sha(obj)
        # GCC -MMD, fijado en platform.txt, genera .cpp.d desde .cpp.o.
        deps=[obj.with_suffix(".d"), Path(str(obj)+".d")]
        deps=[d for d in deps if d.is_file()]
        need(len(deps)==1,
             "TCP_OBJECT_DEPENDENCY_FILE_MISSING_OR_DUPLICATED_"+
             label+"_"+name)
        dep=normalize_compiler_path_text(deps[0].read_text(
            encoding="utf-8",errors="replace"))
        # El .d es generado por el mismo compilador que creó el objeto;
        # exigir fuente de la librería real, no solo nombre de archivo.
        expected=normalize_compiler_path_text(
            str((folder/"src"/name).resolve()))
        relative="/jwplc_modbustcp/src/"+name.casefold()
        need(expected in dep or relative in dep,
             "TCP_COMPILE_DEPENDENCY_WRONG_SOURCE_"+label+"_"+name)
        objects[name+".d"]=sha(deps[0])
    emit("SOURCE_PROVENANCE_"+label,"PASS_OBJECT_DEPENDENCY_MMD")
    return objects
def verify_frozen():
    for p,want in FROZEN.items():
        need((ROOT/p).is_file() and sha(ROOT/p)==want,
             "FROZEN_ARCHIVE_CHANGED_"+p)
    return {p:sha(ROOT/p) for p in FROZEN}
def prepare_overlay(sources):
    phase("OVERLAY","RUNNING")
    overlay=TMP/"libraries/JWPLC_ModbusTCP"
    shutil.copytree(TCP,overlay)
    need(not list(overlay.rglob("*.a")),
         "TCP_OVERLAY_CONTAINS_PRECOMPILED_ARCHIVE")
    expected=ROOT/"tools/alpha13/results"/P1B_ID/"candidate_sources"
    for name in TARGETS:
        src=expected/name
        dst=overlay/"src"/Path(name).name
        need(dst.is_file() and src.is_file(),"TCP_OVERLAY_SOURCE_MISSING_"+name)
        shutil.copy2(src,dst)
        need(sha(dst)==sources[name],"TCP_OVERLAY_COPY_SHA_DIFFERS_"+name)
    need((overlay/"library.properties").read_bytes()==
         (TCP/"library.properties").read_bytes(),
         "TCP_OVERLAY_LIBRARY_PROPERTIES_CHANGED")
    for file in ["src/JWPLC_ModbusTCP_Client.cpp","src/JWPLC_ModbusTCP.cpp"]:
        need((overlay/file).is_file(),"TCP_SOURCE_NOT_IN_OVERLAY_"+file)
    M["overlay"]=str(overlay)
    phase("OVERLAY","PASS")
    return overlay
def compile_one(cli,typ,label,sketch,selected_lib):
    build=TMP/("build_"+typ.lower()+"_"+label.lower())
    argv=[cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
          "--build-path",build]
    if typ=="CANDIDATE":
        argv+=["--library",selected_lib]
    argv+=["--libraries",LIBS,sketch]
    out=call(argv,"COMPILE_"+typ+"_"+label)
    verify_lib(out,selected_lib,typ+"_"+label)
    verify_core(out,typ+"_"+label)
    objects=verify_source_build(out,build,selected_lib,typ+"_"+label)
    emit(typ+"_"+label,"PASS_CORRECT_LIBRARY_AND_SOURCE_OBJECTS")
    return objects
def preflight(cli):
    phase("PREFLIGHT","RUNNING")
    need(Path.cwd().resolve()==ROOT and
         Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
         "RUN_FROM_REPOSITORY_ROOT")
    need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    head=git("rev-parse","HEAD")
    need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
         "HEAD_LOCAL_REMOTE_TRACKING_DIFFER")
    need(subprocess.run(["git","merge-base","--is-ancestor",
                         G10_P0_HEAD,head],cwd=ROOT,capture_output=True).returncode==0,
         "G10_P0_NOT_ANCESTOR")
    need(not git("diff","--name-only") and
         not git("diff","--cached","--name-only") and
         not git("ls-files","--unmerged"),"TRACKED_GIT_NOT_CLEAN")
    need(cli.is_absolute() and cli.is_file() and
         cli.name.lower()=="arduino-cli.exe" and sha(cli)==CLI_SHA,
         "PINNED_ARDUINO_CLI_NOT_FOUND")
    version=call([cli,"version"],"CLI_VERSION")
    need("Version: 1.5.1" in version,"WRONG_ARDUINO_CLI_VERSION")
    cores=call([cli,"core","list"],"CLI_CORES")
    need(re.search(r"(?m)^\s*jwplc_local:esp32\s+2\.1\.0-dev(?:\s|$)",cores),
         "JWPLC_LOCAL_ESP32_CORE_MISSING")
    p0=read_manifest(P0_ID,"A13-G10-P0-READONLY-1",
                     "BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN")
    p1a=read_manifest(P1A_ID,"A13-G10-P1A-REPRO-1",
                      "SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN")
    p1b=read_manifest(P1B_ID,"A13-G10-P1B-CANDIDATE-1",
                      "CANDIDATE_SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN")
    need(subprocess.run(["git","merge-base","--is-ancestor",
                         p1b.get("head","invalid"),head],
                        cwd=ROOT,capture_output=True).returncode==0,
         "CANDIDATE_HEAD_NOT_ANCESTOR")
    old,new=verify_candidate_manifest(p0,p1a,p1b)
    for _,sketch in SKETCHES:
        need((sketch/(sketch.name+".ino")).is_file(),
             "EXAMPLE_SKETCH_MISSING_"+sketch.name)
    archives=verify_frozen()
    M["entry_head"]=head
    M["baseline_run"]=P0_ID
    M["source_repro_run"]=P1A_ID
    M["candidate_run"]=P1B_ID
    M["product_sha256"]=old
    M["candidate_sha256"]=new
    M["closed_archives"]=archives
    phase("PREFLIGHT","PASS")
    return new
def selftest():
    # No local manifests, Arduino CLI or hardware. Test EXACT same key/checker
    # with fake files? Verify source selection contract, fixed lists, and pairs.
    need(len(TARGETS)==4 and len(set(TARGETS))==4 and
         all("/" in s and "\\" not in s for s in TARGETS),
         "SCOPE_PATHS_ARE_NOT_PORTABLE")
    need({k for k,_ in SKETCHES}=={"SERVER","CLIENT"},
         "TWO_SKETCHES_MISSING")
    need(all(Path(k).as_posix()==k for k in TARGETS),
         "MANIFEST_KEYS_NOT_POSIX")
    need(len(FROZEN)==5,"ARCHIVE_FINGERPRINT_LIST_INCOMPLETE")
    for label,sketch in SKETCHES:
        need((sketch/(sketch.name+".ino")).is_file(),
             "EXAMPLE_NOT_FOUND_"+label)
    # Two positive/negative checks that are useful in CI on both platforms.
    sample="Using library JWPLC_ModbusTCP at version 0.1.0 in folder: "
    sample+=str(TCP.resolve())+"\n"
    sample+="Using library JWPLC_Ethernet at version 2.1.0 in folder: "+str((LIBS/"JWPLC_Ethernet").resolve())+"\n"
    verify_lib(sample,TCP,"SELFTEST")
    try:verify_lib(sample,TMP/"wrong" if TMP else ROOT/"wrong","NEGATIVE")
    except RuntimeError:pass
    else:raise RuntimeError("BAD_TCP_LIBRARY_SELECTED_ACCEPTED")
    emit("SELFTEST","PASS_SCOPE_SOURCE_LIBRARY_NEGATIVE")
def main():
    global OUT,TMP
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduino-cli",default=r"C:\Program Files\Arduino CLI\arduino-cli.exe")
    parser.add_argument("--selftest",action="store_true")
    args=parser.parse_args()
    if args.selftest:
        selftest()
        return 0
    rid="g10_p1c_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    OUT=ROOT/"tools/alpha13/results"/rid
    TMP=Path(tempfile.gettempdir())/("jwplc_a13_"+rid)
    need(not OUT.exists() and not TMP.exists(),"RUN_ID_ALREADY_USED")
    OUT.mkdir(parents=True)
    TMP.mkdir(parents=True)
    M.update({"schema":"A13-G10-P1C-ARDUINO-CANDIDATE-1",
              "run_id":rid,"status":"REVIEW_STOP","phases":{},
              "product_modified":False,"git_mutated":False,
              "git_commit":False,"git_push":False,
              "hardware_access":False,"physical_upload":False,
              "sd_fram_write":False})
    try:
        cli=Path(args.arduino_cli).expanduser().resolve()
        new=preflight(cli)
        overlay=prepare_overlay(new)
        phase("ARDUINO_BUILDS","RUNNING")
        builds={}
        for typ in ["BASELINE","CANDIDATE"]:
            folder=TCP if typ=="BASELINE" else overlay
            for label,sketch in SKETCHES:
                builds[typ+"_"+label]=compile_one(cli,typ,label,sketch,folder)
        M["object_sha256"]=builds
        phase("ARDUINO_BUILDS","PASS")
        phase("FINAL_AUDIT","RUNNING")
        need(git("rev-parse","HEAD")==M["entry_head"] and
             not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),
             "TRACKED_PRODUCT_OR_GIT_CHANGED")
        for path,want in M["product_sha256"].items():
            need(sha(ROOT/path)==want,"PRODUCT_FILE_SHA_CHANGED_"+path)
        for path,want in M["candidate_sha256"].items():
            need(sha(overlay/"src"/Path(path).name)==want,
                 "OVERLAY_CANDIDATE_SOURCE_CHANGED_"+path)
        need(verify_frozen()==M["closed_archives"],
             "ARCHIVES_CHANGED_DURING_BUILD")
        phase("FINAL_AUDIT","PASS")
        M["status"]="ARDUINO_BASELINE_AND_CANDIDATE_BUILDS_PASS_PRODUCT_UNCHANGED"
        emit("STATUS",M["status"])
        emit("RESULTS",OUT)
        return 0
    except Exception as error:
        M["reason"]=repr(error)
        M["status"]="REVIEW_STOP"
        emit("STATUS","REVIEW_STOP")
        emit("REASON",repr(error))
        emit("RESULTS",OUT)
        return 2
    finally:
        save()

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as error:
        emit("FATAL",repr(error))
        sys.exit(2)
