# Alpha14 — G3B-E1 INT RSR-drain — resultado 2026-10-02

## Clasificación

```text
HEAD=e109474c8110c0ada2e02a6cb76d2ac7376247ba
E1_TCP=PASS
E1_RTU=PASS
E1_PERIPHERALS=PASS
E1_CROSS_COUNT=PASS
E1_FUNCTIONAL_ERRORS=0
E1_PERFORMANCE_VS_C0=NOT_COMPETITIVE
E1_SECOND_INT_RUN=NO
PRODUCT_DEFAULT_CHANGED=NO
NEXT=CLOSE_INT_FOR_V2
```

## Resultado E1

```text
REQUESTS=60000/60000
REQ_S=999.999
AVG=945.8 us
P95=1494.5 us
P99=3558.8 us
MAX=19311.6 us
LOOP_AVG=398 us
LOOP_MAX=9325 us
TCP_ERRORS=0
RTU=3001/3001
RTU_HZ=50.011
```

Todos los bloques funcionales pasaron: TCP, runtime Master, RTU Master, RTU
Slave, cross-count y periféricos.

## Comparación contra C0 POLLING

Referencia C0 current-head:

```text
REQ_S=1000.00
AVG=884.1 us
P95=1262.6 us
P99=2413.6 us
MAX=8972.1 us
LOOP_AVG=591 us
```

Delta E1 vs C0:

```text
REQ_S=-0.0001 %
AVG=+6.98 %
P95=+18.37 %
P99=+47.45 %
MAX=+115.24 %
LOOP_AVG=-32.66 %
```

E1 libera tiempo del loop y conserva throughput completo, pero queda muy lejos
del criterio previamente fijado para justificar una segunda corrida INT
(P95/P99 aproximadamente dentro de +3 % de C0).

## Decisión

No ejecutar una segunda corrida INT matched. El resultado es suficientemente
lejano de C0 en tails para cerrar la evaluación INT del JWPLC Basic v2.

```text
INT_V2=NOT_PROMOTED
POLLING_C0=SELECTED
INT_BUS_EFFICIENCY=CONFIRMED
INT_SPEED_GAIN=NOT_CONFIRMED
INT_LATENCY_GAIN=NO
INT_REEVALUATION=JWPLC_BASIC_V3
```
