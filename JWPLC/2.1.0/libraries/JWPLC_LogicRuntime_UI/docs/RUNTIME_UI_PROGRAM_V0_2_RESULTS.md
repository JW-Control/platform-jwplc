# Resultado físico — Runtime UI Programa v0.2

## Estado

```text
PALETA VERDE: VALIDADA EN TFT
HOME -> PROGRAMA: VALIDADO
PREPARAR SIN FORMATO: VALIDADO
RUN SIN PROGRAMA: VALIDADO
STOP SEGURO: VALIDADO
VOLVER -> HOME: VALIDADO
RENDERIZADO ESTABLE: VALIDADO
CARGA INICIAL PERCIBIDA LENTA: CORRECCIÓN PREPARADA EN v0.2.1
```

## Compilación validada

```text
Flash:       438305 bytes / 3145728 bytes (13 %)
RAM global:   32796 bytes / 327680 bytes (10 %)
RAM restante: 294884 bytes
```

## Validación visual

Se confirmó físicamente:

- paleta principal verde, blanca y negra;
- selección verde visible en HOME y PROGRAMA;
- encabezados y paneles sin parpadeo periódico;
- navegación HOME -> PROGRAMA;
- navegación PROGRAMA -> HOME mediante `VOLVER`;
- reconstrucción correcta de HOME después de volver.

## Validación funcional

### PREPARAR

Con FRAM sin formato:

```text
Persist: SIN FORMATO | RET -
Error: STORED_PROGRAM_LOAD_FAILED
Mensaje: FRAM sin formato: no hay programa
Runtime: READY
```

La operación no formatea ni escribe la FRAM.

### RUN

Sin programa cargado:

```text
Error: PROGRAM_NOT_LOADED
Mensaje: RUN rechazado: revise programa/error
Runtime: READY
```

El runtime no entra en ejecución.

### STOP

Resultado:

```text
Runtime: STOPPED
Error: NONE
Mensaje: STOP aplicado; salidas apagadas
```

Las salidas permanecen apagadas.

## Observación de rendimiento

La pantalla quedó estable una vez construida, pero la aparición inicial de sus elementos se percibió lenta.

La revisión identificó que `updateTextField()` enviaba el ancho completo del campo como glifos, incluyendo numerosos espacios usados únicamente para limpiar el contenido anterior. En campos de 30 a 51 columnas esto genera muchas operaciones pequeñas de texto durante cada construcción completa.

La corrección v0.2.1 cambia el patrón a:

```text
campo realmente modificado
-> limpieza continua de su rectángulo
-> impresión transparente solo de caracteres útiles
```

También los títulos, etiquetas y textos de botones estáticos se imprimen en modo transparente sobre regiones que ya fueron pintadas.

## Pendiente inmediato

Recompilar y comparar perceptualmente:

```text
IDLE -> HOME
HOME -> PROGRAMA
PROGRAMA -> HOME
```

La corrección debe mantener:

- ausencia de parpadeo en reposo;
- mismos textos y distribución;
- misma paleta;
- mismas acciones;
- menor tiempo visible de construcción.