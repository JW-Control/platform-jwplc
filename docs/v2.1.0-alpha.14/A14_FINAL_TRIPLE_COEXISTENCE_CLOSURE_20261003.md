# Alpha14 - cierre final de coexistencia TCP250 + RTU800 + UDP FAST — 2026-10-03

## Estado

```text
A14_FINAL_TRIPLE_COEXISTENCE=PASS_CONFIRMED
PROFILE=TCP250_RTU800_UDP1M_FULL_RUNTIME
DURATION=600S
CONTRACT=OPERATIONAL
HARD_REALTIME_ZERO_SKIP=NO
```

## Objetivo

Cerrar la caracterización de coexistencia simultánea del JWPLC Basic con:

- Modbus TCP: 250 req/s.
- Modbus RTU: 800 req/s nominales.
- RTU EXP-MIX: 8 transacciones por scan.
- objetivo operacional RTU: >=99 % de 800 req/s y >=99 scans/s.
- UDP FAST RX.
- full runtime activo: Display + SD/DataLog + FRAM + RTC + TCA/I/O + botonera.

La bancada RTU usa un único Slave ID 2 que emula ocho operaciones lógicas
(2 DI + 2 DO + 2 AI + 2 AO). Cada operación conserva su transacción RTU
completa. Esto caracteriza presupuesto temporal equivalente a ocho operaciones
por scan, pero no sustituye una prueba física multidrop con ocho placas.

## Evidencia final confirmada — UDP 1 Mbps / 600 s

HEAD de la corrida física:

```text
924f8b704fac6954d75e3e0ad5b12ee69f935ef6
```

Resultado raíz:

```text
tools/modbus-tcp-benchmark/results/a14_tcp250_rtu800_udp_ladder_20261003_195019
```

Gate:

```text
A14_FINAL_TRIPLE_GATE=PASS_CONFIRMATION_ONLY
RUNNER_EXIT=0
TRACKED_DIRTY_FINAL=0
STAGED_FINAL=0
```

### TCP Modbus

```text
TARGET_REQ_S=250
ACHIEVED_REQ_S=250.001
TARGET_PCT=100.001
REQUESTS_OK=150001
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
FRAME_TIMEOUTS=0
BUS_LOCK_TIMEOUTS=0
P95_US=1772.8
P99_US=4138.4
MAX_US=21412.8
```

Conclusión:

```text
TCP250=PASS
```

### Modbus RTU

```text
TARGET_REQ_S=800
ACHIEVED_REQ_S=796.953
TARGET_PCT=99.619
SCANS_S=99.619
SCAN_PERIOD_MS=10.038
SCANS=59783
TRANSACTIONS_SUCCESS=478264
TRANSACTIONS_FAILED=0
TRANSACTIONS_REJECTED=0
VERIFY_FAILS=0
PERIODS_SKIPPED=229
TRANSACTION_MAX_US=13574
SERVICE_GAP_MAX_US=13200
```

Consistencia por tipo:

```text
DI_SUCCESS=119566
DO_SUCCESS=119566
AI_SUCCESS=119566
AO_SUCCESS=119566

119566 * 4 = 478264
59783 * 8 = 478264
```

Slave:

```text
RX_FRAMES=478264
TX_FRAMES=478264
REQUESTS_OK=478264
CRC_ERRORS=0
EXCEPTIONS_SENT=0
MASTER_TIMEOUTS=0
DISCARDED_TAILS=0
DISCARDED_BYTES=0
```

Conclusión:

```text
RTU800_OPERATIONAL=PASS
RTU100HZ_OPERATIONAL=PASS
RTU100HZ_ZERO_SKIP_DETERMINISTIC=NO
```

Los 229 skipped periods se conservan como evidencia de jitter/deadline miss.
No se afirma hard real-time ni cero-jitter.

### UDP FAST RX

Host pacing:

```text
MODE=DEDICATED_PROCESS_ACTUAL_SEND_INTERVAL
TARGET_MBPS=1
OFFERED_MBPS=0.998529
OFFERED_TARGET_PCT=99.853
MIN_START_GAP_TARGET_PCT=100.000
START_TOO_CLOSE_PACKETS=0
SEND_ERRORS=0
```

DUT:

```text
DELIVERED_MBPS=0.998469
DELIVERED_TARGET_PCT=99.847
SENT_PACKETS=73706
RX_PACKETS=73706
DELIVERY_PCT=100.000
RANGE_MISSING=0
WRONG_SIZE=0
SEQUENCE_DECODE_ERRORS=0
DUPLICATES=0
REORDERS=0
TRANSPORT_ERRORS=0
SPI_LOCK_ERRORS=0
SPI_HOLD_MAX_US=1683
```

Conclusión:

