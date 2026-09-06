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
A11_3A_TEXT_FIELD=IN_PROGRESS

ALPHA11_UX_FOUNDATION=IN_PROGRESS
UX_1_LAYOUT_BASE=IMPLEMENTED_PENDING_VISUAL_GATE
UX_2_OBJECT_LIST=IMPLEMENTED_TEXT_ONLY_PENDING_VISUAL_GATE
UX_2_SELECTION_OVERLAY=IMPLEMENTED_PENDING_VISUAL_GATE
UX_3_TEXT_INSPECTOR=IMPLEMENTED_PENDING_VISUAL_GATE
UX_4_TOOLBAR_SHELL=PARTIAL
UX_5_BOTTOM_PANEL=PARTIAL

A11_3B_VALUE_FIELD=BLOCKED_BY_UX_FOUNDATION
A11_3C_BOOL_FIELD=PENDING
A11_3D_BAR_FIELD=PENDING
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

También genera las variables configuradas visualmente, por ejemplo:

```cpp
float temperatura = 0.0f;
bool motorOn = false;
char estadoTexto[13] = {};
float nivel = 0.0f;
```

No genera el cuerpo de:

```cpp
extern "C" void jwplcUIUpdate()
```

## UX Foundation

Antes de continuar con `VALUE`, `BOOL` y `BAR`, Alpha11 estabiliza el shell del editor siguiendo:

```text
LEFT=PAGES_OBJECTS_COMPONENTS_TOOLS
CENTER=CANVAS_320x170
RIGHT=PREVIEW_PLUS_CONTEXTUAL_INSPECTOR
BOTTOM=TECHNICAL_PANEL
ORANGE=ACTIVE_SELECTION_OR_ACTION
CANVAS_BORDER=NEUTRAL
```

Implementado para validación visual:

- lista de Objetos separada de Componentes y Herramientas;
- propiedades `TEXT` migradas al Inspector derecho;
- Preview 1:1 conservado;
- overlay de geometría independiente del framebuffer;
- field rect, handles, X/Y y dimensiones AUTO resueltas;
- regiones label/value/unit visibles con `Geometría`;
- panel inferior colapsable;
- botón `Ajustar` funcional;
- `Generar C++` abre el contrato/código actual;
- shell de Abrir/Guardar/Undo/Redo visible pero deshabilitado hasta tener contrato funcional.

Documento:

```text
docs/v2.1.0-alpha.11/A11_UX_FOUNDATION.md
```

## Pendiente inmediato

Validar visualmente la nueva UX con A11-3A `TEXT` todavía funcional.

Criterio:

```text
ALPHA11_UX_FOUNDATION=PASS
NEXT=A11_3B_VALUE_FIELD
```

No se inicia `VALUE` hasta cerrar este gate.
