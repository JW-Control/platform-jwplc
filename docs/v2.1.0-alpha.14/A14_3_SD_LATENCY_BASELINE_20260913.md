# Alpha14.3 — baseline de latencia microSD

Fecha: 2026-09-13

## Estado

`A14_3_SD_LATENCY_DIAGNOSTIC=PASS_PHYSICAL_WITH_TEMP_SKETCH_WARNING`

La prueba física completó todos los casos y terminó con `SD_DIAG_ERRORS=0`. La compilación del sketch temporal mostró un único warning `-Wunused-but-set-variable` sobre un buffer local del caso de lectura byte-a-byte. El warning no pertenece a `JW_SD`, `SD`, `FS` ni a otra librería del package y no invalida las mediciones físicas obtenidas.

## Configuración real de microSD

El JWPLC Basic configura actualmente la microSD por SPI a:

```text
JWPLC_SPI_SD_HZ=20000000
SD_INTERFACE=SPI
```

La capa `JWPLC_GlobalPeripherals` pasa `JWPLC_SPI_SD_HZ` a `JW_SD`, y `JW_SD` lo entrega a `SD.begin(...)`.

La implementación vendorizada de `SD` limita además la frecuencia efectiva máxima a `25 MHz` durante la inicialización para compatibilidad.

La tarjeta usada en esta validación reportó:

```text
SD_CARD_TYPE=3
SD_CARD_SIZE_BYTES=15730212864
```

`CARD_TYPE=3` corresponde a SDHC en el enum de la librería vendorizada.

## Resultados físicos

### 1. Append de 32 B con flush + close por registro

```text
DURABLE32_COUNT=20
DURABLE32_AVG_US=9251
DURABLE32_MIN_US=8337
DURABLE32_MAX_US=12580
```

### 2. Append de 32 B con close, sin flush explícito

```text
CLOSEONLY32_COUNT=20
CLOSEONLY32_AVG_US=9110
CLOSEONLY32_MIN_US=8243
CLOSEONLY32_MAX_US=11656
```

La diferencia media entre `flush + close` y `close only` fue de ~141 us, aproximadamente 1.5 %. Por tanto, el `flush()` explícito no explica por sí solo la latencia de ~9 ms del patrón de 32 B.

### 3. Escritura agrupada de 100 x 32 B

```text
BATCH32_RECORDS=100
BATCH32_TOTAL_US=16408
BATCH32_AVG_US_PER_RECORD=164
```

Agrupar 100 registros en una sola apertura y sincronización reduce el coste medio por registro desde ~9.25 ms hasta ~0.164 ms, una diferencia de más de 50x.

Esto indica que el coste dominante del patrón pequeño está en el ciclo repetido de apertura/cierre, metadata/FAT y sincronización asociada, no en transferir los 32 B por SPI.

### 4. Escritura secuencial de 64 KiB

```text
SEQ_WRITE_BYTES=65536
SEQ_WRITE_TOTAL_US=66189
SEQ_WRITE_KIB_S=966.93
```

A 20 MHz, el throughput físico observado de escritura fue ~967 KiB/s.

### 5. Lectura de 32 B byte-a-byte mediante JWPLCFile::read()

```text
READ32_BYTEWISE_COUNT=20
READ32_BYTEWISE_AVG_US=8615
READ32_BYTEWISE_MIN_US=8202
READ32_BYTEWISE_MAX_US=9090
```

### 6. Lectura bulk protegida de 32 B

```text
READ32_BULK_COUNT=20
READ32_BULK_AVG_US=7896
READ32_BULK_MIN_US=7645
READ32_BULK_MAX_US=8137
```

La lectura bulk mejora el promedio en ~719 us, cerca de 8.3 %. Es una mejora útil y confirma un hueco de API en `JWPLCFile`, que actualmente expone `write(buffer,size)` pero no `read(buffer,size)` protegido. Sin embargo, tampoco explica por sí sola los ~8–9 ms totales del ciclo de lectura.

### 7. Lectura secuencial de 64 KiB

```text
SEQ_READ_BYTES=65536
SEQ_READ_TOTAL_US=50538
SEQ_READ_KIB_S=1266.37
```

A 20 MHz, el throughput físico observado de lectura fue ~1266 KiB/s.

