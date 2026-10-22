# Alpha13 G8 — Primera adopción, REVIEW_STOP con rollback PASS

**Fecha:** 2026-10-11 (Windows).

## Evidencia recibida

```text
G8_RUN_ID_PREVIOUS=g8_p1p2_20261011_190500_d59b4022
G8_CANDIDATE_STATUS=CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED
G8_CANDIDATE_ARCHIVE_SHA256=f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416

G8_ADOPTION_ATTEMPT=g8_adopt_20261011_192007_949944b4
G8_PHASE_PREFLIGHT=PASS
G8_SOURCE_CONTRACT_PRE=PASS
G8_PHASE_BACKUP=PASS
G8_PHASE_PRODUCT_ADOPTION=REVIEW_STOP
G8_ADOPT_REASON=DIRTY_SCOPE_NOT_EXACT_THREE_FILES
G8_ADOPT_ROLLBACK=PASS
G8_ADOPTION_STATUS=REVIEW_STOP_ROLLBACK
G8_PRODUCT_REGRESSIONS=NOT_EXECUTED
G8_PRODUCT_COMMIT=NONE
G8_SERIAL_OR_UPLOAD_OR_FRAM_ACCESS=NONE
```

**No atribuir causa sin evidencia:** El proceso copió fuentes y
archive, validó hash del candidato y abortó al comparar el conjunto
de rutas de `git diff --name-only` con las tres rutas permitidas.
El primer ejecutor no registraba la salida Git observada ni las
marcas de índice; por lo tanto, no permite determinar cuál ruta fue
omitida o añadida ni si existen flags `assume-unchanged/skip-worktree`.
No fue un fallo de compilación ni evidencia de defecto de RS-485
en ejecución física; ninguna compilación productiva se alcanzó.

El operador obtuvo `G8_ADOPT_ROLLBACK=PASS`, restaurando los tres
archivos respaldados y comprobando trabajo Git limpio, sin commit/push.

## Corrección del harness y siguiente acción

- Runner `a13_g8_adopt_local.py`: registrar rutas observadas,
  esperadas, staging, comparación `HEAD`→bytes reales y flags
  de `git ls-files -v` en fases de adopción y auditoría.
- Si algún archivo tiene marca que oculte diferencias, STOP explícito
  antes de sobrescribir producto; no cambiar silenciosamente Git.
- Comprobar tres archivos cambiados en checkout efímero de CI,
  incluyendo modificación del binario archive.
- Repetir adopción **solo** después de CI PASS de harness,
  mediante `git pull --ff-only` + BAT versionado. Si vuelve a
  detenerse, usar la salida diagnóstica, sin cambiar archivos
  ni quitar el control de tres rutas.
- `A13-010` y `A13-011` continúan
  `CANDIDATE_VALIDATED_NOT_ADOPTED`.
- Evaluación física RS-485 y commit productivo siguen pendientes.

```text
G8=CURRENT_NOT_CLOSED
G8_ADOPTION=REVIEW_STOP_ROLLBACK_PASS
G8_PRODUCT=UNCHANGED
G8_NEXT=RUN_DIAGNOSTIC_ENHANCED_ADOPTION
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
RELEASE_MERGE_PR_PUBLICATION=NO
```
