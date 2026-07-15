# Validación física — mapa FBD v0.4.2

## Objetivo

Validar la segunda corrección visual del mapa FBD sobre el backend RAM v2 aprobado.

La v0.4.2 conserva el mapa estable y corrige la distribución observada físicamente en v0.4.1:

```text
metadata en el encabezado
mayor altura útil del mapa
bloques ligeramente más anchos
zona exclusiva para pines
negación explícita ! + burbuja
sin contenido residual
```

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

El monitor Serial debe indicar:

```text
UI FBD v0.4.2: LISTA
```

## Programa mostrado

```text
B00  I0.0 simulada
B01  I0.1 simulada
B02  I0.2 simulada
B03  I0.3 simulada
B04  AND4: B00, B01, !B02, B03
B05  OR2: B02, LO
B06  SET/RESET: S=B04, R=B05
B07  TON B06, 2000 ms
B08  NOT B07
B09  Q0.0 lógica <- B07
B10  Q0.1 lógica <- B08
```

## Cambios visuales esperados

### Encabezado

Entre `MAPA FBD` y la insignia `RUN` debe mostrarse una línea compacta similar a:

```text
B00 1/11 X0 Y0
```

Ya no debe existir una franja informativa dentro del panel.

### Área del mapa

El panel debe comenzar en y=28 y terminar en y=154. El footer mantiene su separador en y=156, por lo que solo queda un píxel de separación visual.

### Bloques

```text
ancho: 66 px
alto: 32 px
gutter izquierdo: 17 px
```

El gutter contiene:

```text
puerto de entrada
etiqueta 1..8, S, R o T
! cuando la entrada está negada
0, 1 o X para entradas especiales
```

El título y el dato del bloque comienzan después del separador vertical del gutter. No deben superponerse con etiquetas de pines.

### Negación

La entrada negada de `B04 AND` debe verse mediante:

```text
burbuja hueca en el puerto
etiqueta !3
```

La intención es mantener la convención FBD y añadir una lectura textual inequívoca.

## Controles

```text
LEFT   fuente conectada
RIGHT  consumidor conectado
UP     bloque geográfico superior
DOWN   bloque geográfico inferior
OK     abrir/cerrar detalle
ESC    volver a IDLE
```

## Aislamiento

```text
No escribe FRAM
No usa almacenamiento A/B
No conmuta Q0 físicas
No habilita edición
No elimina periféricos del autoload
```

## Datos a registrar

```text
Flash usada
RAM global
RAM restante
salida Serial inicial
foto de B04 mostrando 1, 2, !3 y 4
foto de B06 mostrando S y R
foto de B07 mostrando T
foto con scroll horizontal hasta B08/B10
```

## Criterio de aprobación

```text
METADATA EN HEADER: OK
MAPA HASTA EL FOOTER: OK
SIN RESIDUOS: OK
SIN SUPERPOSICIÓN DE LABELS: OK
NEGACIÓN !3 LEGIBLE: OK
NAVEGACIÓN: OK
SEÑALES EN VIVO: OK
```
