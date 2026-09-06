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
DESIGNER_GENERATES_JWPLC_UI_UPDATE=NO
USER_WRITES_JWPLC_UI_UPDATE=YES
```

El Designer define IDs, variables HMI, tipos C++, buffers/capacidad, `JWPLC_UIField[]`, estilos, formatos, colores, páginas, geometría y registro de `JWPLC_Display`.

La frontera manual empieza en `jwplcUIUpdate()`: el usuario alimenta las variables e invoca los setters públicos correspondientes.

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
A11_4_CODEGEN_HEADER_FORMAT=IMPLEMENTED_PENDING_GATE
A11_4_CODEGEN_PAGE_GROUPING=IMPLEMENTED_PENDING_GATE
A11_4_IDENTIFIER_DUPLICATE_GUARD=IMPLEMENTED_PENDING_GATE
A11_4_CODEGEN=READY_TO_VALIDATE
A11_5_PHYSICAL_PARITY=BLOCKED_BY_A11_4_GATE
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

No se genera `tft.*` ni el cuerpo de `jwplcUIUpdate()`.

Documentos:

```text
docs/v2.1.0-alpha.11/A11_3B_VALUE_FIELD_GATE.md
docs/v2.1.0-alpha.11/A11_3C_BOOL_FIELD_GATE.md
docs/v2.1.0-alpha.11/A11_3D_BAR_FIELD_GATE.md
```

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

El artefacto recomendado queda separado del sketch de usuario:

```text
JWPLC_HMI_Generated.h
```

El output ahora prepara directamente:

```cpp
#pragma once
#include <JWPLC_Display.h>
```

Se agrupan por comentarios de página:

```text
HMIFieldId
Variables HMI
Setters públicos comentados
```

Ejemplo:

```cpp
// Página 01 · Principal
// Página 02 · Proceso
// Página 03 · Diagnóstico
```

Se agregó protección global contra duplicados editados manualmente en Inspector:

```text
ID_CPP_DUPLICATE_GUARD=YES
VARIABLE_CPP_DUPLICATE_GUARD=YES
SANITIZED_CPP_SYMBOL_COLLISION_GUARD=YES
DUPLICATE_WARNING_INCLUDES_PAGE=YES
```

La creación y duplicación automática ya producían nombres únicos; la protección nueva cubre la edición manual y una validación defensiva antes del codegen.

Documento:

```text
docs/v2.1.0-alpha.11/A11_4_CODEGEN_GATE.md
```

## Pendiente inmediato

Validar visualmente A11-4 tras `Ctrl+F5`:

```text
1. Confirmar #pragma once + include en Código generado.
2. Confirmar comentarios Página NN · Nombre en HMIFieldId.
3. Confirmar comentarios de página en Variables HMI.
4. Confirmar comentarios de página en setters finales.
5. Intentar duplicar un ID C++ entre páginas y verificar aviso con página origen.
6. Intentar duplicar una variable C++ entre páginas y verificar aviso con página origen.
```

Después usar el header generado en Arduino IDE y cerrar compile/link/physical.

```text
NEXT=A11_4_CODEGEN_STATIC_GATE
```
