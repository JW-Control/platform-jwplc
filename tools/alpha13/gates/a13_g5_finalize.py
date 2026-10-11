#!/usr/bin/env python3
"""A13-G5: cierre único de producto TCP + documentación, sin repetir pruebas."""
from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT=Path.cwd().resolve()
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
BASE="ab269503fd25b942faecc9256b7bf35dc5ccbc03"
KNOWN_TOOLING=[
    "79116d90b7fe2fb37b68e8a30c678b1cfaf288ab",
    "3c178f66f60ee0578b83a48c552582135ebccc1a",
    "fb96571166f6e5ee546a82ee0f149d0eaa675beb",
    "f18c4b43d09b77a2132ae0333cacb19826f3f8e0",
]
TOOLING={
    "tools/alpha13/gates/a13_g5_finalize.py",
    "docs/v2.1.0-alpha.13/A13_G5_FINALIZE_PLAN_20261011.md",
}
PREFIX="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
PRODUCT={
    PREFIX+"EthernetClient.cpp":"tools/alpha13/candidates/g5/EthernetClient.cpp",
    PREFIX+"EthernetServer.cpp":"tools/alpha13/candidates/g5/EthernetServer.cpp",
}
CORE="JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
CORE_SHA="a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d"
TFT="JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a"
TFT_SHA="4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc"
DOC="docs/v2.1.0-alpha.13/"
STATUS=DOC+"ALPHA13_STATUS.md"
REPORT=DOC+"A13_G5_CLOSURE_20261011.md"
CHECKLIST=DOC+"A13_G5_CLOSURE_CHECKLIST_20261011.md"
RUN=None
MESSAGES=[]

class Review(Exception):
    pass

def need(test,why):
    if not test:
        raise Review(why)

def note(line):
    value=str(line)
    print(value,flush=True)
    MESSAGES.append(value)
    if RUN is not None:
        (RUN/"G5_FINALIZE_GIT_SUMMARY.log").write_text(
            "\n".join(MESSAGES)+"\n",encoding="utf-8")

def cmd(*args):
    out=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                       stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if out.returncode != 0:
        raise Review("GIT_COMMAND_FAILED "+repr(args)+" "+
                     out.stderr.decode("utf-8","replace")[-600:])
    return out.stdout.decode("utf-8","replace").strip()

