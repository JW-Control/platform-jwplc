# Resultado físico — Runtime UI Programa v0.2 / v0.2.1

## Estado

```text
PALETA VERDE: VALIDADA EN TFT
HOME -> PROGRAMA: VALIDADO
PREPARAR SIN FORMATO: VALIDADO
RUN SIN PROGRAMA: VALIDADO
STOP SEGURO: VALIDADO
VOLVER -> HOME: VALIDADO
RENDERIZADO ESTABLE: VALIDADO
CARGA INICIAL RÁPIDA v0.2.1: VALIDADA EN TFT
```

## Compilación funcional v0.2

```text
Flash:       438305 bytes / 3145728 bytes (13 %)
RAM global:   32796 bytes / 327680 bytes (10 %)
RAM restante: 294884 bytes
```

La optimización visual v0.2.1 fue aprobada físicamente por percepción directa como `súper rápida`. Sus métricas exactas de compilación no se registraron por separado; la siguiente compilación integral corresponde a Bloques v0.3.

## Validación visual

Se confirmó físicamente:

- paleta principal verde, blanca y negra;
- selección verde visible en HOME y PROGRAMA;
- encabezados y paneles sin parpadeo periódico;
- navegación HOME -> PROGRAMA;
- navegación PROGRAMA -> HOME mediante `VOLVER`;
- reconstrucción correcta de HOME después de volver;
- aparición rápida de textos, paneles y botones después de la optimización v0.2.1.

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

## Optimización v0.2.1

La primera versión estable todavía mostraba una construcción inicial perceptiblemente lenta.

La revisión identificó que `updateTextField()` enviaba el ancho completo de cada campo como glifos, incluyendo espacios usados únicamente para limpiar. En campos de 30 a 51 columnas esto generaba muchas operaciones pequeñas.

La corrección aplicada fue:

```text
campo realmente modificado
-> limpieza continua de su rectángulo
-> impresión transparente solo de caracteres útiles
```

También los títulos, etiquetas y textos de botones estáticos pasan a imprimirse en modo transparente sobre regiones ya pintadas.

## Resultado de rendimiento

```text
IDLE -> HOME: rápido
HOME -> PROGRAMA: rápido
PROGRAMA -> HOME: rápido
Parpadeo periódico: no
Distribución visible por partes: eliminada o no perceptible
```

## Decisión

El patrón combinado queda aprobado para las siguientes vistas:

```text
dirty rendering
+ limpieza rectangular continua solo al cambiar
+ texto transparente sin espacios de relleno
+ acciones fuera del callback TFT
```

La siguiente etapa funcional es `RuntimeUIBlocks v0.3`.