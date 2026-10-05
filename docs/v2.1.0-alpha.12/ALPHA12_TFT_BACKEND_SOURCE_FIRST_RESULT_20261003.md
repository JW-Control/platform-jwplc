# Alpha12 — resultado gate source-first TFT / LogicRuntime UI

Fecha: 2026-10-03

## Gate

Script:

```text
tools/alpha12/gates/alpha12_tft_backend_compile.ps1
```

Resultado:

```text
ALPHA12_TFT_BACKEND_COMPILE=PASS
PASS=4
FAIL=0
SOURCE_FIRST=PASS
PHYSICAL_UPLOAD_PERFORMED=NO
```

HEAD probado:

```text
03c42f46ecf1d326d4a9d722572cc0a72e58a699
```

Evidencia local:

```text
tools/alpha12/results/tft_backend_compile_20261003_221426
```

## Casos

### DISPLAY_TFT_DIRECT

```text
COMPILE_EXIT=0
TFT_SOURCE_OBJECT_COUNT=1
DISPLAY_SOURCE_OBJECT_COUNT=1
TFT_PRECOMPILED_MARKER=NO
DISPLAY_PRECOMPILED_MARKER=NO
CLASS=PASS
```

### DISPLAY_USER_CALLBACKS

```text
COMPILE_EXIT=0
TFT_SOURCE_OBJECT_COUNT=1
DISPLAY_SOURCE_OBJECT_COUNT=1
TFT_PRECOMPILED_MARKER=NO
DISPLAY_PRECOMPILED_MARKER=NO
CLASS=PASS
```

### LOGIC_UI_HOME

```text
COMPILE_EXIT=0
TFT_SOURCE_OBJECT_COUNT=1
DISPLAY_SOURCE_OBJECT_COUNT=1
LOGIC_UI_SOURCE_OBJECT_COUNT=1
LOGIC_UI_WIDGET_OBJECT_COUNT=1
TFT_PRECOMPILED_MARKER=NO
DISPLAY_PRECOMPILED_MARKER=NO
LOGIC_UI_PRECOMPILED_MARKER=NO
CLASS=PASS
```

### LOGIC_UI_UNIFIED_FBD

```text
COMPILE_EXIT=0
TFT_SOURCE_OBJECT_COUNT=1
DISPLAY_SOURCE_OBJECT_COUNT=1
LOGIC_UI_SOURCE_OBJECT_COUNT=1
LOGIC_UI_WIDGET_OBJECT_COUNT=1
TFT_PRECOMPILED_MARKER=NO
DISPLAY_PRECOMPILED_MARKER=NO
LOGIC_UI_PRECOMPILED_MARKER=NO
CLASS=PASS
```

## Revisión del ZIP

Los cuatro logs fueron revisados.

Resultado:

```text
COMPILER_WARNING_COUNT=0
COMPILER_ERROR_COUNT=0
ADAFRUIT_ST7789_TOKEN_COUNT=0
ST77XX_TOKEN_COUNT=0
```

Los markers precompilados presentes pertenecieron a otras librerías del package:

```text
JW_FRAM
SD
FS
JW_MatrixButtons
Wire
```

No aparecieron markers precompilados para:

```text
JWPLC_TFT
JWPLC_Display
JWPLC_LogicRuntime_UI
```

## Dependencia TFT_eSPI observada

Durante source-first:

```text
TFT_eSPI 2.5.43
PATH=C:\Users\jeykc\Documentos\Programacion\Arduino\libraries\TFT_eSPI
```

Esto no invalida el gate: la finalidad era demostrar la compatibilidad del
source actual de JWPLC_TFT/Display/LogicRuntime_UI.

Sí genera una obligación de cierre:

1. la regeneración del archive final JWPLC_TFT debe fijar explícitamente la
   versión/fuente de TFT_eSPI usada;
2. CI/package no debe depender accidentalmente de una librería global del host;
3. el package publicado debe continuar ocultando TFT_eSPI al usuario final si
   ése es el contrato decidido;
4. la paridad source/archive debe probarse antes del freeze final.

## Conclusión

```text
STATIC_TFT_BACKEND_MIGRATION=PASS
SOURCE_FIRST_TFT_BACKEND_COMPILE=PASS
F081_PREVENTION_GATE=PASS
PRECOMPILED_REGEN_TFT_DISPLAY=UNBLOCKED_FOR_PREPARATION
PHYSICAL_UPLOAD_REQUIRED_FOR_THIS_GATE=NO
```

El siguiente paso no es un gate físico.

Se debe auditar la matriz completa de sources modificados vs archives
precompilados y regenerar únicamente después de fijar qué artifacts requieren
rebuild y qué dependencia TFT_eSPI se usará de forma reproducible.
