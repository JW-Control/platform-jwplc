# v2.1.0-alpha.14 — Estado

Actualizado: `2026-09-28`

## Identidad

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
BASE_BRANCH=release/v2.1.x
BASE_SHA=165f94fb25617d3a6035fdfd9899bd38a1331583
ALPHA14_STATUS=POST_H3E_THRESHOLD_REQUALIFICATION
ALPHA14_H3E=CLOSED_PASS
ALPHA14_RELEASE_PR_READY=PAUSED_FOR_POST_H3E_REQUALIFICATION
```

## Resultado global

```text
A14_1=PASS
A14_2=PASS
A14_3=PASS_PHYSICAL
A14_4=PASS_PHYSICAL
A14_5_ROBOT_INTEROPERABILITY=DEFERRED_NON_BLOCKING
ALPHA14_CLOSE_READINESS=PASS
ALPHA14_TECHNICAL_CLOSURE_20260913=PASS_HISTORICAL
A14_H3E=PASS
ALPHA14_CURRENT_RELEASE_READINESS=PAUSED_POST_H3E_REQUALIFICATION
```

Alpha14 incorpora y valida `JWPLC_ModbusTCP` como nueva librería del ecosistema JWPLC para servidor y cliente Modbus TCP cooperativos, manteniendo compatibilidad Arduino IDE y coexistencia con el runtime integrado del JWPLC Basic.

No se retiran periféricos del autoload normal.


## Actualización posterior al cierre histórico — H3E (2026-09-28)

El cierre del 13-sep se conserva como evidencia histórica de A14.1-A14.5, pero
dejó de ser el estado final del branch cuando Alpha14 continuó con DataLog,
core precompilado, hardening RTU y Display.

H3E queda cerrado:

```text
A14_H3E=CLOSED_PASS
H3E_PRODUCT_FAILURE=NO
H3E_HARDWARE_FAILURE=NO
```

Hallazgos cerrados:

```text
ORIGINAL_SYSTEMTASK_GAP_DOMINANT=DISPLAY
DISPLAY_ALPHA11_ARCHIVE=STALE_PERFORMANCE
PRECOMPILED_FULL_STRATEGY=VALID
W5500_SPI_HZ=26000000
```

Artifacts finales usados en la regresión full runtime:

```text
JWPLC_Display.a
SHA256=52B9BC617FACB77705161B4F07E6D45571043E4473934EFE19A1F5444BB5D986

libJWPLC_TFT.a
SHA256=5D860A131811DD9A7EB6FA55F5674B1D78B0DE7DFAF8748CE18A60CEED2D3738

core.a
SHA256=4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566

