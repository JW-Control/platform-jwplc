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

El Designer deja definidos desde la interfaz IDs simbólicos, variables HMI, tipos C++, buffers/capacidad, `JWPLC_UIField[]`, estilos, formatos, colores, páginas, geometría y registro/configuración de `JWPLC_Display`.

La frontera manual empieza en el cuerpo de `jwplcUIUpdate()`: el usuario alimenta las variables e invoca los setters públicos correspondientes.

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
A11_3D_BAR_FIELD=READY_TO_IMPLEMENT
A11_3E_MULTI_FIELD_PAGES=PENDING
A11_4_CODEGEN=PENDING
A11_5_PHYSICAL_PARITY=PENDING
A11_6_SKETCH_INTEGRATION=PENDING
ALPHA11_STATUS=IN_PROGRESS
```

## A11-2 — texto y geometría HMI

La muestra RAW confirmó la misma fuente clásica y geometría de celda entre Designer y TFT física. RAW queda sólo como herramienta técnica de referencia y no representa el contrato final del Designer.

El gate público usa únicamente:

```cpp
JWPLC_UITextField(...)
JWPLC_Display.setFields(...)
JWPLC_Display.setText(...)
JWPLC_Display.setUserRefreshMode(...)
JWPLC_Display.setUserPage(...)
JWPLC_Display.enterUserUI()
```

No usa `JWPLC_Display.tft()` ni llamadas `tft.*`.

La corrección A11-2C hizo que el layout declarativo use cuerpo nominal 5×7 sin modificar la rasterización clásica 6×8 de Adafruit GFX:

```text
layoutWidth  = gfxBoundsWidth  - textSize
layoutHeight = gfxBoundsHeight - textSize
FIELD_PADDING=3
effectivePadding=max(3,maxTextSize)
```

La prueba física source posterior mostró el borde visual balanceado.

```text
A11_2C_BALANCED_SOURCE=PASS
A11_2C_PUBLIC_API_ONLY=PASS
A11_2C_VISUAL_PADDING_BALANCED=PASS
```

## Contrato de codegen público

El Designer genera definición y datos HMI mediante la API pública:

```cpp
JWPLC_UIValueField(...)
JWPLC_UITextField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)
JWPLC_Display.setFields(...)
```

También genera las variables configuradas visualmente. No genera el cuerpo de:

```cpp
extern "C" void jwplcUIUpdate()
```

## A11-3A — TEXT

El field `TEXT` queda cerrado como base funcional del Designer.

Validado:

- definición mediante `JWPLC_UITextField(...)`;
- variable `char[]` generada por el Designer;
- geometría AUTO con métrica nominal 5×7;
- raster clásico GFX 6×8;
- padding visual balanceado en TFT física;
- `INLINE` / `STACKED`;
- `LEFT` / `CENTER` / `RIGHT`;
- colores, frame, label, unidad y capacidad;
- codegen sin `tft.*` y sin generar `jwplcUIUpdate()`.

```text
A11_3A_TEXT_FIELD=PASS
```

## UX Foundation

La validación visual realizada con A11-3A `TEXT` confirmó la arquitectura:

```text
LEFT=PAGES_OBJECTS_COMPONENTS_TOOLS
CENTER=CANVAS_320x170
RIGHT=PREVIEW_PLUS_CONTEXTUAL_INSPECTOR
BOTTOM=TECHNICAL_PANEL
ORANGE=ACTIVE_SELECTION_OR_ACTION
CANVAS_BORDER=NEUTRAL
```

Validado:

- layout completo dentro del viewport;
- inspector contextual derecho;
- lista de Objetos sincronizada con canvas;
- `INLINE` / `STACKED`;
- `LEFT` / `CENTER` / `RIGHT`;
- cambio de capacidad con geometría AUTO;
- drag con X/Y sincronizado;
- Preview 1:1 sin overlay del editor;
- cambio a GFX RAW con inspector propio;
- zoom y panel técnico colapsable.

```text
ALPHA11_UX_FOUNDATION=PASS
```

Documento:

```text
docs/v2.1.0-alpha.11/A11_UX_FOUNDATION.md
```

## UX-4 — edición

Gate cerrado:

```text
UX_4_EDITING=PASS
```

Atajos y operaciones:

```text
Ctrl+Z                 Undo
Ctrl+Y                 Redo
Ctrl+Shift+Z           Redo
Arrow keys             mover 1 px
Shift+Arrow keys       mover 10 px
Ctrl+D                 duplicar objeto seleccionado
Delete                 eliminar objeto seleccionado
Esc                    deseleccionar
```

Regla de interacción congelada:

```text
DRAG_STARTS_ONLY_ON_FIELD=YES
CLICK_EMPTY_CANVAS=DESELECT
CLICK_EMPTY_CANVAS_REPOSITIONS_FIELD=NO
```

También:

- botones `Deshacer` / `Rehacer` se habilitan según historial;
- un drag completo produce una sola entrada de historial;
- una secuencia de nudge se consolida al soltar la flecha;
- duplicar crea ID simbólico y variable HMI únicos;
- eliminar el último field deja el proyecto sin selección;
- el framebuffer puede contener varios fields en la misma página;
- el codegen preview agrega las definiciones y variables de los objetos presentes.

Este soporte multi-field es infraestructura de edición y no cierra `A11-3E`, que todavía debe definir páginas y composición multi-tipo completa.

Documento:

```text
docs/v2.1.0-alpha.11/A11_UX4_EDITING_GATE.md
```

## A11-3B — VALUE

Gate cerrado.

```text
PUBLIC_HELPER=JWPLC_UIValueField
FORMAT=JWPLC_UIValueFormat
STYLE=JWPLC_UIValueStyle
RUNTIME_SETTER=JWPLC_Display.setValue
DIRECT_TFT_CALLS=NO
SECOND_HMI_RUNTIME=NO
A11_3B_VALUE_FIELD=PASS
```

El Inspector VALUE agrega:

```text
integerDigits
decimalDigits
signedValue
leadingZeros
```

La geometría AUTO reserva el peor caso del formato, igual que el runtime. El preview implementa decimales fijos, signo permitido, leading zeros y overflow `#`.

