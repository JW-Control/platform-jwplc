# Alpha14 — G3B-D2-C0 current-head POLLING — resultado 2026-10-01

## Cierre

```text
HEAD=62299072c399306a92901293bd74d04fcd1ebbde
C0_STATUS=PASS_COST_CONFIRMED
C0_POLLING_REPRODUCES_LOW_TAIL=YES
C0_INTERPRETATION=D2_TAIL_COST_CONFIRMED
NEXT=G3B_D3_M0_LOAD_CHARACTERIZATION
```

## Resultado formal

```text
TCP_REQ_S=1000.000
TCP_REQUESTS_SENT=120000
TCP_REQUESTS_OK=120000
AVG_US=884.1
P95_US=1262.6
P99_US=2413.6
MAX_US=8972.1
LOOP_AVG_US=591.0
LOOP_MAX_US=9645.0
```

No hubo errores TCP, pérdidas de requests, resets inesperados ni fallos de
periféricos. RTU cerró 6001/6001 a 50.004 Hz.

## Comparación contra POLLING D0

```text
AVG_DELTA=-0.079 %
P95_DELTA=-0.520 %
P99_DELTA=+6.209 %
MAX_DELTA=+12.962 %
```

El control current-head reproduce el régimen de latencia baja observado en D0.

## Comparación contra D2 adaptativo

```text
AVG_DELTA=-5.726 %
P95_DELTA=-7.216 %
P99_DELTA=-18.286 %
MAX_DELTA=-47.785 %
```

Esto confirma que el incremento agregado de cola observado en D2 pertenece a la
política adaptativa actual y no a una degradación general del HEAD/harness.

## Matiz temporal

C0 también presenta variabilidad por bucket:

```text
0..60 s:
P95=1261.7 us
P99=2311.6 us
MAX=8370.8 us

60..120 s:
P95=1262.9 us
P99=3370.4 us
MAX=8972.1 us
```

Por tanto, `D2_TAIL_COST_CONFIRMED` se refiere al resultado agregado matched
current-head, no a que D2 sea peor en cada bucket individual.

## Periféricos

```text
RTU_HZ=50.004
RTU_STARTED=6001
RTU_SUCCESS=6001
RTU_FAILED=0
RTU_SKIPPED=0
RTU_CRC_ERRORS=0
RTU_TIMEOUTS=0

DATALOG_ACCEPTED=3840
DATALOG_COMMITTED=3712
DATALOG_PENDING=128
DATALOG_FAILED_COMMITS=0

FRAM_FAILS=0
RTC_UNAVAILABLE=0
RTC_STALE=0
IO_STALE=0
BUTTON_NOT_READY=0
SPI_PROBE_FAILS=0
PERIPHERAL_FAILURE_COUNT=0
MASTER_TFT=PASS
SLAVE_TFT=PASS
```

## Decisión

No promover D2 de 1500 us como default.

El siguiente gate es D3-M0: caracterizar el comportamiento por carga antes de
fijar thresholds del scheduler load-adaptive.

No se modifica todavía el runtime productivo.
