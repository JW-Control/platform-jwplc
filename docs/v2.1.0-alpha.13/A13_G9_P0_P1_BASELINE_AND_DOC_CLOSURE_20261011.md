# Alpha13 G9 — P0 baseline y P1 documentación activa

**Fecha:** 2026-10-11 (P0 CI concluido el 2026-10-12 UTC).
**Estado:** `G9-P0=PASS_CI_READ_ONLY`, `G9-P1=A13-017_DOC_ONLY_PASS`.
**Producto SD y GlobalPeripherals:** NO MODIFICADO; P2/P3 pendientes.

## P0 — Baseline verificable de la auditoría canónica

Runner:
`tools/alpha13/gates/a13_g9_p0_readonly.py`.
CI Windows/Linux:
[38204046284](https://github.com/JW-Control/platform-jwplc/actions/runs/38204046284)
**PASS en ambos jobs** con historia Git completa, Git limpio
y commit producto G8 validado como ancestro.

Salida CI Windows del checkout HEAD
`feb06862d5723c10a6998fd7682c2cd2e043198c`:

```text
G9_P0_SOURCE_SD_NUL_COUNT=1
G9_P0_SOURCE_SD_NUL_OFFSETS=[28732]
G9_P0_SD_ARCHIVE_SHA256=1b9619ba37295782ade1edc6f06439a38e7e534fc08f5b05757c9af7a645acc0
G9_P0_GLOBAL_RTC_REFS=3
G9_P0_GLOBAL_THROTTLE_REFS=2
G9_P0_README_OUTDATED_ALPHA11=True
G9_P0_STATUS=BASELINE_CAPTURED_READ_ONLY
```

Se confirma que el NUL real de A13-012 persiste en el
offset documentado originalmente y que `JW_SD` es
`precompiled=full` con `libJW_SD.a` de SHA fijado.
Para A13-014 constan tres referencias a `g_rtcEnabled`
y dos a `g_buttonRefreshThrottleMs`.
La captura P0 no equivale a corrección de ambos defectos.

El primer intento CI P0, [run 38203940681](https://github.com/JW-Control/platform-jwplc/actions/runs/38203940681),
se detuvo por checkout superficial (`fetch-depth` implícito 1),
que impide comprobar el commit ancestro G8; **fallo de
infraestructura de prueba, no fallo productivo**. Se
corrigió solo el checkout a `fetch-depth: 0`, manteniendo
la comprobación de procedencia; retest Windows/Linux PASS.
No se crea F-ID adicional por el reintento de esta misma causa.

## P1 — A13-017 documentación activa corregida

Se aplicaron únicamente estos tres cambios:

1. `README.md`: indicar Alpha12 como última publicada
   y `ALPHA13_STATUS=IN_PROGRESS`, retirando la afirmación
   activa contradictoria sobre Alpha11.
2. `JWPLC_ModbusTCP.h`: sustituir promesa de cierre
   futuro de sockets A14.2 por el contrato actual
   verificado de `beginServer()`: misma configuración
   idempotente, otra configuración devuelve BUSY, no
   existe `EthernetServer::stop()` de escucha.
3. `JW_SD.h`: sustituir el texto futuro G2b por
   la descripción real del autoservicio de DataLogs
   mediante `jwplcDataLogTickCallback()`, demostrado
   en `JWPLC_GlobalPeripherals.cpp`.

**Verificación textual:** comparación con HEAD de G8
`8bf81eb0b8bed628091c6c5bf3691bb140590301`.
En ambos headers, **el contenido de líneas no-comentario
permanece idéntico** al baseline; se comprobaron
expresamente los marcadores nuevos de README, las
referencias futuras retiradas y el runtime ya existente.
No se cambió código ejecutable, API ni archive.

```text
A13-017=DOC_ONLY_CORRECTION_APPLIED_AND_AUDITED
A13-012=PENDING_SD_SOURCE_AND_ARCHIVE
A13-014=PENDING_GLOBALPERIPHERALS_EQUIVALENCE
G9-P0=CI_PASS_LOCAL_CAPTURE_AVAILABLE
G9-P1=CLOSED_PASS_DOC_TEXT_CONTRACT
G9-P2=NOT_STARTED
G9-P3=NOT_STARTED
G9_FINAL=IN_PROGRESS
ALPHA13=IN_PROGRESS
RELEASE_MERGE_PR_PUBLICATION=NO
```

## Siguiente puerta

Ejecutar G9-P0 de solo lectura en Windows del operador,
para demostrar paridad del HEAD y archivo SD local antes de
preparar P2/P3. **No repetir G8, no hacer upload, no
usar reset/clean de Git, no modificar periféricos ni APIs**.

Después de P0 local PASS, preparar un único ejecutor
integrado de P2+P3 con variables/fases aisladas, fallback
de rollback y validaciones reales fuente→objeto→archive.
