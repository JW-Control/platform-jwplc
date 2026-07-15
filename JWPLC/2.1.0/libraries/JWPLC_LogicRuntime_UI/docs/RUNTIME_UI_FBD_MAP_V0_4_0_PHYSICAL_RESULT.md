# Resultado físico visual — mapa FBD v0.4.0

## Estado

```text
COMPILACIÓN Y EJECUCIÓN EN HARDWARE: CONFIRMADAS VISUALMENTE
CONCEPTO DE MAPA ESTABLE: APROBADO
CIERRE VISUAL DE v0.4.0: RECHAZADO
SIGUIENTE CORRECCIÓN: v0.4.1
```

No se registraron todavía en este documento las cifras de Flash y RAM ni el log Serial completo de esta ejecución.

## Aspectos validados

Las fotografías confirman:

- disposición FBD con posiciones estables por bloque;
- navegación que mueve la selección sin reconstruir una página por bloque;
- borde amarillo para el nodo seleccionado;
- verde para bloques y conexiones con valor lógico `TRUE`;
- visualización de AND con cuatro entradas;
- visualización de `S` y `R` en SET/RESET;
- visualización del bloque TON y su parámetro;
- desplazamiento horizontal y vertical del viewport.

## Defectos observados

### 1. Contenido residual

Al mover el viewport quedaron rastros de nodos, textos y cables pertenecientes a la posición anterior.

### 2. Invasión de regiones

Los nodos parcialmente visibles podían dibujarse sobre:

```text
encabezado
franja superior del panel
borde del mapa
footer
```

### 3. Superposición de información

La cadena:

```text
Bxx X:n Y:n
```

se dibujaba dentro del mismo rectángulo usado por los nodos, por lo que podía mezclarse con la primera fila.

### 4. Cables parciales sin clipping

Los segmentos ortogonales se dibujaban aun cuando uno de sus extremos quedaba fuera del viewport. Esto permitía líneas residuales fuera de la región gráfica.

## Causa

La implementación v0.4.0 utilizaba un único rectángulo para:

```text
información del seleccionado
viewport del grafo
nodos parcialmente visibles
cables parcialmente visibles
```

La prueba demostró que la comprobación de intersección parcial no equivale a clipping gráfico real.

## Decisión

La v0.4.1 adopta una política mínima robusta antes de implementar clipping avanzado:

1. separar `PANEL`, `INFO` y `MAP`;
2. limpiar `INFO` y `MAP` de forma independiente;
3. dibujar únicamente nodos completamente visibles;
4. dibujar cables solo cuando fuente y consumidor están completamente visibles;
5. limpiar todo el viewport al cambiar selección, scroll o valores;
6. conservar el mapa lógico y la navegación ya aprobados.

Esta política prioriza ausencia de residuos y legibilidad. Los stubs de conexiones hacia nodos fuera de pantalla y el clipping parcial elegante quedan para una fase posterior.
