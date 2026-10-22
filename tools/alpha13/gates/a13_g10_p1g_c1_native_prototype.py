#!/usr/bin/env python3
"""Alpha13 G10-P1G-C1: compilar C++ real aislado, con 4 mutaciones negativas.

Solo prototipo de bus y lease; NO integra EthernetClient/Server/UDP del producto.
Por eso el PASS deja A13-006 BLOCKED, no modifica hardware ni fuentes activos.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
P1G_B_HEAD="6ef3a7fa5b621affff20a3e3673a9b5d601ea71e"
P1G_B_RUN="g10_p1g_b_20261012_234850_ab1c26f8"
BASE=ROOT/"tools/alpha13/candidates/g10_p1g_c"
HEADER=BASE/"JWPLC_W5500_SafeAccess.h"
SOURCE=BASE/"JWPLC_W5500_SafeAccess.cpp"
TEST=BASE/"native_safe_access_test.cpp"
EXPECTED=[
    "C1_BUS_NESTING_CONTENTION=PASS",
    "C1_SOCKET_EPOCH_OWNER_LISTENER=PASS",
    "C1_RESET_INTERLOCK_AND_LINKOFF=PASS",
    "C1_NATIVE_RESULT=PASS_PROTOTYPE_ONLY_NO_PRODUCT_ADOPTION",
]
NEGATIVES=[
    ("NESTED_LOCK", "    if (g_owner.load(std::memory_order_acquire) == me) {",
     "    if (false) {", "NESTED_BUS_LOCK"),
    ("REUSE_OWNER", "t.allocation == s.allocation && t.ownerCookie == s.owner;",
     "true;", "STALE_SAME_EPOCH_ACCEPTED"),
    ("RESET_EPOCH", "    incrementEpochLocked();\n    return reset(context);",
     "    return reset(context);", "RESET_DID_NOT_INCREASE_EPOCH"),
    ("CONTENTION", "    if (!bus) return CloseOutcome::BusBusy;",
     "    if (false) return CloseOutcome::BusBusy;",
     "SPI_DENIAL_NOT_RETRIABLE"),
]
TASK_H="""#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void* TaskHandle_t;
TaskHandle_t xTaskGetCurrentTaskHandle(void);
#ifdef __cplusplus
}
#endif
"""
SPI_H="""#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool jwplcSPI_acquire(uint32_t timeoutMs);
void jwplcSPI_release(void);
void jwplcSPI_deselectAll(void);
#ifdef __cplusplus
}
#endif
"""
def say(k,v):
    print("G10_P1G_C1_"+str(k)+"="+str(v),flush=True)
def need(value,message):
    if not value:
        raise RuntimeError(message)
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for b in iter(lambda:f.read(1024*1024),b""):
            h.update(b)
    return h.hexdigest()
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,capture_output=True,text=True,
                     errors="replace",timeout=25)
    need(p.returncode==0,"GIT_FAILED_"+repr(args))
    return p.stdout.strip()
def load_module(rel,name):
    spec=importlib.util.spec_from_file_location(name,ROOT/rel)
    need(spec is not None and spec.loader is not None,
         "MODULE_UNAVAILABLE_"+name)
    module=importlib.util.module_from_spec(spec)
    sys.modules[name]=module
    spec.loader.exec_module(module)
    return module
def compiler():
    """Buscar compilador NATIVO (x86/x64), nunca usar xtensa-esp32-elf-g++."""
    # Compatibilidad con gates previos: primero respetar el PATH de usuario.
    repro=load_module("tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py",
                      "g10_p1g_c1_compiler_ref")
    path,kind=repro.compiler()
    if path is not None:
        return str(path),kind
    # CXX explícito permite usar un compilador instalado fuera de PATH.
    value=os.environ.get("CXX","").strip().strip('"')
    if value:
        resolved=shutil.which(value)
        if resolved:
            return resolved,"msvc" if Path(resolved).stem.lower()=="cl" else "gcc"
        if Path(value).is_file():
            return str(Path(value)),"msvc" if Path(value).stem.lower()=="cl" else "gcc"
        raise RuntimeError("CXX_ENV_NOT_RESOLVED_"+value)
    if os.name!="nt":
        raise RuntimeError("HOST_CXX_NOT_FOUND")
    # Rutas frecuentes: LLVM y toolchains MinGW nativas (NO Arduino ESP32).
    pf=Path(os.environ.get("ProgramFiles",r"C:\Program Files"))
    pf86=Path(os.environ.get("ProgramFiles(x86)",r"C:\Program Files (x86)"))
    drive=Path(os.environ.get("SystemDrive","C:")+"\\")
    local=Path(os.environ.get("LOCALAPPDATA",str(Path.home()/"AppData/Local")))
    home=Path.home()
    known=[
        pf/"LLVM/bin/clang++.exe",
        drive/"msys64/ucrt64/bin/g++.exe",
        drive/"msys64/mingw64/bin/g++.exe",
        drive/"msys64/clang64/bin/clang++.exe",
        drive/"mingw64/bin/g++.exe",
        local/"Programs/LLVM/bin/clang++.exe",
        home/"scoop/apps/llvm/current/bin/clang++.exe",
        home/"scoop/apps/gcc/current/bin/g++.exe",
    ]
    for candidate in known:
        if candidate.is_file():
            return str(candidate),"gcc"
    # MSVC puede existir sin que PowerShell tenga vcvars64 en su entorno.
    vswhere=pf86/"Microsoft Visual Studio/Installer/vswhere.exe"
    if vswhere.is_file():
        proc=subprocess.run([
            str(vswhere),"-latest","-products","*","-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property","installationPath"],
            capture_output=True,text=True,errors="replace",timeout=15)
        if proc.returncode==0 and proc.stdout.strip():
            vcvars=Path(proc.stdout.strip())/"VC/Auxiliary/Build/vcvars64.bat"
            if vcvars.is_file():
                return str(vcvars),"msvc-vcvars"
    for year in ("2026","2022","2019"):
        for edition in ("BuildTools","Community","Professional","Enterprise"):
            vcvars=pf/"Microsoft Visual Studio"/year/edition/"VC/Auxiliary/Build/vcvars64.bat"
            if vcvars.is_file():
                return str(vcvars),"msvc-vcvars"
    raise RuntimeError("HOST_CXX_NOT_FOUND")
def run_build(base,out,name,mutate=None):
    folder=out/name
    folder.mkdir(parents=True,exist_ok=False)
    inc=folder/"include"
    (inc/"freertos").mkdir(parents=True)
    (inc/"freertos"/"task.h").write_text(TASK_H,encoding="utf-8")
    (inc/"jwplc_spi_bus.h").write_text(SPI_H,encoding="utf-8")
    source=(SOURCE.read_text(encoding="utf-8-sig") if base is None else base)
    if mutate is not None:
        label,before,after,_=mutate
        need(source.count(before)==1,"MUTATION_ANCHOR_"+label)
        source=source.replace(before,after,1)
    src=folder/"JWPLC_W5500_SafeAccess.cpp"
    src.write_text(source,encoding="utf-8")
    host,kind=compiler()
    executable=folder/("safe_access.exe" if os.name=="nt" else "safe_access")
    if kind=="msvc":
        argv=[host,"/nologo","/EHsc","/std:c++17","/W4",
              "/I"+str(inc),"/I"+str(BASE),str(src),str(TEST),
              "/Fe:"+str(executable)]
    elif kind=="msvc-vcvars":
        bat=folder/"compile_with_msvc.bat"
        cl_args=["cl.exe","/nologo","/EHsc","/std:c++17","/W4",
                 "/I"+str(inc),"/I"+str(BASE),str(src),str(TEST),
                 "/Fe:"+str(executable)]
        bat.write_text(
            '@echo off\ncall "'+host+'" >nul\n'
            'if errorlevel 1 exit /b 91\n'
            +subprocess.list2cmdline(cl_args)+'\n'
            'exit /b %ERRORLEVEL%\n',encoding="utf-8")
        argv=["cmd.exe","/d","/c",str(bat)]
    else:
        argv=[host,"-std=c++17","-O0","-Wall","-Wextra",
              "-I"+str(inc),"-I"+str(BASE),str(src),str(TEST),
              "-o",str(executable)]
    # MSYS2 UCRT64/MINGW64/CLANG64 fuera del PATH: g++.exe puede ser
    # encontrado por ruta absoluta y aun asi fallar al cargar cc1plus.exe,
    # lto-wrapper.exe o DLLs de MinGW. No cambiar PATH del operador.
    native_env=os.environ.copy()
    path_prefix=""
    if os.name=="nt" and kind=="gcc":
        path_prefix=str(Path(host).resolve().parent)
        native_env["PATH"]=path_prefix+os.pathsep+native_env.get("PATH","")
    result=subprocess.run(argv,cwd=folder,capture_output=True,
                          text=True,errors="replace",timeout=120,env=native_env)
    compile_log=("COMMAND="+repr(argv)+"\n"
                 "COMPILER="+host+"\n"
                 "COMPILER_KIND="+kind+"\n"
                 "HOST_PATH_PREFIX="+path_prefix+"\n"
                 "EXIT="+str(result.returncode)+"\n"+
                 result.stdout+"\n"+result.stderr)
    (folder/"compile.log").write_text(compile_log,encoding="utf-8")
    if result.returncode!=0 or not executable.is_file():
        say("COMPILER",host)
        say("COMPILE_PHASE",name)
        say("COMPILE_EXIT",result.returncode)
        say("COMPILER_HOST_PATH_PREFIX",path_prefix if path_prefix else "UNCHANGED")
        for line in (result.stderr+"\n"+result.stdout).splitlines()[-24:]:
            if line.strip():
                say("COMPILER_DIAGNOSTIC",line[:350])
        if not result.stderr.strip() and not result.stdout.strip():
            say("COMPILER_DIAGNOSTIC",
                "NO_STDOUT_STDERR_SEE_compile.log_OR_WINDOWS_DLL_DIALOG")
    need(result.returncode==0 and executable.is_file(),
         "HOST_COMPILE_FAILED_"+name+"_SEE_COMPILE_LOG")
    run=subprocess.run([str(executable)],cwd=folder,capture_output=True,
                       text=True,errors="replace",timeout=15,env=native_env)
    text=run.stdout+"\n"+run.stderr
    (folder/"native.log").write_text(
        "EXIT="+str(run.returncode)+"\n"+text,encoding="utf-8")
    return run.returncode,text,sha(src)
def verify(ci):
    need(Path.cwd().resolve()==ROOT and
         Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
         "REPOSITORY_ROOT_REQUIRED")
    h=git("rev-parse","HEAD")
    need(subprocess.run(["git","merge-base","--is-ancestor",P1G_B_HEAD,h],
         cwd=ROOT,capture_output=True).returncode==0,
         "P1G_B_HEAD_NOT_ANCESTOR")
    need(not git("diff","--name-only") and
         not git("diff","--cached","--name-only"),"GIT_TRACKED_DIRTY")
    if not ci:
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        need(h==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "LOCAL_REMOTE_HEAD_DIFFERENT")
        p=ROOT/"tools/alpha13/results"/P1G_B_RUN/"MANIFEST.json"
        need(p.is_file(),"P1G_B_WINDOWS_LOCAL_MANIFEST_MISSING")
        obj=json.loads(p.read_text(encoding="utf-8-sig"))
        need(obj.get("schema")=="A13-G10-P1G-B-SOURCE-WITNESSES-1" and
             obj.get("run_id")==P1G_B_RUN and
             obj.get("head")==P1G_B_HEAD and
             obj.get("status")=="CALLGRAPH_WITNESSES_CAPTURED_ADOPTION_BLOCKED",
             "P1G_B_WINDOWS_LOCAL_MANIFEST_NOT_QUALIFIED")
    gate=load_module("tools/alpha13/gates/a13_g10_p1g_a_mutex_callpaths.py",
                     "g10_p1g_c1_a_frozen")
    need(gate.check_frozen()==5,"ARCHIVES_CHANGED")
    gate.check(gate.sources())
    return h
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selftest",action="store_true")
    ap.add_argument("--ci-standalone",action="store_true")
    ap.add_argument("--compiler-probe",action="store_true",
                    help="Solo detectar compilador host; no compila ni genera manifest")
    args=ap.parse_args()
    if args.compiler_probe:
        try:
            host,kind=compiler()
            say("HOST_CXX",host)
            say("HOST_CXX_KIND",kind)
            say("COMPILER_PROBE","FOUND")
            return 0
        except RuntimeError as err:
            say("COMPILER_PROBE","NOT_FOUND")
            say("COMPILER_REASON",str(err))
            say("NEXT","CI_PROVES_C1_WINDOWS_LINUX_LOCAL_COMPILER_OPTIONAL")
            return 2
    runid="g10_p1g_c1_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    manifest={"schema":"A13-G10-P1G-C1-NATIVE-PROTOTYPE-1",
              "run_id":runid,"status":"REVIEW_STOP",
              "product_mutation":False,"hardware_access":False,
              "candidate_in_product":False,"firmware_compiled":False}
    try:
        head=verify(args.ci_standalone or args.selftest)
        code,log,sourcehash=run_build(None,out,"positive")
        need(code==0 and all(x in log for x in EXPECTED),
             "POSITIVE_CANDIDATE_NATIVE_FAILED")
        say("POSITIVE_NATIVE","PASS_BUS_NESTING_SOCKET_LEASE_RESET")
        negatives=[]
        for mutation in NEGATIVES:
            label,_,_,expected=mutation
            rc,msg,_=run_build(None,out,"negative_"+label,mutation)
            need(rc!=0 and expected in msg,
                 "NEGATIVE_NOT_REJECTED_"+label)
            negatives.append(label)
            say("NEGATIVE_"+label,"PASS_REJECTED")
        need(len(negatives)==4,"NEGATIVE_COUNT_NOT_FOUR")
        manifest.update({"status":"ISOLATED_BUS_LEASE_PROTOTYPE_NATIVE_PASS_ADOPTION_BLOCKED",
                         "head":head,"native_compiler":compiler()[0],
                         "source_sha256":sourcehash,
                         "input_sha256":{p.name:sha(p)
                                         for p in [HEADER,SOURCE,TEST]},
                         "negative_mutations":negatives,
                         "native_evidence":"native.log in positive and negative dirs",
                         "source_wrappers_modified":False,
                         "arduino_builds":"NOT_RUN",
                         "socket_integration":"NOT_DONE",
                         "gpio4_enabled":False})
        say("HEAD",head)
        say("NATIVE_COMPILER",Path(compiler()[0]).name)
        say("NEGATIVES",len(negatives))
        say("FROZEN_ARCHIVES",5)
        say("INTEGRATED_PRODUCT","NO")
        say("STATUS",manifest["status"])
        say("RESULTS",out)
        return 0
    except Exception as err:
        manifest["reason"]=repr(err)
        if "HOST_CXX_NOT_FOUND" in str(err):
            say("LOCAL_BLOCKER","NO_NATIVE_CPP_COMPILER_DETECTED")
            say("NEXT","DO_NOT_USE_ESP32_CROSS_COMPILER_USE_CI_OR_INSTALL_HOST_CXX")
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(err))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(
            json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",
            encoding="utf-8")
if __name__=="__main__":
    try:sys.exit(main())
    except Exception as err:
        say("FATAL",repr(err))
        sys.exit(2)
