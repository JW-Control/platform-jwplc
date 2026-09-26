# Alpha14 — NB3-E3: cierre físico de regresión UDP RX y cierre NB3-E

## Objetivo

Confirmar que el hardening de `EthernetUDP::parsePacket()` no introduce una regresión práctica en el camino normal UDP RX.

## Baseline

```text
UDP_RX_BASELINE_DUT_MBPS=11.410917
```

## Corridas

| Run | PC offered Mbps | DUT Mbps | HOLD_MAX_US | LOOP_GAP_US* | Resultado |
|---|---:|---:|---:|---:|---|
| 1 | 926.125531 | 10.967359 | 187 | 57840 | PASS |
| 2 | 943.822785 | 11.140095 | 137 | 57765 | PASS |
| 3 | 951.449640 | 11.108036 | 165 | 57763 | PASS |

`*` El loop gap UDP RX permanece como evidencia-only porque el snapshot Serial de armado contamina la ventana de medición.

## Resumen

```text
UDP_RX_MEDIAN_PC_OFFERED_MBPS=943.822785
UDP_RX_MEDIAN_DUT_MBPS=11.108036
UDP_RX_MEDIAN_DUT_BASELINE_PCT=97.3
UDP_RX_MEDIAN_LOSS_PERCENT_EVIDENCE=98.819684
UDP_RX_MAX_HOLD_US=187
UDP_RX_MAX_LOOP_GAP_US_EVIDENCE=57840
```

La tasa PC es offered rate del host, no throughput alcanzado por el DUT. La pérdida bajo flood UDP se conserva como evidencia de saturación y no como hard gate.

En las tres corridas:

```text
TRANSPORT_ERRORS=0
UDP_BEGIN_PACKET_ERRORS=0
UDP_WRITE_ERRORS=0
UDP_END_PACKET_ERRORS=0
UDP_SPI_LOCK_ERRORS=0
TCP_SPI_LOCK_ERRORS=0
UDP_LAST_SHORT_WRITE_BYTES=0
```

## Cierre NB3-E

NB3-E1:

```text
source/API/compile=PASS
unbounded while(_remaining)=REMOVED
```

NB3-E2:

```text
partial packet 256 bytes
read 8 bytes
remaining 248 bytes
single drain=PASS
next packet recovery=PASS
drain hold=63 us
loop gap=202 us
```

NB3-E3:

```text
UDP_RX_RUNS=3_PASS
UDP_RX_MEDIAN_DUT_BASELINE_PCT=97.3
UDP_RX_TRANSPORT_ERRORS=ZERO
UDP_RX_SPI_LOCK_ERRORS=ZERO
```

## Decisión

```text
NB3_UDP_PARSE_PACKET_HARDENING=PHYSICAL_CLOSED_PASS
```

La optimización específica de throughput UDP RX queda fuera de NB3 y se retomará después del cierre de hardening.
