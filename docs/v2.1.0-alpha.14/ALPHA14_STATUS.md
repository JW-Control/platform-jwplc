# v2.1.0-alpha.14 — Estado

Actualizado: `2026-09-13`

## Identidad

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
BASE_BRANCH=release/v2.1.x
BASE_SHA=165f94fb25617d3a6035fdfd9899bd38a1331583
ALPHA14_STATUS=READY_FOR_RELEASE_PR
```

## Resultado global

```text
A14_1=PASS
A14_2=PASS
A14_3=PASS_PHYSICAL
A14_4=PASS_PHYSICAL
A14_5_ROBOT_INTEROPERABILITY=DEFERRED_NON_BLOCKING
ALPHA14_CLOSE_READINESS=PASS
ALPHA14_TECHNICAL_CLOSURE=PASS
```

Alpha14 incorpora y valida `JWPLC_ModbusTCP` como nueva librería del ecosistema JWPLC para servidor y cliente Modbus TCP cooperativos, manteniendo compatibilidad Arduino IDE y coexistencia con el runtime integrado del JWPLC Basic.

No se retiran periféricos del autoload normal.

## A14.1 — Foundation + Server

Estado: `PASS`.

```text
A14_1_IMPLEMENTATION=PASS_SOURCE_COMPLETE
A14_1_COMPILE=PASS
A14_1_EMPTY_SKETCH_REGRESSION=PASS
A14_1_SERVER_LISTEN_SMOKE=PASS
A14_1_FC01=PASS
A14_1_FC02=PASS
A14_1_FC03=PASS
A14_1_FC04=PASS
A14_1_FC05=PASS
A14_1_FC06=PASS
A14_1_FC15=PASS
A14_1_FC16=PASS
A14_1_SERVER_FUNCTION_MATRIX=PASS
A14_1_EXCEPTION_RECOVERY=PASS
A14_1_INVALID_MBAP_RECONNECT=PASS
A14_1_SERVER_RUNTIME=PASS
```

Evidencias principales:

- `A14_1_SERVER_FUNCTION_MATRIX_20260911.md`
- `A14_1_INVALID_MBAP_RECONNECT_20260911.md`

## A14.2 — Client cooperativo

Estado: `PASS`.

Evidencia de cierre:

- `A14_2_CLIENT_CLOSURE_20260912.md`

La matriz Client completa fue validada físicamente sobre una única conexión TCP persistente:

```text
FC01=PASS_PHYSICAL
FC02=PASS_PHYSICAL
FC03=PASS_PHYSICAL
FC04=PASS_PHYSICAL
FC05=PASS_PHYSICAL
FC06=PASS_PHYSICAL
FC15=PASS_PHYSICAL
FC16=PASS_PHYSICAL
CONNECTIONS=1
TX_FRAMES=8
RX_FRAMES=8
REQUESTS_OK=8
EXCEPTIONS=0
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
```

Timeout, cierre graceful y reconexión también quedaron validados físicamente.

### Decisión API Sync

```text
A14_2_SYNC_API_DECISION=DEFERRED_POST_ALPHA14
```

No se añaden wrappers bloqueantes en Alpha14. El motor cooperativo permanece como API de referencia.

## A14.3 — Performance

Estado final: `PASS_PHYSICAL`.

Evidencia de cierre:

- `A14_3_PERFORMANCE_CLOSURE_20260913.md`

Hallazgo principal: la degradación inicial de rendimiento no correspondía a la TFT como periférico, sino al patrón de redraw manual periódico usado por el harness legacy.

La API HMI declarativa Dirty / On-Demand recuperó prácticamente todo el rendimiento TCP-only.

### Soak 30 min Full Runtime

```text
TCP_REQUESTED_REQ_S=1000
TCP_ACHIEVED_REQ_S=999.34
TCP_ACHIEVED_PCT=99.934
REQUESTS_OK=1798804/1800000
P95_US=1323.7
P99_US=2406.6
MAX_US=27319.1
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
PERIPHERAL_FAILURE_COUNT=0
FULL_RUNTIME_30MIN_STRONG_PASS=YES
A14_3=PASS_PHYSICAL
```

Durante esta validación permanecieron activos Display/HMI, Ethernet, SD, FRAM, RTC, botonera, TCA/I/O y arbitraje SPI.

### Referencia de rendimiento

- `1000 req/s` se conserva como stress/frontier TCP.
- No se establece como carga industrial obligatoria de coexistencia RTU+TCP.

## A14.4 — RTU + TCP simultáneo

Estado final: `PASS_PHYSICAL`.

Evidencia de cierre:

- `A14_4_RTU_TCP_SIMULTANEOUS_CLOSE_20260913.md`

### Gate final G3 — 30 min

Topología física:

```text
COM4=JWPLC RTU Master
COM14=DUT TCP Server + RTU Slave ID 2 + full runtime
```

Carga simultánea:

```text
TCP=FC03/125 @500 req/s
RTU=FC03/16 @20ms target
RTU_BAUD=115200
RTU_FORMAT=8N1
DURATION=1800s
```

TCP:

```text
REQUESTS=900000/900000
ACHIEVED_REQ_S=500.000
ACHIEVED_PCT=100.0000
P95_US=3725.2
P99_US=4517.7
MAX_US=30365.1
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
```

RTU Master:

```text
REQUESTS_STARTED=89875
REQUESTS_COMPLETED=89875
REQUESTS_SUCCESS=89875
REQUESTS_FAILED=0
REQUESTS_REJECTED=0
VERIFY_FAILS=0
SUCCESS_PCT=100.0000
EFFECTIVE_HZ=49.907
LATENCY_AVG_US=11190
LATENCY_MAX_US=158436
CRC_ERRORS=0
MASTER_TIMEOUTS=0
```

DUT / full runtime:

```text
DUT_RTU_OK=89873
DUT_RTU_CRC_ERRORS=0
DUT_RTU_EXCEPTIONS=0
DUT_RTU_SERVICE_GAP_MAX_US=149013
SD_APPEND_FAILS=0
SD_VERIFY_FAILS=0
FRAM_FAILS=0
RTC_STALE=0
IO_STALE=0
BUTTON_NOT_READY=0
SPI_PROBE_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
FULL_RUNTIME_CLEAN=YES
SIMULTANEOUS_30MIN_STRONG_PASS=YES
A14_4=PASS_PHYSICAL
```

500 req/s TCP equivale conceptualmente a unas 10 transacciones Modbus por cada scan de 20 ms y queda validado como referencia industrial fuerte para Alpha14.

## Incidente de instrumentación IO/RTC

Durante G2 apareció un falso `IO_STALE=1` con `IO_MAX_AGE_MS=4294967295`.

Causa:

- `jwplcSystemTask` actualiza timestamps de I/O y RTC concurrentemente;
- el benchmark podía capturar `millis()` antes de que el runtime actualizara `last_scan_ms`;
- una diferencia efectiva de `-1 ms` se convertía en `UINT32_MAX`.

G2b/G3 demostraron físicamente que no existía congelación real de I/O.

El harness versionado quedó robustecido en:

```text
COMMIT=8d41b8aa51282b4232e43cb0ccff3ac7c4f1ea85
BENCHMARK_COMPILE=PASS
PRODUCTION_RUNTIME_CHANGED=NO
```

La corrección afecta únicamente la instrumentación de freshness del benchmark.

## A14.5 — Robot / interoperabilidad

```text
ROBOT_INTEROPERABILITY=DEFERRED_NON_BLOCKING
CUSTOM_TOOLBOX=PENDING
ALPHA14_CLOSE_BLOCKER=NO
```

El caso robot/KUKA no es requisito de cierre de Alpha14. Se retomará cuando exista acceso al robot y esté definida la integración/toolbox correspondiente.

## Build / compatibilidad

```text
FQBN=jwplc_local:esp32:jwplcbasic
PLATFORM=jwplc_local:esp32 2.1.0-dev
JWPLC_ModbusTCP=0.1.0
LOCAL_PACKAGE_RESOLUTION=PASS
ARDUINO_IDE_COMPATIBILITY=PRESERVED
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

