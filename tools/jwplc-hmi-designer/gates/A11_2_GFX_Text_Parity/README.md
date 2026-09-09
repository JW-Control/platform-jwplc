# A11-2 — Gate físico de paridad GFX RAW

Este gate compara el renderer de texto del **JWPLC HMI Designer** contra la TFT física del **JWPLC Basic v2** usando la fuente clásica de Adafruit GFX.

A11-2 valida únicamente el comportamiento **RAW** equivalente a:

```cpp
tft.setTextSize(2);
tft.setTextColor(ST77XX_RED, ST77XX_WHITE);
tft.setCursor(20, 20);
tft.print("TEMP: 25.6 C");
```

No valida todavía cajas HMI ni padding simétrico de `JWPLC_UIField`.

## Muestra canónica

```text
Texto      : TEMP: 25.6 C
X          : 20
Y          : 20
Text size  : 2
Foreground : RED / 0xF800
Background : WHITE / 0xFFFF
Bounds GFX : 144 x 16 px
```

## Configuración equivalente en el Designer

```text
Herramienta        : Texto GFX RAW · A11-2
Contenido          : TEMP: 25.6 C
X                  : 20
Y                  : 20
Tamaño             : 2x
Foreground         : RED
Fondo de celda GFX : WHITE
```

## Qué debe observarse

La fuente clásica usa una celda lógica de `6 x 8` por carácter. El bitmap visible ocupa cinco columnas de datos y la celda conserva una columna adicional de avance/spacing.

Cuando foreground y background son distintos, el fondo pertenece a la **celda nativa GFX**, no a una caja de UI con padding simétrico.

Por eso la separación visual entre glifo y fondo puede ser asimétrica según el carácter. Esa asimetría forma parte del comportamiento RAW y el Designer debe reproducirla exactamente en A11-2.

## Criterio PASS

A11-2 puede cerrarse cuando:

- los glifos coinciden con la TFT física;
- la secuencia de celdas coincide;
- el ancho lógico coincide en 144 px;
- la altura lógica coincide en 16 px;
- foreground/background coinciden en RGB565;
- la posición inicial coincide en `x=20/y=20`;
- no hay padding simétrico artificial en el Designer RAW;
- no hay cambio de geometría al variar sólo el zoom del Designer.

No se requiere que una fotografía física tenga escala o perspectiva idéntica. La comparación es sobre patrón de píxeles, posición lógica y dimensiones.

## Separación con A11-3

A11-3 reproducirá por separado la geometría actual de los fields declarativos:

```text
FIELD_PADDING=3
FIELD_GAP=4
```

Esos márgenes pertenecen al contenedor `JWPLC_UIField`; no deben confundirse con el fondo nativo de la celda GFX validado en este gate.
