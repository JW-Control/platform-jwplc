# Alpha14 A14.3 — Revalidación del baseline TCP-only

Fecha: 2026-09-13

## Resultado

```text
A14_3_TCP_ONLY_BASELINE=PASS_PHYSICAL
TCP_BASELINE_REPRODUCED=YES
REQUESTED_REQ_S=1000
RUN1_PCT=99.984
RUN2_PCT=99.999
AVG_PCT=99.992
MIN_PCT=99.984
MAX_PCT=99.999
SPREAD_PP=0.015
TCP_CLEAN_ALL_RUNS=YES
NEXT=ISOLATE_FULL_RUNTIME_NON_SD_WORKLOAD
```

## Objetivo

Revalidar el baseline puro de Modbus TCP después de que el perfil `FULL_RUNTIME_REALISTIC` con workload SD completamente fuera del scheduler no lograra reproducir el antiguo 95.993 % y quedara en 89.843 % promedio.

La prueba utilizó directamente el firmware tracked `tools/modbus-tcp-benchmark/firmware/a14_perf_server/a14_perf_server.ino`, sin patch temporal de fuente.

## Compilación y upload

```text
BUILD_UPLOAD_EXIT=0
TCP_ONLY_FIRMWARE_UPLOAD=PASS
SKETCH_BYTES=420069
GLOBAL_VARIABLES_BYTES=29572
SERIAL=COM14
TARGET=192.168.0.31:502
```

El upload finalizó con verificación de hash correcta.

## Evidencia física

### Run 1

```text
ACHIEVED_REQ_S=999.84
ACHIEVED_PCT=99.984
P95_US=1208.2
P99_US=1573.6
MAX_US=22424.6
LOOP_AVG_US=260
LOOP_MAX_US=3833
TCP_CLEAN=YES
OK=59991/60000
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES
```

### Run 2

```text
ACHIEVED_REQ_S=999.99
ACHIEVED_PCT=99.999
P95_US=1197.8
P99_US=1317.2
MAX_US=35151.3
LOOP_AVG_US=261
LOOP_MAX_US=3813
TCP_CLEAN=YES
OK=60000/60000
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES
```

## Comparación con el full runtime actual

El control TCP-only confirma que la infraestructura base de Modbus TCP, Ethernet, PC runner y red puede sostener prácticamente el 100 % del objetivo de 1000 req/s.

La comparación relevante queda:

| Perfil | Achieved |
|---|---:|
| TCP-only, promedio actual | 99.992 % |
| Full runtime + SD workload OFF, promedio actual | 89.843 % |
| Dry-run SD write, promedio D0 | 85.681 % |

La caída del full runtime no puede atribuirse al stack TCP puro ni al runner de PC.

## Conclusión

```text
TCP_STACK_LIMITING=NO
PC_RUNNER_LIMITING=NO
CURRENT_NETWORK_PATH_LIMITING=NO
FULL_RUNTIME_NON_SD_COST=MATERIAL
SD_BATCH_SWEEP=ON_HOLD
FINAL_30MIN_SOAK=ON_HOLD
```

El siguiente aislamiento debe realizarse dentro de los periféricos/workloads no-SD del `FULL_RUNTIME_REALISTIC`.

Por frecuencia y coste esperado, el primer A/B recomendado es el refresco dinámico de TFT: mantener Display inicializado y disponible, mantener el resto del full runtime activo, mantener SD workload fuera del scheduler y hacer que `jwplcUserDisplayRefreshNeededCallback()` retorne temporalmente `false` durante la prueba.

Si el throughput se recupera materialmente hacia el baseline TCP-only, el refresco dinámico TFT será un contribuyente principal. Si permanece alrededor de 90 %, se continuará con FRAM y los demás servicios no-SD.
