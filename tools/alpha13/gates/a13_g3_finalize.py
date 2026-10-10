#!/usr/bin/env python3
"""Cierre Git Alpha13 G3: auditado, sin compile/upload ni operaciones destructivas."""
from __future__ import annotations
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path.cwd().resolve()
BRANCH = 'v2.1.0-alpha.13/feature/cleanup-robustness'
LOCAL = '1f1548d8dffec802771e8f08445d6a419803f87d'
REMOTE = LOCAL
CORE = 'JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a'
CORE_HASH = 'a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d'
SRC = {
  'JWPLC/2.1.0/cores/jwcontrol/peripherals/src/jwplc_i2c_bridge.cpp': 'tools/alpha13/candidates/g3/jwplc_i2c_bridge.cpp',
  'JWPLC/2.1.0/cores/jwcontrol/peripherals/include/jwplc_i2c_bridge.h': 'tools/alpha13/candidates/g3/jwplc_i2c_bridge.h',
  'JWPLC/2.1.0/cores/jwcontrol/peripherals/src/peripheral-tca6424a.c': 'tools/alpha13/candidates/g3/peripheral-tca6424a.c',
  'JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp': 'tools/alpha13/candidates/g3/jwplc_peripherals.cpp',
}
PRODUCT = set(SRC) | {CORE}
DOCROOT = 'docs/v2.1.0-alpha.13/'
STATUS = DOCROOT+'ALPHA13_STATUS.md'
FAILURES = DOCROOT+'A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md'
CLOSURE = DOCROOT+'A13_G3_CLOSURE_20261010.md'
CHECKLIST = DOCROOT+'A13_G3_CLOSURE_CHECKLIST_20261010.md'
REMOTE_SCOPE = {DOCROOT+'A13_G3_FINALIZE_PLAN_20261010.md',
                'tools/alpha13/gates/a13_g3_finalize.py',
                'tools/alpha13/gates/run_a13_g3_finalize.bat'}
RUN = None
EVENTS = []

class Review(Exception):
    pass

def need(condition, reason):
    if not condition:
        raise Review(reason)

def say(message):
    EVENTS.append(str(message))
    print(str(message), flush=True)
    if RUN is not None:
        (RUN/'FINALIZE_GIT_SUMMARY.log').write_text('\n'.join(EVENTS)+'\n', encoding='utf-8')

