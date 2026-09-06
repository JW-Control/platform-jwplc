# JWPLC Basic v2.1.0-alpha.11 — Estado

Fecha: 2026-09-05

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
A11_3E_MULTI_FIELD_PAGES=IMPLEMENTED_PENDING_LIVE_PAGE_GATE
A11_3E_PAGE_INDICATOR=PASS_PHYSICAL
A11_3E_PAGE_BUTTON_ROUTING=PASS_PHYSICAL
A11_3E_PAGE_BOUNDARIES=PASS_PHYSICAL
A11_3E_CONTENT_BUTTON_OWNERSHIP=PASS_PHYSICAL
A11_3E_ESC_TO_SELECTOR=PASS_PHYSICAL
A11_4_CODEGEN=BLOCKED_BY_A11_3E_LIVE_PAGE_GATE
A11_5_PHYSICAL_PARITY=PENDING
A11_6_SKETCH_INTEGRATION=PENDING
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

Implementado en navegador y validado físicamente para navegación con la botonera real.

### Designer

```text
PAGE_0_ID=0
PAGE_0_NAME=Principal
MAX_PAGES_DESIGNER_ALPHA11=16
ACTIVE_PAGE_FILTERS_CANVAS=YES
ACTIVE_PAGE_FILTERS_PREVIEW=YES
ACTIVE_PAGE_FILTERS_OBJECT_LIST=YES
CODEGEN_INCLUDES_ALL_PAGES=YES
NEW_FIELDS_USE_ACTIVE_PAGE=YES
MOVE_FIELD_BETWEEN_PAGES=YES
UNDO_REDO_INCLUDES_PAGE_STATE=YES
```

La extensión:

```text
tools/jwplc-hmi-designer/poc/designer-pages.js
```

agrega panel/tabs de páginas, selector de página en Inspector y overlay compacto `NN/TT`.

BOOL y BAR se ajustaron para que el codegen recorra el conjunto global de fields y no dependa sólo de los objetos visibles en la página activa.

### Indicador físico

```text
PAGE_INDICATOR=NN/TT
POSITION=TOP_RIGHT
X=282
Y=3
W=36
H=12
PAGE_INDICATOR_FIELD=NO
```

Con una sola página no se muestra.

```text
PAGE_SELECT:
  BLACK background / WHITE text
  LEFT/RIGHT = cambiar página
  OK         = entrar
  UP/DOWN    = consumidos

PAGE_CONTENT:
  WHITE background / BLACK text
  LEFT/RIGHT/UP/DOWN/OK = usuario
  ESC                   = volver a PAGE_SELECT
```

### API pública

```cpp
JWPLC_Display.setUserPageCount(count);
JWPLC_Display.userPageCount();
JWPLC_Display.isUserPageSelection();
```

El codegen A11-3E agrega:

```cpp
JWPLC_Display.setUserPageCount(N);
JWPLC_Display.setUserPage(0);
```

### Runtime

Archivos nuevos:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI_Pages.h
JWPLC/2.1.0/libraries/JWPLC_Display/src/JWPLC_UI_Pages.cpp
```

La entrada de sistema se procesa antes del filtro dirty/forced mediante:

```text
jwplcUIRuntimeServiceInput()
```

El overlay se dibuja después de los fields para permanecer visible.

El transporte LIVE no cambia; sigue recibiendo el framebuffer ya compuesto por el Designer.

### Evidencia física de botonera

Gate usado:

```text
tools/jwplc-hmi-designer/gates/A11_Pages_Navigation/
  JWPLC_HMI_Pages_Gate/
    JWPLC_HMI_Pages_Gate.ino
```

Resultado observado en COM4:

```text
page=1/3 mode=SELECT
page=2/3 mode=SELECT
page=3/3 mode=SELECT
page=2/3 mode=SELECT
page=1/3 mode=SELECT
page=1/3 mode=CONTENT
USER RIGHT
USER LEFT
USER UP
USER DOWN
USER OK
page=1/3 mode=SELECT
page=2/3 mode=SELECT
page=2/3 mode=CONTENT
USER UP
USER LEFT
USER DOWN
USER RIGHT
USER OK
page=2/3 mode=SELECT
page=3/3 mode=SELECT
page=3/3 mode=CONTENT
USER UP
USER LEFT
USER DOWN
USER RIGHT
USER OK
page=3/3 mode=SELECT
```

Conclusión:

```text
PAGE_SELECT_LEFT_RIGHT=PASS
PAGE_BOUNDARIES=PASS
OK_ENTERS_PAGE_CONTENT=PASS
PAGE_CONTENT_USER_BUTTONS=PASS
ESC_RETURNS_TO_PAGE_SELECT=PASS
PAGE_INDICATOR_PHYSICAL=PASS
```

No se observaron cambios de página causados por LEFT/RIGHT dentro de PAGE_CONTENT.

### Limitación explícita Alpha11

```text
DECLARATIVE_FIELDS_PAGE_SCOPED=YES
PIXEL_LAYER_PAGE_SCOPED=NO
RAW_GFX_PAGE_SCOPED=NO
```

Pixel/Borrador/GFX RAW siguen siendo herramientas técnicas/globales.

Documento:

```text
docs/v2.1.0-alpha.11/A11_3E_MULTI_FIELD_PAGES_GATE.md
```

## Pendiente inmediato

El gate físico de navegación quedó cerrado. Falta únicamente el smoke físico de cambio de página por LIVE para cerrar A11-3E completo sin inferencias.

```text
LIVE_PAGE_SWITCH_PHYSICAL=PENDING
```

Prueba mínima:

```text
1. Volver a cargar JWPLC_HMI_Live_Bridge.ino.
2. Conectar LIVE desde el Designer.
3. Cambiar 01 -> 02 -> 03 -> 01 desde tabs/panel.
4. Confirmar que TFT sigue cada página y NN/TT sin corrupción.
5. Confirmar errores LIVE = 0.
```

Criterio final:

```text
A11_3E_MULTI_FIELD_PAGES=PASS
A11_3E_PAGE_INDICATOR=PASS
A11_3E_PAGE_BUTTON_ROUTING=PASS
NEXT=A11_4_CODEGEN
```
