# JWPLC Basic v2.1.0-alpha.11 — Estado

Fecha de cierre técnico: 2026-09-09

## Rama

```text
v2.1.0-alpha.11/feature/hmi-designer
```

## Alcance

```text
ALPHA11_SCOPE=JWPLC_HMI_DESIGNER_V1
TARGET_DISPLAY=ST7789_320x170_ROT3
EXISTING_DISPLAY_API=PROTECTED
SECOND_HMI_RUNTIME=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
OPENPLC_RUNTIME_AUTOLOAD=NO
```

Alpha11 consolida JWPLC HMI Designer V1, la API declarativa `JWPLC_UI`, navegación multipágina, LIVE Preview, codegen, integración con sketch, robustez de botonera, integración Windows/Arduino IDE y cierre de precompilados asociados.

## Contrato Designer / sketch

```text
DESIGNER_GENERATES_PAGE_IDS=YES
DESIGNER_GENERATES_FIELD_IDS=YES
DESIGNER_GENERATES_VARIABLE_DECLARATIONS=YES
DESIGNER_GENERATES_FIELD_DEFINITIONS=YES
DESIGNER_GENERATES_HMI_REGISTRATION=YES
DESIGNER_GENERATES_DISPLAY_CONFIGURATION=YES
DESIGNER_GENERATES_JWPLC_UI_UPDATE=YES
USER_WRITES_JWPLC_UI_UPDATE=NO
USER_APPLICATION_LOGIC_LOCATION=loop()
GENERATED_HEADER=JWPLC_HMI_Generated.h
PROJECT_EXTENSION=.jwhmi
```

El Designer genera la capa de presentación. El usuario conserva en `loop()` la lógica de proceso, sensores, E/S, comunicaciones y botones.

## Gates funcionales

```text
A11_0_ARCHITECTURE=PASS
A11_1_PIXEL_CANVAS=PASS
A11_2A_RAW_FONT_PARITY=PASS
A11_2B_PUBLIC_API_TEXT_FIELD=PASS
A11_2C_BALANCED_SOURCE=PASS
A11_2_TEXT_SOURCE=PASS
A11_2_PRECOMPILED_FINAL=PASS
A11_3_PUBLIC_API_CODEGEN_CONTRACT=PASS
A11_3A_TEXT_FIELD=PASS
A11_3B_VALUE_FIELD=PASS
A11_3C_BOOL_FIELD=PASS
A11_3D_BAR_FIELD=PASS
A11_3E_MULTI_FIELD_PAGES=PASS
A11_4_CODEGEN=PASS
A11_5_PHYSICAL_PARITY=PASS_USER_VISUAL

ALPHA11_UX_FOUNDATION=PASS
UX_1_LAYOUT_BASE=PASS
UX_2_OBJECT_LIST=PASS
UX_2_SELECTION_OVERLAY=PASS
UX_3_INSPECTOR=PASS
UX_4_EDITING=PASS
UX_4_UNDO_REDO=PASS
UX_4_KEYBOARD_NUDGE=PASS
UX_4_DUPLICATE_DELETE=PASS
UX_5_BOTTOM_PANEL=PASS_BASE

A11_LIVE_WEB_SERIAL=PASS
A11_LIVE_EVENT_DRIVEN=PASS
A11_LIVE_DIRTY_REGION_JWH2=PASS
A11_LIVE_LATEST_STATE_COALESCING=PASS
A11_LIVE_DIAGNOSTIC_PANEL=PASS
A11_LIVE_PHYSICAL_GATE=PASS
A11_LIVE_TRANSPORT=FROZEN_ALPHA11

A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
A11_BUTTON_PENDING_INPUT_CLEANUP=PASS_PHYSICAL
A11_6_DESKTOP_LAUNCHER=PASS_USER_WINDOWS
A11_6_PROJECT_SAVE_OPEN=PASS_USER_WINDOWS
A11_6_SKETCH_LINK=PASS_USER_WINDOWS
A11_6_HEADER_DIRECT_WRITE=PASS_USER_WINDOWS
A11_6_LIVE_FROM_DESKTOP_APP=PASS_USER_WINDOWS
A11_6_RESPONSIVE_WIDE=PASS_USER_VISUAL
A11_6_RESPONSIVE_MEDIUM_50_PERCENT=PASS_USER_VISUAL
A11_6_FIT_CONTINUOUS=PASS_USER_VISUAL
A11_6_PROJECT_CANONICAL_SAVE=PASS
A11_6_STANDALONE_INSTALLER=PASS_NATIVE_ENTRYPOINT
A11_6_ARDUINO_IDE_ICON=PASS_USER_2_3_4
A11_6_ARDUINO_IDE_LAUNCHER=PASS_EXPERIMENTAL_2_3_4
A11_6_ARDUINO_IDE_AUTOCOMPLETE=PASS_USER_2_3_4
A11_6_SKETCH_INTEGRATION=PASS
```

