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
DESIGNER_GENERATES_HMI_REGISTRATION=YES
DESIGNER_GENERATES_DISPLAY_CONFIGURATION=YES
DESIGNER_GENERATES_JWPLC_UI_UPDATE=NO
DESIGNER_GENERATES_DYNAMIC_BINDINGS=NO
USER_OWNS_JWPLC_UI_UPDATE=YES
```

El Designer termina antes de `jwplcUIUpdate()`.

Su salida debe dejar disponibles IDs simbólicos estables (`FIELD_TEMP`, `FIELD_RUN`, etc.) y la definición/configuración HMI. El usuario es responsable de vincular esos IDs con variables y lógica de aplicación mediante `JWPLC_Display.setValue()`, `setText()`, `setBool()` y `setBar()` donde corresponda.

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

## Corrección de interpretación

Durante A11-2 se interpretó inicialmente una observación como falta de margen respecto al borde físico de la TFT y se añadió temporalmente una `safe area` de 3 px.

La observación real corresponde al margen interno entre el glifo y el **fondo de su celda GFX**.

Por tanto:

```text
DESIGNER_SCREEN_SAFE_AREA=REMOVED
RAW_GFX_PADDING_ARTIFICIAL=NO
JWPLC_DISPLAY_RUNTIME_CHANGE=NO
JWPLC_UI_FIELD_PADDING_CHANGE=NO
```

La safe area fue retirada del PoC antes del cierre del gate y la decisión quedó registrada en:

```text
A11_2_SAFE_AREA_DECISION.md                 -> SUPERSEDED
A11_2_GFX_CELL_BACKGROUND_DECISION.md       -> VIGENTE
```

## Contrato de codegen público fijado

El código generado para el usuario debe usar la API pública declarativa:

```cpp
JWPLC_UIValueField(...)
JWPLC_UITextField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)

JWPLC_Display.setFields(...)
```

El Designer también puede generar las llamadas de configuración seleccionadas visualmente, por ejemplo:

```cpp
JWPLC_Display.setUserRefreshMode(...);
JWPLC_Display.setUserPage(...);
JWPLC_Display.setIdleWakeMode(...);
JWPLC_Display.setIdleReturnMode(...);
```

La frontera termina ahí.

No forman parte del codegen del Designer:

```text
jwplcUIUpdate()
JWPLC_Display.setValue(...)
JWPLC_Display.setText(...)
JWPLC_Display.setBool(...)
JWPLC_Display.setBar(...)
bindings a variables de aplicación
```

Esas llamadas siguen siendo la API pública correcta para el usuario, pero su uso pertenece al código manual de aplicación.

`JWPLC_Display.tft()` permanece público para dibujo directo, pero `Texto GFX RAW` de A11-2 es una herramienta de diagnóstico/paridad y no formará parte del codegen declarativo V1.

La API actual ya expresa el alcance V1, por lo que:

```text
API_CHANGE_REQUIRED_NOW=NO
DESIGNER_PROPERTY_WITHOUT_PUBLIC_CODEGEN=FORBIDDEN
```

Documento vigente:

```text
A11_3_PUBLIC_API_CODEGEN_CONTRACT.md
```

Cualquier texto anterior que describa generación automática de bindings dentro de `jwplcUIUpdate()` queda sustituido por este contrato.

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

Después, A11-3 reproducirá `JWPLC_UIField` mediante la API pública, incluyendo el comportamiento actual de `FIELD_PADDING=3`, `FIELD_GAP=4`, layouts y geometría AUTO.
