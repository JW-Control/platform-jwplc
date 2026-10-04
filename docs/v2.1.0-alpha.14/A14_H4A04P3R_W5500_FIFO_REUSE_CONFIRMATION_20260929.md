# Alpha14 — H4A0.4-P3R — Confirmación FIFO reuse W5500

Fecha: `2026-09-29`

## Resultado

```text
A14_H4A04P3R_W5500_FIFO_REUSE_CONFIRMATION=PASS_DATA_ONLY
H4A04P3R_FIFO_REUSE_VALIDATED_PENDING_PHYSICAL=YES
H4A04P3_PHYSICAL_STABILITY=PENDING_USER
HARNESS_FAILURE=NO
PRODUCT_FAILURE=NO_EVIDENCE
HARDWARE_FAILURE=NO_EVIDENCE
```

P3R repitió el A/B con cinco corridas por variante. Conservó el package
canónico, SPI a 26 MHz y la misma única variable de P3:

```text
ONLY_VARIABLE=DUMMY_FIFO_REFILL_PER_64B_RX_CHUNK
RUNS_PER_VARIANT=5
PERF_DURATION_S=15.0
FIFO_REUSE_DEFAULT=OFF
```

## Integridad separada

```text
RX_BYTES=1673248
FNV_ACTUAL=2647524709
FNV_EXPECTED=2647524709
PAYLOAD_INTEGRITY=PASS
```

Las diez corridas funcionales completaron el ciclo de conexión, tráfico,
freeze y snapshot sin error de transporte, error de lock SPI ni reset
inesperado.

## Resultados

| Variante | TCP mediana | TCP spread | Payload efectivo | Payload spread | us/byte |
|---|---:|---:|---:|---:|---:|
| `DIRECT_RX` | 12.830344 Mbps | 3.506% | 15.965 Mbps | 0.229% | 0.501105 |
| `FIFO_REUSE` | 12.976148 Mbps | 10.189% | 16.785 Mbps | 0.307% | 0.476604 |

```text
FIFO_VS_DIRECT_PAYLOAD_PCT=+5.141
FIFO_VS_DIRECT_TCP_PCT=+1.136
FIFO_VS_DIRECT_US_PER_BYTE_PCT=-4.889
PAYLOAD_REPEATABILITY_OK=True
TCP_SPI_LOCK_ERRORS=0
TRANSPORT_ERRORS=0
UNEXPECTED_RESETS=0
```

El payload interno confirma P3 con margen amplio sobre el umbral de `+1%` y
spread menor a `0.5%` en ambas variantes. TCP vuelve a ser secundario y más
variable.

## Evidencia

```text
HEAD=bd38f7c5dc7d5767d064ebe67b7db22ad8774f08
RESULT_ROOT=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p3r_fifo_reuse_xp83g134
SUMMARY_LOG=C:\Users\jeykc\AppData\Local\Temp\jwplc_a14_h4a04p3r_fifo_reuse_xp83g134\SUMMARY.log
```

## Decisión

```text
FIFO_REUSE_VALIDATED_PENDING_PHYSICAL=YES
FIFO_REUSE_DEFAULT=OFF
NEXT=H4A0.4-P4_CHUNK_MICROPROFILE
PHYSICAL_STABILITY=PENDING_USER
```

No se promociona el candidato hasta la revisión física del usuario. P4 puede
usar `FIFO_REUSE` como mejor candidato medido para localizar el coste restante
del chunk de 64 B, con instrumentación compile-time OFF por defecto.
