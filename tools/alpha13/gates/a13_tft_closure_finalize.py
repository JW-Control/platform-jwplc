#!/usr/bin/env python3
"""Alpha13: commit tooling / TFT product / documental closure, with guarded local Git.

Run once from the existing Windows checkout after PASS physical. No reset, checkout,
clean, pull, force push, release publication, or firmware modifications.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import sys
import traceback
from datetime import datetime
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
BRANCH = 'v2.1.0-alpha.13/feature/cleanup-robustness'
LOCAL_HEAD = 'ac69e41922f512c280ad116bcf79b24596befa02'
EXPECTED_REMOTE = 'a355b9a627d9fc8926e298e06d8cbae7b3f72929'
RUN_ID = '20261009_220034_167fa2ad'
RUN_PATH = f'tools/alpha13/results/tft_closure_{RUN_ID}'
DOC_DIR = 'docs/v2.1.0-alpha.13'
MAIN_STATUS = f'{DOC_DIR}/ALPHA13_STATUS.md'
PLAN = f'{DOC_DIR}/A13_TFT_CLOSURE_EXECUTOR_PLAN_20261009.md'
TOOL_PATHS = [
    'tools/alpha13/gates/a13_tft_closure.py',
    'tools/alpha13/gates/run_a13_tft_closure.bat',
    'tools/alpha13/gates/a13_tft_closure_finalize.py',
    'tools/alpha13/gates/run_a13_tft_closure_finalize.bat',
    PLAN,
]
PRODUCT_HASHES = {
    'JWPLC/2.1.0/libraries/JWPLC_TFT/src/JWPLC_TFT.cpp': '494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25',
    'JWPLC/2.1.0/libraries/JWPLC_TFT/src/tft_setup.h': '8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1',
    'JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a': 'ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8',
}
REMOTE_DOC_ONLY = {
    'docs/JWPLC_COLLABORATION_WORKFLOW.md',
    f'{DOC_DIR}/A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md',
    MAIN_STATUS,
    f'{DOC_DIR}/A13_TFT_CLOSURE_PHYSICAL_EVIDENCE_20261009.md',
}
SUMMARY_FILE = Path(RUN_PATH) / 'SUMMARY.log'
MANIFEST_FILE = Path(RUN_PATH) / 'MANIFEST.json'
LOG = []


class Stop(RuntimeError):
    pass


def emit(msg: str):
    msg = str(msg)
    print(msg, flush=True)
    LOG.append(msg)


def require(test, why: str):
    if not test:
        raise Stop(why)


def file_hash(rel: str) -> str:
    digest = hashlib.sha256()
    with (REPO / rel).open('rb') as f:
        for data in iter(lambda: f.read(1024 * 1024), b''):
            digest.update(data)
    return digest.hexdigest()


def cmd(*args: str, acceptable=(0,), output=False) -> str:
    params = ['git', '-c', 'core.quotepath=false', *args]
    p = subprocess.run(params, cwd=REPO, text=True, errors='replace', encoding='utf-8',
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if p.returncode not in acceptable:
        raise Stop(f'GIT_EXIT={p.returncode} COMMAND={" ".join(params)}\nSTDOUT={p.stdout[-1800:]}\nSTDERR={p.stderr[-1800:]}')
    if output:
        emit('GIT_OUTPUT=' + p.stdout.strip()[:1000])
    return p.stdout.strip()


def names(*args: str) -> set[str]:
    out = cmd(*args, '-z')
    return set(filter(None, out.split('\0')))


def ensure_hashes():
    for path, expected in PRODUCT_HASHES.items():
        got = file_hash(path)
        require(got == expected, f'PRODUCT_HASH_CHANGED {path} expected={expected} actual={got}')
    require((REPO / 'JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a').stat().st_size == 1091990,
            'PRODUCT_ARCHIVE_BYTES_CHANGED')
    emit('PRODUCT_HASHES=PASS_3_OF_3')


def read_evidence():
    require((REPO / MANIFEST_FILE).exists(), f'MISSING_EVIDENCE={MANIFEST_FILE}')
    require((REPO / SUMMARY_FILE).exists(), f'MISSING_EVIDENCE={SUMMARY_FILE}')
    m = json.loads((REPO / MANIFEST_FILE).read_text(encoding='utf-8-sig'))
    s = (REPO / SUMMARY_FILE).read_text(encoding='utf-8-sig')
    require(m.get('run_id') == RUN_ID, 'RUN_ID_MISMATCH')
    require(m.get('hito') == 'TFT-CLOSURE', 'HITO_MISMATCH')
    require(m.get('head_local') == LOCAL_HEAD, 'EVIDENCE_LOCAL_HEAD_MISMATCH')
    require(m.get('product_commit') is False and m.get('product_mutated') is False, 'ORIGINAL_GATE_MUTATED_PRODUCT')
    require(m.get('runtime_archive_sha256') == PRODUCT_HASHES['JWPLC/2.1.0/libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a'], 'RUNTIME_ARCHIVE_HASH_MISMATCH')
    require(m.get('app_binary_sha256') == 'a0a173b2f86957a131ebb03626cf0800f2b54ef2fdf4b38ab138b5586440d910', 'APP_BINARY_HASH_MISMATCH')
    require(m.get('selected_port') == 'COM4', 'PORT_EVIDENCE_MISMATCH')
    required = ['PREFLIGHT', 'TOOLCHAIN', 'SERIAL_PORT', 'BUILD_PHYSICAL',
                'PHYSICAL_UPLOAD', 'SERIAL_FIRST_BOOT', 'SERIAL_AFTER_USB_CYCLE',
                'REGRESSION', 'REBUILD_RECIPE']
    for phase in required:
        require(m.get('phases', {}).get(phase) == 'PASS', f'EVIDENCE_PHASE_NOT_PASS={phase}')
    require(m.get('visual_user_report', {}).get('white_problem') == 'NO', 'VISUAL_WHITE_REPORTED')
    require(m.get('visual_user_report', {}).get('idle_clean') == 'SI', 'VISUAL_IDLE_NOT_CONFIRMED')
    require(m.get('visual_user_report', {}).get('usb_only_power_cycle') == 'USER_CONFIRMED', 'USB_CYCLE_NOT_CONFIRMED')
    token = m.get('runtime_token')
    require(bool(token) and m['SERIAL_FIRST_BOOT_fields']['TFT_CLOSURE_RUN_ID'] == token
            and m['SERIAL_AFTER_USB_CYCLE_fields']['TFT_CLOSURE_RUN_ID'] == token, 'SERIAL_PROVENANCE_RUN_TOKEN_MISMATCH')
    for key in ('SERIAL_FIRST_BOOT_fields', 'SERIAL_AFTER_USB_CYCLE_fields'):
        f = m[key]
        require(f['TFT_CLOSURE_ARCHIVE_SHA256'] == m['runtime_archive_sha256'], f'SERIAL_PROVENANCE_ARCHIVE_MISMATCH={key}')
        require(f['DISPLAY_READY'] == 'YES' and f['IO_READY'] == 'YES', f'INITIALIZATION_NOT_READY={key}')
    require(set(m.get('regressions', [])) == {'IDLE_STATUS', 'HMI_FIELDS', 'IDLE_MODES', 'LOGIC_RUNTIME_UI'}, 'REGRESSION_SET_MISMATCH')
    r = m.get('rebuild_manifest', {})
    require(r.get('member_parity') == 'PASS' and r.get('archive_sha256') == '44916b892fb30988ecdb25a2abef7d8d8b3d4f3f68c9a7a6bbc88ca55e35841d',
            'REBUILD_RECIPE_EVIDENCE_MISMATCH')
    require('STATUS=PROVISIONAL_PASS_AWAITING_VIDEO_AND_COMMIT_AUTHORIZATION' in s, 'SUMMARY_ORIGINAL_STATUS_MISMATCH')
    require(m.get('category') == 'NO_FAILURE', 'EVIDENCE_FAILURE_CATEGORY')
    emit('EVIDENCE=PASS_PHYSICAL_SERIAL_VISUAL_OPERATOR_REGRESSIONS_REBUILD')
    return m


def ensure_git_base():
    require(cmd('rev-parse', '--is-inside-work-tree') == 'true', 'NOT_GIT_WORKTREE')
    require(Path(cmd('rev-parse', '--show-toplevel')).resolve() == REPO, 'REPO_ROOT_MISMATCH')
    require(cmd('branch', '--show-current') == BRANCH, 'WRONG_BRANCH')
    require(cmd('rev-parse', 'HEAD') == LOCAL_HEAD, 'UNEXPECTED_LOCAL_HEAD_STOP_NO_MUTATION')
    origin = cmd('remote', 'get-url', 'origin')
    if os.getenv('A13_FINALIZE_TEST') != '1':
        require(bool(re.search(r'github\.com[:/]JW-Control/platform-jwplc(?:\.git)?$', origin, re.I)),
                f'WRONG_ORIGIN={origin}')
    require(not names('diff', '--cached', '--name-only'), 'STAGED_CHANGES_NOT_EMPTY')
    require(names('diff', '--name-only') == set(PRODUCT_HASHES), 'TRACKED_DIRTY_SCOPE_NOT_EXACTLY_THREE_TFT_FILES')
    require(not cmd('diff', '--check'), 'GIT_DIFF_CHECK_FAILURE')
    for path in TOOL_PATHS:
        require((REPO / path).is_file(), f'MISSING_TOOLING_FILE={path}')
        proc = subprocess.run(['git', 'ls-files', '--error-unmatch', '--', path], cwd=REPO, capture_output=True)
        require(proc.returncode != 0, f'TOOLING_ALREADY_TRACKED={path}')
    for path in ('rebase-merge', 'rebase-apply', 'MERGE_HEAD', 'CHERRY_PICK_HEAD'):
        gp = cmd('rev-parse', '--git-path', path)
        require(not (REPO / gp).exists() if not Path(gp).is_absolute() else not Path(gp).exists(), f'GIT_OPERATION_IN_PROGRESS={path}')
    cmd('var', 'GIT_AUTHOR_IDENT')
    cmd('var', 'GIT_COMMITTER_IDENT')
    emit(f'GIT_PREFLIGHT=PASS HEAD={LOCAL_HEAD[:12]}')


def check_remote_before_mutations():
    # Only a fetch; leaves tracked/untracked working-tree files untouched.
    cmd('fetch', '--no-tags', 'origin', f'refs/heads/{BRANCH}:refs/remotes/origin/{BRANCH}')
    remote = cmd('rev-parse', f'refs/remotes/origin/{BRANCH}')
    require(remote == EXPECTED_REMOTE, f'REMOTE_HEAD_CHANGED_EXPECTED={EXPECTED_REMOTE} GOT={remote}; no local commits made')
    proc = subprocess.run(['git', 'merge-base', '--is-ancestor', LOCAL_HEAD, remote], cwd=REPO)
    require(proc.returncode == 0, 'REMOTE_IS_NOT_DESCENDANT_OF_LOCAL_HEAD')
    changed = names('diff', '--name-only', LOCAL_HEAD, remote)
    require(changed == REMOTE_DOC_ONLY, f'REMOTE_CHANGES_NOT_EXACT_DOC_SET={sorted(changed)}')
    emit(f'REMOTE_PREFLIGHT=PASS REMOTE_HEAD={remote[:12]} REMOTE_FILES={len(changed)}_DOC_ONLY')


def stage_and_commit(paths, message: str) -> str:
    cmd('add', '--', *paths)
    staged = names('diff', '--cached', '--name-only')
    require(staged == set(paths), f'STAGED_SCOPE_MISMATCH_EXPECTED={paths}_ACTUAL={sorted(staged)}')
    require(not cmd('diff', '--cached', '--check'), 'STAGED_DIFF_CHECK_FAILURE')
    cmd('commit', '-m', message)
    c = cmd('rev-parse', 'HEAD')
    actual = names('diff-tree', '--no-commit-id', '--name-only', '-r', c)
    require(actual == set(paths), f'COMMIT_SCOPE_MISMATCH={sorted(actual)}')
    emit(f'COMMIT={c} MESSAGE={message} FILES={len(paths)}')
    return c


def write_final_docs(m, tooling_sha: str, product_sha: str, merge_sha: str):
    run = RUN_ID
    doc_report = f'{DOC_DIR}/A13_TFT_CLOSURE_20261009.md'
    checklist = f'{DOC_DIR}/A13_TFT_CLOSURE_CHECKLIST_20261009.md'
    report = f'''# Alpha13 — cierre TFT-CLOSURE — 2026-10-09

## Decisión

\x60TFT_CLOSURE=CLOSED_PASS\x60, cierre físico validado con archive productivo y observación del operador. Este cierre no publica ni fusiona Alpha13 a release.

## Fuente de evidencia

- Ejecución: \x60{run}\x60; archivo original: \x60{RUN_PATH}/SUMMARY.log\x60 y \x60MANIFEST.json\x60 (resultados locales ignorados por Git).
- Evidencia versionada: [A13_TFT_CLOSURE_PHYSICAL_EVIDENCE_20261009.md](A13_TFT_CLOSURE_PHYSICAL_EVIDENCE_20261009.md).
- FQBN: \x60jwplc_local:esp32:jwplcbasic\x60; USB: \x60COM4\x60, VID:PID \x601A86:7523\x60.
- Archive TFT productivo SHA-256: \x60{m['runtime_archive_sha256']}\x60.
- Binario del sketch subido SHA-256: \x60{m['app_binary_sha256']}\x60.
- Token de ejecución reportado por serial en ambos arranques: \x60{m['runtime_token']}\x60.

| Criterio | Resultado |
|---|---|
| Preflight, toolchain, COM | PASS |
| Compilación normal con \x60JWPLC_TFT.a\x60 de producto | PASS |
| Upload físico y serial inicial | PASS |
| Reinicio USB-only y serial con misma procedencia | PASS |
| \x60DISPLAY_READY=YES\x60 / \x60IO_READY=YES\x60 | PASS |
| Fondo blanco irregular persistente/flicker | NO (operador) |
| TFT IDLE estable, sin boot loop | SÍ (operador) |
| Compilaciones de regresión Display/consumers | 4/4 PASS |
| Reconstrucción source-first y consumo precompiled normal | PASS |
| Paridad de miembros del archive regenerado | PASS |
| Video revisado directamente por asistente | NO, no suministrado |
| Reproducibilidad de hash binario del archive | NO; funcional sí |

\x60\x60\x60text
PRECOMPILED_PRODUCT_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
REBUILT_SHA256=44916b892fb30988ecdb25a2abef7d8d8b3d4f3f68c9a7a6bbc88ca55e35841d
REBUILT_ARCHIVE_BYTES=1092058
SOURCE_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
SOURCE_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
\x60\x60\x60

**Limitación:** el usuario constató tiempos percibidos comparables a la TFT previamente operativa; no se realizó A/B cronometrado. La observación visual del operador no se presenta como revisión de video.

## Trazabilidad Git

\x60\x60\x60text
TOOLING_COMMIT={tooling_sha}
PRODUCT_COMMIT={product_sha}
SYNC_MERGE_COMMIT={merge_sha}
PRODUCT_STAGE_PATH_COUNT=3
REMOTE_PULL_RESET_CHECKOUT_CLEAN=NOT_EXECUTED
MERGE_TO_RELEASE=NOT_EXECUTED
GITHUB_PR_OR_PRE_RELEASE=NOT_EXECUTED
\x60\x60\x60

El commit productivo cambia sólo \x60JWPLC_TFT.cpp\x60, \x60tft_setup.h\x60 y \x60libJWPLC_TFT.a\x60. El ejecutor y la receta de reconstrucción residen en \x60tools/alpha13/gates/a13_tft_closure.py\x60 y \x60run_a13_tft_closure.bat\x60. Se mantuvo autoload normal, API pública, core.a y Display.a.

## Siguiente objetivo

\x60NEXT_GATE=A13-G3-TCA_RMW_SHADOW_ATOMICITY\x60, iniciando por revisión/plan de G3. No repetir PRE5/P0/P1A/P1B/P2A/P2B sin evidencia nueva. Exclusiones: OpenPLC, HMI Designer y nuevas funcionalidades TFT.
'''
    check = '''# Alpha13 — Checklist de cierre TFT-CLOSURE — 2026-10-09

- [x] PRE5 candidato temporal — evidencia preservada.
- [x] P0, P1A, P1B, P2A — cierres anteriores retenidos.
- [x] P2B: compilación y upload reales con archive productivo.
- [x] Firmware identificado con token y hash antes/después de USB-only.
- [x] Usuario confirma ausencia del fondo blanco persistente y IDLE estable.
- [x] Cuatro builds de regresión normativos.
- [x] Recompilación mantenible desde fuentes productivos parcheados.
- [x] Paridad de miembros y consumo normativo del nuevo archive temporal.
- [x] Hashes productivos y scope exacto 3 archivos.
- [x] Commits separados: tooling, producto y documentación.
- [x] Sin cambios de core, Display, API pública ni autoload.
- [ ] Reproducibilidad bit a bit del archive (NO demostrada; no es gate de bloqueo para este fix).
- [ ] Vídeo revisado por asistente (no aportado; confirmación directa del operador registrada).
- [ ] Gate siguiente G3 — pendiente.
- [ ] PR de Alpha13 en español — al cierre del alpha.
- [ ] PreRelease en español — al cierre/publicación del alpha.
- [ ] CI final, package aislado, Arduino IDE final — al cierre del alpha.

**No se ha ejecutado merge a release ni se ha publicado una versión.**
'''
    status_path = REPO / MAIN_STATUS
    old = status_path.read_text(encoding='utf-8')
    require(old.startswith('# v2.1.0-alpha.13 — Estado operativo y continuidad'), 'STATUS_HEADLINE_UNEXPECTED')
    require('NEXT_GATE=TFT-CLOSURE' in old, 'STATUS_NEXT_GATE_NOT_TFT_CLOSURE')
    require('TFT_CLOSURE_CLOSED_PASS_CURRENT_20261009' not in old, 'STATUS_ALREADY_UPDATED')
    addition = f'''
## Estado vigente — TFT-CLOSURE cerrado (2026-10-09)

**TFT_CLOSURE_CLOSED_PASS_CURRENT_20261009**. Este bloque prevalece sobre las entradas históricas P2A/P2B y NEXT_GATE=TFT-CLOSURE más abajo, preservadas para trazabilidad.

\x60\x60\x60text
BRANCH={BRANCH}
ALPHA13_STATUS=IN_PROGRESS
G1_DNS=CLOSED_PASS
G2_TCA_STARTUP=CLOSED_PASS
TFT_PRE6_P2A=CLOSED_PASS
TFT_PRE6_P2B_PHYSICAL=PASS
TFT_CLOSURE=CLOSED_PASS
TFT_VISUAL=PASS_OPERATOR_REPORTED
TFT_VIDEO_REVIEW=NOT_PERFORMED
TFT_REBUILD_FUNCTIONAL=PASS
TFT_REBUILD_BIT_IDENTICAL=NO
TFT_REGRESSION_NORMAL_BUILDS=4_OF_4_PASS
TFT_RUN_ID={RUN_ID}
TFT_ARCHIVE_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
TFT_TOOLING_COMMIT={tooling_sha}
TFT_PRODUCT_COMMIT={product_sha}
TFT_SYNC_MERGE_COMMIT={merge_sha}
NEXT_GATE=A13-G3-TCA_RMW_SHADOW_ATOMICITY
PRODUCT_WORKTREE_EXPECTED=CLEAN_AFTER_COMMIT
RELEASE_MERGE=NOT_EXECUTED
ALPHA13_PUBLICATION=NOT_EXECUTED
NEXT_FAILURE_ID=F110
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
\x60\x60\x60

Fuentes: [cierre TFT](A13_TFT_CLOSURE_20261009.md), [evidencia física](A13_TFT_CLOSURE_PHYSICAL_EVIDENCE_20261009.md) y [checklist](A13_TFT_CLOSURE_CHECKLIST_20261009.md). No repetir gates de TFT ya cerrados. El próximo avance es G3.

---
'''
    split = old.find('\n') + 1
    status_path.write_text(old[:split] + addition + old[split:], encoding='utf-8', newline='\n')
    (REPO / doc_report).write_text(report, encoding='utf-8', newline='\n')
    (REPO / checklist).write_text(check, encoding='utf-8', newline='\n')
    plan_p = REPO / PLAN
    plan_text = plan_p.read_text(encoding='utf-8')
    require(plan_text.startswith('# Alpha13 — TFT-CLOSURE'), 'PLAN_HEADLINE_UNEXPECTED')
    new_plan_header = (f'> **Actualización tras ejecución ({RUN_ID}):** este documento conserva el plan inicial como historial. '
                       f'La ejecución física, regresiones y rebuild dieron PASS, confirmado por el operador. '
                       f'Consultar [cierre final](A13_TFT_CLOSURE_20261009.md).\n\n')
    plan_p.write_text(plan_text[:plan_text.find('\n')+1] + '\n' + new_plan_header + plan_text[plan_text.find('\n')+1:], encoding='utf-8', newline='\n')
    return [doc_report, checklist, MAIN_STATUS, PLAN]


def finalize():
    emit('HITO=A13_TFT_CLOSURE_COMMIT_AND_SYNC')
    emit('MODE=COMMIT_TOOLING_PRODUCT_DOCS_WITH_GIT_GUARDS')
    m = read_evidence()
    ensure_git_base()
    ensure_hashes()
    check_remote_before_mutations()
    require(not names('diff', '--cached', '--name-only'), 'STAGED_CHANGED_DURING_REMOTE_FETCH')
    require(names('diff', '--name-only') == set(PRODUCT_HASHES), 'PRODUCT_DIRTY_SCOPE_CHANGED_DURING_REMOTE_FETCH')
    require(not cmd('diff', '--check'), 'PRODUCT_DIFF_CHECK_AFTER_FETCH')

    tooling_sha = stage_and_commit(TOOL_PATHS, 'test(alpha13): versionar gate integrado de cierre TFT')
    require(names('diff', '--name-only') == set(PRODUCT_HASHES), 'TOOLING_COMMIT_TOUCHED_PRODUCT')
    ensure_hashes()
    product_sha = stage_and_commit(list(PRODUCT_HASHES), 'fix(tft): corregir arranque GRAM y actualizar archive precompilado')
    ensure_hashes()
    require(not names('diff', '--name-only'), 'TRACKED_DIRTY_AFTER_PRODUCT_COMMIT')
    require(not names('diff', '--cached', '--name-only'), 'STAGED_AFTER_PRODUCT_COMMIT')

    emit('SYNC=MERGE_DOCS_ONLY_FROM_REMOTE_FEATURE_BRANCH')
    cmd('merge', '--no-ff', '--no-edit', f'refs/remotes/origin/{BRANCH}')
    merge_sha = cmd('rev-parse', 'HEAD')
    require(len(cmd('rev-list', '--parents', '-n', '1', merge_sha).split()) == 3,
            'SYNC_MERGE_EXPECTED_TWO_PARENTS')
    ensure_hashes()
    require(not names('diff', '--name-only') and not names('diff', '--cached', '--name-only'),
            'DIRTY_AFTER_MERGE')
    emit(f'DOCS_SYNC_MERGE={merge_sha}')

    doc_paths = write_final_docs(m, tooling_sha, product_sha, merge_sha)
    final_sha = stage_and_commit(doc_paths, 'docs(alpha13): cerrar TFT-CLOSURE y abrir gate G3')
    ensure_hashes()
    require(not names('diff', '--name-only') and not names('diff', '--cached', '--name-only'),
            'DIRTY_BEFORE_PUSH')
    emit('PUSH=START_TO_SAME_ALPHA13_FEATURE_BRANCH_ONLY')
    cmd('push', 'origin', f'HEAD:refs/heads/{BRANCH}')
    emit('PUSH=PASS_NON_FORCE')
    emit('STATUS=CLOSED_PASS')
    emit(f'TOOLING_COMMIT={tooling_sha}')
    emit(f'PRODUCT_COMMIT={product_sha}')
    emit(f'DOCS_SYNC_MERGE={merge_sha}')
    emit(f'CLOSURE_DOCUMENT_COMMIT={final_sha}')
    emit('NEXT_GATE=A13-G3-TCA_RMW_SHADOW_ATOMICITY')
    emit('NO_RELEASE_MERGE_OR_PR_OR_PUBLISH=YES')


def main():
    if len(sys.argv) == 2 and sys.argv[1] == '--self-test':
        assert len(PRODUCT_HASHES) == 3 and len(set(TOOL_PATHS)) == 5
        assert len(set(PRODUCT_HASHES) & set(TOOL_PATHS)) == 0
        assert len(LOCAL_HEAD) == 40 and len(EXPECTED_REMOTE) == 40
        print('A13_TFT_CLOSURE_FINALIZE_SELF_TEST=PASS')
        return 0
    if len(sys.argv) != 1:
        print('Uso: run_a13_tft_closure_finalize.bat (sin argumentos)')
        return 2
    report_path = REPO / f'{RUN_PATH}/FINALIZE_GIT_SUMMARY.log'
    try:
        finalize()
        return 0
    except Exception as exc:
        emit(f'STATUS=REVIEW_STOPPED_NO_AUTOMATIC_ROLLBACK TYPE={type(exc).__name__} REASON={exc}')
        emit('IMPORTANT=SI_YA_SE_CREARON_COMMITS_NO_REPETIR_AUTOMATICAMENTE_ESTA_EJECUCION')
        return 1
    finally:
        if report_path.parent.exists():
            report_path.write_text('\n'.join(LOG) + '\n', encoding='utf-8')
            print(f'LOG={report_path}', flush=True)


if __name__ == '__main__':
    sys.exit(main())