## Interpretación

Los resultados descartan que la microSD esté simplemente operando a una frecuencia SPI demasiado baja.

`20 MHz` ya representa el 80 % del límite actual de `25 MHz` del stack vendorizado. Elevar 20 -> 25 MHz sólo puede aportar como máximo 25 % de mejora de reloj bruto y no puede explicar ni eliminar una latencia de ~9 ms causada mayormente por apertura/cierre y operaciones de filesystem.

El patrón `open -> write 32 B -> flush -> close` usado en `FULL_RUNTIME_REALISTIC` es deliberadamente exigente. La evidencia aislada muestra que:

- `flush()` explícito añade poco frente a `close()`;
- abrir/cerrar por cada registro domina la latencia;
- escribir en batch reduce el coste por registro más de 50x;
- la lectura bulk mejora ~8 %, pero el overhead de open/size/seek/close sigue dominando;
- el throughput secuencial de la tarjeta no es anormalmente bajo para SPI/FAT a 20 MHz.

Los máximos de ~28–29 ms observados previamente en `FULL_RUNTIME_REALISTIC` son mayores que los máximos aislados (~12.6 ms escritura y ~9.1 ms lectura). Debe medirse si esos picos integrados provienen de crecimiento de archivo/asignación de clusters, metadata FAT, scheduling o contención durante el runtime combinado.

## Observación TFT durante el diagnóstico

Durante casi todo el `setup()` del sketch temporal la TFT sólo mostró:

- divisiones verticales/horizontales;
- título `JWPLC Basic`;
- separadores `:` del RTC.

El resto del layout apareció recién cerca del final de la prueba.

Esta observación no apunta a la microSD. El diseño actual del IDLE reconstruye el layout en varias llamadas consecutivas: la fase 0 dibuja precisamente el frame base, divisores, título y decoraciones estáticas del RTC; las fases siguientes dibujan status/headers, I/O y valores.

Además, `initPeripherals()` ejecuta una sola llamada inicial de `jwplcDisplayRefreshCallback(...)` antes de `setup()`, mientras que `jwplcSystemTask` espera a que `setup()` termine antes de comenzar sus refrescos periódicos. Por tanto, un `setup()` largo deja visible únicamente la fase 0 hasta que retorna.

Clasificación de esta observación:

```text
TFT_PARTIAL_DURING_LONG_SETUP=REPRODUCED_BY_DESIGN_ORDER
TFT_SD_CONTENTION_EVIDENCE=NO
```

Conviene revisar en un gate separado si el frame IDLE completo debe completarse antes de entregar control a `setup()` sin convertir el arranque en un bloqueo largo.

## Decisiones

- No modificar todavía `JW_SD`, `SD` ni `FS`.
- No cambiar todavía `JWPLC_SPI_SD_HZ`.
- Siguiente prueba de microSD: sweep controlado 10/20/25 MHz para cuantificar sensibilidad real a frecuencia usando la misma tarjeta y patrones equivalentes.
- Después del sweep decidir si conviene subir el default a 25 MHz.
- Evaluar por separado una API `JWPLCFile::read(buffer,size)` protegida y una estrategia de logging persistente/batch con flush periódico.
- Mantener `A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED` y `LONG_RUN=ON_HOLD` hasta cerrar los diagnósticos abiertos.

## Precompilación y JW-Libraries

`JW_SD` sigue siendo `precompiled=full`. En esta prueba no se cambió ninguna librería, por lo que no fue necesario anular la precompilación.

Si se realizan cambios reales en `JW_SD`, la validación local deberá desactivar temporalmente la precompilación correspondiente para asegurar que se compile el source modificado y no el `.a` anterior.

Al cerrar cualquier cambio de `JW_SD`, deberá replicarse en `JW-Control/JW-Libraries` y luego sincronizarse hacia `platform-jwplc`. La configuración actual de sync de `JW-Libraries/main` sigue apuntando a una rama/ruta anterior (`develop/alpha31-release-readiness`, `sync/jw-libraries-alpha31`, `JWPLC/JWPLC-2.0.0/libraries`), por lo que ese flujo deberá actualizarse explícitamente antes de usarlo para Alpha14 / 2.1.0.
