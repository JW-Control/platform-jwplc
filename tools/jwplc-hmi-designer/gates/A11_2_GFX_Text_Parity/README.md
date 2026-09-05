# A11-2 — Gate físico de paridad GFX

Este gate compara el renderer de texto del **JWPLC HMI Designer** contra la TFT física del **JWPLC Basic v2** usando la fuente clásica de Adafruit GFX.

## Muestra canónica

```text
Texto      : TEMP: 25.6 C
Text size  : 2
Foreground : RED / 0xF800
Background : WHITE / 0xFFFF
Bounds     : 144 x 16 px
```

El sketch tiene dos páginas:

```text
LEFT  -> página 0 -> RAW edge -> x=0, y=0
RIGHT -> página 1 -> safe area -> x=3, y=3
```

La página `0` demuestra que el API GFX crudo no introduce margen implícito.

La página `1` valida la recomendación visual de 3 px adoptada por el Designer sin modificar las coordenadas reales.

## Configuración equivalente en el Designer

Para comparar página 0:

```text
Contenido : TEMP: 25.6 C
X         : 0
Y         : 0
Tamaño    : 2x
Foreground: RED
Fondo     : WHITE
```

Para comparar página 1:

```text
Contenido : TEMP: 25.6 C
X         : 3
Y         : 3
Tamaño    : 2x
Foreground: RED
Fondo     : WHITE
```

## Criterio PASS

A11-2 puede cerrarse cuando:

- los glifos coinciden visualmente con la TFT física;
- el ancho lógico coincide en 144 px;
- la altura lógica coincide en 16 px;
- página 0 aparece pegada a `x=0/y=0`, sin offset automático;
- página 1 aparece exactamente 3 px desplazada en X e Y;
- no hay cambio de geometría al variar sólo el zoom del Designer.

No se requiere que la fotografía física tenga escala o perspectiva idéntica; la comparación es sobre el patrón de píxeles, posición lógica y dimensiones.
