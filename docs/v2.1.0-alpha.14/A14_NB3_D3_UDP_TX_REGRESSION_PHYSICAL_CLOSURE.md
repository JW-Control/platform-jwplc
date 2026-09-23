# Alpha14 — NB3-D3: cierre físico UDP TX y cierre de UDP SEND cooperative engine

## Objetivo

Confirmar que el backend UDP SEND cooperativo validado en NB3-D1/D2 no introduce regresión de throughput, integridad, SPI o loop timing en UDP TX raw.

## Baseline previo

```text
UDP_TX_BASELINE_PC_MBPS=5.174312
UDP_TX_BASELINE_DUT_MBPS=5.178235
```

## Corridas

| Run | PC Mbps | DUT Mbps | HOLD_MAX_US | LOOP_GAP_MAX_US | Secuencias | Resultado |
|---|---:|---:|---:|---:|---:|---|
| 1 | 5.138566 | 5.142489 | 197 | 3531 | 1311 | PASS |
| 2 | 5.129586 | 5.133511 | 205 | 3724 | 1308 | PASS |
| 3 | 5.140676 | 5.144600 | 128 | 3921 | 1311 | PASS |

En las tres corridas:

```text
TRANSPORT_ERRORS=0
UDP_BEGIN_PACKET_ERRORS=0
UDP_WRITE_ERRORS=0
UDP_END_PACKET_ERRORS=0
UDP_SPI_LOCK_ERRORS=0
TCP_SPI_LOCK_ERRORS=0
UDP_LAST_SHORT_WRITE_BYTES=0

UDP_TX_SEQUENCE_DECODE_ERRORS=0
UDP_TX_SEQUENCE_TOTAL_DUPLICATES=0
UDP_TX_SEQUENCE_REORDERS=0
UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING=0
UDP_TX_UNEXPECTED_SOURCE_PACKETS=0
UDP_TX_WRONG_SIZE_FROM_DUT=0
```

## Resumen

```text
UDP_TX_MEDIAN_PC_MBPS=5.138566
UDP_TX_MEDIAN_DUT_MBPS=5.142489
UDP_TX_MEDIAN_DUT_BASELINE_PCT=99.3
UDP_TX_MAX_HOLD_US=205
UDP_TX_MAX_LOOP_GAP_US=3921
VISUAL_SPI_EVENTS=0
```

La mediana DUT conserva 99.3% del throughput baseline.

## Cierre NB3-D

NB3-D1:

```text
source/API/compile=PASS
```

NB3-D2:

```text
DNS_BEGIN_HOLD_MAX_US: 6033 -> 1571 us
reduccion=74%
DNS_POLL_HOLD_MAX_US=373
LOOP_GAP_MAX_US=2363
SPI_LOCK_ERRORS=0
```

NB3-D3:

```text
UDP_TX_RUNS=3_PASS
UDP_TX_INTEGRITY=PASS
UDP_TX_TRANSPORT_ERRORS=ZERO
UDP_TX_SPI_LOCK_ERRORS=ZERO
UDP_TX_MEDIAN_GE_90PCT_BASELINE=PASS
```

## Decisión

```text
NB3_UDP_SEND_COOPERATIVE_ENGINE=PHYSICAL_CLOSED_PASS
```

El siguiente objetivo de NB3 es endurecer `EthernetUDP::parsePacket()` sin mezclar optimización de throughput UDP RX.
