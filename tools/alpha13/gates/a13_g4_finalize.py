#!/usr/bin/env python3
"""Cierre G4: audita evidencia, registra producto y documentos sin recompilar."""
from __future__ import annotations
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT=Path.cwd().resolve()
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
BASE="bc9e7c938506bd872cbb4d943d58ef9644be6e5d"
TOOLING_HEAD="b15c59cd599a1d5e7dbabc594a38ccdc0c49f791"
HEAD_REPAIR="21a0816c6bfb605de8b1b6efb2ef27d86051ac35"
P="JWPLC/2.1.0/libraries/JWPLC_TFT/src/"
CANDIDATES={
 P+"JWPLC_TFT.cpp":"tools/alpha13/candidates/g4/JWPLC_TFT.cpp",
 P+"JWPLC_TFT.h":"tools/alpha13/candidates/g4/JWPLC_TFT.h"
}
ARCHIVE=P+"esp32/libJWPLC_TFT.a"
EXPECTED_ARCHIVE="4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc"
CORE="JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a"
CORE_SHA="a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d"
DISPLAY="JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a"
DISPLAY_SHA="c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02"
SETUP=P+"tft_setup.h"
SETUP_SHA="8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1"
PRODUCT=set(CANDIDATES)|{ARCHIVE}
DOC="docs/v2.1.0-alpha.13/"
STATUS=DOC+"ALPHA13_STATUS.md"
FAILURES=DOC+"A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md"
REPORT=DOC+"A13_G4_CLOSURE_20261010.md"
CHECK=DOC+"A13_G4_CLOSURE_CHECKLIST_20261010.md"
TOOLING={"tools/alpha13/gates/a13_g4_finalize.py",
         DOC+"A13_G4_FINALIZE_PLAN_20261010.md"}
RUN=None
MESSAGES=[]

class Review(Exception):
    pass
def need(ok,reason):
    if not ok: raise Review(reason)
def say(s):
    s=str(s)
    print(s,flush=True)
    MESSAGES.append(s)
    if RUN is not None:
        (RUN/"G4_FINALIZE_GIT_SUMMARY.log").write_text("\n".join(MESSAGES)+"\n",encoding="utf-8")
def git(*args):
    r=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                     stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if r.returncode: raise Review("GIT_EXIT="+str(r.returncode)+" git "+" ".join(args)+" "+r.stderr.decode("utf-8","replace")[-900:])
    return r.stdout.decode("utf-8","replace").strip()
