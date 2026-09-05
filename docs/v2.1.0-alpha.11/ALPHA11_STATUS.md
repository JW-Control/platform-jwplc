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
A11_2_GFX_TEXT_PARITY=IN_PROGRESS
A11_3_PUBLIC_API_CODEGEN_CONTRACT=PASS
A11_3_FIELDS=PENDING
A11_4_CODEGEN=PENDING
A11_5_PHYSICAL_PARITY=PENDING
A11_6_SKETCH_INTEGRATION=PENDING
ALPHA11_STATUS=IN_PROGRESS
```

## A11-2 implementado hasta ahora

El PoC contiene una implementación de texto basada en el bitmap clásico `glcdfont.c` incluido en el package.

```text
ASCII_PRINTABLE=32..126
GLYPH_DATA_COLUMNS=5
CELL=6x8
TEXT_SIZE=1x..4x
FOREGROUND=RGB565
BACKGROUND=GFX_CELL_BACKGROUND
SYMMETRIC_PADDING=NO
LIVE_EDIT=YES
CLICK_DRAG_POSITION=YES
PREVIEW_1_TO_1=YES
```

La implementación replica el principio de la fuente clásica de Adafruit GFX: cinco columnas de bitmap dentro de una celda lógica de seis columnas por ocho filas, escalada por un entero.

## Corrección de interpretación A11-2

Durante A11-2 se añadió temporalmente una `safe area` por interpretar una observación como margen respecto al borde físico de la TFT.

La observación real corresponde al margen interno entre el glifo y el fondo de su celda GFX.

```text
DESIGNER_SCREEN_SAFE_AREA=REMOVED
RAW_GFX_PADDING_ARTIFICIAL=NO
JWPLC_DISPLAY_RUNTIME_CHANGE=NO
JWPLC_UI_FIELD_PADDING_CHANGE=NO
```

Documentos:

```text
A11_2_SAFE_AREA_DECISION.md                 -> SUPERSEDED
A11_2_GFX_CELL_BACKGROUND_DECISION.md       -> VIGENTE
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

`JWPLC_Display.tft()` permanece público para dibujo directo, pero `Texto GFX RAW` de A11-2 es sólo diagnóstico/paridad y no forma parte del codegen declarativo V1.

Documento vigente:

```text
A11_3_PUBLIC_API_CODEGEN_CONTRACT.md
```

## Pendiente inmediato

Ejecutar el gate físico con la muestra canónica:

```text
TEMP: 25.6 C
X=20
Y=20
size=2
RED sobre WHITE
bounds GFX=144x16
```

Comparar:

1. Designer RAW;
2. `JWPLC_Display.tft()` / Adafruit GFX en JWPLC Basic físico.

A11-2 sólo pasa después de esa comparación.

Después, A11-3 reproducirá `JWPLC_UIField` mediante la API pública, incluyendo `FIELD_PADDING=3`, `FIELD_GAP=4`, layouts y geometría AUTO.
