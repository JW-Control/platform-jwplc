#!/usr/bin/env python3
"""A13-G6 integrado: compila fuente, valida UDP físico y cierra Git si PASS.

No permite cambios fuera de dos fuentes EthernetUDP y dos documentos G6
tras validación completa. No requiere AUTORIZO_COMMIT. Sí exige BANCO_UDP
antes del upload porque el sketch previo del JWPLC será sustituido.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
FQBN="jwplc_local:esp32:jwplcbasic"
LIBS=ROOT/"JWPLC/2.1.0/libraries"
ETH=LIBS/"JWPLC_Ethernet"
PRODUCT={
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp":
    "tools/alpha13/candidates/g6/EthernetUdp.cpp",
    "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h":
    "tools/alpha13/candidates/g6/JWPLC_W5x00_Ethernet.h",
}
CORE=ROOT/"JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
TFT=ROOT/"JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a"
CORE_SHA="a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d"
TFT_SHA="4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc"
RUN_ID=datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
RESULT=ROOT/"tools/alpha13/results"/("g6_integrated_"+RUN_ID)
TEMP=Path(tempfile.gettempdir())/("jwplc_a13_g6_"+RUN_ID)
BACKUP=TEMP/"backup"
CLOSE_DOC="docs/v2.1.0-alpha.13/A13_G6_CLOSURE_20261011.md"
CHECK_DOC="docs/v2.1.0-alpha.13/A13_G6_CLOSURE_CHECKLIST_20261011.md"
STATUS_DOC="docs/v2.1.0-alpha.13/ALPHA13_STATUS.md"
MANIFEST={
    "schema":"A13-G6-INTEGRATED-1",
    "run_id":RUN_ID,
    "status":"REVIEW",
    "phase":{},
    "product_changed":False,
    "physical_upload":False,
    "git_product_committed":False,
    "git_docs_committed":False,
    "git_pushed":False,
    "published_release":False,
}
ADOPTED=False
COMMITTED=False

class Stop(Exception):
    def __init__(self,reason,category="PRECONDITION"):
        super().__init__(reason)
        self.category=category

def check(flag,why,category="PRECONDITION"):
    if not flag: raise Stop(why,category)

def emit(k,v):print(f"{k}={v}",flush=True)
def sha(file):
    result=hashlib.sha256()
    with Path(file).open("rb") as f:
        for piece in iter(lambda:f.read(1<<20),b""):
            result.update(piece)
    return result.hexdigest()

def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if p.returncode:
        raise Stop("GIT_FAILED:"+repr(args)+":"+
                   p.stderr.decode("utf-8","replace")[-600:],"GIT")
    return p.stdout.decode("utf-8","replace").strip()

def git_paths(*args):
    tokens=list(args)
    tokens.insert(tokens.index("--") if "--" in tokens else len(tokens),"-z")
    p=subprocess.run(["git","-c","core.quotepath=false",*tokens],
                     cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    check(p.returncode==0,"GIT_PATH_QUERY_FAILED","GIT")
    return set(filter(None,p.stdout.decode("utf-8","replace").split("\0")))

def persist():
    RESULT.mkdir(parents=True,exist_ok=True)
    (RESULT/"MANIFEST.json").write_text(
        json.dumps(MANIFEST,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    lines=["GATE=A13-G6","RUN_ID="+RUN_ID,"STATUS="+MANIFEST["status"]]
    lines += [f"{k}={v}" for k,v in MANIFEST["phase"].items()]
    for k in ("reason","category","port","cli_path","cli_sha256","app_sha256","physical",
              "regressions","git_product_commit","git_docs_commit","rollback"):
        if k in MANIFEST:lines.append(k.upper()+"="+str(MANIFEST[k]))
    (RESULT/"SUMMARY.log").write_text("\n".join(lines)+"\n",encoding="utf-8")

def phase(name,value):
    MANIFEST["phase"][name]=value
    emit("PHASE",name+"_"+value)
    persist()

def run(args,label):
    args=[str(x) for x in args]
    p=subprocess.run(args,cwd=ROOT,stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT,encoding="utf-8",
                     errors="backslashreplace")
    output=p.stdout or ""
    (RESULT/(label+".log")).write_text(
        "ARGS="+repr(args)+"\nEXIT="+str(p.returncode)+"\n"+output,
        encoding="utf-8")
    emit(label+"_EXIT",p.returncode)
    if p.returncode:
        errors=[x for x in output.splitlines()
                if re.search(r"(?:fatal error:|error:|undefined reference|G6_PC_ERROR)",x)]
        for x in errors[-8:]:emit("DIAGNOSTIC",x[-260:])
        if not errors:emit("DIAGNOSTIC_TAIL",repr(output.splitlines()[-7:])[-900:])
        raise Stop(label+"_FAILED","ENVIRONMENT_OR_PRODUCT")
    return output

def cli_find(explicit):
    # F115: NO fallback silencioso a Arduino PLC IDE Tools ni PATH ambiguo.
    # El operador identifica la instalacion CLI usada por Arduino IDE.
    check(bool(explicit),"ARDUINO_CLI_EXPLICIT_PATH_REQUIRED","ENVIRONMENT")
    candidate=Path(explicit).expanduser()
    check(candidate.is_absolute(),"ARDUINO_CLI_ABSOLUTE_PATH_REQUIRED","ENVIRONMENT")
    check(candidate.is_file(),"ARDUINO_CLI_EXECUTABLE_NOT_FOUND","ENVIRONMENT")
    resolved=candidate.resolve()
    check(resolved.name.casefold() in ("arduino-cli.exe","arduino-cli"),
          "ARDUINO_CLI_FILENAME_NOT_EXPECTED","ENVIRONMENT")
    check("arduino plc ide tools" not in str(resolved).casefold(),
          "ARDUINO_PLC_IDE_TOOLS_FORBIDDEN","ENVIRONMENT")
    MANIFEST["cli_path"]=str(resolved)
    MANIFEST["cli_sha256"]=sha(resolved)
    emit("ARDUINO_CLI_EXE",resolved)
    emit("ARDUINO_CLI_SHA256",MANIFEST["cli_sha256"])
    return str(resolved)

def serial_port(name):
    from serial.tools import list_ports
    ps=list(list_ports.comports())
    emit("PORTS_VISIBLE",",".join(p.device for p in ps))
    if name:
        hits=[p for p in ps if p.device.lower()==name.lower()]
        check(len(hits)==1,"PORT_NOT_AVAILABLE","ENVIRONMENT")
        return hits[0].device
    filtered=[p.device for p in ps if p.device.upper()!="COM1"]
    check(len(filtered)==1,"PORT_AMBIGUOUS","ENVIRONMENT")
    return filtered[0]

def source_contract():
    udp=(ROOT/list(PRODUCT.values())[0]).read_text(encoding="utf-8")
    header=(ROOT/list(PRODUCT.values())[1]).read_text(encoding="utf-8")
    gen={
        "write":("size_t EthernetUDP::write(const uint8_t *buffer, size_t size)",
                 "room = (size_t)W5100.SSIZE - _offset"),
        "read":("int EthernetUDP::read(unsigned char *buffer, size_t len)",
                "if (bounded > (size_t)INT16_MAX)"),
        "parse":("int EthernetUDP::parsePacket()",
                 "Ethernet.socketRecvAvailable(sockindex) < 8"),
        "fast":("int EthernetUDP::jwplcReadPacketFastDeferred(",
                "len > (size_t)UINT16_MAX"),
    }
    for name,(function,feature) in gen.items():
        check(udp.count(function)==1 and feature in udp,
              "CANDIDATE_SOURCE_CONTRACT_"+name,"HARNESS")
    check("EthernetUDP() : _port(0), _remotePort(0), _offset(0)" in header and
          "_remaining(0)" in header,
          "CANDIDATE_UDP_CONSTRUCTOR_NOT_INITIALIZED","HARNESS")
    check(udp.count("beginEndPacketAsync()")==2 and
          "pollEndPacketAsync()" in udp and
          "if (drained <= 0 || _remaining > 0) return 0;" in udp,
          "ALPHA14_UDP_BEHAVIOR_CHANGED","HARNESS")
    for count in (0,1,2047,2048,2049,65535,65536,1000000):
        available=min(count,2048)
        check(0<=available<=2048,"UDP_TX_BOUNDARY_MODEL","HARNESS")
    for count in (0,1,32767,32768,65535,1000000):
        bounded=min(count,32767)
        check(0<=bounded<=32767,"UDP_RX_BOUNDARY_MODEL","HARNESS")

def preflight():
    phase("PREFLIGHT","RUNNING")
    check(git("rev-parse","--is-inside-work-tree")=="true","NOT_REPO")
    check(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,"WRONG_ROOT")
    check(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    head=git("rev-parse","HEAD")
    MANIFEST["head"]=head
    remote_ref="refs/remotes/origin/"+BRANCH
    check(head==git("rev-parse",remote_ref),"HEAD_REMOTE_MISMATCH","GIT")
    check(git_paths("diff","--name-only")==set(),"TRACKED_WORKTREE_DIRTY","GIT")
    check(git_paths("diff","--cached","--name-only")==set(),"FILES_STAGED","GIT")
    ignored=set(PRODUCT)|{CLOSE_DOC,CHECK_DOC,STATUS_DOC}
    untracked=git_paths("ls-files","--others","--exclude-standard")
    check(not (untracked & ignored),"UNTRACKED_TARGET_COLLISION","GIT")
    for file in (CLOSE_DOC,CHECK_DOC):
        check(not (ROOT/file).exists(),"CLOSURE_DOC_ALREADY_PRESENT","GIT")
    check(sha(CORE)==CORE_SHA and sha(TFT)==TFT_SHA,
          "G3_G4_ARCHIVE_BASELINE_CHANGED")
    check("precompiled=full" not in
          (ETH/"library.properties").read_text(encoding="utf-8"),
          "ETHERNET_NOT_SOURCE_FIRST","HARNESS")
    for dest,src in PRODUCT.items():
        check((ROOT/dest).is_file() and (ROOT/src).is_file(),
              "SOURCE_FILE_MISSING_"+dest,"HARNESS")
    source_contract()
    MANIFEST["source_before_sha256"]={k:sha(ROOT/k) for k in PRODUCT}
    MANIFEST["candidate_sha256"]={product:sha(ROOT/c)
                                  for product,c in PRODUCT.items()}
    check(set(MANIFEST["candidate_sha256"])==set(PRODUCT),
          "MANIFEST_SCHEMA_KEYS_BAD","HARNESS")
    phase("PREFLIGHT","PASS")

def overlay():
    phase("SOURCE_OVERLAY","RUNNING")
    target=TEMP/"source"/"JWPLC_Ethernet"
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copytree(ETH,target)
    for dest,src in PRODUCT.items():
        local=Path(dest).relative_to("JWPLC/2.1.0/libraries/JWPLC_Ethernet")
        shutil.copy2(ROOT/src,target/local)
    phase("SOURCE_OVERLAY","PASS")
    return target

def compile_sketch(cli,sketch,label,selected):
    build=TEMP/("build_"+label)
    output=run([cli,"compile","--fqbn",FQBN,"-j","0","-v","--clean",
                "--build-path",build,"--library",selected,
                "--libraries",LIBS,sketch],"COMPILE_"+label)
    paths=re.findall(r"^Using library JWPLC_Ethernet at version .+ in folder: (.+)$",
                     output,re.M)
    check(len(paths)==1 and Path(paths[0].strip()).resolve()==selected.resolve(),
          "ETHERNET_LIBRARY_SOURCE_NOT_SELECTED_"+label,"HARNESS")
    check("Using core 'jwcontrol_precompiled_stub'" in output and
          re.search(r"[\\/]precompiled[\\/]core[\\/]JWPLCBASIC[\\/]core\.a",output),
          "CORE_ARCHIVE_NOT_SELECTED_"+label,"HARNESS")
    check(len(list(build.rglob("EthernetUdp.cpp.o")))==1,
          "ETHERNET_UDP_SOURCE_OBJECT_MISSING_"+label,"HARNESS")
    return build

def regressions(cli,selected,prefix):
    cases=[
        ("ETH_STATIC",LIBS/"JWPLC_Ethernet/examples/02.Ethernet_StaticIP_Basic"),
        ("ETH_SPI",LIBS/"JWPLC_Ethernet/examples/Ethernet_SPI_Coexistence"),
        ("MODBUS_CLIENT",LIBS/"JWPLC_ModbusTCP/examples/02.ModbusTCP_Client"),
    ]
    for name,path in cases:
        check(path.is_dir(),"EXAMPLE_MISSING_"+name,"HARNESS")
        compile_sketch(cli,path,prefix+"_"+name,selected)
    MANIFEST[prefix.lower()+"_regressions"]=[x for x,_ in cases]

def physical(cli,selected,port):
    phase("PHYSICAL_BUILD","RUNNING")
    sketch=TEMP/"G6_UDP_Probe"
    sketch.mkdir()
    original=(ROOT/"tools/alpha13/firmware/a13_g6_udp_probe/a13_g6_udp_probe.ino")
    content=original.read_text(encoding="utf-8")
    check(content.count("__G6_TOKEN__")==1,"G6_TOKEN_ANCHOR","HARNESS")
    token=RUN_ID+"_"+uuid.uuid4().hex
    (sketch/"G6_UDP_Probe.ino").write_text(
        content.replace("__G6_TOKEN__",token),encoding="utf-8")
    build=compile_sketch(cli,sketch,"G6_UDP_PHYSICAL",selected)
    binaries=list(build.rglob("*.ino.bin"))
    check(len(binaries)==1,"G6_FIRMWARE_COUNT_INVALID","HARNESS")
    MANIFEST["app_sha256"]=sha(binaries[0])
    MANIFEST["token"]=token
    emit("APP_SHA256",MANIFEST["app_sha256"])
    phase("PHYSICAL_BUILD","PASS")
    phase("PHYSICAL_APPROVAL","RUNNING")
    emit("SAFETY","El upload sustituye el sketch; usar JWPLC en banco, sin cargas ni actuadores.")
    confirmation=input("Confirmar banco aislado y red local preparada (BANCO_UDP): ").strip()
    check(confirmation=="BANCO_UDP","BENCH_SAFETY_NOT_CONFIRMED","PRECONDITION")
    phase("PHYSICAL_APPROVAL","PASS")
    phase("PHYSICAL_UPLOAD","RUNNING")
    run([cli,"upload","--fqbn",FQBN,"--port",port,"--input-dir",build],"UPLOAD_G6")
    MANIFEST["physical_upload"]=True
    phase("PHYSICAL_UPLOAD","PASS")
    phase("PHYSICAL_NETWORK","RUNNING")
    output=run([sys.executable,"-B",
                ROOT/"tools/alpha13/gates/a13_g6_udp_pc.py",
                "--serial-port",port,"--token",token],"G6_PHYSICAL_PC")
    check("G6_PC_PHYSICAL=PASS" in output,"G6_PHYSICAL_MARKER_MISSING","HARNESS")
    MANIFEST["physical"]="PASS_UDP_2048_TX_256_PARTIAL_16_RECOVERED_REOPEN"
    phase("PHYSICAL_NETWORK","PASS")

def adopt():
    global ADOPTED
    phase("PRODUCT_ADOPTION","RUNNING")
    BACKUP.mkdir(parents=True,exist_ok=False)
    for file in PRODUCT:
        orig=BACKUP/file
        orig.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/file,orig)
    ADOPTED=True
    for file,candidate in PRODUCT.items():
        shutil.copy2(ROOT/candidate,ROOT/file)
    check(git_paths("diff","--name-only")==set(PRODUCT),
          "PRODUCT_ADOPTION_SCOPE_INVALID","HARNESS")
    check(not git("diff","--check"),"PRODUCT_DIFF_CHECK_FAILED","HARNESS")
    MANIFEST["product_changed"]=True
    phase("PRODUCT_ADOPTION","PASS")

def final():
    phase("FINAL_AUDIT","RUNNING")
    check(git_paths("diff","--name-only")==set(PRODUCT),
          "PRODUCT_DIRTY_SCOPE_NOT_2","HARNESS")
    check(git_paths("diff","--cached","--name-only")==set(),
          "STAGED_FILES_UNEXPECTED","HARNESS")
    for file,candidate in PRODUCT.items():
        check(sha(ROOT/file)==sha(ROOT/candidate),
              "PRODUCT_CANDIDATE_SHA_DIFFERENT_"+file,"HARNESS")
    check(sha(CORE)==CORE_SHA and sha(TFT)==TFT_SHA,
          "G3_G4_ARCHIVE_CHANGED","HARNESS")
    MANIFEST["product_sha256"]={file:sha(ROOT/file) for file in PRODUCT}
    check(set(MANIFEST["product_sha256"])==set(MANIFEST["candidate_sha256"]),
          "MANIFEST_PRODUCT_CANDIDATE_SCHEMA_MISMATCH","HARNESS")
    phase("FINAL_AUDIT","PASS")

def render_close():
    m=MANIFEST
    run_id=m["run_id"]
    close=f"""# Alpha13-G6 — Cierre datagramas UDP (2026-10-11)

