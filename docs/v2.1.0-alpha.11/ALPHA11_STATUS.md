# JWPLC Basic v2.1.0-alpha.11 — Estado

Fecha: 2026-09-06

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
```

## Frontera de responsabilidad del Designer

```text
DESIGNER_GENERATES_FIELD_DEFINITIONS=YES
DESIGNER_GENERATES_VARIABLE_DECLARATIONS=YES
DESIGNER_GENERATES_HMI_REGISTRATION=YES
DESIGNER_GENERATES_DISPLAY_CONFIGURATION=YES
DESIGNER_GENERATES_HMI_PAGE_IDS=YES
DESIGNER_GENERATES_JWPLC_UI_UPDATE=YES
USER_WRITES_JWPLC_UI_UPDATE=NO
USER_APPLICATION_LOGIC_LOCATION=loop()
```

El Designer genera `JWPLC_HMI_Generated.h` con IDs de página/campo, variables HMI, tipos C++, buffers/capacidad, `JWPLC_UIField[]`, estilos, formatos, colores, páginas, geometría, `jwplcHMISetup()` y `jwplcUIUpdate()`.

La lógica de aplicación permanece en `loop()`: botonera, sensores, E/S, Modbus, estados y cálculos modifican las variables HMI; el callback generado sincroniza sólo la página activa con los setters públicos.

## Política de desarrollo de JWPLC_Display en Alpha11

Mientras Alpha11 siga modificando el motor HMI, `JWPLC_Display` se compila desde source para evitar validar accidentalmente un archive obsoleto.

```text
ALPHA11_DISPLAY_DEVELOPMENT_MODE=SOURCE
JWPLC_DISPLAY_PRECOMPILED_ARCHIVE_ACTIVE=NO
LIBRARY_PROPERTIES_PRECOMPILED_FULL=PRESERVED
```

El archive final se regenerará al cierre del alpha y deberá repetir gates source/precompiled y build-speed.

## Gates

```text
A11_0_ARCHITECTURE=PASS
A11_1_PIXEL_CANVAS=PASS
A11_2A_RAW_FONT_PARITY=PASS
A11_2B_PUBLIC_API_TEXT_FIELD=PASS
A11_2C_BALANCED_SOURCE=PASS
A11_2_TEXT_SOURCE=PASS
A11_2_PRECOMPILED_FINAL=DEFERRED_TO_ALPHA11_CLOSE
A11_3_PUBLIC_API_CODEGEN_CONTRACT=PASS
A11_3A_TEXT_FIELD=PASS

ALPHA11_UX_FOUNDATION=PASS
UX_1_LAYOUT_BASE=PASS
UX_2_OBJECT_LIST=PASS_TEXT_BASE
UX_2_SELECTION_OVERLAY=PASS
UX_3_TEXT_INSPECTOR=PASS
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

A11_3B_VALUE_FIELD=PASS
A11_3C_BOOL_FIELD=PASS
A11_3D_BAR_FIELD=PASS
A11_3E_MULTI_FIELD_PAGES=PASS
A11_3E_PAGE_INDICATOR=PASS
A11_3E_PAGE_BUTTON_ROUTING=PASS
A11_3E_PAGE_BOUNDARIES=PASS
A11_3E_CONTENT_BUTTON_OWNERSHIP=PASS
A11_3E_ESC_TO_SELECTOR=PASS
A11_3E_LIVE_PAGE_SWITCH=PASS

A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
A11_BUTTON_PENDING_INPUT_CLEANUP=PASS_PHYSICAL

A11_4_CODEGEN_HEADER_FORMAT=PASS
A11_4_CODEGEN_PAGE_GROUPING=PASS
A11_4_IDENTIFIER_DUPLICATE_GUARD=PASS
A11_4_CODEGEN_STATIC=PASS
A11_4_CODEGEN_COMPILE=PASS
A11_4_CODEGEN_PHYSICAL=PASS
A11_4_CODEGEN=PASS

