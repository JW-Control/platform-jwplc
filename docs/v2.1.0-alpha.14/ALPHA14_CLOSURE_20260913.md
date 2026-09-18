# v2.1.0-alpha.14 — Cierre técnico

Fecha: `2026-09-13`

Rama:

```text
v2.1.0-alpha.14/feature/modbus-tcp
```

Base:

```text
release/v2.1.x
BASE_SHA=165f94fb25617d3a6035fdfd9899bd38a1331583
```

## Clasificación final

```text
ALPHA14_TECHNICAL_CLOSURE=PASS
A14_1=PASS
A14_2=PASS
A14_3=PASS_PHYSICAL
A14_4=PASS_PHYSICAL
A14_5=DEFERRED_NON_BLOCKING
ALPHA14_CLOSE_READINESS=PASS
```

Alpha14 queda técnicamente cerrada y lista para PR hacia `release/v2.1.x`.

## Objetivo del alpha

Incorporar Modbus TCP al ecosistema JWPLC Basic sin romper compatibilidad Arduino IDE ni degradar la estabilidad de los periféricos integrados.

El alcance incluyó:

- Modbus TCP Server;
- Modbus TCP Client cooperativo;
- matriz de funciones FC01/02/03/04/05/06/15/16;
- timeout, cierre graceful y reconexión;
- benchmark de rendimiento TCP;
- full runtime con HMI, SD, FRAM, RTC, I/O, botonera y SPI;
- coexistencia física simultánea Modbus RTU + Modbus TCP;
- soak físico de 30 minutos;
- saneamiento final de instrumentación de freshness.

## A14.1 — Foundation + Server

Resultado: `PASS`.

Funciones validadas:

```text
FC01=PASS
FC02=PASS
FC03=PASS
FC04=PASS
FC05=PASS
FC06=PASS
FC15=PASS
FC16=PASS
```

También quedaron validados:

- listen del servidor;
- excepciones;
- MBAP inválido;
- reconexión;
- regresión de empty sketch.

Evidencias:

- `A14_1_SERVER_FUNCTION_MATRIX_20260911.md`
- `A14_1_INVALID_MBAP_RECONNECT_20260911.md`

## A14.2 — Client cooperativo

Resultado: `PASS`.

La matriz Client completa fue validada físicamente sobre una sesión persistente:

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
ERRORS=0
```

Timeout y reconexión:

```text
FIRST_TIMEOUT_OBSERVED=YES
SESSION_CLOSED_AFTER_TIMEOUT=YES
SECOND_REQUEST_RECOVERED=YES
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
```

Decisión API:

```text
A14_2_SYNC_API_DECISION=DEFERRED_POST_ALPHA14
```

La API cooperativa permanece como referencia. Alpha14 no introduce wrappers Sync bloqueantes.

Evidencia:

- `A14_2_CLIENT_CLOSURE_20260912.md`

## A14.3 — Performance

Resultado: `PASS_PHYSICAL`.

### Hallazgo HMI

La pérdida de rendimiento inicial estaba dominada por el patrón legacy de redraw manual, no por la mera presencia de la TFT.

La API declarativa HMI Dirty / On-Demand recuperó prácticamente el baseline TCP-only.

### Soak final 30 min

Perfil:

```text
FC03/125
REQUESTED=1000 req/s
DURATION=1800 s
```

Resultado:

```text
ACHIEVED=999.34 req/s
ACHIEVED_PCT=99.934
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
```

Todos los periféricos integrados permanecieron activos y limpios.

Evidencia:

- `A14_3_PERFORMANCE_CLOSURE_20260913.md`

## A14.4 — RTU + TCP simultáneo

Resultado: `PASS_PHYSICAL`.

### Topología física

```text
COM4=JWPLC RTU Master
COM14=DUT TCP Server + RTU Slave ID 2 + full runtime
```

### Perfil final G3

```text
TCP=FC03/125 @500 req/s
RTU=FC03/16 @20ms target
RTU_BAUD=115200
RTU_FORMAT=8N1
DURATION=1800 s
```

### TCP

```text
REQUESTS=900000/900000
ACHIEVED=500.000 req/s
ACHIEVED_PCT=100.0000
P95_US=3725.2
P99_US=4517.7
MAX_US=30365.1
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
```

### RTU Master

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

### DUT y full runtime

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
```

