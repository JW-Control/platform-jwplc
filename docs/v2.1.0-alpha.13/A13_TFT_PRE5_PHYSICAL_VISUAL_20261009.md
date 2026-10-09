# Alpha13 — TFT-PRE5 R3 — Validación física y visual del arranque ST7789

Fecha: 2026-10-09

## Resultado

```text
GATE=A13-TFT-PRE5
ATTEMPT=R3
GATE_STATUS=PASS
REASON=SOURCE_FIRST_CANDIDATE_FLASHED
VISUAL_DIRTY_WHITE_BACKGROUND=NOT_OBSERVED_ON_CANDIDATE
VISUAL_CONFIRMATION=USER_REPORTED_WITH_VIDEO_FRAMES
VISUAL_IDLE_PHASED_RENDERING=STILL_VISIBLE
PERMANENT_PACKAGE_FIX=NOT_YET_INTEGRATED
```

**Interpretación:** se valida el comportamiento del **firmware de prueba**
con un backend temporal, no el archivo `libJWPLC_TFT.a` publicado dentro
del package. El usuario confirma: «ahora ya no sale el fondo blanco, se
mantiene limpio». Las cinco capturas muestran la transición de panel limpio
a IDLE dibujado por fases, sin el fondo blanco/GRAM irregular del baseline.
No se dispone de prueba documentada de múltiples placas ni de un gate
adicional de regresión con el archive productivo.

## Identidad y procedencia

```text
HEAD=2a2d2943db22c69d2da6cc8377f0341a5e6f8ddb
SERIAL_PORT=COM4
ARDUINO_CLI=1.0.2
TEMP_CPP_CANDIDATE_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
TEMP_SETUP_CANDIDATE_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
TEMP_ST7789_INIT_CANDIDATE_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
RUNTIME_CANDIDATE_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
INSTRUMENTED_BACKEND_CPP_SHA256=ae34e2f98aad1ea066e365ffef425e173062e3c3cd9a8cbd708ec6d36f804cb2
INSTRUMENTED_INIT_SHA256=01a9761f8542f945737f113a4f2ff6c17e63926693d8143b9ea55543b2612b4b
```

El backend temporal se identificó frente a TFT_eSPI 2.5.43:
`ST7789_Init.h` modificado, `TFT_eSPI.cpp` sólo instrumentado para
verificar propagación de `ST7789_DRIVER` y
`JWPLC_TFT_DEFER_DISPON`. La instalación global quedó intacta.

## Cadena de compilación, subida y serial

```text
A13_TFT_PRE5_SYNTAX=PASS
PROBE_INO_COUNT=1
BACKEND_CONFIG_GUARD=ENABLED
BACKEND_SOURCE_CONFIG_GUARD=ENABLED
COMPILE_EXIT=0
BACKEND_SETUP_DISCOVERY=SKETCH_LOCAL_TFT_SETUP
JWPLC_TFT_TEMP_SELECTED=True
TFT_ESPI_TEMP_SELECTED=True
JWPLC_TFT_SOURCE_OBJECT_COUNT=1
TFT_ESPI_SOURCE_OBJECT_COUNT=1
JWPLC_TFT_PRECOMPILED=False
JWPLC_DISPLAY_PRECOMPILED=True
USES_STUB_CORE=True
CORE_A_LINKED=True
UPLOAD_EXIT=0
CLIENT_EXIT=0
DISPLAY_READY=YES
IO_READY=YES
TFT_RST_OUTPUT_ENABLE=YES
TFT_RST_OUTPUT_LATCH=HIGH
TFT_CS_OUTPUT_ENABLE=YES
TFT_CS_OUTPUT_LATCH=HIGH
STALE_BASELINE_SERIAL_ACCEPTED=NO
```

## Tiempo medido

| Caso | Entrada a `setup()` |
| --- | ---: |
| TFT-PRE1 package baseline | 664 ms |
| TFT-PRE2 instrumentado | 664.542 ms |
| TFT-PRE5 R3 firmware candidato | 676 ms |

Diferencia entre PRE1 y PRE5: +12 ms nominales. Una sola medición por
caso no justifica afirmaciones estadísticas ni conclusiones de optimización.

## Integridad del package

```text
CORE_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
TFT_ARCHIVE_SHA256=5d860a131811dd9a7eb6fa55f5674b1d78b0de7dfaf8748ce18a60ceed2d3738
DISPLAY_ARCHIVE_SHA256=c960d718433e29a40e3cc55bc745c9a2e121ee1ee598ec72c04872327592dc02
PRODUCT_REPO_MUTATED=NO
INSTALLED_TFT_ESPI_MUTATED=NO
WORKTREE_FINAL=CLEAN
DIFF_CHECK_FINAL=True
```

**Importante:** el archive productivo `libJWPLC_TFT.a` aún contiene la
secuencia previa. Si se compila un sketch normal del package sin el
candidato temporal, el fondo blanco puede reaparecer.

## Causa y corrección candidata

```text
BASELINE:
  init TFT_eSPI -> DISPON -> delay(120) -> clear/draw IDLE

CANDIDATE:
  init TFT_eSPI (DISPON diferido) -> fillScreen(BLACK)
  -> DISPON -> delay(120) -> draw IDLE
```

No se redujeron delays del ST7789, no se movieron RTC/FRAM/SD/botones
y no se alteraron APIs de usuario.

El dibujado escalonado de IDLE permanece por diseño, visible en el video
durante la transición. No se abre un nuevo alcance `TFT_NEW_FEATURES`.

## Siguiente paso y condición de cierre productivo

```text
NEXT_GATE=TFT-PRE6
OBJECTIVE=REPRODUCIBLE_MAINTAINER_BACKEND_BUILD_AND_ARCHIVE_REFRESH
PRODUCT_SCOPE=JWPLC_TFT_SOURCE_AND_PRECOMPILED_ARCHIVE_ONLY
NO_GLOBAL_TFT_ESPI_PATCH=YES
NO_API_CHANGE=YES
NO_PUBLISH_YET=YES
```

1. Versionar una **receta de mantenimiento** con fuente TFT_eSPI 2.5.43
   de identidad comprobada, aplicar el parche al backend **en copia
   temporal**, compilar `JWPLC_TFT.cpp.o` y `TFT_eSPI.cpp.o`,
   reconstruir el archive `libJWPLC_TFT.a` con miembros controlados.
2. Exigir que un build normal de Arduino IDE/CLI enlace el nuevo archive
   sin instalar ni compilar TFT_eSPI del usuario.
3. Ejecutar regresiones de Display, HMI/consumers de Alpha12 y la prueba
   física normal precompiled; comprobar el arranque visual **con el archive
   real**, no con el sketch de prueba temporal.
4. Auditar fuente/archivo binario, `git diff --check`, commit de producto y
   documentación. No pasar a G3/G4 sin cerrar esta observación.

Este documento cierra PRE5 como experimento positivo, no el fix productivo.