## Fields y PixelMap

```text
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
PIXELMAP=PASS
PIXELMAP_PACKED_SPAN16=PASS
PIXELMAP_VISIBILITY_RUNTIME=PASS
MAX_FIELDS=32
MAX_PAGES_DESIGNER=16
```

El codegen puede seleccionar `PACKED_SPAN16` cuando reduce memoria/código y la paleta cabe en 16 colores. La persistencia `.jwhmi` conserva el proyecto del Designer y el header generado permanece separado de la lógica del `.ino`.

## Navegación multipágina

```text
VISIBLE_INDICATOR=NN/TT
PAGE_IDS_INTERNAL=0_BASED
VISIBLE_PAGE_NUMBERS=1_BASED
```

Semántica física:

```text
PAGE_SELECT
  BLACK background / WHITE text
  LEFT/RIGHT -> cambiar página
  OK         -> PAGE_CONTENT

PAGE_CONTENT
  WHITE background / BLACK text
  LEFT/RIGHT/UP/DOWN/OK -> aplicación
  ESC                    -> PAGE_SELECT
```

La transición `CONTENT -> SELECT` limpia eventos pendientes y resincroniza el estado físico para evitar reingresos fantasma.

## Robustez de botonera y runtime cerrado

Alpha11 cerró dos problemas distintos relacionados con loops intensivos:

1. escaneo de botonera bajo `pressed()/released()/isDown()` sin `delay()` ni Serial;
2. starvation del runtime al ejecutar `digitalWrite(Q0_0, estado)` continuamente desde `loop()`.

La segunda incidencia expuso que `precompiled/core/JWPLCBASIC/core.a` estaba desfasado respecto del source Alpha11. El source ya contenía:

```text
TCA6424A_OUTPUT_SHADOW=YES
REDUNDANT_OUTPUT_WRITE_I2C=NO
JWPLC_SYSTEM_TASK_PRIORITY=2
```

pero el archive versionado todavía correspondía a un estado anterior.

Se regeneró el core desde `cores/jwcontrol`, se verificó el target normal `jwcontrol_precompiled_stub + core.a` y se repitió la prueba física con el sketch original.

Resultado:

```text
ALPHA11_RUNTIME_CLOSED_LOOP=PASS_PHYSICAL
ALPHA11_DIGITALWRITE_REPEATED_STATE=PASS_PHYSICAL
ALPHA11_USER_DELAY_REQUIRED=NO
ALPHA11_RTC_SERVICE=PASS_PHYSICAL
ALPHA11_DISPLAY_SERVICE=PASS_PHYSICAL
ALPHA11_CORE_PRECOMPILED_SYNC=PASS
```

Core final:

```text
COMMIT=3cf37145d4555689b9bff80c8b3793128bb9090e
ARCHIVE=JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
ARCHIVE_BYTES=3019320
ARCHIVE_SHA256=6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68
SOURCE_BUILD_JWCONTROL_TUS=64
NORMAL_BUILD_JWCONTROL_TUS=0
NORMAL_BUILD_STUB_TUS=1
CORE_PRECOMPILED_BUILD=PASS
CORE_PRECOMPILED_VERIFY_BASIC=PASS
```

## JWPLC_Display precompilado final

El archive final de Display ya había sido cerrado antes del hotfix del core.

```text
COMMIT=4142f801fbacc9388bf63c3a6352696522d6b445
ARCHIVE=JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
ARCHIVE_BYTES=849596
ARCHIVE_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
DISPLAY_TUS=6
ARCHIVE_MEMBERS_EXACT=PASS
PRECOMPILED_DISPLAY_SOURCE_TUS=0
SOURCE_ARCHIVE_EMPTY_PARITY=PASS
SOURCE_ARCHIVE_HMI_PARITY=PASS
ALPHA11_DISPLAY_FINAL_ARCHIVE=PASS
```

Los cambios posteriores al archive en `JWPLC_Display` fueron de interfaz/header y autocompletado; no modificaron los `.cpp` contenidos en `libJWPLC_Display.a`.

## Benchmark final

Herramienta:

```text
tools/build-speed-benchmark/Run-JWPLCBuildBenchmark.ps1
```

Resultado:

