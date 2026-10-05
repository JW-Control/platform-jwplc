# Alpha14 — H4A0.4-P6 — `available()` + `read()` frente a `read()` directo

Fecha: `2026-09-29`

## Resultado

```text
A14_H4A04P6_TCP_AVAILABLE_READ_AB=PASS_DATA_ONLY
H4A04P6_INTERPRETATION=AVAILABLE_READ_SMALL_OR_INCONCLUSIVE_EFFECT
PHYSICAL_STABILITY=PENDING_USER
HARNESS_FAILURE=NO
PRODUCT_FAILURE=NO_EVIDENCE
HARDWARE_FAILURE=NO_EVIDENCE
```

El A/B cambió una sola variable del scheduler TCP RX:

```text
BASELINE=AVAILABLE_READ
CANDIDATE=READ_DIRECT
ONLY_VARIABLE=PRE_READ_AVAILABLE_PROBE
FIFO_REUSE=ON_BOTH_VARIANTS
SPI_HZ=26000000
RUNS_PER_VARIANT=3
```

`EthernetClient::read()` ya es no bloqueante: si el cache RX está vacío,
`socketRecv()` refresca `Sn_RX_RSR` dentro de su propia transacción. Por eso el
candidato pudo omitir la llamada previa a `available()` sin cambiar API.

## Integridad

```text
VERIFY_RX_BYTES=1700044
FNV_ACTUAL=366975385
FNV_EXPECTED=366975385
PAYLOAD_INTEGRITY=PASS
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
```

## A/B

| Métrica mediana | `AVAILABLE_READ` | `READ_DIRECT` | Delta candidata |
|---|---:|---:|---:|
| payload efectivo | 16.795211 Mbps | 16.853677 Mbps | +0.348% |
| TCP end-to-end | 13.620213 Mbps | 13.424499 Mbps | −1.437% |
| path RX | 0.544076 us/B | 0.539249 us/B | −0.887% |
| hold SPI | 0.568248 us/B | 0.569987 us/B | +0.306% |
| `available()` calls | 33,930 | 0 | eliminadas |
| payload spread | 0.187% | 0.132% | ambos PASS |
| TCP spread | 0.344% | 4.440% | candidata variable |

La sonda previa trasladó su trabajo al refresh interno de `socketRecv()`:

```text
AVAILABLE_READ_AVAILABLE_RSR_CALLS=31121
AVAILABLE_READ_AVAILABLE_RSR_TOTAL_US=798404
AVAILABLE_READ_RECV_RSR_CALLS=0
READ_DIRECT_AVAILABLE_RSR_CALLS=0
READ_DIRECT_RECV_RSR_CALLS=35369
READ_DIRECT_RECV_RSR_TOTAL_US=489623
```

Aunque el path RX bajó 0.887%, no alcanzó el umbral de 1%, el hold SPI no
mejoró y TCP fue más variable. La mejora de payload de 0.348% está dentro de
la banda sin efecto material.

## Evidencia

```text
HEAD=d13df201accd51288b8f4fd8ec40aa4447dabd8a
RESULT_ROOT=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p6_available_read_p7e0qvmt
SUMMARY_LOG=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p6_available_read_p7e0qvmt\SUMMARY.log
```

## Decisión

No se añade una API redundante ni se promociona `READ_DIRECT` como mejora de
velocidad. El resultado confirma que el siguiente coste separado, commit
`RX_RD + Sock_RECV`, merece el gate P7.

```text
FIFO_REUSE_DEFAULT=OFF
PHYSICAL_STABILITY=PENDING_USER
NEXT=H4A0.4-P7_TCP_RX_COMMIT_COALESCING
```
