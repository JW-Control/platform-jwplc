#!/usr/bin/env python3
"""Alpha13 G7 FRAM: source→object→archive→link→physical→rollback/commit.

No merge ni release. Exige CLI oficial absoluto. Sólo pide BANCO_FRAM antes
de reemplazar el sketch. Git productivo se cierra automáticamente tras PASS.
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
FQBN="jwplc_local:esp32:jwplcbasic"
LIBS=ROOT/"JWPLC/2.1.0/libraries"
FRAM=LIBS/"JW_FRAM"
PRODUCT={
    "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.cpp":
    "tools/alpha13/candidates/g7/JW_FRAM.cpp",
    "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.h":
    "tools/alpha13/candidates/g7/JW_FRAM.h",
}
ARCHIVE="JWPLC/2.1.0/libraries/JW_FRAM/src/esp32/libJW_FRAM.a"
STATUS_DOC="docs/v2.1.0-alpha.13/ALPHA13_STATUS.md"
CLOSE_DOC="docs/v2.1.0-alpha.13/A13_G7_FRAM_CLOSURE_20261011.md"
CHECK_DOC="docs/v2.1.0-alpha.13/A13_G7_FRAM_CHECKLIST_20261011.md"
RUN_ID="g7_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
RESULT=ROOT/"tools/alpha13/results"/("g7_fram_"+RUN_ID)
TEMP=Path(tempfile.gettempdir())/("jwplc_a13_"+RUN_ID)
BACKUP=TEMP/"product_backup"
CORE=ROOT/"JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
TFT=ROOT/"JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a"
CORE_SHA="a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d"
TFT_SHA="4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc"
MANIFEST={
    "schema":"A13-G7-INTEGRATED-1","run_id":RUN_ID,"status":"REVIEW",
    "phase":{},"physical_upload":False,"fram_restored":False,
    "product_adopted":False,"git_product_committed":False,
    "git_docs_committed":False,"git_pushed":False,
}
ADOPTED=False
COMMITTED=False

class Review(Exception):
    def __init__(self,message,kind="HARNESS_OR_ENVIRONMENT"):
        super().__init__(message)
        self.kind=kind

def need(value,reason,kind="HARNESS_OR_ENVIRONMENT"):
    if not value:raise Review(reason,kind)

def log(key,value): print(str(key)+"="+str(value),flush=True)

def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda:stream.read(1<<20),b""):h.update(chunk)
    return h.hexdigest()

def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                     stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    need(p.returncode==0,"GIT_ERROR_"+p.stderr.decode("utf-8","replace")[-500:],"GIT")
    return p.stdout.decode("utf-8","replace").strip()

def git_paths(*args):
    tokens=list(args)
    tokens.insert(tokens.index("--") if "--" in tokens else len(tokens),"-z")
    p=subprocess.run(["git","-c","core.quotepath=false",*tokens],cwd=ROOT,
                     stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    need(p.returncode==0,"GIT_PATHS_ERROR","GIT")
    return set(filter(None,p.stdout.decode("utf-8","replace").split("\0")))

def save():
    RESULT.mkdir(parents=True,exist_ok=True)
    (RESULT/"MANIFEST.json").write_text(
        json.dumps(MANIFEST,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    rows=["RUN_ID="+RUN_ID,"STATUS="+MANIFEST["status"]]
    rows += ["PHASE_"+k+"="+v for k,v in MANIFEST["phase"].items()]
    for k in ("reason","category","cli_path","cli_sha256","port",
              "archive_sha256","app_sha256","rollback"):
        if k in MANIFEST:rows.append(k.upper()+"="+str(MANIFEST[k]))
    (RESULT/"SUMMARY.log").write_text("\n".join(rows)+"\n",encoding="utf-8")

def stage(name,state):
    MANIFEST["phase"][name]=state
    log("PHASE",name+"_"+state)
    save()

def call(argv,label,cwd=ROOT):
    cmd=[str(v) for v in argv]
    out=subprocess.run(cmd,cwd=cwd,stdout=subprocess.PIPE,
                       stderr=subprocess.STDOUT,encoding="utf-8",
                       errors="backslashreplace")
    text=out.stdout or ""
    (RESULT/(label+".log")).write_text(
        "ARGS="+repr(cmd)+"\nEXIT="+str(out.returncode)+"\n"+text,
        encoding="utf-8")
    log(label+"_EXIT",out.returncode)
    if out.returncode:
        diagnostics=[v for v in text.splitlines()
            if re.search(r"(error:|fatal error:|undefined reference|G7_SERIAL_ERROR)",v,re.I)]
        for line in diagnostics[-8:]:log("DIAGNOSTIC",line[-300:])
        if not diagnostics:log("DIAGNOSTIC_TAIL",repr(text.splitlines()[-8:])[-900:])
        category=("HARNESS" if label in (
            "F112_PORTABILITY_SELFTEST","G7_CONTRACT_SELFTEST") else
            "PRODUCT_OR_ENVIRONMENT")
        raise Review(label+"_FAILED",category)
    return text

def preflight(cli,port):
    stage("PREFLIGHT","RUNNING")
    need(cli.is_absolute() and cli.is_file() and
         cli.name.lower()=="arduino-cli.exe","CLI_ABSOLUTE_EXE_REQUIRED")
    need("arduino plc ide tools" not in str(cli).lower(),
         "ARDUINO_PLC_IDE_TOOLS_FORBIDDEN")
    need(git("branch","--show-current")==BRANCH,"BRANCH_INCORRECT","GIT")
    need(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
         "ROOT_MISMATCH","GIT")
    need(not git_paths("diff","--name-only"),"WORKTREE_TRACKED_DIRTY","GIT")
    need(not git_paths("diff","--cached","--name-only"),"GIT_INDEX_STAGED","GIT")
    needs=set(PRODUCT)|{ARCHIVE,CLOSE_DOC,CHECK_DOC,STATUS_DOC}
    need(not(git_paths("ls-files","--others","--exclude-standard") & needs),
         "UNTRACKED_TARGET_COLLISION","GIT")
    for name in (CLOSE_DOC,CHECK_DOC):
        need(not (ROOT/name).exists(),"CLOSE_DOC_COLLISION","GIT")
    head=git("rev-parse","HEAD")
    need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
         "LOCAL_REMOTE_HEAD_DIFFER","GIT")
    MANIFEST["entry_head"]=head
    MANIFEST["cli_path"]=str(cli)
    MANIFEST["cli_sha256"]=sha(cli)
    need(MANIFEST["cli_sha256"]==
         "1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571",
         "OFFICIAL_ARDUINO_CLI_SHA_DIFFERS_FROM_G6","ENVIRONMENT")
    MANIFEST["port"]=port
    log("CLI_EXE",cli)
    log("CLI_SHA256",MANIFEST["cli_sha256"])
    version=call([cli,"version"],"CLI_VERSION")
    need("Version: 1.5.1" in version,"CLI_VERSION_NOT_PINNED")
    cores=call([cli,"core","list"],"CLI_CORES")
    need(re.search(r"(?m)^\s*jwplc_local:esp32\s+2\.1\.0-dev(?:\s|$)",cores),
         "JWPLC_LOCAL_CORE_MISSING")
    need(sha(CORE)==CORE_SHA and sha(TFT)==TFT_SHA,
         "G3_G4_ARCHIVE_BASELINE_CHANGED")
    properties=(FRAM/"library.properties").read_text(encoding="utf-8")
    need(re.search(r"(?m)^precompiled=full\s*$",properties),
         "JW_FRAM_PRECOMPILED_MODE_CHANGED")
    for path in [*PRODUCT,ARCHIVE]:
        need((ROOT/path).is_file(),"PRODUCT_PATH_MISSING_"+path)
    for path in PRODUCT.values():
        need((ROOT/path).is_file(),"CANDIDATE_PATH_MISSING_"+path)
    MANIFEST["before_sha256"]={p:sha(ROOT/p) for p in [*PRODUCT,ARCHIVE]}
    MANIFEST["candidate_sha256"]={p:sha(ROOT/c) for p,c in PRODUCT.items()}
    # Source/manifest keys MUST use product paths; prevents F114.
    need(set(MANIFEST["candidate_sha256"])==set(PRODUCT),
         "CANDIDATE_MANIFEST_KEY_INVALID")
    from serial.tools import list_ports
    ports=[p.device for p in list_ports.comports()]
    log("PORTS_VISIBLE",",".join(ports))
    need(port.lower() in [x.lower() for x in ports],
         "SERIAL_PORT_NOT_DETECTED","ENVIRONMENT")
    stage("PREFLIGHT","PASS")

def library_properties(text,source=False):
    rows=[line for line in text.splitlines()
          if not line.startswith("precompiled=")]
    if not source:rows.append("precompiled=full")
    return "\n".join(rows)+"\n"

def prep_source():
    stage("SOURCE_OVERLAY","RUNNING")
    source=TEMP/"source"/"JW_FRAM"
    (source/"src").mkdir(parents=True)
    for product,candidate in PRODUCT.items():
        shutil.copy2(ROOT/candidate,source/"src"/Path(candidate).name)
    prop=(FRAM/"library.properties").read_text(encoding="utf-8")
    (source/"library.properties").write_text(
        library_properties(prop,source=True),encoding="utf-8")
    # Preservar bundled BusIO en el include, no sustituir por biblioteca externa.
    hdr=(source/"src/JW_FRAM.h").read_text(encoding="utf-8")
    need(hdr.index("#include <JWPLC_Bundled_Adafruit_BusIO.h>") <
         hdr.index("#include <Adafruit_SPIDevice.h>"),
         "BUNDLED_BUSIO_INCLUDE_ORDER")
    stage("SOURCE_OVERLAY","PASS")
    return source

def compile(cli,sketch,label,selected):
    build=TEMP/("build_"+label)
    output=call([cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
                 "--build-path",build,"--library",selected,
                 "--libraries",LIBS,sketch],"COMPILE_"+label)
    hits=re.findall(r"(?m)^Using library JW_FRAM at version .+ in folder: (.+)$",output)
    need(len(hits)==1 and Path(hits[0].strip()).resolve()==selected.resolve(),
         "WRONG_JW_FRAM_SELECTED_"+label)
    busio=re.findall(r"(?m)^Using library Adafruit BusIO at version .+ in folder: (.+)$",output)
    bundled=LIBS/"Adafruit_BusIO"
    need(len(busio)==1 and Path(busio[0].strip()).resolve()==bundled.resolve(),
         "WRONG_BUNDLED_BUSIO_"+label)
    need(re.search(r"[/\\]precompiled[/\\]core[/\\]JWPLCBASIC[/\\]core\.a",output),
         "CORE_ARCHIVE_NOT_LINKED_"+label)
    return output,build

def sample_sketch():
    demo=TEMP/"sample"/"sample"
    # This helper is called by SOURCE_REBUILD, CANDIDATE_LINK and
    # subsequent regressions; reuse it without crashing on second call.
    demo.mkdir(parents=True,exist_ok=True)
    (demo/"sample.ino").write_text(
        "#include <Arduino.h>\n"
        "#include <JW_FRAM.h>\n"
        "JW_FRAM g_fram(5);\n"
        "struct X { unsigned long seq; unsigned char enabled; };\n"
        "void setup(){X x={12,1};(void)g_fram.size();"
        "(void)g_fram.readBlock(0,x,1);}\n"
        "void loop(){}\n",encoding="utf-8")
    return demo

def build_archive(cli,source):
    stage("SOURCE_REBUILD","RUNNING")
    verbose,build=compile(cli,sample_sketch(),"FRAM_SOURCE",source)
    obj=list(build.rglob("JW_FRAM.cpp.o"))
    need(len(obj)==1,"FRAM_SOURCE_OBJECT_NOT_UNIQUE")
    need(not re.search(r"Using precompiled library JW_FRAM",verbose),
         "SOURCE_UNEXPECTED_PRECOMPILED_MARKER")
    archiveTool=re.findall(r'["\x27]([^"\x27\r\n]*xtensa-esp32-elf-gcc-ar(?:\.exe)?)["\x27]',
                           verbose,re.I)
    archiver=next((Path(x) for x in archiveTool if Path(x).is_file()),None)
    if archiver is None:
        # Arduino CLI prints the g++ command but need not print gcc-ar.
        # Use the archiver shipped alongside the COMPILER ACTUALLY USED.
        compilerTools=re.findall(
            r'["\x27]([^"\x27\r\n]*xtensa-esp32-elf-g\+\+(?:\.exe)?)["\x27]',
            verbose,re.I)
        for compiler in compilerTools:
            for name in ("xtensa-esp32-elf-gcc-ar.exe","xtensa-esp32-elf-gcc-ar"):
                candidate=Path(compiler).parent/name
                if candidate.is_file():
                    archiver=candidate
                    break
            if archiver is not None:
                break
    need(archiver is not None,"ARCHIVER_PATH_NOT_FOUND_IN_COMPILE")
    MANIFEST["source_object_sha256"]=sha(obj[0])
    stage("SOURCE_REBUILD","PASS")
    stage("ARCHIVE_REBUILD","RUNNING")
    pre=TEMP/"candidate"/"JW_FRAM"
    (pre/"src/esp32").mkdir(parents=True)
    shutil.copy2(source/"src/JW_FRAM.h",pre/"src/JW_FRAM.h")
    prop=(FRAM/"library.properties").read_text(encoding="utf-8")
    (pre/"library.properties").write_text(
        library_properties(prop),encoding="utf-8")
    archive=pre/"src/esp32/libJW_FRAM.a"
    call([archiver,"crs",archive,obj[0]],"ARCHIVE_CREATE")
    members=call([archiver,"t",archive],"ARCHIVE_MEMBERS").splitlines()
    need([x.strip() for x in members if x.strip()]==["JW_FRAM.cpp.o"],
         "ARCHIVE_MEMBER_SET_WRONG")
    extract=TEMP/"extract"
    extract.mkdir()
    call([archiver,"x",archive],"ARCHIVE_EXTRACT",cwd=extract)
    extracted=extract/"JW_FRAM.cpp.o"
    need(extracted.is_file() and sha(extracted)==sha(obj[0]),
         "SOURCE_OBJECT_ARCHIVE_PARITY_FAILED")
    MANIFEST["archive_sha256"]=sha(archive)
    MANIFEST["archive_object_sha256"]=sha(extracted)
    log("SOURCE_OBJECT_SHA256",sha(obj[0]))
    log("ARCHIVE_MEMBER_BYTE_PARITY","PASS")
    log("NEW_FRAM_ARCHIVE_SHA256",MANIFEST["archive_sha256"])
    stage("ARCHIVE_REBUILD","PASS")
    return pre,archive

def assert_fram_archive_link(verbose, selected, label):
    """Verify exact -L directory, -l flag and candidate .a identity.

    Arduino CLI's linker need not spell libJW_FRAM.a literally. It
    normally emits -L<selected>/src/esp32 -lJW_FRAM. Do not confuse
    a mere 'Using library' message with proof of the linker inputs.
    """
    expected_dir=(selected/"src/esp32").resolve()
    archive=expected_dir/"libJW_FRAM.a"
    need(archive.is_file(),"FRAM_ARCHIVE_FILE_ABSENT_"+label,"HARNESS")
    need(sha(archive)==MANIFEST["archive_sha256"],
         "FRAM_ARCHIVE_SHA_MISMATCH_"+label,"HARNESS")
    # Some Windows command renderings escape \ as \\; accept both
    # without requiring the .a basename to appear in stdout.
    link_lines=[x.replace("\\\\","\\") for x in verbose.splitlines()
                if "-lJW_FRAM" in x and ".elf" in x]
    need(len(link_lines)==1,"FRAM_LINKER_COMMAND_NOT_UNIQUE_"+label,"HARNESS")
    line=link_lines[0]
    need(re.search(r'(?<!\S)["\x27]?-lJW_FRAM(?=\s|["\x27]|$)',line),
         "FRAM_LINKER_LIBRARY_FLAG_MISSING_"+label,"HARNESS")
    dirs=re.findall(
        r'(?<!\S)["\x27]?-L["\x27]?([A-Za-z]:[\\/][^\s"\x27]+)',
        line)
    selected_dirs=[Path(x).resolve() for x in dirs]
    need(expected_dir in selected_dirs,
         "FRAM_LINKER_DIRECTORY_MISMATCH_"+label,"HARNESS")
    log("FRAM_LINK_"+label,"PASS_EXACT_L_PATH_L_FLAG_AND_ARCHIVE_SHA")
    return True

def resume_verified_archive(run_id):
    """Reuse a prior physical-NOT-STARTED run's verified .a, not its gate status."""
    allowed="g7_20261011_162621_6ca7668e"
    need(run_id==allowed,"RESUME_RUN_NOT_ALLOWLISTED","HARNESS")
    previous=ROOT/"tools/alpha13/results"/("g7_fram_"+run_id)
    oldtemp=Path(tempfile.gettempdir())/("jwplc_a13_"+run_id)
    mf=previous/"MANIFEST.json"
    need(mf.is_file(),"RESUME_MANIFEST_MISSING","ENVIRONMENT")
    m=json.loads(mf.read_text(encoding="utf-8-sig"))
    need(m.get("schema")=="A13-G7-INTEGRATED-1" and
         m.get("run_id")==run_id and
         m.get("entry_head")=="08bee3b1b2d9f3d7696a8ebcdf069c8d190260b1",
         "RESUME_MANIFEST_PROVENANCE","HARNESS")
    need(m.get("status")=="REVIEW_STOP" and
         m.get("reason")=="PRECOMPILED_ARCHIVE_LINK_MISSING_CANDIDATE_LINK" and
         m.get("physical_upload") is False and
         m.get("product_adopted") is False and
         m.get("git_product_committed") is False and
         m.get("git_docs_committed") is False and
         m.get("git_pushed") is False,
         "RESUME_NOT_SAFE_UNUPLOADED_RUN","HARNESS")
    phases=m.get("phase",{})
    for name in ("PREFLIGHT","SOURCE_OVERLAY","SOURCE_REBUILD","ARCHIVE_REBUILD"):
        need(phases.get(name)=="PASS","RESUME_PHASE_NOT_PASS_"+name,"HARNESS")
    need(phases.get("CANDIDATE_LINK")=="RUNNING","RESUME_STOP_POSITION_WRONG","HARNESS")
    need(m.get("cli_sha256")==MANIFEST["cli_sha256"] and
         m.get("port")==MANIFEST["port"],
         "RESUME_CLI_OR_PORT_CHANGED","ENVIRONMENT")
    need(m.get("before_sha256")==MANIFEST["before_sha256"] and
         m.get("candidate_sha256")==MANIFEST["candidate_sha256"],
         "RESUME_PRODUCT_OR_CANDIDATE_CHANGED","HARNESS")
    archive_sha=m.get("archive_sha256")
    object_sha=m.get("source_object_sha256")
    need(isinstance(archive_sha,str) and len(archive_sha)==64 and
         isinstance(object_sha,str) and len(object_sha)==64,
         "RESUME_ARCHIVE_OR_OBJECT_SHA_MISSING","HARNESS")
    source=oldtemp/"source/JW_FRAM"
    oldselected=oldtemp/"candidate/JW_FRAM"
    oldarchive=oldselected/"src/esp32/libJW_FRAM.a"
    oldmembers=oldtemp/"extract/JW_FRAM.cpp.o"
    oldobject=list((oldtemp/"build_FRAM_SOURCE").rglob("JW_FRAM.cpp.o"))
    need(len(oldobject)==1 and oldmembers.is_file() and oldarchive.is_file(),
         "RESUME_ARTIFACTS_MISSING","ENVIRONMENT")
    need(sha(oldobject[0])==object_sha and sha(oldmembers)==object_sha,
         "RESUME_OBJECT_ARCHIVE_PARITY_MISMATCH","HARNESS")
    need(sha(oldarchive)==archive_sha,"RESUME_ARCHIVE_SHA_MISMATCH","HARNESS")
    for prod,cand in PRODUCT.items():
        name=Path(prod).name
        need(sha(source/"src"/name)==MANIFEST["candidate_sha256"][prod] and
             sha(oldselected/"src"/name)==MANIFEST["candidate_sha256"][prod]
             if name=="JW_FRAM.h" else
             sha(source/"src"/name)==MANIFEST["candidate_sha256"][prod],
             "RESUME_SOURCE_CANDIDATE_CHANGED_"+name,"HARNESS")
    compile_log=previous/"COMPILE_CANDIDATE_LINK.log"
    need(compile_log.is_file(),"RESUME_LINK_LOG_MISSING","HARNESS")
    text=compile_log.read_text(encoding="utf-8-sig")
    need(re.search(r"(?m)^EXIT=0\s*$",text),
         "RESUME_PRIOR_CANDIDATE_COMPILE_NOT_PASS","HARNESS")
    hits=re.findall(r"(?m)^Using library JW_FRAM at version .+ in folder: (.+)$",text)
    need(len(hits)==1 and
         Path(hits[0].strip()).resolve()==oldselected.resolve(),
         "RESUME_PRIOR_LIBRARY_SELECTION_MISMATCH","HARNESS")
    MANIFEST["archive_sha256"]=archive_sha
    MANIFEST["source_object_sha256"]=object_sha
    assert_fram_archive_link(text,oldselected,"RESUME_PREVIOUS_LINK")
    selected=TEMP/"candidate/JW_FRAM"
    need(not selected.exists(),"RESUME_TEMP_CANDIDATE_ALREADY_EXISTS","HARNESS")
    shutil.copytree(oldselected,selected)
    need(sha(selected/"src/esp32/libJW_FRAM.a")==archive_sha and
         sha(selected/"src/JW_FRAM.h")==MANIFEST["candidate_sha256"][
            "JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.h"],
         "RESUME_COPY_VERIFICATION_FAILED","HARNESS")
    MANIFEST["resume_from_run"]=run_id
    MANIFEST["resume_from_archive"]=str(oldarchive)
    MANIFEST["phase"]["SOURCE_REBUILD"]="PASS_REUSED_FROM_VERIFIED_RUN"
    MANIFEST["phase"]["ARCHIVE_REBUILD"]="PASS_REUSED_FROM_VERIFIED_RUN"
    log("G7_REUSED_ARCHIVE_SHA256",archive_sha)
    log("G7_REUSED_OBJECT_SHA256",object_sha)
    log("G7_REUSED_PRIOR_LINK","PASS")
    save()
    return selected,selected/"src/esp32/libJW_FRAM.a"

