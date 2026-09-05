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

El usuario no vuelve a declarar variables ni fields: utiliza los símbolos ya generados y dentro de `jwplcUIUpdate()` decide cómo actualizar sus valores y qué llamadas `setValue()/setText()/setBool()/setBar()` realizar.

## Gates

```text
A11_0_ARCHITECTURE=PASS
A11_1_PIXEL_CANVAS=PASS
A11_2_GFX_TEXT_PARITY=PASS
A11_3_PUBLIC_API_CODEGEN_CONTRACT=PASS
A11_3_FIELDS=IN_PROGRESS
A11_4_CODEGEN=PENDING
A11_5_PHYSICAL_PARITY=PENDING
A11_6_SKETCH_INTEGRATION=PENDING
ALPHA11_STATUS=IN_PROGRESS
```

## A11-2 — cerrado

El PoC reprodujo la fuente clásica `glcdfont.c` y se comparó contra un JWPLC Basic físico con la muestra canónica:

```text
TEMP: 25.6 C
X=20
Y=20
size=2
RED sobre WHITE
bounds GFX=144x16
```

Resultado:

```text
A11_2_GFX_CLASSIC_FONT=PASS
A11_2_TEXT_SIZE_2X=PASS
A11_2_FOREGROUND_BACKGROUND=PASS
A11_2_CELL_GEOMETRY_144X16=PASS
A11_2_RAW_NO_SYMMETRIC_PADDING=PASS
A11_2_PHYSICAL_VISUAL_MATCH=PASS
A11_2_GFX_TEXT_PARITY=PASS
```

Documento de validación:

```text
A11_2_GFX_TEXT_PARITY_VALIDATION.md
```

Se mantiene:

```text
GFX_RAW_TOOL=DIAGNOSTIC_ONLY
DESIGNER_SCREEN_SAFE_AREA=REMOVED
RAW_GFX_PADDING_ARTIFICIAL=NO
JWPLC_DISPLAY_RUNTIME_CHANGE=NO
JWPLC_UI_FIELD_PADDING_CHANGE=NO
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

Ejemplo conceptual de código manual:

```cpp
extern "C" void jwplcUIUpdate()
{
    temperatura = obtenerTemperatura();
    JWPLC_Display.setValue(FIELD_TEMP, temperatura);
}
```

```text
DESIGNER_GENERATES_VARIABLE_CONTRACT=YES
DESIGNER_GENERATES_JWPLC_UI_UPDATE_BODY=NO
USER_CONFIGURES_JWPLC_UI_UPDATE=YES
API_CHANGE_REQUIRED_NOW=NO
```

`JWPLC_Display.tft()` permanece público para dibujo directo, pero `Texto GFX RAW` es sólo diagnóstico/paridad y no forma parte del codegen declarativo V1.

Documento vigente:

```text
A11_3_PUBLIC_API_CODEGEN_CONTRACT.md
```

## Pendiente inmediato — A11-3

Implementar y validar en el Designer los cuatro tipos declarativos actuales:

```text
VALUE
TEXT
BOOL
BAR
```

El renderer debe reproducir exactamente la geometría de `JWPLC_UIField`:

```text
FIELD_PADDING=3
FIELD_GAP=4
AUTO width/height
INLINE / STACKED
LEFT / CENTER / RIGHT
label / value / unit
frame / background / colors
```

Cada field dinámico debe incluir desde la interfaz su contrato de variable HMI (nombre, tipo y preview), sin generar el cuerpo de `jwplcUIUpdate()`.
