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

El Designer sí debe dejar definidos desde la interfaz:

```text
- IDs simbólicos;
- variables HMI;
- tipos C++ de variables;
- buffers de texto/capacidad;
- JWPLC_UIField[];
- estilos, formatos, colores, páginas y geometría;
- registro/configuración de JWPLC_Display.
```

La frontera manual empieza en `jwplcUIUpdate()`.

## Gates

```text
A11_0_ARCHITECTURE=PASS
A11_1_PIXEL_CANVAS=PASS
A11_2A_RAW_FONT_PARITY=PASS
A11_2B_PUBLIC_API_TEXT_FIELD=IN_PROGRESS
A11_2_TEXT=IN_PROGRESS
A11_3_PUBLIC_API_CODEGEN_CONTRACT=PASS
A11_3_FIELDS=PENDING
A11_4_CODEGEN=PENDING
A11_5_PHYSICAL_PARITY=PENDING
A11_6_SKETCH_INTEGRATION=PENDING
ALPHA11_STATUS=IN_PROGRESS
```

## A11-2A — paridad RAW cerrada como subgate

La muestra RAW:

```text
TEMP: 25.6 C
X=20
Y=20
size=2
RED sobre WHITE
bounds GFX=144x16
```

confirmó la misma fuente clásica y geometría de celda entre Designer y TFT física.

```text
A11_2A_GFX_CLASSIC_FONT=PASS
A11_2A_TEXT_SIZE_2X=PASS
A11_2A_CELL_GEOMETRY_144X16=PASS
A11_2A_PHYSICAL_VISUAL_MATCH=PASS
```

Sin embargo, la evidencia también confirma que el fondo RAW mantiene la asimetría de la celda GFX respecto al glifo. Además el gate RAW usa dibujo directo `tft.*`, por lo que no representa el contrato final del Designer.

Por tanto:

```text
RAW_FONT_PARITY_PASS_DOES_NOT_CLOSE_A11_2=YES
```

## A11-2B — validación mediante API pública

Se añade el gate:

```text
tools/jwplc-hmi-designer/gates/A11_2B_Public_API_Text_Field/
```

Este gate no usa:

```text
JWPLC_Display.tft()
tft.*
```

Usa únicamente:

```cpp
JWPLC_UITextField(...)
JWPLC_Display.setFields(...)
JWPLC_Display.setText(...)
JWPLC_Display.setUserRefreshMode(...)
JWPLC_Display.setUserPage(...)
JWPLC_Display.enterUserUI()
```

La geometría actual esperada para `TEMP: 25.6 C` con `textSize=2` y `capacity=12` es:

```text
GFX_CELL=144x16
FIELD_PADDING=3
AUTO_FIELD=150x22
```

Este gate decide si el comportamiento público actual de borde/fondo es suficiente.

Si no lo es:

```text
DESIGNER_ONLY_VISUAL_FIX=FORBIDDEN
PUBLIC_API_CHANGE_IF_REQUIRED=YES
BREAK_EXISTING_DISPLAY_API=NO
```

La corrección deberá ser aditiva y quedar expresable tanto en el runtime como en el codegen del Designer.

## Contrato de codegen público

El Designer genera definición y datos HMI mediante la API pública:

```cpp
JWPLC_UIValueField(...)
JWPLC_UITextField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)
JWPLC_Display.setFields(...)
```

Además genera las variables configuradas visualmente, por ejemplo:

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

1. ejecutar `A11_2B_Public_API_Text_Field.ino` en un JWPLC Basic;
2. observar el borde/fondo real generado por `JWPLC_UITextField`;
3. decidir `API_CHANGE_REQUIRED_FOR_TEXT_BOX=YES/NO`;
4. sólo entonces continuar con `VALUE / TEXT / BOOL / BAR` en el Designer.