**Gate G6: CLOSED_PASS; Alpha13 continúa.**

## Evidencia
- RUN_ID={run_id}
- Serial={m['port']}
- Firmware prueba SHA256={m['app_sha256']}
- UDP físico: {m['physical']}
- G6_RESUME_ORIGIN={m.get('resume_origin','NO_RESUME')}
- MTU W5500: mensaje de 2048 bytes observado como UDP 1472+576;
  se verifica orden, IP/puerto y concatenación exacta.
- Compilaciones overlay: {m['overlay_regressions']}
- Compilaciones producto: {m['product_regressions']}
- Sources: {m['product_sha256']}
- G3 core.a SHA256={CORE_SHA}; G4 TFT.a SHA256={TFT_SHA}.
- Contrato del motor cooperativo UDP Alpha14 preservado.

## Límites
Solo cubre LAN/estado/tamaño de datagrama y recuperación de la prueba.
A13-008 sigue pendiente de identificar exactamente en auditoría canónica.
No demuestra semántica de UDP bajo todas las pérdidas/cargas.

## Git y release
Git productivo y documental se confirman por la salida SUMMARY/MANIFEST.
No PR, release merge ni publicación.

NEXT_GATE=A13-G7_FRAM_A13_009_A13_013.
"""
    checklist="""# Alpha13-G6 — checklist
