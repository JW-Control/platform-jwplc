# Alpha14 A14.3 — aislamiento SD sin flush explícito

Fecha: 2026-09-13

## Objetivo

Separar el coste del `flush()` explícito del coste del `write()`/ruta de filesystem dentro del workload microSD del perfil `FULL_RUNTIME_REALISTIC`.

La variante diagnóstica mantuvo:

- microSD inicializada y disponible;
- archivo persistente abierto;
- `serviceSdAppend()` activo cada 1 s;
- escritura de 32 bytes por ciclo activa;
- `serviceSdVerify()` desactivado;
- `flush()` explícito periódico desactivado;
- resto del runtime integrado activo sin cambios.

No se eliminó SD del autoload normal y no se modificó `JW_SD` ni `JWPLC_Ethernet`.

## Evidencia física

Se ejecutaron dos corridas consecutivas de FC03/125 a 1000 req/s durante 60 s usando el mismo firmware diagnóstico.

### Corrida 1

- `ACHIEVED_REQ_S=886.16`
- `ACHIEVED_PCT=88.616`
- `LATENCY_AVG_US=1121.4`
- `P95_US=1307.0`
- `P99_US=16770.3`
- `MAX_US=42038.1`
- `TCP_CLEAN=YES`
- `SD_APPEND_CYCLES=59`
- `SD_APPEND_FAILS=0`
- `SD_FLUSH_CYCLES=0`
- `SD_VERIFY_CYCLES=0`
- `PERIPHERAL_FAILURE_COUNT=0`

### Corrida 2

- `ACHIEVED_REQ_S=902.79`
- `ACHIEVED_PCT=90.279`
- `LATENCY_AVG_US=1100.9`
- `P95_US=1291.9`
- `P99_US=16898.9`
- `MAX_US=30748.0`
- `TCP_CLEAN=YES`
- `SD_APPEND_CYCLES=59`
- `SD_APPEND_FAILS=0`
- `SD_FLUSH_CYCLES=0`
- `SD_VERIFY_CYCLES=0`
- `PERIPHERAL_FAILURE_COUNT=0`

### Resumen

- `AVERAGE_ACHIEVED_PCT=89.448`
- `MIN_ACHIEVED_PCT=88.616`
- `MAX_ACHIEVED_PCT=90.279`
- `RUN_SPREAD_PP=1.663`
- `TCP_CLEAN_ALL_RUNS=YES`
- `NO_EXPLICIT_FLUSH_CLASSIFICATION=WRITE_OR_FS_PATH_REMAINS_DOMINANT`
- `A14_3_SD_FLUSH_ISOLATION=PASS_DIAGNOSTIC`

## Interpretación

Desactivar el `flush()` explícito no recuperó el rendimiento. De hecho, las dos corridas permanecieron claramente por debajo del umbral de estabilidad de 95 %, con promedio de 89.448 %.

Esto descarta al `flush/5` como causa dominante de la pérdida de throughput. La evidencia apunta ahora principalmente a la llamada de escritura `JWPLCFile::write()` y/o a la ruta subyacente FS/SD asociada a cada append de 32 bytes.

La ausencia de errores TCP, de timeouts de bus, de fallos de append y de fallos periféricos indica una degradación de rendimiento limpia, no un fallo funcional.

## Estado

```text
A14_3_SD_FLUSH_ISOLATION=PASS_DIAGNOSTIC
EXPLICIT_FLUSH_DOMINANT=NO
WRITE_OR_FS_PATH_REMAINS_DOMINANT=YES
TCP_CLEAN_ALL_RUNS=YES
PERIPHERAL_FAILURE_COUNT=0
FINAL_1000RPS_60S=PENDING_DIAGNOSTIC
FINAL_1000RPS_30MIN=ON_HOLD
```

## Siguiente paso

Aislar el coste de `JWPLCFile::write()` ejecutando el scheduler y la generación del registro cada 1 s, pero sustituyendo temporalmente la escritura física por un dry-run. `verify` y `flush` permanecerán desactivados. Si el rendimiento vuelve a >=95 % de forma repetible, la ruta física de escritura/FS quedará confirmada como cuello de botella principal y el siguiente experimento deberá evaluar batching de registros en RAM antes de una escritura SD menos frecuente.
