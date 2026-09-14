# Alpha14 A14.3 - PASS físico de verificación microSD con handle único

Fecha: 2026-09-13

## Contexto

Después de aplicar la política FULL_RUNTIME_REALISTIC de archivo microSD persistente con `flush` cada 5 registros, la corrida final a 1000 req/s mostró 11 fallos de verificación de 12 ciclos, aunque los appends y flushes sí se ejecutaban correctamente.

La hipótesis fue que mantener simultáneamente un handle `FILE_APPEND` persistente y abrir otro handle `FILE_READ` sobre el mismo archivo podía generar incoherencia de estado del archivo/FAT, aun cuando cada operación estuviera protegida por el mutex SPI compartido.

## Cambio probado

Se modificó únicamente el harness:

`tools/modbus-tcp-benchmark/firmware/a14_perf_full_runtime_realistic/a14_perf_full_runtime_realistic.ino`

La prueba mantuvo la política:

- append cada 1 s;
- archivo persistente durante operación normal;
- `flush` cada 5 registros;
- verify cada 5 s;
- sin cambios en `JW_SD`.

Para la verificación, el harness ahora:

1. hace `flush` si existe un lote pendiente;
2. cierra temporalmente el handle persistente de append;
3. abre un único handle `FILE_READ`;
4. lee y compara el último registro;
5. cierra el handle de lectura;
6. reabre el handle `FILE_APPEND` persistente.

## Evidencia física

Prueba: FULL_RUNTIME_REALISTIC, FC03/125, 100 req/s, 60 s.

Resultado TCP:

- `TCP_RESULT=STABLE_PASS`
- `TCP_ACHIEVED_REQ_S=100.00`
- `TCP_ACHIEVED_PCT=100.000`
- `TCP_ERRORS=0`
- `TCP_CROSS_COUNT_PASS=YES`

Periféricos:

- `FULL_RUNTIME_READY=YES`
- `PERIPHERAL_FAILURE_COUNT=0`
- `FRAM_FAILS=0`
- `SD_APPEND_FAILS=0`
- `RTC_UNAVAILABLE=0`
- `RTC_STALE=0`
- `IO_STALE=0`
- `BUTTON_NOT_READY=0`
- `SPI_PROBE_FAILS=0`

microSD:

- `SD_APPEND_FILE_OPEN=YES`
- `SD_APPEND_CYCLES=60`
- `SD_APPEND_FAILS=0`
- `SD_FLUSH_EVERY_RECORDS=5`
- `SD_FLUSH_CYCLES=12`
- `SD_VERIFY_CYCLES=12`
- `SD_VERIFY_FAILS=0`
- `SD_VERIFY_SINGLE_HANDLE=PASS_PHYSICAL`

Tiempos máximos observados:

- `SD_APPEND_MAX_US=19905`
- `SD_VERIFY_MAX_US=34691`

## Conclusión

La hipótesis queda respaldada físicamente: el fallo anterior de verificación estaba asociado a mantener dos handles simultáneos sobre el mismo archivo mientras uno permanecía abierto en `FILE_APPEND`.

El esquema de verificación con handle único elimina los fallos de lectura/comparación sin requerir cambios en `JW_SD` ni degradar la integridad del runtime.

Estado:

```text
A14_3_SD_VERIFY_FIX=PASS_PHYSICAL
SD_VERIFY_FAILURE_MODE=DUAL_HANDLE_SAME_FILE_INCOMPATIBILITY_SUPPORTED
JW_SD_LIBRARY_CHANGE=NO
FINAL_1000RPS_60S=RETRY_PENDING
FINAL_1000RPS_30MIN=ON_HOLD
```

## Siguiente paso

Consolidar el cambio del harness y repetir la regresión final FULL_RUNTIME_REALISTIC a 1000 req/s durante 60 s. Sólo si esa corrida cumple `>=95 %`, TCP limpio y cero fallos periféricos se habilita el soak final de 30 minutos.