A11_5_PHYSICAL_PARITY=READY_TO_VALIDATE
A11_6_SKETCH_INTEGRATION=BLOCKED_BY_A11_5_GATE
ALPHA11_STATUS=IN_PROGRESS
```

## TEXT / VALUE / BOOL / BAR

Los cuatro tipos declarativos están cerrados:

```text
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
```

Helpers públicos:

```cpp
JWPLC_UITextField(...)
JWPLC_UIValueField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)
```

Setters públicos:

```cpp
JWPLC_Display.setText(...)
JWPLC_Display.setValue(...)
JWPLC_Display.setBool(...)
JWPLC_Display.setBar(...)
```

No se genera `tft.*` ni llamadas directas a Adafruit.

## LIVE Preview

El transporte físico queda congelado para Alpha11.

```text
SERIAL_BAUD=921600
SERIAL_RX_BUFFER=8192
FRAME_BUFFER_ROWS=32
EVENT_DRIVEN=YES
DIRTY_REGION_JWH2=YES
FLOW_CONTROL=ACK
LATEST_STATE_COALESCING=YES
VISUAL_CORRUPTION=0
LIVE_ERRORS=0
A11_LIVE_TRANSPORT=FROZEN_ALPHA11
```

La validación mostró predominio casi total de REGION sobre FULL, heap estable y cero errores.

## A11-3E — páginas

Gate cerrado en Designer, runtime físico y transporte LIVE.

```text
PAGE_SELECT_LEFT_RIGHT=PASS
PAGE_BOUNDARIES=PASS
OK_ENTERS_PAGE_CONTENT=PASS
PAGE_CONTENT_USER_BUTTONS=PASS
ESC_RETURNS_TO_PAGE_SELECT=PASS
PAGE_INDICATOR_PHYSICAL=PASS
LIVE_PAGE_01=PASS
LIVE_PAGE_02=PASS
LIVE_PAGE_03=PASS
LIVE_PAGE_INDICATOR_NN_TT=PASS
LIVE_ERRORS=0
```

Limitación explícita Alpha11:

```text
DECLARATIVE_FIELDS_PAGE_SCOPED=YES
PIXEL_LAYER_PAGE_SCOPED=NO
RAW_GFX_PAGE_SCOPED=NO
```

Documento:

```text
docs/v2.1.0-alpha.11/A11_3E_MULTI_FIELD_PAGES_GATE.md
```

## A11-4 — Codegen integral

Artefacto oficial:

```text
JWPLC_HMI_Generated.h
```

Contrato cerrado:

```text
HMIPageId=GENERATED
HMIFieldId=GENERATED
VARIABLES_HMI=GENERATED
HMI_FIELDS=GENERATED
JWPLC_HMI_SETUP=GENERATED
JWPLC_UI_UPDATE=GENERATED
APPLICATION_LOGIC=USER_LOOP
NORMAL_REFRESH_MODE=USER_REFRESH_PERIODIC
```

El `jwplcUIUpdate()` generado usa `switch (JWPLC_Display.userPage())` y sólo ejecuta setters de la página activa.

Protección de símbolos:

```text
ID_CPP_DUPLICATE_GUARD=PASS
VARIABLE_CPP_DUPLICATE_GUARD=PASS
SANITIZED_CPP_SYMBOL_COLLISION_GUARD=PASS
DUPLICATE_WARNING_INCLUDES_PAGE=PASS
```

Documento:

```text
docs/v2.1.0-alpha.11/A11_4_CODEGEN_GATE.md
```

## Robustez de botonera

Durante A11-4 se logró reproducir un fallo intermitente previamente observado en ejercicios de taller: un `loop()` cerrado consultando `pressed()` podía competir con el scanner de botonera y dejar la interacción sin respuesta.

Correcciones cerradas físicamente:

```text
BUTTON_SCAN_TASK_PRIORITY=2
BUTTON_SCAN_PERIOD_MS=5
BUTTON_SCAN_TIGHT_LOOP_NO_DELAY=PASS
BUTTON_SCAN_MULTI_RESET=PASS
SERIAL_REQUIRED=NO
USER_DELAY_REQUIRED=NO
PAGE_CONTENT_ESC_CLEANUP=PASS
PENDING_OK_NO_REENTRY=PASS
```

También se corrigió la transición `PAGE_CONTENT -> PAGE_SELECT` para limpiar latches/eventos pendientes y evitar que un `OK` viejo provoque reingreso fantasma.

Documento:

```text
docs/v2.1.0-alpha.11/A11_BUTTON_ROBUSTNESS_GATE.md
```

Este hallazgo debe mencionarse en el README final de Alpha11, explicando que el package mantiene el scanner automáticamente y que `pressed()` / `released()` pueden usarse desde `loop()` sin delays artificiales.

```text
README_BUTTON_ROBUSTNESS_NOTE=REQUIRED_AT_ALPHA11_CLOSE
```

## Pendiente inmediato

A11-4 queda cerrado. El siguiente gate es paridad física integral entre:

```text
Designer canvas 3x
Vista previa 1:1
LIVE físico
Sketch compilado desde JWPLC_HMI_Generated.h
```

Debe comprobarse la misma geometría, texto, colores, estilos, páginas y comportamiento para TEXT / VALUE / BOOL / BAR.

```text
NEXT=A11_5_PHYSICAL_PARITY
```