Resultado formal:

```text
SIMULTANEOUS_30MIN_STRONG_PASS=YES
A14_4=PASS_PHYSICAL
```

Referencia industrial establecida para Alpha14:

```text
TCP_REFERENCE=500 req/s
RTU_REFERENCE≈50 Hz
SCAN_REFERENCE=20 ms
```

500 req/s equivalen conceptualmente a unas 10 transacciones Modbus por scan de 20 ms.

Evidencia:

- `A14_4_RTU_TCP_SIMULTANEOUS_CLOSE_20260913.md`

## Instrumentación de freshness

Durante G2 apareció:

```text
IO_STALE=1
IO_MAX_AGE_MS=4294967295
```

Se determinó que era un falso positivo del benchmark por carrera de lectura entre `loopTask` y `jwplcSystemTask`.

No hubo congelación real del TCA/I/O.

G2b y G3 confirmaron:

```text
IO_STALE=0
IO_MAX_AGE_MS=24..27
PERIPHERAL_FAILURE_COUNT=0
```

El harness versionado quedó saneado en:

```text
COMMIT=8d41b8aa51282b4232e43cb0ccff3ac7c4f1ea85
BENCHMARK_COMPILE=PASS
PRODUCTION_RUNTIME_CHANGED=NO
```

El cambio cubre freshness de I/O y RTC tanto en muestreo acumulativo como en valores current-age del snapshot.

## Compatibilidad y alcance

```text
ARDUINO_IDE_COMPATIBILITY=PRESERVED
PUBLIC_API_BREAKING_CHANGE=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
OPENPLC_AUTOLOAD_INTEGRATION=NO
OTA=NOT_DEFINED
FLASH_FREQ_FINAL=NOT_DEFINED
BOOTLOADER_FINAL_PUBLICATION=NO
```

`JWPLC_ModbusTCP` permanece opt-in durante Alpha14.

Periféricos preservados:

```text
DISPLAY=YES
ETHERNET_W5500=YES
MICROSD=YES
FRAM=YES
RTC=YES
BUTTONS=YES
RS485=YES
MODBUS_RTU=YES
TCA_IO=YES
SPI_ARBITRATION=YES
```

## A14.5 — Robot / interoperabilidad

Estado:

```text
ROBOT_INTEROPERABILITY=DEFERRED_NON_BLOCKING
CUSTOM_TOOLBOX=PENDING
ALPHA14_CLOSE_BLOCKER=NO
```

La interoperabilidad con robot/KUKA se retomará cuando exista acceso al robot y esté definida la integración/toolbox correspondiente.

No bloquea Alpha14.

## Readiness final

Auditoría previa al cierre:

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

## Decisiones finales

1. Cerrar Alpha14 con `JWPLC_ModbusTCP` Server + Client cooperativos validados.
2. Mantener API cooperativa como referencia; Sync queda diferida.
3. Mantener HMI declarativa Dirty / On-Demand como camino recomendado de integración.
4. Conservar 1000 req/s como stress/frontier TCP.
5. Adoptar 500 req/s TCP + RTU ~50 Hz como referencia industrial fuerte validada durante 30 minutos.
6. Mantener todos los periféricos del autoload normal.
7. Mantener A14.5 como `DEFERRED_NON_BLOCKING`.
8. No introducir cambios al runtime de producción por el fix de freshness del benchmark.

## Estado para PR

```text
ALPHA14_TECHNICAL_CLOSURE=PASS
ALPHA14_STATUS=READY_FOR_RELEASE_PR
CI_REQUIRED_BEFORE_MERGE=YES
PR_TARGET=release/v2.1.x
```
