# Alpha14 — H4A0.4-P8 — redundancia `socketStatus()/connected()`

Fecha: `2026-09-29`

## Resultado

```text
A14_H4A04P8_SOCKET_STATUS_AB=PASS_DATA_ONLY
H4A04P8_INTERPRETATION=SINGLE_STATUS_GAIN_CONFIRMED
PHYSICAL_STABILITY=PENDING_USER
HARNESS_FAILURE=NO
PRODUCT_FAILURE=NO_EVIDENCE
HARDWARE_FAILURE=NO_EVIDENCE
```

El scheduler consultaba `connected()` dentro de `acceptTcpClient()` y volvía a
consultarlo en `serviceTcpUnlocked()` durante la misma pasada. El candidato
reutilizó únicamente el resultado inmediato de `acceptTcpClient()`:

```text
BASELINE=DOUBLE_STATUS
CANDIDATE=SINGLE_STATUS
ONLY_VARIABLE=SECOND_CONNECTED_PROBE_SAME_PASS
STATUS_CACHE_SCOPE=CURRENT_SCHEDULER_PASS_ONLY
READ_PATTERN=READ_DIRECT_BOTH_VARIANTS
RX_COMMIT=IMMEDIATE_BOTH_VARIANTS
FIFO_REUSE=ON_BOTH_VARIANTS
SPI_HZ=26000000
RUNS_PER_VARIANT=3
```

No se añadió cache persistente ni se modificó la API Ethernet. La variante
queda controlada por `JWPLC_H4A04P8_REUSE_CONNECTED_RESULT`, OFF por defecto
en el profiler.

## Integridad y recuperación

```text
VERIFY_RX_BYTES=1716448
FNV_ACTUAL=3335309605
FNV_EXPECTED=3335309605
PAYLOAD_INTEGRITY=PASS
RECONNECT_CASE_1=PASS
RECONNECT_CASE_2=PASS
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
```

## A/B

| Métrica mediana | `DOUBLE_STATUS` | `SINGLE_STATUS` | Delta candidata |
|---|---:|---:|---:|
| payload efectivo | 16.821631 Mbps | 16.770394 Mbps | −0.305% |
| TCP end-to-end | 13.267113 Mbps | 14.129575 Mbps | +6.501% |
| scheduler perfilado | 0.569412 us/B | 0.543472 us/B | −4.556% |
| hold SPI | 0.575520 us/B | 0.548937 us/B | −4.619% |
| status calls por MB | 1230.652902 | 362.785559 | −70.521% |
| status time | 0.027212 us/B | 0.009679 us/B | −64.431% |
| payload spread | 0.151946% | 0.214147% | ambos PASS |

TCP end-to-end tuvo dispersión alta y permanece como dato secundario. La
decisión se apoya en la reducción directa del contador, el coste del scheduler
y el hold SPI, con payload repetible dentro de la banda sin efecto material.

## Evidencia

```text
HEAD=526690c2b61124c3e7fad1163edcd558861229d3
RESULT_ROOT=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p8_socket_status_sib9cl9k
SUMMARY_LOG=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p8_socket_status_sib9cl9k\SUMMARY.log
```

## Decisión

El patrón `SINGLE_STATUS` queda validado por datos para el scheduler del
profiler, pendiente de revisión física. No se convierte en cache persistente
ni cambia el camino legacy del producto. El siguiente gate medirá fairness y
hold máximo al variar el batch raw.

```text
FIFO_REUSE_DEFAULT=OFF
P8_REUSE_CONNECTED_RESULT_DEFAULT=OFF
PHYSICAL_STABILITY=PENDING_USER
NEXT=H4A0.4-P9_RX_BATCH_RAW_8_16_32
```
