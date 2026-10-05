# Alpha14 — G3B-R1 H3E-R INT full-runtime — 2026-10-01

## Clasificación

```text
HEAD=3f0fec0f05822f7ef67db1a995b8fdac1856d02b
G3B_R1_PHYSICAL_RUN=COMPLETE
G3B_R1_RESULT=FAIL_RATE_HEADROOM
PRODUCT_FUNCTIONAL_FAILURE=NO_EVIDENCE
INT_FUNCTIONAL_FAILURE=SUSPECTED_PERFORMANCE_REGRESSION
HARDWARE_FAILURE=NO_EVIDENCE
HARNESS_FAILURE=NO_EVIDENCE
PRODUCT_DEFAULT_CHANGED=NO
NEXT=G3B_D0_MATCHED_POLLING_FULL_RUNTIME
```

La repetición no reprodujo el outlier MAX de 203 ms del intento 1, pero sí
reprodujo y amplificó la incapacidad de completar el target temporal exacto de
120000 solicitudes en 120 s.

## Intento 1 vs repetición

| Métrica | G3B intento 1 | G3B-R1 |
|---|---:|---:|
| requests OK | 119979/120000 | 119744/120000 |
| achieved req/s | 999.79 | 997.86 |
| achieved % | 99.979 % | 99.786 % |
| AVG | 975.5 us | 977.5 us |
| P95 | 1373.1 us | 1373.6 us |
| P99 | 2459.3 us | 2659.8 us |
| MAX | 203297.4 us | 8155.3 us |
| loop avg | 268 us | 266 us |
| loop max | 10019 us | 8613 us |
| TCP errors | 0 | 0 |
| RTU | 50.002 Hz | 50.004 Hz |

El segundo intento elimina la hipótesis de que los 21 slots perdidos del
intento 1 fueran explicados únicamente por el outlier de 203 ms.

## Buckets G3B-R1

```text
0..60 s:
REQ_S=999.75
OK=59985
AVG=966.2 us
P95=1362.1 us
P99=2788.3 us
MAX=7565.5 us

60..120 s:
REQ_S=995.98
OK=59759
AVG=988.8 us
P95=1385.6 us
P99=2501.5 us
MAX=8155.3 us
```

El segundo bucket muestra pérdida sostenida de headroom aun sin un outlier
extremo.

## Integridad del runtime

```text
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
TCP_CLEAN=YES

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

El candidato no presenta fallo funcional; el problema observado es de margen de
rendimiento end-to-end a 1000 req/s dentro del runtime completo.

## Comparación con H3E-R POST-P4.2

Referencia anterior sin candidato INT:

```text
REQ_S=1000.00
AVG=880.3 us
P95=1253.2 us
P99=2273.1 us
MAX=8831.5 us
LOOP_AVG=633 us
LOOP_MAX=12012 us
```

G3B-R1 con INT:

```text
REQ_S=997.86
AVG=977.5 us
P95=1373.6 us
P99=2659.8 us
MAX=8155.3 us
LOOP_AVG=266 us
LOOP_MAX=8613 us
```

INT libera claramente tiempo del loop, pero incrementa la latencia
request-response suficiente para dejar el target de 1 kHz con muy poco margen.

## Hipótesis

La política INT actual ejecuta trabajo extra de rearmado dentro del camino de
cada request:

- lectura `SnIR`;
- posible escritura para limpiar IRQ;
- lectura estable `Sn_RX_RSR`;
- chequeo del pin INT.

Esto puede explicar parte del incremento de latencia, pero no se modifica aún
el producto.

Antes de tocar la implementación se necesita una baseline matched-current-head.

## Próximo gate: G3B-D0

Ejecutar el mismo H3E-R, mismo HEAD, mismo hardware, misma duración y mismos
periféricos, pero con:

```text
JWPLC_MODBUS_TCP_INT_GUIDED_RX=0
```

No se modifica source ni harness.

Interpretación:

- si POLLING vuelve a 120000/120000 y colas cercanas al baseline histórico,
  la regresión queda atribuida al candidato INT;
- si POLLING también cae por debajo de 1000 req/s, se investiga
  ambiente/harness/current-package antes de modificar INT.

No se abre G3C hasta cerrar esta comparación.
