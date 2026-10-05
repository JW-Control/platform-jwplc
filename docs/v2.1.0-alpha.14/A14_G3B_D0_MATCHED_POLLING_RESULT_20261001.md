# Alpha14 — G3B-D0 H3E-R matched POLLING control — resultado 2026-10-01

## Cierre

```text
HEAD=d16be42f0aa5077b7f410023c992e24cf9eba627
G3B_D0_RESULT=PASS
INT_GUIDED_RX_BUILD=OFF
PRODUCT_DEFAULT_INT=0
TCP_REQUESTS=120000/120000
TCP_REQ_S=1000.00
RTU_HZ=50.004
PRODUCT_FAILURE=NO_EVIDENCE
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO_EVIDENCE
CONCLUSION=INT_CURRENT_IMPLEMENTATION_CAUSES_FULL_RUNTIME_HEADROOM_REGRESSION
NEXT=G3B_D1_OPTIMIZE_INT_REARM
```

## Objetivo

Aislar la variable INT después de dos corridas full-runtime con el candidato
`JWPLC_MODBUS_TCP_INT_GUIDED_RX=1` que no alcanzaron el conteo exacto de
120000 requests.

Este control usa:

- el mismo HEAD;
- el mismo H3E-R;
- el mismo Master/Slave;
- los mismos periféricos;
- 120 s;
- FC03/125 @1000 req/s;
- RTU 50 Hz;
- sin override INT.

## Resultado TCP

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
AVG_US=884.8
P95_US=1269.2
P99_US=2272.5
MAX_US=7942.6
LOOP_AVG_US=633
LOOP_MAX_US=12101
```

Buckets:

| Bucket | req/s | OK | AVG us | P95 us | P99 us | MAX us |
|---|---:|---:|---:|---:|---:|---:|
| 0–60 s | 999.98 | 59999 | 883.2 | 1261.6 | 2219.6 | 7715.3 |
| 60–120 s | 1000.02 | 60001 | 886.5 | 1276.1 | 2298.9 | 7942.6 |

## Resultado RTU/periféricos

```text
RTU_STARTED=6001
RTU_SUCCESS=6001
RTU_FAILED=0
RTU_SKIPPED=0
RTU_CRC_ERRORS=0
RTU_TIMEOUTS=0
RTU_HZ=50.004

DATALOG_FAILED_COMMITS=0
FRAM_FAILS=0
RTC_UNAVAILABLE=0
RTC_STALE=0
IO_STALE=0
BUTTON_NOT_READY=0
SPI_PROBE_FAILS=0
PERIPHERAL_FAILURE_COUNT=0

MASTER_TFT_PHYSICAL=PASS
SLAVE_TFT_PHYSICAL=PASS
```

## Comparación matched contra G3B-R1 INT

| Métrica | POLLING D0 | INT G3B-R1 | Delta INT |
|---|---:|---:|---:|
| requests OK | 120000 | 119744 | -256 |
| req/s | 1000.00 | 997.86 | -0.214 % |
| AVG | 884.8 us | 977.5 us | +10.99 % |
| P95 | 1269.2 us | 1373.6 us | +8.23 % |
| P99 | 2272.5 us | 2659.8 us | +17.04 % |
| MAX | 7942.6 us | 8155.3 us | +2.68 % |
| loop avg | 633 us | 266 us | -57.98 % |
| loop max | 12101 us | 8613 us | -28.82 % |

La comparación matched-current-head descarta que la pérdida de rate observada
en G3B/G3B-R1 sea una degradación general del package o del harness actual.

## Diagnóstico

La política INT sí libera el loop, pero la implementación G3 actual añade
trabajo síncrono en cada wake RX:

1. lectura de `SnIR`;
2. clear del evento;
3. lectura estable de `Sn_RX_RSR`;
4. lectura del pin INT.

En un workload secuencial de 1 kHz ese costo reduce el margen request-response
hasta perder slots temporales, aunque no produzca errores funcionales.

## Decisión

```text
G3C_PROMOTION=BLOCKED
INT_MECHANISM=KEEP
INT_CURRENT_REARM=OPTIMIZE
POLLING_BASELINE=CLEAN
NEXT=G3B_D1_OPTIMIZE_INT_REARM
```

No se desactiva ni descarta el enfoque INT: G2/G2-R1/G3A demostraron ahorro
muy grande de polling. Se optimiza únicamente el rearmado antes de repetir
Modbus TCP real y después H3E-R.
