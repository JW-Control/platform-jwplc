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

El PoC ya contiene una primera implementación de texto basada en el bitmap clásico `glcdfont.c` incluido en el package.

```text
ASCII_PRINTABLE=32..126
GLYPH_BITMAP=5_columns_x_8_bits
CELL=6x8
TEXT_SIZE=1x..4x
FOREGROUND=RGB565
BACKGROUND=RGB565
LIVE_EDIT=YES
CLICK_DRAG_POSITION=YES
PREVIEW_1_TO_1=YES
```

La implementación replica el principio de `Adafruit_GFX::drawChar()` para la fuente clásica: cinco columnas de bitmap y una sexta columna de spacing, escaladas por un entero.

## Pendiente inmediato

Validar visualmente el nuevo editor de texto y preparar una muestra idéntica para:

1. Designer;
2. `JWPLC_Display.tft()` / Adafruit GFX en JWPLC Basic físico.

El gate A11-2 sólo pasa después de esa comparación.