```text
TOTAL_PHASES=72
FAILED_PHASES=0
ALPHA11_BUILD_BENCHMARK_3X=PASS
Basic cold compilers=15
Core cold compilers=78
Warm compilers=1
ALPHA11_COMPILER_STRUCTURE_PARITY=PASS
ALPHA11_WARM_PERFORMANCE_STABLE=PASS
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
ALPHA11_EXACT_SPEEDUP_CLAIM=NOT_USED
```

Alpha11 preserva el rendimiento warm de Alpha10 dentro de la variación normal del host. El beneficio defendible de `libJWPLC_Display.a` es evitar recompilar sus TUs, no una afirmación artificial de aceleración porcentual global.

Documento:

```text
docs/v2.1.0-alpha.11/ALPHA11_BUILD_BENCHMARK.md
```

## HMI Designer Windows

Arquitectura instalada:

```text
INSTALL_ROOT=%LOCALAPPDATA%\JWPLC\HMI Designer
NATIVE_ENTRYPOINT=JWPLC-HMI-Designer.exe
LOCAL_SERVER_INTERNAL=YES
CUSTOM_URI_PROTOCOL=jwplc-hmi://open
PROJECT_EXTENSION=.jwhmi
SKETCH_REQUIRES_INO=YES
HEADER_OVERWRITE_CONFIRM=YES
INO_AUTOMATIC_MODIFICATION=NO
```

Convención:

```text
MiProyecto/
├─ MiProyecto.ino
├─ MiProyecto.jwhmi
└─ JWPLC_HMI_Generated.h
```

## Arduino IDE 2.3.4

El VSIX experimental no modifica ni forkea Arduino IDE. La versión final Alpha11 es:

```text
JWPLC_HMI_LAUNCHER_VERSION=0.1.6
VSIX=%USERPROFILE%\.arduinoIDE\plugins\jwplc-hmi-launcher-0.1.6.vsix
TARGET_IDE=2.3.4
TARGET_THEIA=1.41.x
```

Además del launcher, 0.1.6 ofrece autocompletado contextual curado para `JWPLC_Display`:

```text
JWPLC_Display. -> API recomendada
setIdleWakeMode( -> IDLE_WAKE_*
setIdleWakeButton( -> BTN_*
setIdleReturnMode( -> IDLE_RETURN_*
setIdleReturnButton( -> BTN_*
setUserRefreshMode( -> USER_REFRESH_*
```

La extensión evita sugerir aliases/getters redundantes para mantener una API práctica. Las APIs compatibles continúan existiendo aunque no se prioricen en el autocompletado.

El instalador del VSIX quedó desacoplado de la presencia del Designer: el autocompletado puede instalarse aunque la aplicación todavía no esté disponible; la falta del Designer sólo afecta al botón de lanzamiento.

```text
ALPHA11_AUTOCOMPLETE_DISPLAY=PASS_USER
ALPHA11_IDE_COMPILE_UPLOAD_REGRESSION=0
```

## Inicialización TFT

Alpha11 añade estabilización de arranque:

```text
TFT_CS_INITIAL=HIGH
TFT_RST_HELD_LOW_DURING_AUTOLOAD=YES
FIRST_IDLE_FRAME_IMMEDIATE_AFTER_DISPLAY_BEGIN=YES
```

El objetivo es reducir el contenido indeterminado visible durante power-on. La funcionalidad normal de TFT/IDLE quedó validada; la eliminación visual absoluta del ruido durante la fase previa al firmware se considera dependiente también del hardware/backlight y no bloquea el cierre funcional.

## Decisiones heredadas que no cambian

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
OPENPLC_RUNTIME_AUTOLOAD=NO
```

No se publica `bootloader.bin` como definitivo y no se declara una configuración universal futura de FlashFreq.

## Estado de cierre

```text
ALPHA11_FUNCTIONAL_SCOPE=PASS
ALPHA11_DESIGNER_V1=PASS_USER
ALPHA11_DISPLAY_PRECOMPILED=PASS
ALPHA11_CORE_PRECOMPILED=PASS
ALPHA11_BUILD_SPEED=PASS_WITH_HOST_VARIATION
ALPHA11_RUNTIME_REGRESSION_GATE=PASS_PHYSICAL
ALPHA11_AUTOCOMPLETE=PASS_USER
ALPHA11_TECHNICAL_CLOSURE=PASS
ALPHA11_PR_READY=YES
ALPHA11_PUBLICATION=PENDING_PR_MERGE
ALPHA11_STATUS=TECHNICALLY_CLOSED
```

Siguiente paso:

```text
PR_ES -> release/v2.1.x
CI_GREEN_REQUIRED
MERGE
PRE_RELEASE_ES
PUBLISHED_PACKAGE_ISOLATED_VALIDATION
```
