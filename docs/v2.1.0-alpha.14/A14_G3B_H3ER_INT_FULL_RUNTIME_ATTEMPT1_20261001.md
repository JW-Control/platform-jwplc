# Alpha14 — G3B H3E-R INT full-runtime — intento 1 — 2026-10-01

## Clasificación

```text
PHYSICAL_RUN=COMPLETE
G3B_ATTEMPT_1=REVIEW
PRODUCT_FUNCTIONAL_FAILURE=NO_EVIDENCE
INT_FUNCTIONAL_FAILURE=NO_EVIDENCE
HARDWARE_FAILURE=NO_EVIDENCE
EXACT_REQUEST_COUNT_GUARD=FAIL
TAIL_OUTLIER=OBSERVED
ROOT_CAUSE=UNRESOLVED_HOST_OR_END_TO_END_TAIL
PRODUCT_DEFAULT_CHANGED=NO
NEXT=G3B_R1_REPEAT_SAME_HARNESS
```

No se promociona INT todavía.

## Composición

```text
HEAD=0a60320e8681049ddfe6493041df00d7b8e85049
INT_GUIDED_RX_BUILD=ON
INT_GUIDED_RX_PACKAGE_DEFAULT=0
MASTER_BUILD_OVERRIDE=-DJWPLC_MODBUS_TCP_INT_GUIDED_RX=1
W5500_SPI_HZ=26000000
TCP=FC03/125 @ 1000 req/s
WINDOW=120 s
RTU=50 Hz
DATALOG=BUFFERED_AUTOSERVICE
DISPLAY=HMI_ON_DEMAND_DIRTY
```

## TCP

```text
TARGET_REQUESTS=120000
REQUESTS_SENT=119979
REQUESTS_OK=119979
ACHIEVED_REQ_S=999.79
ACHIEVED_PCT=99.979
TOTAL_TCP_PAYLOAD_MBPS=2.1676
USEFUL_DATA_MBPS=1.9996
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
TCP_CLEAN=YES
```

Todos los requests enviados recibieron respuesta válida. El wrapper H3E-R
conservó el guard histórico de conteo exacto y detuvo el cierre porque faltaron
21 slots temporales del target.

## Latencia y buckets

```text
AVG_US=975.5
P95_US=1373.1
P99_US=2459.3
MAX_US=203297.4
LOOP_AVG_US=268
LOOP_MAX_US=10019
```

| Bucket | req/s | OK | AVG us | P95 us | P99 us | MAX us |
|---|---:|---:|---:|---:|---:|---:|
| 0–60 s | 999.40 | 59964 | 978.1 | 1369.3 | 2327.6 | 203297.4 |
| 60–120 s | 1000.25 | 60015 | 972.9 | 1376.3 | 2600.1 | 8700.3 |

P95/P99 de ambos buckets permanecieron dentro de los guards H3E-R. El pico
203.297 ms apareció sólo en el primer bucket. El segundo volvió a 8.700 ms.

No se atribuye aún el pico al DUT: el loop max del Master fue 10.019 ms y el
SPI probe final tuvo max wait de 58 us. Es compatible con un outlier
end-to-end/host, pero requiere repetición idéntica antes de promoción.

## RTU

```text
STARTED=6001
COMPLETED=6001
SUCCESS=6001
FAILED=0
PERIODS_SKIPPED=0
CRC_ERRORS=0
MASTER_TIMEOUTS=0
ACHIEVED_HZ=50.002
CROSS_COUNT_PASS=YES
```

## DataLog y periféricos

```text
DATALOG_ACCEPTED_BYTES=3840
DATALOG_COMMITTED_BYTES=3744
DATALOG_PENDING_BYTES=96
DATALOG_FAILED_COMMITS=0
FRAM_FAILS=0
RTC_UNAVAILABLE=0
RTC_STALE=0
IO_STALE=0
BUTTON_NOT_READY=0
SPI_PROBE_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
SPI_PROBE_MAX_WAIT_US=58
SPI_PROBE_OVER_1MS=0
SPI_PROBE_OVER_10MS=0
```

## TFT

```text
MASTER_TFT_PHYSICAL_PASS=TRUE
SLAVE_TFT_PHYSICAL_PASS=TRUE
TFT_PHYSICAL_PASS=TRUE
```

## Decisión

No se debilita el guard de conteo exacto para hacer pasar el candidato.

```text
G3B_R1_VARIABLE_CHANGE=NONE
G3B_R1_PRODUCT_MUTATION=NO
G3B_R1_HARNESS_MUTATION=NO
G3B_R1_PHYSICAL_REPEAT=REQUIRED
```

Si la repetición completa 120000/120000 y conserva integridad/tails, el intento
1 se clasificará como outlier temporal no sostenido. Si vuelve a perder slots o
reaparece una cola extrema, se abrirá diagnóstico antes de G3C.