def check_precompiled(cli,selected,label):
    stage(label,"RUNNING")
    verbose,build=compile(cli,sample_sketch(),label,selected)
    need(not list(build.rglob("JW_FRAM.cpp.o")),
         "PRECOMPILED_POLICY_BROKEN_"+label)
    assert_fram_archive_link(verbose,selected,label)
    stage(label,"PASS")

def regression(cli,selected,prefix):
    stage(prefix,"RUNNING")
    demos=[
        ("GLOBAL_FRAM",LIBS/"JWPLC_GlobalPeripherals/examples/04.FRAM_Basic"),
        ("CONFIG_BLOCK",LIBS/"JW_FRAM/examples/Block_Config"),
    ]
    for name,path in demos:
        need(path.is_dir(),"SKETCH_NOT_FOUND_"+name)
        verbose,build=compile(cli,path,prefix+"_"+name,selected)
        need(not list(build.rglob("JW_FRAM.cpp.o")),
             "PRECOMPILED_REGRESSION_BROKEN_"+name)
        assert_fram_archive_link(verbose,selected,prefix+"_"+name)
        log(prefix+"_"+name,"PASS")
    MANIFEST[prefix.lower()+"_regressions"]=[p for p,_ in demos]
    stage(prefix,"PASS")

def upload_physical(cli,selected,port):
    stage("PHYSICAL_BUILD","RUNNING")
    sketch=TEMP/"probe"/"probe"
    sketch.mkdir(parents=True)
    original=(ROOT/"tools/alpha13/firmware/a13_g7_fram_probe/a13_g7_fram_probe.ino")
    code=original.read_text(encoding="utf-8")
    need(code.count("__G7_TOKEN__")==1,"TOKEN_ANCHOR_INVALID")
    (sketch/"probe.ino").write_text(code.replace("__G7_TOKEN__",RUN_ID),
                                     encoding="utf-8")
    verbose,build=compile(cli,sketch,"G7_PHYSICAL",selected)
    assert_fram_archive_link(verbose,selected,"PHYSICAL")
    bins=list(build.rglob("*.ino.bin"))
    need(len(bins)==1,"PHYSICAL_APP_BIN_NOT_UNIQUE")
    MANIFEST["app_sha256"]=sha(bins[0])
    log("APP_SHA256",MANIFEST["app_sha256"])
    stage("PHYSICAL_BUILD","PASS")
    stage("BENCH_SAFETY","RUNNING")
    log("SAFETY","Se reemplaza el sketch actual. FRAM 0x1F00..0x1F7F se respalda en NVS y restaura. Equipo sin cargas ni actuadores.")
    safety=input("JWPLC en banco, FRAM disponible y sin actuadores (BANCO_FRAM): ").strip()
    need(safety=="BANCO_FRAM","BENCH_SAFETY_NOT_CONFIRMED","PRECONDITION")
    stage("BENCH_SAFETY","PASS")
    stage("PHYSICAL_UPLOAD","RUNNING")
    call([cli,"upload","--fqbn",FQBN,"--port",port,"--input-dir",build],
         "UPLOAD_G7")
    MANIFEST["physical_upload"]=True
    stage("PHYSICAL_UPLOAD","PASS")
    stage("FRAM_PHYSICAL","RUNNING")
    evidence=call([sys.executable,"-B",
        ROOT/"tools/alpha13/gates/a13_g7_fram_serial.py",
        "--serial-port",port,"--token",RUN_ID],"FRAM_PHYSICAL_SERIAL")
    need("G7_FRAM_PHYSICAL=PASS_BACKUP_RESTORE_CHECKSUM_NULL_BOUNDS" in evidence,
         "FRAM_PHYSICAL_RESULT_MISSING")
    MANIFEST["fram_restored"]=True
    stage("FRAM_PHYSICAL","PASS")

