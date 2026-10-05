# Alpha12 — cierre de migración de backend TFT

Fecha: 2026-10-03

## Contexto

Durante el cierre del package Alpha12 se detectó que la migración:

```text
JWPLC_Display -> JWPLC_TFT
```

estaba funcionalmente integrada en el runtime, pero no todos los consumers
distribuidos habían sido migrados al nuevo tipo gráfico.

Contrato anterior:

```cpp
Adafruit_ST7789 &JWPLC_Display.tft();
Adafruit_ST7789 &JWPLC_Display.display();
```

Contrato actual:

```cpp
JWPLC_TFTClass &JWPLC_Display.tft();
JWPLC_TFTClass &JWPLC_Display.display();
```

## Hallazgo

Se encontraron referencias actuales a:

```text
Adafruit_ST7789
ST77XX_*
```

en:

- `JWPLC_LogicRuntime_UI`;
- renderers FBD históricos que todavía se compilan como parte de la librería;
- renderers unified/U4;
- ejemplos actuales de `JWPLC_Display`.

Esto es un blocker de package porque `JWPLC_LogicRuntime_UI`, aunque mantenga
partes experimentales, se distribuye en `JWPLC/2.1.0/libraries` y debe seguir
compilando.

## Corrección

Los consumers actuales fueron migrados a:

```text
JWPLC_TFTClass

JWPLC_TFT_BLACK
JWPLC_TFT_WHITE
JWPLC_TFT_RED
JWPLC_TFT_GREEN
JWPLC_TFT_BLUE
JWPLC_TFT_YELLOW
JWPLC_TFT_CYAN
JWPLC_TFT_MAGENTA
```

No se reintroduce `Adafruit_ST7789` como dependencia pública.

No se modificaron documentos históricos ni la librería Adafruit vendorizada
sólo para ocultar referencias antiguas.

## Commits de migración

```text
7c117617a11e7a7d365aecd50d24c4bf9a4a4bc6
  base widgets/screens -> JWPLC_TFT

e7d9074eac0843412b58634ff0003915f0025e71
  FBD legacy V2/V3/V4/V5 -> JWPLC_TFT

b4a761465b18270b1156f0bd8220b8ae1ce0cadb
  FBD V5/V7/V8/V10/V11 -> JWPLC_TFT

2f17144b8b27b8660bfbdbab7ede9939743bd315
  active/unified renderers -> JWPLC_TFT

80a1c38c2cb1ab82750bdd35be9cbe8986539dd7
  unified editor/U4 -> JWPLC_TFT

3f6d695e255f1f9c387ff33848616178836c50b6
  U4 + ejemplos principales Display

f7ccfffd72f8242110820360a762d5b07ab2d472
  resto de ejemplos Display -> colores JWPLC_TFT
```

## Auditoría estática

Se revisaron todos los `.h/.hpp/.cpp/.ino` actuales bajo:

```text
JWPLC/2.1.0/libraries/JWPLC_LogicRuntime_UI/
JWPLC/2.1.0/libraries/JWPLC_Display/examples/
```

Resultado:

```text
CURRENT_PRODUCT_CONSUMER_ADAFRUIT_ST7789_REFS=0
CURRENT_PRODUCT_CONSUMER_ST77XX_REFS=0
STATIC_TFT_BACKEND_MIGRATION=PASS
```

Las referencias que permanezcan en documentación histórica o en la propia
librería Adafruit vendorizada no representan consumers actuales del contrato
`JWPLC_Display.tft()`.

## Cobertura CI

Se amplió:

```text
.github/workflows/ci-jwplc-package-smoke.yml
```

con:

```text
JWPLC_LogicRuntime_UI_Home
JWPLC_LogicRuntime_UI_FBD_Unified_Map_Detail_RAM
```

Commit:

```text
1d7df145f59e7627f5b6907377a8fb75370990d4
```

Objetivo: una futura migración de backend no puede cerrar CI compilando sólo el
productor Display y omitiendo su librería dependiente.

## Gate local source-first

Se creó:

```text
tools/alpha12/gates/alpha12_tft_backend_compile.ps1
```

Commit:

```text
b2afc8639a80722f53f2cc631c7d53ed5fd62038
```

Compila:

1. `04.Display_TFT_Direct`;
2. `Display_UserUI_Callbacks`;
3. `JWPLC_LogicRuntime_UI_Home`;
4. `JWPLC_LogicRuntime_UI_FBD_Unified_Map_Detail_RAM`.

Además exige objetos source de:

```text
JWPLC_TFT.cpp.o
JWPLC_Display.cpp.o
JWPLC_LogicRuntime_UI.cpp.o
RuntimeUIWidgets.cpp.o
```

según corresponda, y rechaza markers de librería precompilada para esas
librerías.

## Relación con prevención de fallos

Nueva familia:

```text
F081=BACKEND_MIGRATION_LEFT_DISTRIBUTED_CONSUMERS_STALE
```

Regla:

```text
BACKEND_CHANGE
-> SEARCH_DISTRIBUTED_CONSUMERS
-> MIGRATE_PRODUCT_CODE_AND_CURRENT_EXAMPLES
-> SOURCE_FIRST_DEPENDENT_LIBRARY_COMPILE
-> CI_COVERAGE
-> ONLY_THEN_PRECOMPILE
```

## Estado

```text
STATIC_MIGRATION=PASS
CI_COVERAGE_UPDATED=PASS
LOCAL_SOURCE_FIRST_GATE=PREPARED
LOCAL_SOURCE_FIRST_GATE_RESULT=PENDING
PRECOMPILED_REGEN=BLOCKED
PHYSICAL_GATE_REQUIRED_NOW=NO
```

No regenerar `libJWPLC_Display.a`, `libJWPLC_TFT.a` ni otros archives
afectados hasta obtener PASS del gate source-first.