libJWPLC_ModbusRTU.a
SHA256=486BE38AE088B94898E516FFBC125855F22C2EC5EE8A6FA9E10F35D7CAC3A3BE
```

Regresión H3E.5:

```text
TCP=120000/120000 @ 1000.00 req/s
RTU=6001/6001 @ 50.004 Hz
RTU_TIMEOUTS=0
RTU_CRC_ERRORS=0
SD_DATALOG_FAILED_COMMITS=0
PERIPHERAL_FAILURE_COUNT=0
TFT_MASTER_SLAVE_PHYSICAL=PASS
A14_H3E5_EXISTING_RUN_REVALIDATION=PASS
```

Core y Modbus RTU fueron adoptados en:

```text
f1648654eac265de64aefd1ea601c28f01db9db8
```

Evidencia consolidada:

- `A14_H3E_CLOSURE_20260928.md`


### P5 final / core autoservice — reconciliación de pendientes históricos

También quedan cerrados los pendientes que el checklist del 24-sep todavía
mostraba como abiertos.

P5-D final con DataLog físico, 600 s:

```text
TCP_ACHIEVED_REQ_S=979.68
RTU_REQUESTS=30002/30002
RTU_ACHIEVED_HZ=50.001
RTU_FAILED=0
RTU_TIMEOUTS=0
SD_DATALOG_ACCEPTED_BYTES=19200
SD_DATALOG_COMMITTED_BYTES=19136
SD_DATALOG_PENDING_BYTES=64
SD_DATALOG_FAILED_COMMITS=0
SD_APPEND_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
```

P5-E2-R1, mismo core final `4BFF...`, 600 s:

```text
TCP_UNPACED_ACHIEVED_REQ_S=1006.066
RTU_ACHIEVED_HZ=50.002
SD_FAILED_COMMITS=0
PERIPHERAL_FAILURE_COUNT=0
TFT_MASTER_SLAVE=PASS
USER_MANUAL_TCP_TASK_REQUIRED=NO
PUBLIC_API_CHANGE=NO
```

Por tanto:

```text
P5_D_FINAL_600S=PASS
SD_DATALOG_PHYSICAL_AUTOSERVICE=PASS
P5_F_CORE_AUTOSERVICE=PASS
P5_E2_R1_CORE_AUTOSERVICE_600S=PASS
ALL_PERIPHERAL_600S_SOAK=PASS
CORE_FINAL_SHA256=4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566
```

El hash protegido de `common.ps1` se actualiza a este core final antes del
readiness global.


La readiness de PR debe auditarse nuevamente contra el estado actual completo del
branch; no reutilizar automáticamente el readiness del 13-sep.


## Readiness final R0 — 2026-09-28

R0 está cerrado:

```text
A14_FINAL_READINESS_R0=PASS
GIT_STATE=PASS
ARTIFACT_INVARIANTS=PASS
ETHERNET_NB3_PRODUCT_HASHES=PASS
W5500_SPI_HZ=26000000
MODBUS_TCP_AUTOSERVICE_CONTRACT=PASS
MODBUS_TCP_SERVER_CLI=PASS
MODBUS_TCP_CLIENT_CLI=PASS
FULL_RUNTIME_MASTER_CLI=PASS
RTU_SLAVE_CLI=PASS
REPOSITORY_MUTATION=NO
```

Arduino CLI observada:

```text
1.0.2 / 33dfa8e8 / 2024-07-02
```

F068 quedó clasificado como falso negativo de harness por contar 4 llamadas
textuales del callback de autoservicio en vez de los 2 puntos lógicos
pre-loop/post-loop.

Evidencia:

- `A14_FINAL_READINESS_R0_R1_20260928.md`

Próximo gate:

```text
R1 = ARDUINO IDE REAL COMPILE + UPLOAD + NORMAL AUTOLOAD PHYSICAL
SKETCH = 06_alpha4_local_physical_gate
PORT = COM14
```

No abrir PR hasta cerrar R1.



## Readiness final R1 — Arduino IDE / físico

R1 está cerrado:

```text
A14_R1_ARDUINO_IDE_COMPILE=PASS
A14_R1_ARDUINO_IDE_UPLOAD=PASS
A14_R1_NORMAL_AUTOLOAD_PHYSICAL=PASS
A14_FINAL_ARDUINO_IDE_GATE=PASS
```

Gate reutilizado:

```text
06_alpha4_local_physical_gate.ino
Board=JWPLC Basic
Port=COM14
Serial=115200
```

Resultado físico:

```text
DISPLAY_READY=PASS
RTC=PASS
FRAM=PASS
SD=PASS
BUTTONS_6_OF_6=PASS
DIGITAL_INPUTS_8_OF_8=PASS
DIGITAL_OUTPUTS_8_OF_8=PASS
TFT_VISUAL=PASS
LOCAL_PHYSICAL_GATE=PASS
```

Boot observado post-upload:

```text
POWERON_RESET
SPI_FAST_FLASH_BOOT
mode:DIO
clock div:2
```

Esto es evidencia del perfil ejecutado; no redefine una FlashFreq universal
final ni autoriza publicar un `bootloader.bin` definitivo.

Con R0 + R1:

```text
FINAL_REAUDIT=PASS
ALPHA14_RELEASE_PR_READY=YES_PENDING_CI
```



## Requalification post-H3E de límites TCP / RTU

La mejora drástica de Display/TFT en H3E obliga a reinterpretar algunos
frontiers medidos antes de H3E.

Evidencia causal relevante:

```text
RTU-H3C.1 pre-H3E:
RTU_SERVICE_GAP_MAX_US=19307
LOOP_GAP_MAX_US=19301

H3E baseline histórico:
SYS_DISPLAY ~= 23.6 ms

H3E.5 final:
LOOP_MAX_US=8212
TCP=1000.0 req/s
RTU=50.004 Hz
```

Por tanto:

```text
R0=PASS_RETAINED
R1=PASS_RETAINED
PR99_MERGE_READINESS=PAUSED
POST_H3E_THRESHOLD_REQUALIFICATION=OPEN
```

Plan:

- H4A0: nuevo ceiling RAW Ethernet TCP/UDP en Mbps.
- H4A1: nuevo frontier Modbus TCP-only en req/s.
- H4B: frontier TCP con RTU fijo a 50 Hz.
- H4C: RTU FAST 500 kbaud, 100 us vs 75 us; 50 us excluido.
- H4D: SFIFO/BULK post-H3E, incluyendo perfil estable FIFO9/FIFO8.
- H4E: coexistencia extrema TCP+RTU para cifra reproducible de datasheet.

Documento:

- `A14_POST_H3E_THRESHOLD_REQUALIFICATION_20260928.md`


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
NEXT=RUN_H4A0_POST_H3E_RAW_ETHERNET_CEILING
CI_REQUIRED_BEFORE_MERGE=YES
HISTORICAL_READY_FOR_RELEASE_PR_20260913=SUPERSEDED_BY_POST_CLOSURE_HARDENING
```