def adopt(archive):
    global ADOPTED
    stage("PRODUCT_ADOPTION","RUNNING")
    BACKUP.mkdir(parents=True,exist_ok=False)
    for dest in [*PRODUCT,ARCHIVE]:
        target=BACKUP/dest
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/dest,target)
    ADOPTED=True
    for dest,candidate in PRODUCT.items():
        shutil.copy2(ROOT/candidate,ROOT/dest)
    shutil.copy2(archive,ROOT/ARCHIVE)
    MANIFEST["product_adopted"]=True
    need(git_paths("diff","--name-only")==set([*PRODUCT,ARCHIVE]),
         "PRODUCT_DIFF_SCOPE_NOT_3","GIT")
    need(not git("diff","--check"),"PRODUCT_DIFF_WHITESPACE","GIT")
    stage("PRODUCT_ADOPTION","PASS")

def audit(archive):
    stage("FINAL_AUDIT","RUNNING")
    for prod,candidate in PRODUCT.items():
        need(sha(ROOT/prod)==sha(ROOT/candidate),
             "SOURCE_CANDIDATE_MISMATCH_"+prod)
    need(sha(ROOT/ARCHIVE)==sha(archive),
         "ARCHIVE_CANDIDATE_MISMATCH")
    need(sha(CORE)==CORE_SHA and sha(TFT)==TFT_SHA,
         "G3_G4_ARCHIVES_CHANGED")
    need(git_paths("diff","--name-only")==set([*PRODUCT,ARCHIVE]),
         "PRODUCT_SCOPE_CHANGED","GIT")
    need(not git_paths("diff","--cached","--name-only"),
         "UNEXPECTED_STAGED_FILES","GIT")
    MANIFEST["product_sha256"]={p:sha(ROOT/p) for p in [*PRODUCT,ARCHIVE]}
    need(set(MANIFEST["product_sha256"])==set(MANIFEST["before_sha256"]),
         "MANIFEST_PRODUCT_KEY_SCHEMA")
    need(MANIFEST.get("fram_restored") is True,"FRAM_NOT_RESTORED")
    need(MANIFEST["phase"].get("PRODUCT_REGRESSIONS")=="PASS" and
         MANIFEST["phase"].get("FRAM_PHYSICAL")=="PASS",
         "GATE_NOT_FULLY_PASS")
    stage("FINAL_AUDIT","PASS")

