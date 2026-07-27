# Resultado físico — mapa FBD v0.4.4

## Estado

```text
COMPILACIÓN: aprobada por el usuario
EJECUCIÓN: aprobada
NAVEGACIÓN: operativa
PARPADEO: reducido
RESULTADO VISUAL: APROBADO COMO BASE / REQUIERE COMPACTACIÓN FINAL
```

## Resultado observado

El usuario calificó la v0.4.4 como una mejora importante. Las fotografías físicas confirmaron:

- mapa limpio y legible;
- burbuja de negación gris;
- compuertas AND/OR sin números permanentes de pin;
- S y R dentro del bloque SET/RESET;
- señales activas en verde;
- selección en amarillo;
- navegación completa desde B00 hasta B10;
- cableado conservado al desplazar el viewport;
- referencias laterales limitadas a columnas próximas;
- menor parpadeo durante los cambios lógicos.

## Ajustes finales solicitados

1. Eliminar el footer `L/R: conexion  UP/DN: mapa  OK: detalle`.
2. Extender el área útil del mapa hasta el borde inferior de la TFT.
3. Compactar los bloques del mapa a dos líneas:

```text
B04
AND
```

4. Eliminar del mapa los resúmenes `4 IN`, `2 IN` y `1 IN`.
5. Conservar recursos I/O como segunda línea:

```text
B00
I0.0
```

6. Representar simétricamente la columna inmediatamente anterior y la siguiente, permitiendo referencias a ambos lados a la vez.
7. Evitar que una referencia derecha tape el bloque principal.
8. Desplazar ligeramente a la derecha la metadata del encabezado.
9. Reemplazar el detalle textual por una vista gráfica ampliada inspirada en la forma de lectura de bloques FBD de LOGO!.

## Decisión para v0.4.5

```text
Renderer nuevo RuntimeUIFBDMapV4.
Mapa compacto 48 x 30 px.
Separación de columnas de 64 px.
Panel útil desde y=27 hasta y=168.
Sin footer permanente.
Metadata del encabezado desde x=112.
Bloques del mapa: Bxx / tipo o recurso.
Hints laterales: cajas de 42 x 24 px, simétricas y solo para la columna adyacente.
Detalle gráfico: fuentes a la izquierda, cableado y bloque ampliado a la derecha.
Detalle sigue siendo de solo lectura.
```
