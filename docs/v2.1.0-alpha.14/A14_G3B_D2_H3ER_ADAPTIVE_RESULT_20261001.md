# Alpha14 — G3B-D2 H3E-R adaptativo — resultado 2026-10-01

## Cierre formal

```text
HEAD=b2a45b46222248eeb4cc2f1003cf542214a2af9c
G3B_D2_H3ER=PASS
TCP_REQUESTS=120000/120000
TCP_REQ_S=1000.00
RTU_HZ=50.004
TCP_ERRORS=0
RTU_ERRORS=0
PERIPHERAL_FAILURE_COUNT=0
MASTER_TFT_PHYSICAL=PASS
SLAVE_TFT_PHYSICAL=PASS
PACKAGE_DEFAULT_INT=0
PACKAGE_DEFAULT_HOT_POLL_US=0
BUILD_INT_GUIDED_RX=1
BUILD_HOT_POLL_US=1500
```

El gate formal pasa. Sin embargo, la promoción a default se mantiene en HOLD
hasta ejecutar un control POLLING en el mismo HEAD actual, porque el full-runtime
adaptativo recupera el rate pero muestra una cola de latencia más alta que el
control POLLING previo.

## TCP

```text
REQUESTED_REQ_S=1000
ACHIEVED_REQ_S=1000.00
ACHIEVED_PCT=100.000
TARGET_REQUESTS=120000
REQUESTS_SENT=120000
REQUESTS_OK=120000
TOTAL_TCP_PAYLOAD_MBPS=2.1680
USEFUL_DATA_MBPS=2.0000
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
```

Latencia:

```text
AVG_US=937.8
P95_US=1360.8
P99_US=2953.7
MAX_US=17182.9
LOOP_AVG_US=879
LOOP_MAX_US=12415
```

Buckets:

```text
0..60 s:
REQ_S=999.98
OK=59999
AVG=935.2 us
P95=1356.2 us
P99=2752.6 us
MAX=17182.9 us

60..120 s:
REQ_S=1000.02
OK=60001
AVG=940.5 us
P95=1364.1 us
P99=3007.8 us
MAX=8842.5 us
```

El rate es temporalmente estable. P95 cambia poco entre buckets. P99 aumenta en
el segundo bucket, pero ambos permanecen dentro de los guards H3E-R.

## RTU y periféricos

```text
RTU_STARTED=6001
RTU_SUCCESS=6001
RTU_FAILED=0
RTU_SKIPPED=0
RTU_CRC_ERRORS=0
RTU_TIMEOUTS=0
RTU_HZ=50.004

DATALOG_ACCEPTED_BYTES=3840
DATALOG_COMMITTED_BYTES=3840
DATALOG_PENDING_BYTES=0
DATALOG_FAILED_COMMITS=0

FRAM_FAILS=0
RTC_UNAVAILABLE=0
RTC_STALE=0
IO_STALE=0
BUTTON_NOT_READY=0
SPI_PROBE_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
```

El SPI probe final terminó con max wait 39 us, cero >1 ms y cero >10 ms.

## Comparaciones

### Contra control POLLING G3B-D0

```text
POLLING_D0:
REQ_S=1000.00
AVG=884.8 us
P95=1269.2 us
P99=2272.5 us
MAX=7942.6 us
LOOP_AVG=633 us
LOOP_MAX=12101 us

ADAPTIVE_D2:
REQ_S=1000.00
AVG=937.8 us
P95=1360.8 us
P99=2953.7 us
MAX=17182.9 us
LOOP_AVG=879 us
LOOP_MAX=12415 us
```

Delta adaptativo vs POLLING D0:

```text
REQ_S=0.000 %
AVG=+5.990 %
P95=+7.217 %
P99=+29.976 %
MAX=+116.338 %
LOOP_AVG=+38.863 %
LOOP_MAX=+2.595 %
```

### Contra INT puro G3B-R1

```text
INT_PURE_R1:
REQ_S=997.86
AVG=977.5 us
P95=1373.6 us
P99=2659.8 us
MAX=8155.3 us
LOOP_AVG=266 us
LOOP_MAX=8613 us
```

D2 recupera completamente el rate y mejora AVG/P95 respecto a INT puro, pero
P99 y MAX son más altos en esta única corrida full-runtime.

### Contra baseline POST-P4.2

```text
POST_P4_2:
REQ_S=1000.00
AVG=880.3 us
P95=1253.2 us
P99=2273.1 us
MAX=8831.5 us
LOOP_AVG=633 us
LOOP_MAX=12012 us
```

Delta adaptativo:

```text
REQ_S=0.000 %
AVG=+6.532 %
P95=+8.586 %
P99=+29.941 %
MAX=+94.564 %
LOOP_AVG=+38.863 %
LOOP_MAX=+3.355 %
```

## Interpretación

El scheduler adaptativo resuelve el defecto principal del INT puro: recupera
120000/120000 y 1000 req/s dentro del full-runtime completo.

También conserva la ventaja demostrada previamente en IDLE: >99 % menos
status/available calls cuando el socket queda sin tráfico.

Pero el full-runtime revela un costo de tail latency frente a POLLING. El gate
formal pasa porque:

```text
P95_GUARD=PASS
P99_GUARD=PASS
TAIL_REGRESSION=NOT_PRESENT
```

Eso no equivale a demostrar equivalencia con POLLING.

## Decisión

```text
G3B_D2_H3ER_FORMAL=PASS
G3C_PROMOTION=HOLD
REASON=MATCHED_CURRENT_HEAD_POLLING_CONTROL_REQUIRED
NEXT=G3B_D2_C0_CURRENT_HEAD_POLLING_FULL_RUNTIME
```

El siguiente paso debe usar el mismo HEAD actual, mismo H3E-R, mismo hardware y
sin override INT/HOT. No se modifica source ni harness.

Si POLLING actual reproduce ~884/1269/2273 us, el tradeoff adaptativo queda
confirmado y debe decidirse/tunearse antes de promoción. Si POLLING actual
también muestra colas cercanas a D2, se reclasifica la diferencia como
variabilidad/runtime actual y se puede reabrir G3C.