def setpaths(*args):
    out=subprocess.run(["git","-c","core.quotepath=false",*args,"-z"],cwd=ROOT,
                       stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    need(out.returncode==0,"GIT_LIST_FAILED "+repr(args))
    return set(filter(None,out.stdout.decode("utf-8").split("\0")))
def sha(p):
    h=hashlib.sha256()
    with (ROOT/p).open("rb") as f:
        for chunk in iter(lambda:f.read(1048576),b""): h.update(chunk)
    return h.hexdigest()
def product():
    for dest,src in CANDIDATES.items():
        need((ROOT/dest).is_file() and (ROOT/src).is_file(),"SOURCE_NOT_FOUND "+dest)
        need((ROOT/dest).read_bytes()==(ROOT/src).read_bytes(),"SOURCE_DIFFERS_FROM_CANDIDATE "+dest)
    for file,digest in ((ARCHIVE,EXPECTED_ARCHIVE),(CORE,CORE_SHA),
                        (DISPLAY,DISPLAY_SHA),(SETUP,SETUP_SHA)):
        need((ROOT/file).is_file() and sha(file)==digest,"PRODUCT_SHA_CHANGED "+file)
    say("PRODUCT=PASS_2_SOURCES_ARCHIVE_SHA")
def evidence():
    global RUN
    good=[]
    for f in (ROOT/"tools/alpha13/results").glob("g4_integrated_*/MANIFEST.json"):
        try: m=json.loads(f.read_text(encoding="utf-8-sig"))
        except (ValueError,OSError): continue
        if m.get("hito")=="A13-G4" and m.get("status")=="PASS_PHYSICAL_AND_REBUILT_NORMAL_ARCHIVE" and m.get("head")==BASE and m.get("archive_sha256")==EXPECTED_ARCHIVE:
            good.append((f.parent,m))
    need(len(good)==1,"G4_PASS_MANIFEST_COUNT="+str(len(good)))
    RUN,m=good[0]
    need(m.get("product_changed_by_gate") is True and m.get("product_commit") is False and m.get("release_publish") is False,
         "G4_PRODUCT_COMMIT_EARLY")
    for key in ("PREFLIGHT","BACKEND_RECIPE","SOURCE_REBUILD","ARCHIVE_CREATE",
                "TEMP_ARCHIVE_NORMAL_LINK","PHYSICAL_BUILD","PHYSICAL_APPROVAL",
                "PHYSICAL_UPLOAD","PHYSICAL_SERIAL","VISUAL_OPERATOR",
                "PRODUCT_ADOPTION","PRODUCT_NORMAL_REGRESSION","FINAL_AUDIT"):
        need(m.get("phases",{}).get(key)=="PASS","G4_PHASE_NOT_PASS "+key)
    need(m.get("operator_bench_confirmed") is True,"BENCH_NOT_CONFIRMED")
    need(m.get("visual_operator")=={"white_problem":"NO","tft_stable":"SI"},"G4_VISUAL_NOT_PASS")
    need(set(m.get("regressions",[]))=={"IDLE_STATUS","HMI_FIELDS","IDLE_MODES","LOGIC_UI"},
         "G4_FOUR_REGRESSIONS_NOT_PASS")
    sr=m.get("serial_result",{})
    need(sr.get("trials")==50 and sr.get("errors")==0 and sr.get("batch_final")=="RELEASED",
         "G4_CONCURRENCY_NOT_PASS")
    need(m.get("selected_port")=="COM4" and m.get("vid_pid")=="1A86:7523",
         "PORT_IDENTIFICATION_CHANGED")
    need(m.get("app_sha256")=="ae79545579a86420cc1fc3b767ab4398cb69c77f83da5393af3595b88e3d4ee6",
         "APP_SHA_UNEXPECTED")
    token=m.get("runtime_token","")
    need(isinstance(token,str) and token.startswith(m["run_id"]+"_"),"RUNTIME_TOKEN_INVALID")
    serial=RUN/"serial_g4.log"
    need(serial.is_file(),"SERIAL_LOG_NOT_FOUND")
    lines=serial.read_text(encoding="utf-8-sig").splitlines()
    def vals(key): return [line[len(key)+1:] for line in lines if line.startswith(key+"=")]
    for k,v in (("G4_RESULT","PASS"),("G4_ERRORS","0"),("G4_TRIALS","50"),
                ("G4_BATCH_FINAL","RELEASED"),("G4_DONE",token)):
        need(vals(k)==[v],"SERIAL_EVIDENCE_"+k)
    need(token in vals("G4_READY"),"SERIAL_TOKEN_MISMATCH")
    for name in ("CLI_VERSION","COMPILE_SOURCE_REBUILD","ARCHIVE_CREATE",
                 "ARCHIVE_MEMBERS","ARCHIVE_EXTRACT","COMPILE_TEMP_ARCHIVE",
                 "COMPILE_PHYSICAL","UPLOAD_G4","COMPILE_REG_IDLE_STATUS",
                 "COMPILE_REG_HMI_FIELDS","COMPILE_REG_IDLE_MODES","COMPILE_REG_LOGIC_UI"):
        f=RUN/(name+".log")
        need(f.is_file() and "EXIT=0" in f.read_text(encoding="utf-8-sig")[:1000],
             "G4_LOG_MISSING_OR_FAILED_"+name)
    f=RUN/"SUMMARY.log"
    need(f.is_file() and "STATUS=PASS_PHYSICAL_AND_REBUILT_NORMAL_ARCHIVE" in
         f.read_text(encoding="utf-8-sig"),"G4_SUMMARY_NOT_PASS")
    for candidate in CANDIDATES.values():
        need(m.get("source_sha256",{}).get(candidate)==sha(candidate),"CANDIDATE_MANIFEST_DIFF")
    for file in PRODUCT:
        need(m.get("product_sha256",{}).get(file)==sha(file),"PRODUCT_MANIFEST_DIFF "+file)
    need(m.get("backend_init_temp_sha256")==
         "44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc",
         "ST7789_PATCHED_SHA_DIFFERENT")
    say("EVIDENCE=PASS_50_CYCLES_4_REGRESSIONS_VISUAL")
    say("RUN_ID="+m["run_id"])
    return m
def preflight():
    need(git("rev-parse","--is-inside-work-tree")=="true","NOT_REPO")
    need(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,"WRONG_REPO_ROOT")
    need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    head=git("rev-parse","HEAD")
    cached=git("rev-parse","refs/remotes/origin/"+BRANCH)
    # GitHub Desktop puede haber actualizado HEAD al tooling previo.
    # SOLO se admiten los tres ancestros concretos y el remote validado.
    need(head in (BASE,TOOLING_HEAD,HEAD_REPAIR,cached),
         "LOCAL_HEAD_NOT_RECOGNIZED_"+head)
    say("LOCAL_HEAD_ACCEPTED="+head)

    need(bool(re.search(r"github\.com[:/]JW-Control/platform-jwplc(?:\.git)?$",
                        git("remote","get-url","origin"),re.I)),"WRONG_ORIGIN")
    need(setpaths("diff","--name-only")==PRODUCT,"DIRTY_SCOPE_NOT_EXACT_3_TFT")
    need(not setpaths("diff","--cached","--name-only"),"STAGED_FILES_PRESENT")
    need(not setpaths("ls-files","--others","--exclude-standard"),"UNTRACKED_FILES_PRESENT")
    need(not git("diff","--check"),"GIT_DIFF_CHECK_FAILED")
    for op in ("MERGE_HEAD","CHERRY_PICK_HEAD","REBASE_HEAD"):
        p=Path(git("rev-parse","--git-path",op))
        need(not (p if p.is_absolute() else ROOT/p).exists(),"GIT_OPERATION_ACTIVE")
    git("var","GIT_AUTHOR_IDENT")
    git("var","GIT_COMMITTER_IDENT")
    say("GIT_PREFLIGHT=PASS")
def remote(local_head):
    ref=git("rev-parse","refs/remotes/origin/"+BRANCH)
    need(ref not in (BASE,TOOLING_HEAD),"REMOTE_REPAIR_COMMIT_NOT_FETCHED")
    # Exactamente tres commits no productivos: b15c, 21a, cierre reparado.
    ancestors=git("rev-list","--reverse",BASE+".."+ref).splitlines()
    need(len(ancestors)==3 and ancestors[:2]==[TOOLING_HEAD,HEAD_REPAIR]
         and ancestors[2]==ref,
         "REMOTE_HISTORY_NOT_EXPECTED_3_TOOLING_COMMITS")
    for child,parent in ((TOOLING_HEAD,BASE),(HEAD_REPAIR,TOOLING_HEAD),
                         (ref,HEAD_REPAIR)):
        need(git("rev-list","--parents","-n","1",child).split()==[child,parent],
             "TOOLING_PARENT_CHANGED_"+child)
    need(setpaths("diff","--name-only",BASE,ref)==TOOLING,
         "REMOTE_DIFF_NOT_EXACT_2_TOOLING_FILES")
    need(local_head in (BASE,TOOLING_HEAD,HEAD_REPAIR,ref),
         "LOCAL_HEAD_NOT_AUTHORIZED_"+local_head)

    # Los bytes del worktree pueden tener CRLF (Git autocrlf en Windows)
    # aun cuando el blob comprometido sea LF y 'git diff' esté limpio.
    # Usar el índice y las comprobaciones de Git, nunca igualdad raw bytes.
    for path in TOOLING:
        file=ROOT/path
        if local_head==BASE:
            need(not file.exists(),"UNEXPECTED_TOOLING_COLLISION_"+path)
        else:
            need(file.is_file(),"LOCAL_TOOLING_MISSING_"+path)
            tracked=subprocess.run(["git","ls-files","--error-unmatch","--",path],
                                   cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            need(tracked.returncode==0,"LOCAL_TOOLING_NOT_TRACKED_"+path)
            unstaged=subprocess.run(["git","diff","--quiet","HEAD","--",path],
                                    cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            staged=subprocess.run(["git","diff","--cached","--quiet","HEAD","--",path],
                                  cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
            need(unstaged.returncode==0 and staged.returncode==0,
                 "LOCAL_TOOLING_HAS_GIT_CHANGES_"+path)
            blob=git("rev-parse",local_head+":"+path)
            cached_blob=git("rev-parse",":"+path)
            need(blob==cached_blob,"LOCAL_TOOLING_INDEX_MISMATCH_"+path)
    say("LOCAL_TOOLING=PASS_GIT_NORMALIZED_EOLS")

    # Asegurar que el finalizador extraído a %TEMP% es exactamente el
    # versionado en la rama remota validada, sin ejecución de variantes.
    runner=ROOT/"tools/alpha13/gates/a13_g4_finalize.py"
    p=subprocess.run(["git","show",ref+":tools/alpha13/gates/a13_g4_finalize.py"],
                     cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    need(p.returncode==0 and p.stdout==Path(__file__).read_bytes(),
         "RUNNER_BYTES_NOT_MATCHING_VERIFIED_REMOTE")
    say("REMOTE_PREFLIGHT=PASS_3_TOOLING_COMMITS_ONLY")
    say("REMOTE_FINALIZER="+ref)
    return ref

def commit(scope,message):
    git("add","--",*sorted(scope))
    need(setpaths("diff","--cached","--name-only")==scope,"STAGED_SCOPE_MISMATCH")
    need(not git("diff","--cached","--check"),"STAGED_DIFF_CHECK")
    git("commit","-m",message)
    ref=git("rev-parse","HEAD")
    need(setpaths("diff-tree","--no-commit-id","--name-only","-r",ref)==scope,
         "COMMIT_SCOPE_MISMATCH")
    say("COMMIT="+ref+" FILES="+str(len(scope))+" MESSAGE="+message)
    return ref
def write_docs(m,tool,prod,merge):
    report=f"""# Alpha13 G4 — cierre TFT batch task ownership
Fecha: 2026-10-10. Estado: CLOSED_PASS de G4, no de Alpha13 completo.

## Evidencia
- Gate: tools/alpha13/results/g4_integrated_{m['run_id']}/ (logs locales ignorados por Git).
- Puerto: {m['selected_port']}, VID/PID: {m['vid_pid']}.
- Firmware de prueba SHA256: {m['app_sha256']}.
- Token serial: {m['runtime_token']}.
- Dos tareas: 50/50 ciclos de propiedad y exclusividad SPI PASS; 0 errores; mutex liberado.
- TFT: operador reporto fondo blanco/flicker=NO, estabilidad=SI. Video no revisado.
- Backend oficial TFT_eSPI 2.5.43 mantenido en copia temporal, init parcheado por receta con SHA pinneado.
- Archive con 2 miembros verificados y consumidores normales precompilados: 4/4 PASS.
- Archive TFT SHA256: {m['archive_sha256']}; bytes: {m.get('archive_bytes','NO_REGISTRADOS')}.
- G3 core.a SHA256 preservado: {CORE_SHA}; Display.a y setup TFT sin cambios.

## Archivos productivos
- JWPLC_TFT.cpp: ownership basado en identificador atomico de tarea FreeRTOS.
- JWPLC_TFT.h: estado privado sin alterar firmas publicas.
- libJWPLC_TFT.a: archive precompilado fuente-first probado fisicamente.
- No se editaron el backend TFT_eSPI global, core.a ni librerias de otros perifericos.

## Limitaciones
El ensayo de 50 ciclos no garantiza ausencia de todas las carreras. La inspeccion visual es declaracion del operador. El firmware de prueba cargado no restaura automaticamente el sketch anterior.

## Git
TOOLING_COMMIT={tool}
PRODUCT_COMMIT={prod}
SYNC_MERGE_COMMIT={merge}
RELEASE_MERGE=NO
PR=NO
PUBLISH=NO

## Continuidad
G4=CLOSED_PASS. NEXT_GATE=A13-G5_TCP_CORRECTNESS_A13_005_A13_006.
"""
    checklist="""# Alpha13 — checklist G4
- [x] Correccion de ownership de batch por tarea FreeRTOS.
- [x] F111: SHA historico erroneo diagnosticado y corregido.
- [x] Backend TFT_eSPI externo sin alteracion.
- [x] Archive de 2 miembros con paridad y link precompilado normal.
- [x] Serial unico, ensayo fisico 50/50 PASS y 0 errores.
- [x] Operador confirmo pruebas en banco y TFT visual estable.
- [x] Regresiones con producto adoptado: 4/4 PASS.
- [x] Tres cambios productivos versionados por commit separado.
- [x] Sin merge a release, PR ni publicacion.
- [ ] G5 correccion TCP.
- [ ] CI final, Arduino IDE/package aislado, PR y PreRelease en espanol.
"""
    title="# v2.1.0-alpha.13 — Estado operativo y continuidad\n"
    old=(ROOT/STATUS).read_text(encoding="utf-8-sig")
    need(old.startswith(title),"STATUS_HEADER_CHANGED")
    prefix=f"""
## Estado vigente — G4 cerrado (2026-10-10)

Este bloque es canonico y prevalece sobre las entradas anteriores.

ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3=CLOSED_PASS
G4=CLOSED_PASS
G4_RUN_ID={m['run_id']}
G4_PHYSICAL=50_OF_50_PASS
G4_ERRORS=0
G4_VISUAL=PASS_OPERATOR_REPORTED
G4_REGRESSIONS=4_OF_4_PASS
G4_TFT_ARCHIVE_SHA256={m['archive_sha256']}
G4_PRODUCT_COMMIT={prod}
G4_SYNC_MERGE_COMMIT={merge}
F111=CLOSED_HARNESS_REMEDIATED
NEXT_GATE=A13-G5_TCP_CORRECTNESS_A13_005_A13_006
NEXT_FAILURE_ID=F112
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE

Cierre: A13_G4_CLOSURE_20261010.md. Checklist: A13_G4_CLOSURE_CHECKLIST_20261010.md.
Sin publicacion ni merge a release.

---

"""
    history=(ROOT/FAILURES).read_text(encoding="utf-8-sig")
    need("## F111 —" in history and "F111=CLOSED_HARNESS_REMEDIATED" not in history,
         "F111_ALREADY_CLOSED_OR_ABSENT")
    note=f"""
### F111 — cerrado tras G4 PASS
F111=CLOSED_HARNESS_REMEDIATED
RUN_ID={m['run_id']}
ROOT_CAUSE=TRUNCATED_SHA_LITERAL_IN_G4_HARNESS
PHYSICAL=50_OF_50_PASS
NORMAL_REGRESSIONS=4_OF_4_PASS
NEXT_FAILURE_ID=F112

El error estaba en el pin del harness, no en el backend instalado.
"""
    changes={REPORT:report,CHECK:checklist,STATUS:old.replace(title,title+prefix,1),
             FAILURES:history+note}
    for path,content in changes.items():
        if path in (REPORT,CHECK):
            need(not (ROOT/path).exists(),"CLOSURE_DOC_ALREADY_EXISTS_"+path)
        (ROOT/path).write_text(content,encoding="utf-8",newline="\n")
    say("DOCUMENTS=4_FILES_CREATED_OR_UPDATED")
    return set(changes)
def main():
    try:
        say("HITO=A13_G4_FINALIZE")
        preflight()
        product()
        m=evidence()
        product()
        local_head=git("rev-parse","HEAD")
        tool=remote(local_head)
        product()
        say("READY_FOR_OPERATOR_AUTHORIZATION=YES")
        confirmation=input("Autorizar commits y push de G4 (AUTORIZO_COMMIT_G4): ").strip()
        need(confirmation=="AUTORIZO_COMMIT_G4","COMMIT_NOT_AUTHORIZED")
        prod=commit(PRODUCT,"fix(tft): asegurar propiedad de tarea en batch SPI")
        product()
        need(not setpaths("diff","--name-only"),"DIRTY_AFTER_PRODUCT_COMMIT")
        if local_head==tool:
            # El repo ya contiene el finalizador; commit productivo lineal.
            merged="NOT_REQUIRED"
        else:
            # Commit productivo desde el baseline/primer tooling; integración
            # sin tocar ni sobrescribir los tres bytes TFT probados.
            git("merge","--no-ff","-m","chore(alpha13): integrar ejecutor de cierre G4",
                "refs/remotes/origin/"+BRANCH)
            merged=git("rev-parse","HEAD")
        need(not setpaths("diff","--name-only") and
             not setpaths("diff","--cached","--name-only"),
             "DIRTY_AFTER_TOOLING_INTEGRATION")
        product()
        say("SYNC_MERGE="+merged)
        changes=write_docs(m,tool,prod,merged)
        closure=commit(changes,"docs(alpha13): cerrar G4 y abrir G5 TCP")
        product()
        need(not setpaths("diff","--name-only") and
             not setpaths("diff","--cached","--name-only"),
             "DIRTY_AFTER_G4_CLOSURE")
        say("PUSH=START_NON_FORCE")
        git("push","origin","HEAD:refs/heads/"+BRANCH)
        say("PUSH=PASS_NON_FORCE")
        say("STATUS=CLOSED_PASS")
        say("TOOLING_COMMIT="+tool)
        say("PRODUCT_COMMIT="+prod)
        say("SYNC_MERGE_COMMIT="+merged)
        say("CLOSURE_COMMIT="+closure)
        say("NEXT_GATE=A13-G5_TCP_CORRECTNESS_A13_005_A13_006")
        say("RELEASE_MERGE_PR_PUBLICATION=NO")
        return 0
    except (Review,Exception,KeyboardInterrupt) as exc:
        say("STATUS=REVIEW_STOP")
        say("REASON="+str(exc))
        say("NO_RESET_CHECKOUT_CLEAN_OR_RETRY_BLINDLY=YES")
        return 2
if __name__=="__main__":sys.exit(main())