El codegen mezcla `TEXT` y `VALUE`, declara `float` para VALUE y conserva `char[]` para TEXT.

Documento:

```text
docs/v2.1.0-alpha.11/A11_3B_VALUE_FIELD_GATE.md
```

## A11-3C — BOOL

Gate cerrado tras validación visual/funcional con `TEXT + VALUE + BOOL` y Live Preview activo.

```text
PUBLIC_HELPER=JWPLC_UIBoolField
BOOL_TEXT=JWPLC_UIBoolText
STYLE=JWPLC_UIBoolStyle
RUNTIME_SETTER=JWPLC_Display.setBool
DIRECT_TFT_CALLS=NO
SECOND_HMI_RUNTIME=NO
A11_3C_BOOL_FIELD=PASS
```

Validado visualmente:

```text
FALSE=OFF
TRUE=ON
INLINE=PASS
STACKED=PASS
ALIGN=PASS
FRAME=PASS
COLORS=PASS
DRAG=PASS
DUPLICATE=PASS
TEXT_VALUE_BOOL_COEXIST=PASS
LIVE_PREVIEW_BOOL=PASS
VISUAL_CORRUPTION=0
```

La duplicación conserva la configuración y produce ID/variable independientes. El codegen de la extensión fue contrastado con la API pública y no introduce `tft.*` ni genera `jwplcUIUpdate()`.

Documento:

```text
docs/v2.1.0-alpha.11/A11_3C_BOOL_FIELD_GATE.md
```

## LIVE Preview

El transporte físico queda congelado para Alpha11 tras la validación con Dirty Regions.

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

La última validación mostró predominio casi total de REGION sobre FULL, heap estable y cero errores.

Documento:

```text
docs/v2.1.0-alpha.11/A11_LIVE_DIRTY_REGIONS.md
```

## Pendiente inmediato

Iniciar A11-3D BAR sobre la API pública ya existente:

```text
PUBLIC_HELPER=JWPLC_UIBarField
RANGE=JWPLC_UIRange
STYLE=JWPLC_UIBarStyle
RUNTIME_SETTER=JWPLC_Display.setBar
```

Criterio siguiente:

```text
A11_3D_BAR_FIELD=PASS
NEXT=A11_3E_MULTI_FIELD_PAGES
```