`JWPLC_ModbusTCP` permanece opt-in durante Alpha14 y no se añade al autoload global.

## Readiness de cierre

Auditoría final:

```text
GIT_DIFF_CHECK=PASS
CONFLICT_MARKERS=PASS
A14_1=PASS
A14_2=PASS
A14_3=PASS
A14_4=PASS
A14_5_DEFERRED=PASS
MODBUS_TCP_LIBRARY=PASS
BENCHMARK_ARTIFACTS=PASS
READINESS_FAIL_COUNT=0
ALPHA14_CLOSE_READINESS=PASS
```

Estado de rama al readiness:

```text
BASE_ONLY_COMMITS=0
ALPHA14_ONLY_COMMITS=142
```

## Decisiones finales

1. Alpha14 cierra con Modbus TCP Server + Client cooperativos validados.
2. No se añaden wrappers Sync bloqueantes en este alpha.
3. No se retira ningún periférico del autoload normal.
4. La HMI declarativa Dirty / On-Demand es el camino recomendado para carga integrada.
5. `1000 req/s` queda como stress/frontier TCP.
6. `500 req/s TCP + RTU ~50 Hz` queda como referencia industrial fuerte validada físicamente durante 30 min.
7. A14.5 robot/interoperabilidad permanece diferida y no bloqueante.
8. El fix final de freshness modifica sólo el benchmark, no el runtime de producción.

## Siguiente paso

```text
NEXT=OPEN_RELEASE_PR_TO_release/v2.1.x
CI_REQUIRED_BEFORE_MERGE=YES
```