def close_documents():
    m=MANIFEST
    text=f"""# Alpha13-G7 — Cierre FRAM ({datetime.now().strftime('%Y-%m-%d')})

**G7=PASS físico, compilado y archive; Alpha13 permanece en curso.**

- RUN_ID={RUN_ID}
- CLI={m['cli_path']}
- CLI_SHA256={m['cli_sha256']}
- Firmware SHA256={m['app_sha256']}; COM={m['port']}
- FRAM: escritura en 0x1F00..0x1F7F, respaldo NVS, restauración verificada.
- NULL y bounds, writeBlock/readBlock, checksum y preservación de salida: PASS.
- Archivo objeto recompilado SHA256={m['source_object_sha256']}
- Archivo .a SHA256={m['archive_sha256']}
- Paridad objeto↔miembro archive: PASS
- Precompiled=full, enlace candidato y enlace normal: PASS
- Regresiones candidato: {m['candidate_regressions']}
- Regresiones producto: {m['product_regressions']}
- SHA finales: {m['product_sha256']}
- core.a G3 y TFT.a G4: intactos.

**Límite:** el informe canónico A13-009/A13-013 no está presente
íntegro; no se atribuye su cierre semántico hasta contrastarlo.
No se tocó OpenPLC ni HMI Designer.
No PR ni publicación de alpha.

NEXT_GATE=A13-G8_SERIAL_A13_010_A13_011.
"""
    check="""# Alpha13-G7 — Checklist FRAM
- [x] CLI oficial 1.5.1 y core local comprobados.
- [x] Fuentes compiladas sin archive.
- [x] Archive .a regenerado y miembro igual al objeto.
- [x] Enlace precompiled=full del candidato verificado.
- [x] FRAM en banco, ventana respaldada en NVS.
- [x] Lectura/escritura, checksum inválido y punteros nulos comprobados.
- [x] FRAM restaurada y verificada.
- [x] Regresiones normales source/archive y hashes finales.
- [x] Commits/push no-force automáticos tras PASS.
- [ ] Contrastar atribución A13-009/A13-013 con auditoría canónica.
- [ ] G8 y gates restantes antes del freeze Alpha13.
"""
    previous=(ROOT/STATUS_DOC).read_text(encoding="utf-8-sig")
    marker="# v2.1.0-alpha.13 — Estado operativo y continuidad\n"
    need(previous.startswith(marker) and
         "## Estado vigente — G7 cerrado" not in previous,
         "STATUS_HEADER_OR_CLOSURE_CHANGED")
    status=f"""
## Estado vigente — G7 cerrado, G8 siguiente ({datetime.now().strftime('%Y-%m-%d')})

ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6=CLOSED_PASS
G7_FRAM=CLOSED_PASS
G7_RUN_ID={RUN_ID}
G7_FRAM_RESTORED=PASS
G7_ARCHIVE_SHA256={m['archive_sha256']}
G7_SOURCE_ARCHIVE_PARITY=PASS
G7_A13_009_013_CANONICAL_MAPPING=REVIEW_PENDING
NEXT_GATE=A13-G8_SERIAL_A13_010_A13_011
NEXT_FAILURE_ID=F118
RELEASE_MERGE_PR_PUBLICATION=NO

Cierre: A13_G7_FRAM_CLOSURE_20261011.md

---

"""
    return {CLOSE_DOC:text,CHECK_DOC:check,
            STATUS_DOC:previous.replace(marker,marker+status,1)}

