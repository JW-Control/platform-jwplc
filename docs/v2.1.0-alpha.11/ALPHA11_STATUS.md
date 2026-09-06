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
A11_3E_MULTI_FIELD_PAGES=READY_TO_IMPLEMENT
A11_4_CODEGEN=BLOCKED_BY_A11_3E_GATE
A11_5_PHYSICAL_PARITY=PENDING
A11_6_SKETCH_INTEGRATION=PENDING
ALPHA11_STATUS=IN_PROGRESS
```

## A11-2 — texto y geometría HMI

La muestra RAW confirmó la misma fuente clásica y geometría de celda entre Designer y TFT física. RAW queda sólo como herramienta técnica de referencia y no representa el contrato final del Designer.

La corrección A11-2C hizo que el layout declarativo use cuerpo nominal 5×7 sin modificar la rasterización clásica 6×8 de Adafruit GFX:

```text
layoutWidth  = gfxBoundsWidth  - textSize
layoutHeight = gfxBoundsHeight - textSize
FIELD_PADDING=3
effectivePadding=max(3,maxTextSize)
```

```text
A11_2C_BALANCED_SOURCE=PASS
A11_2C_PUBLIC_API_ONLY=PASS
A11_2C_VISUAL_PADDING_BALANCED=PASS
```

## Contrato de codegen público

El Designer usa únicamente helpers públicos:

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

Gate cerrado.

```text
A11_3A_TEXT_FIELD=PASS
```

Validado:

- variable `char[]`;
- geometría AUTO;
- INLINE / STACKED;
- LEFT / CENTER / RIGHT;
- colores y borde;
- capacidad;
- codegen público sin `tft.*`;
- sin generar `jwplcUIUpdate()`.

## UX Foundation / UX-4

Arquitectura validada:

```text
LEFT=PAGES_OBJECTS_COMPONENTS_TOOLS
CENTER=CANVAS_320x170
RIGHT=PREVIEW_PLUS_CONTEXTUAL_INSPECTOR
BOTTOM=TECHNICAL_PANEL
ORANGE=ACTIVE_SELECTION_OR_ACTION
CANVAS_BORDER=NEUTRAL
```

Edición congelada:

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

Regla de interacción:

```text
DRAG_STARTS_ONLY_ON_FIELD=YES
CLICK_EMPTY_CANVAS=DESELECT
CLICK_EMPTY_CANVAS_REPOSITIONS_FIELD=NO
```

## A11-3B — VALUE

Gate cerrado.

```text
PUBLIC_HELPER=JWPLC_UIValueField
FORMAT=JWPLC_UIValueFormat
STYLE=JWPLC_UIValueStyle
RUNTIME_SETTER=JWPLC_Display.setValue
A11_3B_VALUE_FIELD=PASS
```

El preview replica decimales fijos, signo, leading zeros y overflow `#`. La geometría reserva el peor caso del formato.

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
A11_3C_BOOL_FIELD=PASS
```

Validado:

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
LIVE_PREVIEW_BOOL=PASS
VISUAL_CORRUPTION=0
```

Documento:

```text
docs/v2.1.0-alpha.11/A11_3C_BOOL_FIELD_GATE.md
```

## A11-3D — BAR

Gate cerrado tras validación visual final en navegador.

```text
PUBLIC_HELPER=JWPLC_UIBarField
RANGE=JWPLC_UIRange
STYLE=JWPLC_UIBarStyle
RUNTIME_SETTER=JWPLC_Display.setBar
A11_3D_BAR_FIELD=PASS
```

Validado:

- rango mínimo/máximo;
- valor de prueba y porcentaje normalizado;
- ancho AUTO;
- ancho FIJO con recálculo de región;
- altura BAR de 12 px según runtime;
- STACKED;
- etiqueta y unidad;
- borde y colores;
- drag y geometría;
- coexistencia con otros fields;
- Preview 1:1.

Corrección de cierre:

```text
BAR_GENERIC_TEST_VALUE_CONTROL=HIDDEN
BAR_RANGE_TEST_VALUE_CONTROL=VISIBLE
BAR_CAPACITY_CONTROL=HIDDEN
```

El único `Valor de prueba` de BAR pertenece a `Rango BAR`.

Documento:

```text
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

Documento:

```text
docs/v2.1.0-alpha.11/A11_LIVE_DIRTY_REGIONS.md
```

## A11-3E — páginas

Gate definido y listo para implementación.

```text
PAGE_0_ID=0
PAGE_0_NAME=Principal
MAX_PAGES_ALPHA11=8
ACTIVE_PAGE_FILTERS_CANVAS=YES
ACTIVE_PAGE_FILTERS_OBJECT_LIST=YES
CODEGEN_INCLUDES_ALL_PAGES=YES
NEW_FIELDS_USE_ACTIVE_PAGE=YES
MOVE_FIELD_BETWEEN_PAGES=YES
LIVE_PROTOCOL_CHANGE=NO
```

El alcance incluye crear/cambiar páginas, filtrar canvas/Preview/Objetos por página activa y conservar el `pageId` correcto en cada field generado.

No se incorpora lógica automática de navegación dentro de `jwplcUIUpdate()`.

Documento:

```text
docs/v2.1.0-alpha.11/A11_3E_MULTI_FIELD_PAGES_GATE.md
```

## Pendiente inmediato

Implementar A11-3E y validar una composición con al menos dos páginas y los cuatro tipos declarativos.

Criterio:

```text
A11_3E_MULTI_FIELD_PAGES=PASS
NEXT=A11_4_CODEGEN
```
