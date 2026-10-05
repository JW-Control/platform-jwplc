# Alpha14 — G3A INT Modbus TCP real — resultado 2026-10-01

## Cierre

```text
HEAD=cec0c71b13a9410cad6071e0f56600f0fef545b0
G3A_STATUS=PASS
G3A_PRODUCT_CANDIDATE_READY_FOR_H3ER=YES
G3A_PRODUCT_DEFAULT_CHANGED=NO
NEXT=G3B_H3ER_INT_FULL_RUNTIME
```

G3A validó el candidato interno `JWPLC_MODBUS_TCP_INT_GUIDED_RX` con Modbus
TCP real FC03/125 a 1000 req/s. Se ejecutaron dos corridas POLLING y dos
INT_GUIDED, 30 s por corrida, en orden balanceado.

## Integridad funcional

Las cuatro corridas completaron:

```text
TARGET_REQUESTS=30000
REQUESTS_SENT=30000
REQUESTS_OK=30000
FUNCTIONAL_PASS=YES
```

No se observaron fallos de protocolo, transporte ni accounting funcional.

## Resultados agregados

| Métrica | POLLING | INT_GUIDED | Cambio |
|---|---:|---:|---:|
| req/s mediana | 999.987 | 999.991 | ~0.000 % |
| dispersión req/s | 0.001 % | 0.001 % | equivalente |
| P95 | 1059.300 us | 1106.258 us | +4.433 % |
| P99 | 1127.650 us | 1221.951 us | +8.363 % |
| MAX mediana | 2163.000 us | 7387.600 us | revisar outlier |
| loop max mediana | 3042 us | 3116 us | +2.43 % |
| status calls | 171388 | 30000 | -82.496 % |
| available calls | 201388 | 60000 | -70.207 % |
| available-zero | 141388 | 0 | eliminado |

## Corridas individuales

### POLLING #1

```text
REQ_S=999.990
AVG_US=745.597
P95_US=1082.800
P99_US=1142.500
MAX_US=2677.700
LOOP_MAX_US=3012
STATUS_CALLS=171138
AVAILABLE_CALLS=201138
AVAILABLE_ZERO=141138
```

### INT_GUIDED #1

```text
REQ_S=999.988
AVG_US=776.916
P95_US=1116.510
P99_US=1237.902
MAX_US=12267.100
LOOP_MAX_US=3204
STATUS_CALLS=30001
AVAILABLE_CALLS=60001
AVAILABLE_ZERO=1
```

### INT_GUIDED #2

```text
REQ_S=999.993
AVG_US=760.312
P95_US=1096.005
P99_US=1206.000
MAX_US=2508.100
LOOP_MAX_US=3029
STATUS_CALLS=30000
AVAILABLE_CALLS=60000
AVAILABLE_ZERO=0
```

### POLLING #2

```text
REQ_S=999.985
AVG_US=703.126
P95_US=1035.800
P99_US=1112.801
MAX_US=1648.300
LOOP_MAX_US=3072
STATUS_CALLS=171639
AVAILABLE_CALLS=201639
AVAILABLE_ZERO=141639
```

## Interpretación

El mecanismo INT queda confirmado en tráfico Modbus TCP real:

```text
STATUS_CALL_REDUCTION=82.496 %
AVAILABLE_CALL_REDUCTION=70.207 %
RATE_DELTA≈0 %
P95_GUARD=PASS
P99_GUARD=PASS
```

La mejora principal es eficiencia del bus compartido, no throughput. La tasa
de 1000 req/s se mantiene sin regresión material.

El aumento agregado de P95/P99 permanece dentro de los guards definidos. La
primera corrida INT presentó un MAX aislado de 12.267 ms que no se repitió en
la segunda corrida INT (2.508 ms); por ello no se clasifica como regresión
sostenida, pero se mantiene como punto de observación obligatorio en G3B.

## Decisión

```text
G3A_INT_MECHANISM_CONFIRMED=YES
G3A_RATE_GUARD=PASS
G3A_P95_GUARD=PASS
G3A_P99_GUARD=PASS
G3A_PRODUCT_CANDIDATE_READY_FOR_H3ER=YES
G3A_DEFAULT_PROMOTION=NOT_YET
```

El siguiente gate es G3B full-runtime: TCP 1000 req/s + RTU 50 Hz + DataLog +
Display/TFT + FRAM + RTC + TCA/I-O + botonera + SPI probe, usando el candidato
INT sólo mediante build override. El default del package permanece OFF hasta
cerrar esa regresión.
