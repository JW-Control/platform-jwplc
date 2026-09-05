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

El Designer debe dejar definidos desde la interfaz:

```text
- IDs simbólicos;
- variables HMI;
- tipos C++ de variables;
- buffers de texto/capacidad;
- JWPLC_UIField[];
- estilos, formatos, colores, páginas y geometría;
- registro/configuración de JWPLC_Display.
```

La frontera manual empieza en el cuerpo de `jwplcUIUpdate()`: el usuario alimenta las variables e invoca los setters públicos correspondientes.

## Política de desarrollo de JWPLC_Display en Alpha11

Mientras Alpha11 siga modificando el motor HMI, `JWPLC_Display` se compila desde source para evitar validar accidentalmente un archive obsoleto.

```text
ALPHA11_DISPLAY_DEVELOPMENT_MODE=SOURCE
JWPLC_DISPLAY_PRECOMPILED_ARCHIVE_ACTIVE=NO
LIBRARY_PROPERTIES_PRECOMPILED_FULL=PRESERVED
```

El archive final se regenerará únicamente al cierre del alpha y deberá repetir gates source/precompiled y build-speed.

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
A11_3B_VALUE_FIELD=PENDING
A11_3C_BOOL_FIELD=PENDING
A11_3D_BAR_FIELD=PENDING
A11_3E_MULTI_FIELD_PAGES=PENDING
A11_4_CODEGEN=PENDING
A11_5_PHYSICAL_PARITY=PENDING
A11_6_SKETCH_INTEGRATION=PENDING
ALPHA11_STATUS=IN_PROGRESS
```

## A11-2 — texto y geometría HMI

### A11-2A — paridad RAW

La muestra RAW confirmó la misma fuente clásica y geometría de celda entre Designer y TFT física:

```text
TEMP: 25.6 C
X=20
Y=20
size=2
RED sobre WHITE
bounds GFX=144x16
```

```text
A11_2A_GFX_CLASSIC_FONT=PASS
A11_2A_TEXT_SIZE_2X=PASS
A11_2A_CELL_GEOMETRY_144X16=PASS
A11_2A_PHYSICAL_VISUAL_MATCH=PASS
```

RAW queda sólo como herramienta técnica de referencia. No representa el contrato final del Designer porque usa la celda GFX 6×8 y dibujo directo.

### A11-2B — gate por API pública

El gate público usa únicamente:

```cpp
JWPLC_UITextField(...)
JWPLC_Display.setFields(...)
JWPLC_Display.setText(...)
JWPLC_Display.setUserRefreshMode(...)
JWPLC_Display.setUserPage(...)
JWPLC_Display.enterUserUI()
```

No usa:

```text
JWPLC_Display.tft()
tft.*
```

La primera validación física confirmó que el padding fijo de 3 px se veía como aproximadamente 3 px arriba/izquierda y 5 px abajo debido a la fila/spacing nativo de la celda clásica 6×8.

### A11-2C — métricas balanceadas

Se corrigió `JWPLC_UI.cpp` para que el layout declarativo use cuerpo nominal 5×7 sin cambiar la rasterización de Adafruit GFX.

```text
layoutWidth  = gfxBoundsWidth  - textSize
layoutHeight = gfxBoundsHeight - textSize
```

El padding efectivo es:

```text
max(FIELD_PADDING, maxTextSize)
FIELD_PADDING=3
```

La prueba física source posterior mostró el borde visual balanceado.

```text
A11_2C_BALANCED_SOURCE=PASS
A11_2C_PUBLIC_API_ONLY=PASS
A11_2C_VISUAL_PADDING_BALANCED=PASS
```

Validación detallada:

```text
docs/v2.1.0-alpha.11/A11_2C_BALANCED_SOURCE_VALIDATION.md
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

El usuario implementa ese callback y utiliza las variables e IDs ya generados.

```text
DESIGNER_GENERATES_VARIABLE_CONTRACT=YES
DESIGNER_GENERATES_JWPLC_UI_UPDATE_BODY=NO
USER_CONFIGURES_JWPLC_UI_UPDATE=YES
```

## Pendiente inmediato

A11-3A activa `TEXT field` en el Designer y debe reproducir exactamente la geometría pública vigente:

```text
FIELD_PADDING=3 mínimo
effectivePadding=max(3,maxTextSize)
FIELD_GAP=4
AUTO width/height
INLINE / STACKED
LEFT / CENTER / RIGHT
label / value / unit
frame / colors
text capacity
```

El preview debe usar las mismas métricas nominales 5×7 del runtime corregido y mantener la rasterización clásica 6×8 dentro del field.
