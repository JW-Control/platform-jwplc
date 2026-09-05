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

## Gates

```text
A11_0_ARCHITECTURE=PASS
A11_1_PIXEL_CANVAS=PASS
A11_2_GFX_TEXT_PARITY=IN_PROGRESS
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

A11-3 reproducirá por separado `JWPLC_UIField`, incluyendo `FIELD_PADDING=3`, `FIELD_GAP=4`, layouts y geometría AUTO.