def commit_exact(paths,message):
    git("add","--",*sorted(paths))
    need(git_paths("diff","--cached","--name-only")==paths,
         "STAGED_SCOPE_NOT_EXACT","GIT")
    need(not git("diff","--cached","--check"),
         "STAGED_DIFF_WHITESPACE","GIT")
    git("commit","-m",message)
    head=git("rev-parse","HEAD")
    need(git_paths("diff-tree","--no-commit-id","--name-only","-r",head)==paths,
         "COMMIT_PATHS_NOT_EXACT","GIT")
    log("COMMIT",head)
    return head

def auto_git_close():
    global COMMITTED
    stage("AUTO_GIT_CLOSURE","RUNNING")
    old=MANIFEST["entry_head"]
    need(git("rev-parse","HEAD")==old,"GIT_HEAD_MOVED","GIT")
    lines=git("ls-remote","origin","refs/heads/"+BRANCH).splitlines()
    need(len(lines)==1 and lines[0].split()[0]==old,
         "REMOTE_CHANGED_DURING_G7","GIT")
    documents=close_documents()
    need(set(documents)=={CLOSE_DOC,CHECK_DOC,STATUS_DOC},
         "DOCUMENT_SCOPE_CHANGED")
    product=set([*PRODUCT,ARCHIVE])
    need(git_paths("diff","--name-only")==product,
         "PRECOMMIT_PRODUCT_SCOPE_CHANGED","GIT")
    need(not git_paths("diff","--cached","--name-only"),
         "PRECOMMIT_INDEX_STAGED","GIT")
    for f in (CLOSE_DOC,CHECK_DOC):
        need(not (ROOT/f).exists(),"NEW_DOC_COLLISION","GIT")
    product_head=commit_exact(product,"fix(fram): robustecer validaciones y lectura de bloques")
    COMMITTED=True
    MANIFEST["git_product_committed"]=True
    MANIFEST["git_product_commit"]=product_head
    for dest,content in documents.items():
        target=ROOT/dest
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text(content,encoding="utf-8",newline="\n")
    doc_head=commit_exact(set(documents),"docs(alpha13): cerrar G7 y preparar G8 serial")
    MANIFEST["git_docs_committed"]=True
    MANIFEST["git_docs_commit"]=doc_head
    need(not git_paths("diff","--name-only") and
         not git_paths("diff","--cached","--name-only"),
         "POSTCOMMIT_TRACKED_DIRTY","GIT")
    git("push","origin","HEAD:refs/heads/"+BRANCH)
    MANIFEST["git_pushed"]=True
    stage("AUTO_GIT_CLOSURE","PASS")
    log("PRODUCT_COMMIT",product_head)
    log("DOCUMENTS_COMMIT",doc_head)
    log("PUSH","PASS_NON_FORCE")
    log("NEXT_GATE","A13-G8_SERIAL_A13_010_A13_011")