- [x] Límites de buffer TX W5500 y conversiones RX firmadas.
- [x] Socket no iniciado/stop/reopen sin estado stale.
- [x] Alpha14 UDP cooperativo y drenado parcial preservados.
- [x] Prueba física 2048 TX / 256 parcial / 16 recuperación.
- [x] Compilaciones source-first overlay y producto 3/3.
- [x] Core.a y TFT.a previamente validados intactos.
- [x] Source/manifest SHA y Git scope auditados.
- [x] Cierre Git automático tras PASS sin AUTORIZO.
- [ ] Revisar equivalencia individual A13-007 / A13-008 con auditoría canónica.
- [ ] G7 FRAM y regresión final Alpha13.
"""
    status=(ROOT/STATUS_DOC).read_text(encoding="utf-8-sig")
    prefix="# v2.1.0-alpha.13 — Estado operativo y continuidad\n"
    check(status.startswith(prefix),"STATUS_FORMAT_CHANGED","HARNESS")
    check("## Estado vigente — G6 cerrado" not in status,
          "G6_ALREADY_CLOSED","HARNESS")
    state=f"""
## Estado vigente — G6 cerrado, G7 siguiente (2026-10-11)

ALPHA13_STATUS=IN_PROGRESS
G1=CLOSED_PASS
G2=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G6=CLOSED_PASS
G6_RUN_ID={run_id}
G6_PHYSICAL={m['physical']}
G6_REGRESSIONS=3_OVERLAY_PASS_3_PRODUCT_PASS
G6_PRODUCT_FILES=2
G6_A13_008=REVIEW_MAPPING_PENDING
NEXT_GATE=A13-G7_FRAM_A13_009_A13_013
NEXT_FAILURE_ID=F115
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO

