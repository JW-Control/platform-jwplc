# Alpha11 — A11-2C métricas de texto HMI balanceadas

Fecha: 2026-09-05

## Evidencia física

Con `JWPLC_UITextField()` y `textSize=2`, la muestra `TEMP: 25.6 C` mostró aproximadamente:

```text
margen superior  = 3 px
margen izquierdo = 3 px
margen inferior  = 5 px
```

La causa no está en el Designer. El runtime actual usa `Adafruit_GFX::getTextBounds()` sobre la fuente clásica; para la fuente por defecto Adafruit mide una celda fija `6x8` por carácter. El field agrega además `FIELD_PADDING=3`.

Para texto clásico nominal `5x7` a `textSize=2`, la fila libre de la celda aporta 2 px adicionales abajo. El spacer horizontal final hace lo mismo en el extremo derecho cuando el último glifo ocupa sus cinco columnas.

## Decisión

No se añadirá una corrección visual exclusiva en el Designer.

La HMI declarativa debe calcular su geometría con el cuerpo nominal `5x7`, manteniendo la celda `6x8` únicamente como detalle de rasterización de Adafruit GFX.

```text
DESIGNER_ONLY_FIX=NO
PUBLIC_HMI_RUNTIME_FIX=YES
PUBLIC_API_SIGNATURE_CHANGE=NO
```

La API pública existente (`JWPLC_UITextField`, `JWPLC_UIValueField`, `JWPLC_UIBoolField`, `JWPLC_UIBarField`) es suficiente; se corrige el comportamiento interno de geometría.

## Regla propuesta

Para textos no vacíos de la fuente clásica:

```text
layoutWidth  = gfxCellWidth  - textSize
layoutHeight = gfxCellHeight - textSize
```

Esto elimina del cálculo de layout:

- la sexta columna de avance posterior al último carácter;
- la octava fila reservada por la celda clásica.

El padding visual mínimo sigue siendo 3 px. Para tamaños mayores se usa:

```text
effectivePadding = max(3, textSize)
```

para asegurar que una posible fila 8 utilizada por descenders permanezca dentro del field.

La limpieza de valores dinámicos debe cubrir verticalmente una escala adicional para evitar restos de descenders al cambiar el contenido.

## Alcance tipográfico

El balance se define respecto al cuerpo nominal `5x7` de la fuente clásica. Caracteres con descenders pueden utilizar parte del padding inferior; esto es comportamiento tipográfico válido y no debe provocar escritura fuera del rectángulo del field.

## Gate esperado

Para:

```text
TEXT capacity=12
textSize=2
x=20
y=20
FIELD_PADDING=3
```

la geometría AUTO esperada pasa de:

```text
150 x 22
```

a:

```text
148 x 20
```

La muestra física debe mostrar nominalmente 3 px arriba, izquierda, derecha e inferior para `TEMP: 25.6 C`.

## Implementación experimental

Se añade un parche reproducible:

```text
tools/jwplc-hmi-designer/gates/A11_2C_Balanced_Public_API/Apply-A11-2C-TightMetrics.ps1
```

El parche modifica únicamente el runtime `JWPLC_UI.cpp`; no cambia firmas públicas.

A11-2 no se vuelve a cerrar hasta validar físicamente este comportamiento usando exclusivamente API pública.
