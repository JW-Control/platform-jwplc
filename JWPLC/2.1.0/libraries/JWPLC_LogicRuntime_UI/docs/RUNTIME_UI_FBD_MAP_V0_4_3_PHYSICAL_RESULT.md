# Resultado físico — mapa FBD v0.4.3

## Estado

```text
COMPILACIÓN: aprobada por el usuario
EJECUCIÓN: aprobada
NAVEGACIÓN: operativa
CABLEADO RECORTADO: visible
RESULTADO VISUAL: PARCIAL / REQUIERE v0.4.4
```

## Avances confirmados

Las fotografías físicas confirmaron que la v0.4.3:

- conserva conexiones aunque una fuente o consumidor quede fuera del viewport;
- muestra bloques parciales y referencias laterales;
- permite recorrer desde B00 hasta B10;
- mantiene señales TRUE en verde y selección en amarillo;
- mantiene el mapa y el footer separados.

## Observaciones del usuario

1. La burbuja de negación de B04 debe usar el mismo gris del cableado inactivo.
2. Las etiquetas `1`, `2`, `!3`, `4` dentro de AND y las etiquetas numéricas de OR ocupan demasiado espacio.
3. AND y OR deben conservar únicamente sus puertos gráficos y el resumen `4 IN` o `2 IN`.
4. La edición de conexiones se hará después en una pantalla grande, siguiendo el flujo de edición tipo LOGO!.
5. En SET/RESET, `S` debe bajar y `R` debe subir para quedar dentro del bloque.
6. El viewport inicial mostraba referencias B08/B09 cuando el bloque inmediato era B07.
7. Los indicadores deben limitarse a la columna o fila inmediatamente adyacente, no a dos o más niveles de distancia.
8. La limpieza completa del mapa durante cambios lógicos produce parpadeo perceptible.

## Decisión implementada para v0.4.4

```text
Renderer actual RuntimeUIFBDMapV2 conservado para no duplicar código experimental.
Burbuja negada siempre COLOR_MUTED.
Sin números, ! ni constantes dentro de AND/OR/NOT/Q.
Mantener 4 IN / 2 IN / 1 IN en el cuerpo del bloque.
Mantener S/R y T dentro del gutter.
SET/RESET: S en y+10 y R en y+22.
Hints solo para el nivel o fila fuera de pantalla más cercano.
Si ya existe un bloque parcial en un borde, no se agrega un hint más lejano.
Repintado dinámico de cables y nodos sin limpiar todo el panel.
Redibujado completo reservado para entrada, detalle, navegación o cambio de layout.
```

La pantalla de edición gráfica de entradas queda pendiente y no forma parte de v0.4.4.