```text
UDP_FAST_1MBPS=PASS_CONFIRMED_600S
```

## Full runtime

Durante la confirmación permanecieron activos y ready:

- Display/HMI.
- Ethernet W5500.
- SD/DataLog.
- FRAM.
- RTC.
- TCA/I/O.
- botonera.
- RS-485 / Modbus RTU.

Resultado:

```text
FULL_RUNTIME_READY=YES
PERIPHERAL_FAILURE_COUNT=0
SD_DATALOG_FAILED_COMMITS=0
SPI_PROBE_FAILS=0
SPI_PROBE_MAX_WAIT_US=547
SPI_PROBE_OVER_1MS=0
SPI_PROBE_OVER_10MS=0
LOOP_GAP_MAX_US=13202
RESET=NO
RUNTIME_CLEAN=YES
```

## Ladder previo con host pacing validado

Resultados de 300 s:

| UDP ofrecido | TCP req/s | RTU req/s | scans/s | UDP DUT | delivery | Operacional |
|---:|---:|---:|---:|---:|---:|:---:|
| 0 Mbps | 250.003 | 799.899 | 99.987 | 0 | 100 % | PASS |
| 1 Mbps | 250.003 | 797.659 | 99.707 | 0.997 | 100.000 % | PASS |
| 2 Mbps | 250.003 | 793.007 | 99.126 | 1.996 | 99.996 % | PASS |
| 4 Mbps | 250.003 | 727.840 | 90.980 | 3.873 | 97.734 % | FAIL |
| 6 Mbps | 250.003 | 641.024 | 80.128 | 5.687 | 95.003 % | FAIL |
| 8 Mbps | 250.003 | 562.457 | 70.307 | 6.493 | 81.368 % | FAIL |
| 10 Mbps | 250.002 | 525.831 | 65.729 | 7.084 | 71.075 % | FAIL |
| 12 Mbps | 250.002 | 500.290 | 62.536 | 7.414 | 61.947 % | FAIL |

## Candidato UDP 2 Mbps — no confirmado

2 Mbps pasó el escalón de 300 s:

```text
TCP=250.003 req/s
RTU=793.007 req/s
SCANS=99.126/s
UDP_DUT=1.996 Mbps
UDP_DELIVERY=99.996 %
OPERATIONAL_PASS=True
```

Pero no sostuvo el contrato en confirmación de 600 s:

```text
TCP=250.001 req/s
RTU=788.239 req/s
SCANS=98.530/s
UDP_DUT=1.996 Mbps
UDP_DELIVERY=99.988 %
RUNTIME_CLEAN=True
RTU_OPERATIONAL_PASS=False
UDP_PASS=True
```

La caída fue RTU, no UDP.

Conclusión:

```text
UDP_FAST_2MBPS=PASS_300S_NOT_CONFIRMED_600S
```

## Contrato publicable

La cifra fuerte y confirmada para coexistencia simultánea queda:

```text
TCP Modbus: 250 req/s
Modbus RTU: 796.953 req/s medidos
             99.619 scans/s
             8 operaciones RTU por scan
UDP FAST RX: 0.998 Mbps medidos
Full runtime: activo y limpio
Duración de confirmación: 600 s
```

Forma resumida:

```text
TCP250 + RTU800 nominal (~100 scans/s x 8 operaciones)
+ UDP FAST 1 Mbps
+ full runtime
= PASS operacional confirmado 600 s
```

No debe publicarse como:

```text
100 Hz hard real-time
0 jitter
0 deadline misses
```

porque la confirmación registró 229 skipped periods.

## Relación con ceilings RAW

Este resultado no sustituye los ceilings RAW individuales ya medidos:

```text
TCP RX RAW ~14.11 Mbps
TCP TX RAW ~4.65 Mbps
UDP RX legacy RAW ~11.60 Mbps
UDP TX RAW ~5.18 Mbps
UDP RX FAST RAW ~13.18 Mbps
```

Esos valores caracterizan capacidad individual. El presente cierre caracteriza
coexistencia simultánea bajo full runtime y cargas TCP/RTU fijadas.

## Decisión final

```text
FINAL_TRIPLE_PROFILE=TCP250_RTU800_UDP1M_FULL_RUNTIME
UDP1M_CONFIRM_600S=PASS
UDP2M_CONFIRM_600S=FAIL_RTU_OPERATIONAL_THRESHOLD
TCP250_STABLE=YES
RTU800_OPERATIONAL=YES
UDP_FAST_INTEGRITY=YES
FULL_RUNTIME_CLEAN=YES
HARD_REALTIME_ZERO_SKIP=NO
PRODUCT_SOURCE_MUTATION_FOR_FINAL_CONFIRM=NO
```
