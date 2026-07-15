# Validación física — mapa FBD v0.4.3

## Objetivo

Validar la continuidad visual del mapa FBD cuando existen bloques fuera o parcialmente fuera del viewport.

La v0.4.3 conserva las mejoras de v0.4.2 y añade:

```text
bloques laterales resumidos
indicadores de borde Bxx TIPO
cables recortados contra el viewport
rutas separadas para entradas múltiples
stubs para entradas X, HI y LO
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
UI FBD v0.4.3: LISTA
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

## Bloques en los bordes

Se mantienen tres modos de representación:

```text
COMPLETO     caja, título, dato y pines
PARCIAL      porción visible de la caja y título resumido
INDICADOR    pestaña compacta Bxx TIPO con flecha de dirección
```

Un bloque ubicado hasta una columna o fila fuera del mapa debe dejar un indicador en el borde. Las flechas usadas son:

```text
<  bloque hacia la izquierda
>  bloque hacia la derecha
^  bloque hacia arriba
v  bloque hacia abajo
```

La selección continúa marcada en amarillo y el estado lógico TRUE en verde.

## Cableado

Los cables ya no dependen de que la fuente y el consumidor estén completamente visibles.

Cada enlace se divide en:

```text
segmento horizontal desde la fuente
segmento vertical de ruta
segmento horizontal hacia la entrada
```

Cada segmento se recorta de manera independiente contra el rectángulo del mapa.

Para bloques con varias entradas, cada pin recibe un carril vertical diferente dentro del espacio entre columnas. En `B04 AND` deben distinguirse los cuatro recorridos correspondientes a:

```text
1   B00
2   B01
!3  B02 negada
4   B03
```

La entrada especial `LO` de `B05 OR` debe conservar su etiqueta `0` y mostrar un stub corto hacia el pin.

## Encabezado

La metadata usa el formato compacto:

```text
B00 1/11 0,0
```

para evitar invadir la insignia RUN.

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
foto inicial con B03 o siguiente bloque indicado en el borde
foto de B04 con sus cuatro cables distinguibles
foto desplazada mostrando continuidad de un cable desde fuera del mapa
foto con B07, B08, B09 y B10 en la navegación horizontal
```

## Criterio de aprobación

```text
BLOQUES LATERALES IDENTIFICABLES: OK
CABLES DESDE FUERA DEL VIEWPORT: OK
CUATRO ENTRADAS DE B04 VISIBLES: OK
STUB LO DE B05: OK
SIN RESIDUOS: OK
SIN INVASIÓN DE HEADER/FOOTER: OK
NAVEGACIÓN: OK
SEÑALES EN VIVO: OK
```