def paths(*args):
    # -z debe ir antes de "--" y de los pathspecs para que Git
    # no lo interprete como un nombre de archivo literal.
    normalized=list(args)
    normalized.insert(normalized.index("--") if "--" in normalized else len(normalized),"-z")
    out=subprocess.run(["git","-c","core.quotepath=false",*normalized],
                       cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    need(out.returncode==0,"GIT_PATH_QUERY_FAILED "+repr(args))
    return set(filter(None,out.stdout.decode("utf-8","replace").split("\0")))

def sha(path):
    digest=hashlib.sha256()
    with (ROOT/path).open("rb") as stream:
        for chunk in iter(lambda:stream.read(1<<20),b""):
            digest.update(chunk)
    return digest.hexdigest()

def original_backup_sha(run_id,path):
    # G5 generó un backup en TEMP antes de adoptar las fuentes. Si todavía
    # existe, verificar sus bytes exactos con el SHA del manifest; Git puede
    # contener LF mientras el checkout en Windows conserva CRLF (F112).
    import tempfile
    backup=(Path(tempfile.gettempdir())/("jwplc_a13_g5_"+run_id)/
            "product_backups"/path)
    if not backup.is_file():
        return None
    digest=hashlib.sha256()
    with backup.open("rb") as stream:
        for chunk in iter(lambda:stream.read(1<<20),b""):
            digest.update(chunk)
    return digest.hexdigest()

def preflight():
    need(cmd("rev-parse","--is-inside-work-tree")=="true","NOT_A_REPOSITORY")
    need(Path(cmd("rev-parse","--show-toplevel")).resolve()==ROOT,"WRONG_ROOT")
    need(cmd("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    need(bool(re.search(r"github\.com[:/]JW-Control/platform-jwplc(?:\.git)?$",
                        cmd("remote","get-url","origin"),re.I)),"WRONG_ORIGIN")
    local=cmd("rev-parse","HEAD")
    remote=cmd("rev-parse","refs/remotes/origin/"+BRANCH)
    expected_lineage=KNOWN_TOOLING+[remote]
    need(local in ([BASE]+expected_lineage),"LOCAL_HEAD_UNEXPECTED "+local)
    lineage=cmd("rev-list","--reverse",BASE+".."+remote).splitlines()
    need(lineage==expected_lineage,
         "REMOTE_HISTORY_NOT_EXACT_5_TOOLING_COMMITS")
    parent=BASE
    for child in expected_lineage:
        need(cmd("rev-list","--parents","-n","1",child).split()==
             [child,parent],"TOOLING_PARENT_CHANGED_"+child)
        parent=child
    need(paths("diff","--name-only",BASE,remote)==TOOLING,
         "REMOTE_TOOLING_SCOPE_UNEXPECTED")
    need(paths("diff","--name-only")==set(PRODUCT),"PRODUCT_DIRTY_SCOPE_NOT_2")
    need(not paths("diff","--cached","--name-only"),"FILES_ALREADY_STAGED")
    need(not paths("ls-files","--others","--exclude-standard"),"UNTRACKED_FILES")
    need(not cmd("diff","--check"),"PRODUCT_DIFF_WHITESPACE")
    for op in ("MERGE_HEAD","CHERRY_PICK_HEAD","REBASE_HEAD"):
        loc=Path(cmd("rev-parse","--git-path",op))
        need(not (loc if loc.is_absolute() else ROOT/loc).exists(),
             "GIT_OPERATION_IN_PROGRESS_"+op)
    cmd("var","GIT_AUTHOR_IDENT")
    cmd("var","GIT_COMMITTER_IDENT")
    for name in TOOLING:
        path=ROOT/name
        if local==BASE:
            need(not path.exists(),"TOOLING_COLLISION "+name)
        else:
            need(path.is_file(),"TOOLING_NOT_PRESENT "+name)
            need(not paths("diff","--name-only","HEAD","--",name),
                 "TOOLING_WORKTREE_CHANGED "+name)
            need(not paths("diff","--cached","--name-only","HEAD","--",name),
                 "TOOLING_STAGED_CHANGED "+name)
            need(cmd("rev-parse",":"+name)==cmd("rev-parse","HEAD:"+name),
                 "TOOLING_INDEX_DIFFERS "+name)
    # El launcher obtiene a %TEMP% el blob del commit remoto, sin conversion
    # de EOL. Evitar las falsas discrepancias Windows detectadas en F112.
    runner=Path(__file__).read_bytes()
    src=subprocess.run(["git","show",remote+":tools/alpha13/gates/a13_g5_finalize.py"],
                       cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    need(src.returncode==0 and src.stdout==runner,
         "RUNNER_BYTES_DO_NOT_MATCH_REMOTE")
    note("LOCAL_HEAD="+local)
    note("REMOTE_HEAD="+remote)
    note("GIT_PREFLIGHT=PASS")
    return local,remote

def product():
    for dst,src in PRODUCT.items():
        need((ROOT/dst).is_file() and (ROOT/src).is_file(),
             "PRODUCT_FILE_MISSING "+dst)
        need(sha(dst)==sha(src),"PRODUCT_DIFFERS_FROM_G5_CANDIDATE "+dst)
    for path,expected in ((CORE,CORE_SHA),(TFT,TFT_SHA)):
        need((ROOT/path).is_file() and sha(path)==expected,
             "G3_OR_G4_ARCHIVE_CHANGED "+path)
    note("PRODUCT=PASS_2_ETHERNET_SOURCES_AND_G3_G4_ARCHIVES")

def evidence():
    global RUN
    found=[]
    for file in (ROOT/"tools/alpha13/results").glob("g5_integrated_*/MANIFEST.json"):
        try:
            m=json.loads(file.read_text(encoding="utf-8-sig"))
        except (OSError,ValueError):
            continue
        if (m.get("hito")=="A13-G5" and
            m.get("head")==BASE and
            m.get("status")=="PASS_PHYSICAL_NORMAL_SOURCE"):
            found.append((file.parent,m))
    need(len(found)==1,"G5_PASS_MANIFEST_COUNT="+str(len(found)))
    RUN,m=found[0]
    need(RUN.name=="g5_integrated_"+m.get("run_id",""),"RUN_ID_DIRECTORY_MISMATCH")
    need(m.get("product_changed") is True and
         m.get("commit_executed") is False and
         m.get("release_published") is False and
         m.get("upload_executed") is True,
         "G5_MANIFEST_PRODUCT_OR_UPLOAD_FLAGS")
    need(m.get("port")=="COM4","G5_PORT_NOT_EXPECTED")
    need(m.get("app_sha256")==
         "f9318bdbcdd3e5250aded615be6f52a6efc8de9ef7d393e8b5b356978ddcfe76",
         "G5_APP_SHA_MISMATCH")
    need(m.get("physical")=="PASS_5000_TX_128_RX_64_SERVER",
         "G5_PHYSICAL_CONTRACT_NOT_PASS")
    expected_reg={"ETH_STATIC","ETH_SPI","MODBUS_CLIENT"}
    need(set(m.get("preview_regressions",[]))==expected_reg and
         set(m.get("regressions",[]))==expected_reg,
         "G5_REGRESSIONS_NOT_3_OF_3")
    for phase in ("PREFLIGHT","SOURCE_OVERLAY","CANDIDATE_BUILD",
                  "PHYSICAL_BUILD","PHYSICAL_APPROVAL","PHYSICAL_UPLOAD",
                  "PHYSICAL_NETWORK","PRODUCT_ADOPTION",
                  "PRODUCT_REGRESSIONS","FINAL_AUDIT"):
        need(m.get("phase",{}).get(phase)=="PASS",
             "G5_PHASE_NOT_PASS_"+phase)
    token=m.get("token","")
    need(isinstance(token,str) and token.startswith(m["run_id"]+"_"),
         "G5_RUNTIME_TOKEN_MISSING")
    need(m.get("candidate_sha256",{}).keys() ==
         {src for src in PRODUCT.values()},
         "G5_CANDIDATE_SET_CHANGED")
    for dest,src in PRODUCT.items():
        need(m["candidate_sha256"][src]==sha(src),
             "G5_MANIFEST_CANDIDATE_SHA "+dest)
        need(m.get("product_sha256",{}).get(dest)==sha(dest),
             "G5_MANIFEST_PRODUCT_SHA "+dest)
        original=m.get("original_sha256",{}).get(dest)
        need(isinstance(original,str) and
             re.fullmatch("[a-f0-9]{64}",original) is not None,
             "G5_ORIGINAL_SHA_MISSING "+dest)
        backup=original_backup_sha(m["run_id"],dest)
        if backup is not None:
            need(original==backup,"G5_ORIGINAL_BACKUP_SHA_DIFFERENT "+dest)
    log_names=["F112_PORTABILITY_SELFTEST","CLI_VERSION",
               "COMPILE_OVERLAY_ETH_STATIC","COMPILE_OVERLAY_ETH_SPI",
               "COMPILE_OVERLAY_MODBUS_CLIENT","COMPILE_G5_PHYSICAL",
               "UPLOAD_G5","PHYSICAL_G5_PC","COMPILE_PRODUCT_ETH_STATIC",
               "COMPILE_PRODUCT_ETH_SPI","COMPILE_PRODUCT_MODBUS_CLIENT"]
    for name in log_names:
        path=RUN/(name+".log")
        need(path.is_file() and "\nEXIT=0\n" in
             path.read_text(encoding="utf-8-sig")[:1400],
             "G5_LOG_MISSING_OR_FAILED_"+name)
    summary=RUN/"SUMMARY.log"
    need(summary.is_file() and "STATUS=PASS_PHYSICAL_NORMAL_SOURCE" in
         summary.read_text(encoding="utf-8-sig"),
         "G5_SUMMARY_NOT_PASS")
    physical=(RUN/"PHYSICAL_G5_PC.log").read_text(encoding="utf-8-sig")
    markers=[
        "G5_PC_PHYSICAL=PASS",
        "G5_SERIAL_PASS=PASS",
        "G5_SERIAL_ERRORS_ZERO=PASS",
        "G5_SERIAL_TX_COMPLETE=PASS",
        "G5_SERIAL_RX_COMPLETE=PASS",
        "G5_SERIAL_SERVER_COMPLETE=PASS",
        "G5_SERVER_EMPTY_ZERO=PASS",
        "G5_PC_RECEIVED_EXACT_5000=PASS",
        "G5_PC_RECEIVED_SERVER_64=PASS",
        "G5_DONE="+token,
        "G5_RESULT=PASS",
        "G5_ERRORS=0",
        "G5_TX_BYTES=5000",
        "G5_RX_BYTES=128",
        "G5_SERVER_BYTES=64",
        "G5_NO_CLIENT_BYTES=0",
    ]
    for marker in markers:
        need(marker in physical,"G5_TCP_EVIDENCE_MISSING_"+marker)
    note("EVIDENCE=PASS_TCP_5000_TX_128_RX_64_SERVER_3_REGRESSIONS")
    note("RUN_ID="+m["run_id"])
    note("APP_SHA256="+m["app_sha256"])
    return m

def commit(scope,message):
    cmd("add","--",*sorted(scope))
    need(paths("diff","--cached","--name-only")==scope,
         "UNEXPECTED_STAGED_FILES")
    need(not cmd("diff","--cached","--check"),"GIT_STAGED_DIFF_CHECK")
    cmd("commit","-m",message)
    head=cmd("rev-parse","HEAD")
    need(paths("diff-tree","--no-commit-id","--name-only","-r",head)==scope,
         "PRODUCT_COMMIT_SCOPE_MISMATCH")
    note("COMMIT="+head+" FILES="+str(len(scope))+" MESSAGE="+message)
    return head

def document(m,tool,product_commit,merge):
    report=f"""# Alpha13-G5 — Cierre de corrección TCP
Fecha: 2026-10-11. Estado: CLOSED_PASS de G5; Alpha13 continúa.

## Evidencia física y de compilación
- Run ID: {m['run_id']}.
- Manifiesto local: tools/alpha13/results/g5_integrated_{m['run_id']}/MANIFEST.json.
- Puerto serial {m['port']}; token: {m['token']}.
- Firmware temporal SHA256 {m['app_sha256']}.
- TX cliente: 5000 bytes, patrón validado de extremo a extremo.
- RX cliente: 128 bytes solicitando read(buf, 32768).
- Servidor: 64 bytes recibidos por PC; sin clientes retorna 0.
- PC y serial: PASS; Arduino overlay 3/3; Arduino con producto 3/3.
- F112: selftest de portabilidad PASS.
- G3 core.a SHA256 {CORE_SHA} y G4 TFT.a SHA256 {TFT_SHA} intactos.

## Correcciones de producto
- EthernetClient.cpp: fragmentación de write, tiempo global y retorno honesto.
- EthernetClient.cpp: acotación de read a INT16_MAX.
- EthernetServer.cpp: retorno real de broadcast y cero sin clientes.
- API Arduino y autoload mantenidos. Ethernet compilado desde fuente (no archive).
- No se modificó software OpenPLC, HMI Designer ni funcionalidades TFT.

## Observaciones y límites
- El test cubre una transferencia normal; no prueba exhaustivamente
  backpressure, múltiples peers, agotamiento de timeout ni todos los fallos.
- La correspondencia uno a uno con A13-005/A13-006 del informe original
  todavía requiere consulta de ese informe; no atribuir hallazgos inventados.
- El sketch temporal subido no restaura el firmware previo del PLC.
- El aviso VS Code CMake/ESP-IDF observado por el operador fue externo
  a Arduino CLI y no motivó cambios en CMakeLists.txt.

## Commits
TOOLING_COMMIT={tool}
PRODUCT_COMMIT={product_commit}
SYNC_MERGE_COMMIT={merge}
RELEASE_MERGE=NO
PR=NO
PUBLISH=NO

NEXT_GATE=A13-G6_DATAGRAM_ROBUSTNESS_A13_007_A13_008_REVIEW.
"""
    checklist="""# Alpha13 — Checklist de cierre G5
- [x] Fuente Ethernet previa protegida por hashes y respaldos temporales.
- [x] Escrituras TCP divididas en fragmentos de tamaño W5500.
- [x] Lectura TCP con límite INT16_MAX.
- [x] Escritura del servidor devuelve cero sin cliente.
- [x] Test PC ↔ JWPLC: 5000 TX / 128 RX / 64 servidor.
- [x] Regresiones de compilación overlay 3/3 y normales 3/3.
- [x] F112 self-test Python/Git ejecutado.
- [x] Solo dos fuentes TCP versionadas en commit productivo.
- [x] Archivos core.a/TFT.a de G3/G4 sin cambios.
- [x] Cierre documental sin publicación ni PR.
- [ ] Consultar informe canónico para mapping individual A13-005/A13-006.
- [ ] G6 datagram robustness (A13-007 y revisión A13-008).
- [ ] Regresión final CI/Arduino IDE y publicación Alpha13 en español.
"""
    original=(ROOT/STATUS).read_text(encoding="utf-8-sig")
    marker="# v2.1.0-alpha.13 — Estado operativo y continuidad\n"
    need(original.startswith(marker),"STATUS_HEADER_NOT_EXPECTED")
    need("## Estado vigente — G5 cerrado" not in original,
         "G5_ALREADY_MARKED_CLOSED")
    pre=f"""
## Estado vigente — G5 cerrado y G6 siguiente (2026-10-11)

El bloque es el estado vigente; los antiguos bloques G4/G5 preparado
permanecen como histórico.

ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G5=CLOSED_PASS
G5_RUN_ID={m['run_id']}
G5_PHYSICAL=PASS_TCP_5000_TX_128_RX_64_SERVER
G5_REGRESSIONS=3_OF_3_OVERLAY_AND_3_OF_3_PRODUCT
G5_PRODUCT_FILES=2
G5_PRODUCT_COMMIT={product_commit}
G5_TOOLING_COMMIT={tool}
G5_SYNC_MERGE_COMMIT={merge}
F112=CLOSED_PREVENTION_REGISTERED
NEXT_FAILURE_ID=F113
NEXT_GATE=A13-G6_DATAGRAM_ROBUSTNESS_A13_007_A13_008_REVIEW
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE

Cierre: A13_G5_CLOSURE_20261011.md.
Checklist: A13_G5_CLOSURE_CHECKLIST_20261011.md.
Alpha13 todavía no tiene merge release, PR ni PreRelease.

---

"""
    changes={REPORT:report,CHECKLIST:checklist,
             STATUS:original.replace(marker,marker+pre,1)}
    for path,text in changes.items():
        if path in (REPORT,CHECKLIST):
            need(not (ROOT/path).exists(),"G5_DOC_ALREADY_EXISTS "+path)
        (ROOT/path).write_text(text,encoding="utf-8",newline="\n")
    note("DOCUMENTS=3_FILES_CREATED_OR_UPDATED")
    return set(changes)

def main():
    try:
        note("HITO=A13_G5_FINALIZE")
        local,remote=preflight()
        product()
        m=evidence()
        product()
        note("READY_FOR_OPERATOR_AUTHORIZATION=YES")
        approval=input("Autorizar commits/push G5 (AUTORIZO_COMMIT_G5): ").strip()
        need(approval=="AUTORIZO_COMMIT_G5","NOT_AUTHORIZED")
        prod=commit(set(PRODUCT),"fix(ethernet): corregir límites y retornos TCP")
        product()
        need(not paths("diff","--name-only"),"PRODUCT_DIRTY_AFTER_COMMIT")
        if local==remote:
            merged="NOT_REQUIRED"
        else:
            cmd("merge","--no-ff","-m",
                "chore(alpha13): integrar tooling de cierre G5",
                "refs/remotes/origin/"+BRANCH)
            merged=cmd("rev-parse","HEAD")
        need(not paths("diff","--name-only") and
             not paths("diff","--cached","--name-only"),
             "TREE_DIRTY_AFTER_TOOLING_INTEGRATION")
        product()
        note("SYNC_MERGE="+merged)
        docs=document(m,remote,prod,merged)
        doccommit=commit(docs,"docs(alpha13): cerrar G5 y preparar G6")
        product()
        need(not paths("diff","--name-only") and
             not paths("diff","--cached","--name-only") and
             not paths("ls-files","--others","--exclude-standard"),
             "FINAL_WORKTREE_NOT_CLEAN")
        note("PUSH=START_NON_FORCE")
        cmd("push","origin","HEAD:refs/heads/"+BRANCH)
        note("PUSH=PASS_NON_FORCE")
        note("STATUS=CLOSED_PASS")
        note("PRODUCT_COMMIT="+prod)
        note("CLOSURE_COMMIT="+doccommit)
        note("NEXT_GATE=A13-G6_DATAGRAM_ROBUSTNESS_A13_007_A13_008_REVIEW")
        note("RELEASE_MERGE_PR_PUBLICATION=NO")
        return 0
    except (Review,Exception,KeyboardInterrupt) as ex:
        note("STATUS=REVIEW_STOP")
        note("REASON="+str(ex))
        note("NO_RESET_NO_CHECKOUT_NO_CLEAN_NO_BLIND_RETRY=YES")
        return 2

if __name__=="__main__":
    sys.exit(main())