def git(*args, accepted=(0,), raw=False):
    p = subprocess.run(['git', '-c', 'core.quotepath=false', *args], cwd=ROOT,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode not in accepted:
        raise Review('GIT_ERROR '+str(p.returncode)+' '+ ' '.join(args)+' '+
                     p.stderr.decode('utf-8','replace')[-850:])
    return p.stdout if raw else p.stdout.decode('utf-8','replace').strip()

def changed(*args):
    b = git(*args,'-z',raw=True)
    return set(p for p in b.decode('utf-8').split('\0') if p)

def sha(path):
    d=hashlib.sha256()
    with (ROOT/path).open('rb') as handle:
        for chunk in iter(lambda:handle.read(1024*1024), b''):
            d.update(chunk)
    return d.hexdigest()

def product_ok():
    for path, candidate in SRC.items():
        need((ROOT/path).is_file() and (ROOT/candidate).is_file(),'SOURCE_MISSING '+path)
        need((ROOT/path).read_bytes()==(ROOT/candidate).read_bytes(), 'SOURCE_HASH_MISMATCH '+path)
    need(sha(CORE)==CORE_HASH,'CORE_HASH_MISMATCH')
    say('PRODUCT=PASS_4_SOURCES_CORE_SHA')

def evidence():
    global RUN
    eligible=[]
    for f in (ROOT/'tools/alpha13/results').glob('g3_integrated_*/MANIFEST.json'):
        try:
            m=json.loads(f.read_text(encoding='utf-8-sig'))
        except (OSError,ValueError):
            continue
        if (m.get('status')=='PASS_PHYSICAL_AND_NORMAL_CORE' and
            m.get('head')==LOCAL and m.get('core_after')==CORE_HASH):
            eligible.append((f.parent,m))
    need(len(eligible)==1,'PASS_MANIFEST_COUNT='+str(len(eligible)))
    RUN,m=eligible[0]
    need(m.get('hito')=='A13-G3','WRONG_EVIDENCE_HITO')
    for phase in ('PREFLIGHT','ADOPTION','SOURCE_TO_CORE_ARCHIVE',
                  'NORMAL_PRECOMPILED_LINK','PHYSICAL_PROBE_BUILD',
                  'REGRESSION_NORMAL_CONSUMERS','PHYSICAL_UPLOAD',
                  'PHYSICAL_SERIAL','FINAL_AUDIT'):
        need(m.get('phases',{}).get(phase)=='PASS','PHASE_NOT_PASS '+phase)
    need(m.get('phases',{}).get('PHYSICAL_SAFETY_CHECK')=='RUNNING',
         'PHYSICAL_SAFETY_CHECK_ABSENT')
    need(m.get('product_committed') is False and m.get('release_published') is False,
         'PREVIOUS_GATE_MUTATED_GIT')
    need(m.get('source_contract')=='PASS', 'SOURCE_CONTRACT_NOT_PASS')
    need(m.get('physical_serial')=='PASS','PHYSICAL_SERIAL_NOT_PASS')
    need(set(m.get('regressions',[]))=={'IDLE_STATUS','HMI_FIELDS','LOGIC_RUNTIME_UI'},
         'REGRESSIONS_NOT_3_OF_3')
    token=m.get('token','')
    need(isinstance(token,str) and token.startswith(m['run_id']+'_'), 'TOKEN_INVALID')
    need(re.fullmatch('[0-9a-f]{64}',m.get('app_sha256','')) is not None,'APP_HASH_INVALID')
    serial=RUN/'serial_g3.log'
    need(serial.is_file(),'SERIAL_LOG_MISSING')
    lines=serial.read_text(encoding='utf-8-sig').splitlines()
    def vals(key):return [x[len(key)+1:] for x in lines if x.startswith(key+'=')]
    for key,expect in [('G3_RESULT','PASS'),('G3_ERRORS','0'),('G3_TRIALS','150'),
                       ('G3_FINAL_OUTPUT','0'),('G3_DONE',token)]:
        need(vals(key)==[expect],'SERIAL_EVIDENCE '+key)
    need(token in vals('G3_READY'),'READY_PROVENANCE_NOT_FOUND')
    for label in ('BUILD_OFFICIAL_CORE','VERIFY_OFFICIAL_CORE','COMPILE_G3_PROBE',
                  'COMPILE_REG_IDLE_STATUS','COMPILE_REG_HMI_FIELDS',
                  'COMPILE_REG_LOGIC_RUNTIME_UI','UPLOAD_G3_PROBE'):
        f=RUN/(label+'.log')
        need(f.is_file() and 'EXIT=0' in f.read_text(encoding='utf-8-sig')[:1000],
             'LOG_NOT_PASS '+label)
    s=RUN/'SUMMARY.log'
    need(s.is_file() and 'STATUS=PASS_PHYSICAL_AND_NORMAL_CORE' in s.read_text(encoding='utf-8-sig'),
         'SUMMARY_NOT_PASS')
    for dst in SRC:
        need(m.get('candidate_sha256',{}).get(dst)==sha(dst),'MANIFEST_CANDIDATE_HASH '+dst)
    need(sha(CORE)==m['core_after'],'MANIFEST_CORE_HASH')
    say('EVIDENCE=PASS_150_CYCLES_ZERO_ERRORS_3_REGRESSIONS')
    say('RUN_ID='+m['run_id'])
    return m

def preflight():
    need(git('rev-parse','--is-inside-work-tree')=='true','NOT_REPO')
    need(Path(git('rev-parse','--show-toplevel')).resolve()==ROOT,'NOT_REPO_ROOT')
    need(git('branch','--show-current')==BRANCH,'WRONG_BRANCH')
    need(git('rev-parse','HEAD')==LOCAL,'LOCAL_HEAD_CHANGED')
    need(re.search(r'github\.com[:/]JW-Control/platform-jwplc(?:\.git)?$',
                   git('remote','get-url','origin'),re.I),'WRONG_ORIGIN')
    need(changed('diff','--name-only')==PRODUCT,'DIRTY_SCOPE_NOT_5_FILES')
    need(not changed('diff','--cached','--name-only'),'STAGED_INDEX_NOT_EMPTY')
    need(not git('diff','--check'),'DIFF_CHECK_FAILED')
    need(changed('ls-files','--others','--exclude-standard') == REMOTE_SCOPE,
         'UNTRACKED_TOOLING_SCOPE_NOT_EXACT')
    for name in ('MERGE_HEAD','CHERRY_PICK_HEAD','REBASE_HEAD'):
        p=Path(git('rev-parse','--git-path',name))
        need(not (p if p.is_absolute() else ROOT/p).exists(),'GIT_OPERATION_IN_PROGRESS')
    git('var','GIT_AUTHOR_IDENT')
    git('var','GIT_COMMITTER_IDENT')
    say('GIT_PREFLIGHT=PASS')

def remote_check():
    git('fetch','--no-tags','origin',
        'refs/heads/'+BRANCH+':refs/remotes/origin/'+BRANCH)
    head=git('rev-parse','refs/remotes/origin/'+BRANCH)
    need(head==REMOTE,'REMOTE_HEAD_UNEXPECTED '+head)
    p=subprocess.run(['git','merge-base','--is-ancestor',LOCAL,head],cwd=ROOT)
    need(p.returncode==0,'REMOTE_NOT_DESCENDANT')
    need(head==LOCAL,'REMOTE_CHANGED_DURING_CLOSURE')
    for path in REMOTE_SCOPE:
        need((ROOT/path).is_file(),'MISSING_TOOLING '+path)
        p=subprocess.run(['git','ls-files','--error-unmatch','--',path],
                         cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        need(p.returncode!=0,'TOOLING_ALREADY_TRACKED '+path)
    say('REMOTE_PREFLIGHT=PASS_NO_REMOTE_CHANGES')

def commit_only(paths, message):
    git('add','--',*sorted(paths))
    need(changed('diff','--cached','--name-only')==paths,'STAGED_SCOPE_MISMATCH')
    need(not git('diff','--cached','--check'),'STAGED_DIFF_CHECK_FAILED')
    git('commit','-m',message)
    commit=git('rev-parse','HEAD')
    need(changed('diff-tree','--no-commit-id','--name-only','-r',commit)==paths,
         'COMMIT_SCOPE_MISMATCH')
    say('COMMIT='+commit+' FILES='+str(len(paths))+' MESSAGE='+message)
    return commit

def documents(m,product,tool):
    report='''# Alpha13 — G3 / A13-004 — cierre TCA RMW/shadow

Estado: **CLOSED_PASS** (hito técnico; Alpha13 continúa en desarrollo).

## Evidencia real

- Ejecución: RUN_ID.
- FQBN: jwplc_local:esp32:jwplcbasic; puerto: PORT.
- Binario de aplicación: APP_SHA.
- Token de procedencia serial: TOKEN.
- Reconstrucción source-first, core precompilado normal y 3/3 consumidores: PASS.
- Dos tareas sobre Q0_0 y Q0_1: 150/150 ciclos PASS, 0 errores.
- Registro TCA OUTPUT1 y shadow JWPLC concordaron en los ciclos.
- Registro OUTPUT1 final: 0x00.
- Core anterior: 6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983.
- Core nuevo: CORE_SHA.
- Fuentes probadas: 4 y archive: 1; no se cambiaron otras librerías.

El operador confirmó cargas/actuadores desconectados antes del upload. No se
infieren pruebas visuales ni inmunidad absoluta frente a cualquier carrera.
Las escrituras directas al registro del TCA fuera del driver quedan fuera del
contrato del shadow; se preservan APIs de usuario, autoload y periféricos.

## Trazabilidad Git

- Tooling remoto: TOOL_SHA.
- Commit productivo: PRODUCT_COMMIT.
- Integración documental: MERGE_COMMIT.
- Cierre documental: commit de este archivo.
- No merge a release, PR ni publicación.

## Continuidad

G3=CLOSED_PASS. NEXT_GATE=A13-G4_TFT_BATCH_TASK_OWNERSHIP.
'''
    for key,value in {'RUN_ID':m['run_id'],'PORT':m['serial_port'],
                      'APP_SHA':m['app_sha256'],'TOKEN':m['token'],
                      'CORE_SHA':m['core_after'],'TOOL_SHA':tool,
                      'PRODUCT_COMMIT':product,'MERGE_COMMIT':'NOT_REQUIRED'}.items():
        report=report.replace(key,value)
    check='''# Alpha13 — G3 checklist (2026-10-10)

- [x] P0 reproducción determinista del riesgo.
- [x] F110 diagnosticado, corregido, mitigación comprobada.
- [x] Cuatro fuentes de corrección + core.a precompilado.
- [x] Build core fuente y enlace normal precompilado.
- [x] Tres regresiones de consumidores PASS.
- [x] Confirmación operador: cargas externas desconectadas.
- [x] Upload real, procedencia serial única y 150 ciclos sin error.
- [x] Registro y shadow consistentes; salidas a cero al final.
- [x] Cinco archivos productivos auditados en commit separado.
- [x] Sin merge a release, PR ni publicación.
- [ ] CI/Arduino IDE final y pruebas de package aislado (cierre Alpha13).
- [ ] PR/PreRelease en español (cierre Alpha13).
- [ ] G4 A13-003 TFT batch task ownership (pendiente).
'''
    prior=(ROOT/STATUS).read_text(encoding='utf-8-sig')
    need(prior.startswith('# v2.1.0-alpha.13'),'STATUS_UNEXPECTED')
    block='''\n## Estado vigente — G3 A13-004 cerrado (2026-10-10)

**Prevalece sobre los bloques anteriores de G3.**

G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_CLOSURE=CLOSED_PASS
G3_P0=PASS_BASELINE_RACE_REPRODUCED
G3_SOURCE_CORE_REBUILD=PASS
G3_NORMAL_PRECOMPILED_LINK=PASS
G3_REGRESSIONS=3_OF_3_PASS
G3_PHYSICAL=150_OF_150_PASS
G3_SERIAL_ERRORS=0
G3_FINAL_OUTPUT=0x00
G3=CLOSED_PASS
G3_RUN_ID=RUN_ID
G3_CORE_SHA256=CORE_SHA
G3_PRODUCT_COMMIT=PRODUCT_COMMIT
G3_SYNC_MERGE_COMMIT=MERGE_COMMIT
F110=CLOSED_HARNESS_REMEDIATED
NEXT_GATE=A13-G4_TFT_BATCH_TASK_OWNERSHIP
NEXT_FAILURE_ID=F111
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE

Documentos: [cierre G3](A13_G3_CLOSURE_20261010.md) y
[checklist G3](A13_G3_CLOSURE_CHECKLIST_20261010.md).
Sin publicación ni merge a release.

---
'''
    for key,value in {'RUN_ID':m['run_id'],'CORE_SHA':m['core_after'],
                      'PRODUCT_COMMIT':product,'MERGE_COMMIT':'NOT_REQUIRED'}.items():
        block=block.replace(key,value)
    original_failure=(ROOT/FAILURES).read_text(encoding='utf-8-sig')
    need('## F110 —' in original_failure and
         'F110=CLOSED_HARNESS_REMEDIATED' not in original_failure,
         'FAILURE_REGISTRY_UNEXPECTED')
    append='''\n### F110 — cierre verificado tras el gate integrado G3

F110=CLOSED_HARNESS_REMEDIATED
G3_RUN_ID=RUN_ID
SOURCE_CORE=PASS
NORMAL_PRECOMPILED_LINK=PASS
REGRESSIONS=3_OF_3_PASS
PHYSICAL=150_OF_150_PASS
NEXT_FAILURE_ID=F111

Las siete firmas duplicadas no reaparecieron. No crear nuevos IDs por esta
misma causa sin nueva evidencia.
'''.replace('RUN_ID',m['run_id'])
    for path,text in [(CLOSURE,report),(CHECKLIST,check),
                      (STATUS,prior.split('\n',1)[0]+'\n'+block+prior.split('\n',1)[1]),
                      (FAILURES,original_failure+append)]:
        if path in (CLOSURE,CHECKLIST):
            need(not (ROOT/path).exists(),'CLOSURE_DOC_EXISTS '+path)
        (ROOT/path).write_text(text,encoding='utf-8',newline='\n')
    say('DOCUMENTS=4_FILES_CREATED_OR_UPDATED')
    return {CLOSURE,CHECKLIST,STATUS,FAILURES}

def main():
    try:
        say('HITO=A13_G3_FINALIZE')
        preflight()
        product_ok()
        m=evidence()
        remote_check()
        product_ok()
        say('READY_FOR_OPERATOR_AUTHORIZATION=YES')
        answer=input('Para autorizar commits de G3 y push sin fuerza, escribir AUTORIZO_COMMIT_G3: ').strip()
        need(answer=='AUTORIZO_COMMIT_G3','COMMIT_NOT_AUTHORIZED')
        tool=commit_only(REMOTE_SCOPE,'test(alpha13): versionar cierre integrado G3')
        product_ok()
        product=commit_only(PRODUCT,'fix(core): asegurar atomicidad TCA RMW y coherencia de shadows')
        product_ok()
        need(not changed('diff','--name-only'),'DIRTY_AFTER_PRODUCT_COMMIT')
        say('SYNC_MERGE=NOT_REQUIRED')
        docs=documents(m,product,tool)
        closure=commit_only(docs,'docs(alpha13): cerrar G3 y abrir G4')
        product_ok()
        need(not changed('diff','--name-only') and
             not changed('diff','--cached','--name-only'),'FINAL_WORKTREE_NOT_CLEAN')
        say('PUSH=START_NON_FORCE')
        git('push','origin','HEAD:refs/heads/'+BRANCH)
        say('PUSH=PASS_NON_FORCE')
        say('STATUS=CLOSED_PASS')
        say('PRODUCT_COMMIT='+product)
        say('TOOLING_COMMIT='+tool)
        say('CLOSURE_COMMIT='+closure)
        say('NEXT_GATE=A13-G4_TFT_BATCH_TASK_OWNERSHIP')
        say('RELEASE_MERGE_PR_PUBLICATION=NO')
        return 0
    except (Exception,KeyboardInterrupt) as exc:
        say('STATUS=REVIEW_STOP')
        say('REASON='+str(exc))
        say('NO_RESET_CHECKOUT_CLEAN=YES')
        return 2

if __name__=='__main__':
    sys.exit(main())