def rollback():
    if not ADOPTED or COMMITTED:return
    for path in [*PRODUCT,ARCHIVE]:
        saved=BACKUP/path
        if saved.is_file():shutil.copy2(saved,ROOT/path)
    ok=all((BACKUP/p).is_file() and sha(BACKUP/p)==sha(ROOT/p)
           for p in [*PRODUCT,ARCHIVE])
    MANIFEST["rollback"]="PASS" if ok else "FAIL"
    log("ROLLBACK",MANIFEST["rollback"])

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduino-cli",required=True)
    parser.add_argument("--serial-port",required=True)
    parser.add_argument("--resume-from-run",default="",
                        help="Reutilizar archivo y evidencia source de R2")
    args=parser.parse_args()
    RESULT.mkdir(parents=True,exist_ok=False)
    TEMP.mkdir(parents=True,exist_ok=False)
    try:
        cli=Path(args.arduino_cli).expanduser().resolve()
        preflight(cli,args.serial_port)
        call([sys.executable,"-B",
              ROOT/"tools/alpha13/gates/a13_finalizer_portability_selftest.py"],
             "F112_PORTABILITY_SELFTEST")
        call([sys.executable,"-B",
              ROOT/"tools/alpha13/gates/a13_g7_contract_selftest.py"],
             "G7_CONTRACT_SELFTEST")
        if args.resume_from_run:
            pre,archive=resume_verified_archive(args.resume_from_run)
        else:
            source=prep_source()
            pre,archive=build_archive(cli,source)
        check_precompiled(cli,pre,"CANDIDATE_LINK")
        regression(cli,pre,"CANDIDATE_REGRESSIONS")
        upload_physical(cli,pre,args.serial_port)
        adopt(archive)
        regression(cli,FRAM,"PRODUCT_REGRESSIONS")
        audit(archive)
        auto_git_close()
        MANIFEST["status"]="CLOSED_PASS"
        log("STATUS","CLOSED_PASS")
        save()
        return 0
    except (Exception,KeyboardInterrupt) as exc:
        MANIFEST["reason"]=str(exc)
        MANIFEST["category"]=(exc.kind if isinstance(exc,Review) else
                              "USER_ABORT" if isinstance(exc,KeyboardInterrupt)
                              else "HARNESS")
        # Si Git ya avanzó, ni siquiera un error entre commit y bandera
        # COMMITTED permite restaurar archivos sobre el HEAD nuevo.
        advanced=False
        try:
            current=git("rev-parse","HEAD")
            advanced=(current!=MANIFEST.get("entry_head",current))
        except Exception:
            advanced=True
        if not COMMITTED and not advanced:
            rollback()
        elif ADOPTED:
            log("ROLLBACK","SKIPPED_GIT_HEAD_ADVANCED_OR_UNVERIFIED")
        MANIFEST["status"]="REVIEW_STOP"
        log("STATUS","REVIEW_STOP")
        log("REASON",MANIFEST["reason"])
        log("CATEGORY",MANIFEST["category"])
        log("GIT_PARTIAL_COMMIT",COMMITTED or advanced)
        log("FRAM_RESTORE_VERIFIED",MANIFEST.get("fram_restored"))
        save()
        return 2

if __name__=="__main__":
    sys.exit(main())
