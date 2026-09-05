# Alpha11 — Decisión sobre fondo de celda GFX RAW

Fecha: 2026-09-05
Estado: vigente

## Contexto

Durante la validación visual de A11-2 se observó que, con texto rojo sobre fondo blanco y `textSize=2`, el fondo visible alrededor de ciertos glifos no tiene un margen simétrico.

La observación correcta no corresponde al borde físico de la TFT. Corresponde a la relación entre el bitmap del glifo clásico de Adafruit GFX y su celda de texto.

## Modelo RAW validado

Para la fuente clásica usada en Alpha11 V1:

```text
GLYPH_DATA_COLUMNS=5
CELL_WIDTH=6
CELL_HEIGHT=8
TEXT_SCALE=integer
```

El Designer A11-2 reproduce una celda lógica de `6 x 8` por carácter.

Cuando foreground y background son distintos, los píxeles apagados de esa celda se dibujan con el background. Ese background no debe interpretarse como un contenedor de UI con padding simétrico.

La columna adicional de la celda sirve como spacing/avance horizontal. La distribución vertical de píxeles apagados depende del bitmap del carácter. Por eso el espacio visible alrededor de un glifo puede ser asimétrico.

## Decisión

```text
A11_GFX_RAW_CELL_PARITY=REQUIRED
A11_GFX_RAW_SYMMETRIC_PADDING=NO
A11_GFX_RAW_BACKGROUND=GFX_CELL_BACKGROUND
A11_GFX_RAW_CURSOR_OFFSET=NO
DESIGNER_SCREEN_SAFE_AREA=NO
JWPLC_DISPLAY_RUNTIME_CHANGE_FOR_A11_2=NO
```

A11-2 no intentará embellecer el fondo RAW.

El objetivo del gate es que:

```text
Designer RAW == Adafruit GFX RAW == TFT física
```

## Interfaz del Designer

Para evitar ambigüedad, el control antes llamado `Fondo` pasa a mostrarse como:

```text
Fondo de celda GFX
```

La UI de A11-2 también debe indicar explícitamente:

```text
Padding simétrico: NO · modo RAW
```

## Relación con JWPLC_UIField

La HMI declarativa actual tiene un contenedor diferente y ya usa:

```text
FIELD_PADDING=3
FIELD_GAP=4
```

Ese padding pertenece al `JWPLC_UIField`, no al glifo GFX.

Alpha11 no modifica `FIELD_PADDING` durante A11-2.

A11-3 deberá reproducir exactamente la caja y geometría del runtime actual antes de decidir si una futura API necesita hacer configurable el padding.

## Mejora visual futura

Si posteriormente se desea un objeto de texto con fondo visualmente simétrico, debe modelarse como:

```text
background box + padding + GFX text
```

no como un offset oculto aplicado a `setCursor()` ni como una alteración del renderer RAW.

Esa posible evolución se evaluará después de reproducir los fields existentes en A11-3.