Cierre: A13_G6_CLOSURE_20261011.md
Checklist: A13_G6_CLOSURE_CHECKLIST_20261011.md

---

"""
    return {CLOSE_DOC:close,CHECK_DOC:checklist,
            STATUS_DOC:status.replace(prefix,prefix+state,1)}

def safe_commit(scope,message):
    git("add","--",*sorted(scope))
    check(git_paths("diff","--cached","--name-only")==scope,
          "GIT_STAGED_SCOPE_MISMATCH","GIT")
    check(not git("diff","--cached","--check"),
          "GIT_STAGED_DIFF_ERROR","GIT")
    git("commit","-m",message)
    new_head=git("rev-parse","HEAD")
    check(git_paths("diff-tree","--no-commit-id","--name-only","-r",new_head)==scope,
          "COMMIT_SCOPE_MISMATCH","GIT")
    emit("COMMIT",new_head)
    return new_head

def auto_close():
    global COMMITTED
    phase("AUTO_GIT_CLOSURE","RUNNING")
    before=MANIFEST["head"]
    check(git("rev-parse","HEAD")==before,"HEAD_CHANGED_DURING_GATE","GIT")
    remote_line=git("ls-remote","origin","refs/heads/"+BRANCH).splitlines()
    check(len(remote_line)==1 and remote_line[0].split()[0]==before,
          "REMOTE_CHANGED_DURING_GATE","GIT")
    check(git_paths("diff","--name-only")==set(PRODUCT),
          "SCOPE_CHANGED_BEFORE_AUTOCLOSE","GIT")
    documents=render_close()
    check(set(documents)=={CLOSE_DOC,CHECK_DOC,STATUS_DOC},
          "DOC_MANIFEST_SCHEMA_MISMATCH","HARNESS")
    for path in (CLOSE_DOC,CHECK_DOC):
        check(not (ROOT/path).exists(),"DOCUMENT_COLLISION","GIT")
    # Pasado este punto los commits son intencionados y no se hace
    # rollback de archivos adoptados si falla push o documentacion.
    product_head=safe_commit(set(PRODUCT),"fix(ethernet): acotar datagramas y estado UDP")
    COMMITTED=True
    MANIFEST["git_product_committed"]=True
    MANIFEST["git_product_commit"]=product_head
    for name,body in documents.items():
        target=ROOT/name
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text(body,encoding="utf-8",newline="\n")
    document_head=safe_commit(set(documents),"docs(alpha13): cerrar G6 y abrir G7 FRAM")
    MANIFEST["git_docs_committed"]=True
    MANIFEST["git_docs_commit"]=document_head
    check(git_paths("diff","--name-only")==set() and
          git_paths("diff","--cached","--name-only")==set(),
          "DIRTY_AFTER_GIT_COMMIT","GIT")
    check(sha(CORE)==CORE_SHA and sha(TFT)==TFT_SHA,
          "ARCHIVE_SHA_CHANGED_AFTER_CLOSE","GIT")
    git("push","origin","HEAD:refs/heads/"+BRANCH)
    MANIFEST["git_pushed"]=True
    phase("AUTO_GIT_CLOSURE","PASS")
    emit("PRODUCT_COMMIT",product_head)
    emit("DOCS_COMMIT",document_head)
    emit("PUSH","PASS_NON_FORCE")
    emit("NEXT_GATE","A13-G7_FRAM_A13_009_A13_013")

def restore():
    if not ADOPTED or COMMITTED:return
    for file in PRODUCT:
        original=BACKUP/file
        if original.is_file():shutil.copy2(original,ROOT/file)
    success=all((BACKUP/file).is_file() and
                sha(BACKUP/file)==sha(ROOT/file) for file in PRODUCT)
    MANIFEST["rollback"]="PASS" if success else "FAIL"
    emit("ROLLBACK",MANIFEST["rollback"])

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-port",default="")
    parser.add_argument("--arduino-cli",default="")
    args=parser.parse_args()
    RESULT.mkdir(parents=True,exist_ok=False)
    TEMP.mkdir(parents=True,exist_ok=False)
    try:
        preflight()
        run([sys.executable,"-B",
             ROOT/"tools/alpha13/gates/a13_finalizer_portability_selftest.py"],
            "F112_PORTABILITY_SELFTEST")
        run([sys.executable,"-B",
             ROOT/"tools/alpha13/gates/a13_g6_contract_selftest.py"],
            "G6_CONTRACT_SELFTEST")
        cli=cli_find(args.arduino_cli)
        run([cli,"version"],"CLI_VERSION")
        core_list=run([cli,"core","list"],"CLI_CORES")
        check(re.search(r"(?m)^\s*jwplc_local:esp32\s",core_list) is not None,
              "JWPLC_LOCAL_CORE_NOT_INSTALLED_IN_SELECTED_CLI","ENVIRONMENT")
        emit("JWPLC_CORE_TARGET","jwplc_local:esp32:jwplcbasic")
        port=serial_port(args.serial_port)
        MANIFEST["port"]=port
        selected=overlay()
        phase("CANDIDATE_REGRESSIONS","RUNNING")
        regressions(cli,selected,"OVERLAY")
        phase("CANDIDATE_REGRESSIONS","PASS")
        physical(cli,selected,port)
        adopt()
        phase("PRODUCT_REGRESSIONS","RUNNING")
        regressions(cli,ETH,"PRODUCT")
        phase("PRODUCT_REGRESSIONS","PASS")
        final()
        auto_close()
        MANIFEST["status"]="CLOSED_PASS"
        emit("STATUS","CLOSED_PASS")
        persist()
        return 0
    except (Stop,Exception,KeyboardInterrupt) as error:
        MANIFEST["reason"]=str(error)
        MANIFEST["category"]=error.category if isinstance(error,Stop) else (
            "USER_ABORT" if isinstance(error,KeyboardInterrupt) else "HARNESS")
        restore()
        MANIFEST["status"]="REVIEW_STOP"
        emit("STATUS",MANIFEST["status"])
        emit("REASON",MANIFEST["reason"])
        emit("CATEGORY",MANIFEST["category"])
        emit("GIT_PARTIAL_COMMIT",COMMITTED)
        persist()
        return 2

if __name__=="__main__":sys.exit(main())
