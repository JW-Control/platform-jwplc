# Alpha14.3 — sweep de frecuencia microSD

Fecha: 2026-09-13

## Objetivo

Determinar cuánto aporta realmente la frecuencia SPI de la microSD al tiempo de operaciones pequeñas y al throughput secuencial antes de modificar la configuración por defecto del JWPLC Basic.

Configuración de referencia del package:

```text
AUTOLOAD_SD_HZ=20000000
STACK_CAP_HZ=25000000
SD_INTERFACE=SPI
```

No se modificaron librerías ni se anuló `precompiled=full` durante esta prueba.

## Evidencia física

Tarjeta detectada y funcional. Sweep ejecutado a 4, 10, 20 y 25 MHz.

```text
4 MHz
DURABLE32_AVG_US=23380
DURABLE32_MIN_US=21393
DURABLE32_MAX_US=29383
SEQ_WRITE_KIB_S=328.53
SEQ_READ_KIB_S=394.95

10 MHz
DURABLE32_AVG_US=12555
DURABLE32_MIN_US=11623
DURABLE32_MAX_US=16248
SEQ_WRITE_KIB_S=684.42
SEQ_READ_KIB_S=870.18

20 MHz
DURABLE32_AVG_US=9217
DURABLE32_MIN_US=8704
DURABLE32_MAX_US=12059
SEQ_WRITE_KIB_S=962.93
SEQ_READ_KIB_S=1439.69

25 MHz
DURABLE32_AVG_US=9140
DURABLE32_MIN_US=8336
DURABLE32_MAX_US=12033
SEQ_WRITE_KIB_S=1069.88
SEQ_READ_KIB_S=1452.10

SD_FREQ_SWEEP_ERRORS=0
A14_SD_FREQ_SWEEP=PASS_PHYSICAL
```

## Comparación 20 MHz vs 25 MHz

```text
Durable 32 B:
  20 MHz = 9.217 ms
  25 MHz = 9.140 ms
  mejora aproximada = 0.84 %

Escritura secuencial:
  20 MHz = 962.93 KiB/s
  25 MHz = 1069.88 KiB/s
  mejora aproximada = 11.11 %

Lectura secuencial:
  20 MHz = 1439.69 KiB/s
  25 MHz = 1452.10 KiB/s
  mejora aproximada = 0.86 %
```

## Interpretación

La frecuencia SPI sí domina claramente entre 4 y 20 MHz, pero aparece una meseta fuerte al pasar de 20 a 25 MHz.

Para el patrón crítico de 32 bytes con durabilidad, subir de 20 a 25 MHz mejora menos de 1 %. La lectura secuencial también mejora menos de 1 %. Sólo la escritura secuencial obtiene una mejora apreciable, de aproximadamente 11 %.

Por tanto, la latencia de ~9 ms de las operaciones pequeñas no está limitada principalmente por el reloj SPI a 20 MHz. El coste dominante queda asociado al patrón de filesystem / FAT / apertura-cierre y a la sincronización interna de la tarjeta.

El baseline previo ya mostró además:

```text
32 B + flush + close   ~9.251 ms
32 B + close only      ~9.110 ms
100 x 32 B agrupados   ~0.164 ms por registro amortizado
```

Esto confirma que la política de I/O tiene mucho más margen de mejora que el aumento de 20 a 25 MHz.

## Decisión provisional

- Mantener `JWPLC_SPI_SD_HZ=20000000` por ahora.
- No subir a 25 MHz sólo por rendimiento: el beneficio sobre latencia pequeña es marginal.
- Priorizar optimización de la política de I/O de microSD:
  - evitar open/close por cada registro cuando el caso de uso permita buffering;
  - usar escrituras agrupadas;
  - revisar estrategia de flush/sync;
  - evaluar añadir `JWPLCFile::read(buffer, size)` como API bulk no rompiente.
- Si más adelante se considera 25 MHz como configuración final, debe pasar validación física prolongada y regresión SPI compartido antes de cambiar el default.

## Observación sobre captura

El runner automático abrió el puerto serie después de esperar el reboot y sólo alcanzó a capturar el tramo final de 25 MHz. La evidencia completa de 4/10/20/25 MHz se obtuvo mediante el monitor serie durante la misma ejecución física. El firmware final reportó `SD_FREQ_SWEEP_ERRORS=0` y `A14_SD_FREQ_SWEEP=PASS_PHYSICAL`.

## Relación con PERF-S2

En `FULL_RUNTIME_REALISTIC @ 1000 req/s` se observaron máximos SD de aproximadamente 28–29 ms. El sweep aislado muestra que la operación durable de 32 B es normalmente ~9 ms a 20 MHz, por lo que esos máximos integrados son picos bajo carga compartida y no el coste nominal puro de la SD.

La siguiente investigación debe medir una política de I/O más realista/bufferizada dentro del runtime integrado antes del long-run.

## Estado

```text
A14_3_SD_FREQUENCY_SWEEP=PASS_PHYSICAL
SD_DEFAULT_FREQUENCY_DECISION=KEEP_20_MHZ_FOR_NOW
SD_IO_POLICY=REVIEW_OPTIMIZATION
A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED
FULL_RUNTIME_LONG_RUN=ON_HOLD
```
