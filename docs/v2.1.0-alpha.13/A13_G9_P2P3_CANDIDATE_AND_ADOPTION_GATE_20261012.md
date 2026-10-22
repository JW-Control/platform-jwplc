# Alpha13 G9-P2/P3 — Candidato PASS y adopción controlada preparada

**Fecha:** 2026-10-12. **Estado:** `CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED`;
adopción `PREPARED_NOT_EXECUTED`; G9 y Alpha13 continúan abiertos.

## Evidencia Windows del candidato

```text
G9_CANDIDATE_RUN_ID=g9_p2p3_20261012_000449_23842b4f
G9_CANDIDATE_PREFLIGHT=PASS
G9_CANDIDATE_SOURCE_CONTRACT=PASS
G9_CANDIDATE_SOURCE_OVERLAY=PASS
G9_CANDIDATE_COMPILE_SOURCE_SD_EXIT=0
G9_CANDIDATE_SOURCE_BUILD=PASS
G9_CANDIDATE_ARCHIVE_CREATE_EXIT=0
G9_CANDIDATE_ARCHIVE_MEMBERS_EXIT=0
G9_CANDIDATE_ARCHIVE_EXTRACT_EXIT=0
G9_CANDIDATE_ARCHIVE_MEMBER_BYTE_PARITY=PASS
G9_CANDIDATE_NEW_SD_ARCHIVE_SHA256=58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e
G9_CANDIDATE_ARCHIVE_REBUILD=PASS
G9_CANDIDATE_REGRESSION_SD_BASIC=PASS
G9_CANDIDATE_REGRESSION_GLOBAL_SD=PASS
G9_CANDIDATE_REGRESSION_GLOBAL_RTC=PASS
G9_CANDIDATE_REGRESSION_GLOBAL_BUTTONS=PASS
G9_CANDIDATE_ARCHIVE_LINK_ALL_FOUR=PASS_EXACT_PATH_FLAG_SHA
G9_CANDIDATE_FINAL_AUDIT=PASS
G9_CANDIDATE_STATUS=CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED
G9_CANDIDATE_PRODUCT_CHANGE=NONE
G9_CANDIDATE_HARDWARE_ACCESS=NONE
```

Ubicación local reportada (no se presume subida a GitHub):
`tools/alpha13/results/g9_p2p3_20261012_000449_23842b4f`.
El archive original de SD en el package sigue siendo
`1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0`.
El archive **candidato** obtenido recompilando `JW_SD.cpp` es distinto:
`58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e`.
No asumir cambios de Flash/RAM o comportamiento runtime a partir de la diferencia de SHA.

## Alcance exacto permitido en adopción productiva

```text
JWPLC/2.1.0/libraries/JW_SD/src/JW_SD.cpp
JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a
JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/src/JWPLC_GlobalPeripherals.cpp
```

- A13-012: un NUL real de `JW_SD.cpp` se expresa como literal
  textual `'\0'`; sin variar la comparación.
- A13-014: flag privado RTC constante eliminado, guard
  `rtc == nullptr` conservado y refresco botonera
  expresado `constexpr` manteniendo exactamente 25 ms.
- `JW_SD` conserva `precompiled=full`, todos sus headers/APIs
  y el archive enlazado. GlobalPeripherals continúa compilando
  desde fuente. Ninguna poda de periféricos ni cambio de autoload.

## Ejecutable preparado

```text
tools/alpha13/gates/a13_g9_adopt_local.py
tools/alpha13/gates/run_a13_g9_adopt.bat
tools/alpha13/gates/a13_g9_adopt_selftest.py
.github/workflows/alpha13-g9-adoption-readonly.yml
```

Autoprueba CI [38209971080](https://github.com/JW-Control/platform-jwplc/actions/runs/38209971080):
**Windows y Linux PASS**, sin modificar el checkout.
En un repositorio Git temporal se validaron index flags,
rutas POSIX en Windows y el rechazo de una sola ruta
dirty o archivos staged. No se efectuó upload/COM/SD.

### Secuencia que ejecutará el BAT

1. Verificar Git limpio, rama, HEAD remoto, ancestro de CI,
   toolchain Arduino CLI 1.5.1 SHA fijado, core local,
   manifests exactos de P0 y P2/P3, hashes de archive,
   objetos y candidatos.
2. Reproducir desde las fuentes originales las transformaciones
   exactas (incluyendo CRLF Windows) y cotejarlas con los
   candidatos por bytes.
3. Respaldar y verificar SHA de los tres archivos.
4. Sustituir únicamente las tres rutas y comprobar
   `git diff` exacto, índice normal `H`, sin stage.
5. Compilar cuatro ejemplos con `--libraries` normal,
   **sin overlays de librerías**; comprobar source object
   `JWPLC_GlobalPeripherals.cpp.o`, ausencia de
   `JW_SD.cpp.o`, bandera `-lJW_SD`, ruta y SHA del
   archive nuevo y enlace core precompilado.
6. Repetir hashes/auditoría final y emitir
   `PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_COMMIT`.
   En fallo después del backup: restauración exacta y
   auditoría Git; nunca hacer commit automático.

**La adopción no incluye** prueba física microSD,
escritura FRAM, acceso USB/COM, upload, git add/commit/push,
merge, PR ni release. Los dos JWPLC utilizados en G8
conservan sus firmwares de diagnóstico, sin restauración
automática desde este gate.

### Comandos de operador, solo cuando se soliciten

```powershell
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g9_adopt.bat
```

Si termina PASS, **no ejecutar otro `git pull`** hasta
el gate de commit de producto, ya que los tres archivos
quedarán intencionalmente modificados localmente.

```text
G9_P0=CLOSED_PASS_READ_ONLY
G9_P1=A13-017_DOC_ONLY_PASS
G9_P2P3_CANDIDATE=PASS
G9_P2P3_ADOPTION=PREPARED_NOT_EXECUTED
G9_PRODUCT_COMMIT=NONE
G9_P2P3_PHYSICAL=NOT_REQUESTED
G9_FINAL=IN_PROGRESS
ALPHA13=IN_PROGRESS
RELEASE_PR_MERGE=NO
```
