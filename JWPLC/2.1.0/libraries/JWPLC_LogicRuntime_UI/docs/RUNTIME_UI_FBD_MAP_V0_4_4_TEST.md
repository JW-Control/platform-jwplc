# Validación física — mapa FBD v0.4.4

## Objetivo

Validar el renderer incremental `RuntimeUIFBDMapV3` sobre el mismo motor RAM v2 ya aprobado.

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

El monitor Serial debe indicar:

```text
Motor v2: RUNNING
UI FBD v0.4.4: LISTA
```

## Cambios esperados

### B04 AND

```text
No aparecen 1, 2, !3 ni 4 como texto.
Permanece 4 IN.
Los cuatro puertos y sus cuatro cables siguen visibles.
La tercera entrada conserva burbuja de negación gris.
```

### B05 OR

```text
No aparecen etiquetas numéricas ni 0 dentro del gutter.
Permanece 2 IN.
Los dos puertos y sus dos conexiones siguen visibles.
```

### B06 SET/RESET

```text
S queda dentro del bloque y ligeramente más abajo.
R queda dentro del bloque y más arriba que en v0.4.3.
Permanece 2 IN.
```

### Indicadores de borde

En la vista inicial, el bloque parcial o indicador derecho debe corresponder a `B07 TON`. No deben adelantarse `B08 NOT` ni `B09 Q` mientras B07 sea la siguiente columna.

Al avanzar hasta B07/B08:

```text
solo se anuncia la columna inmediatamente anterior o siguiente;
no aparecen referencias de dos o más columnas de distancia;
la misma regla aplica para filas superior e inferior.
```

### Parpadeo

Los cambios de fase lógica deben repintar cables y nodos sin borrar el panel completo. El encabezado `RUN` solo debe redibujarse cuando cambie el estado del motor.

Se acepta un redibujado completo únicamente cuando:

```text
se entra a USER;
se abre o cierra detalle;
el viewport cambia por desplazamiento;
se fuerza un redraw externo.
```

## Seguridad

```text
No escribe FRAM.
No conmuta Q0 físicas.
No modifica el programa.
No habilita todavía edición gráfica.
No elimina periféricos del autoload.
```

## Datos a registrar

```text
Flash usada.
RAM global.
RAM restante.
Salida Serial inicial.
Foto de B04 y B05.
Foto de B06.
Foto de vista inicial mostrando B07 como siguiente columna.
Foto al seleccionar B08 o B10.
Percepción de parpadeo durante un ciclo completo de 9 s.
```

## Criterio de aprobación

```text
NEGACIÓN GRIS: OK
AND/OR SIN NÚMEROS: OK
N IN CONSERVADO: OK
S/R DENTRO DEL BLOQUE: OK
HINT SOLO ADYACENTE: OK
CABLES COMPLETOS: OK
PARPADEO REDUCIDO: OK
NAVEGACIÓN: OK
```
