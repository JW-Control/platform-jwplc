# Alpha14 — NB3-C: cierre físico de primitives acotadas

## Objetivo

Validar físicamente las primitives W5500 acotadas introducidas en NB3-B antes de modificar el SEND UDP.

## Configuración

```text
SPI_ETHERNET=26 MHz
DUT_IP=descubierta dinámicamente por Serial/DHCP
DURATION=3 s por modo
TCP_RX preferred hold=5000 us
hard hold=10000 us
hard loop gap=15000 us
```

## Resultado

```text
NB3_BOUNDED_PRIMITIVES_RAW_COMPILE=PASS
NB3_BOUNDED_PRIMITIVES_PHYSICAL_FOUR_MODES=PASS
NB3_TRANSPORT_ERRORS=ZERO
NB3_UDP_TX_INTEGRITY=PASS
NB3_VISUAL_SPI=PASS
NB3_HARD_HOLD_10MS=PASS
A14_NB3_BOUNDED_SOCKET_PRIMITIVES_PHYSICAL=PASS
```

## Métricas

| Modo | PC Mbps | DUT Mbps | HOLD_MAX_US | LOOP_GAP_MAX_US | Resultado |
|---|---:|---:|---:|---:|---|
| TCP_RX | 13.798245 | 13.798245 | 5238 | 7923 | PASS / hold REVIEW |
| TCP_TX | 4.900468 | 4.900468 | 2107 | 3331 | PASS |
| UDP_RX | 959.058722 ofrecido | 11.410917 | 187 | 57735* | PASS |
| UDP_TX | 5.174312 | 5.178235 | 149 | 4439 | PASS |

`*` El loop gap de UDP_RX no se usa como gate: el runner congelado toma un snapshot Serial de armado después de resetear contadores y antes del flood, contaminando esa métrica.

## UDP TX integrity

```text
SEQUENCE_DECODE_ERRORS=0
DUPLICATES=0
REORDERS=0
RANGE_MISSING=0
UNEXPECTED_SOURCE_PACKETS=0
WRONG_SIZE_FROM_DUT=0
SEQUENCE_TOTAL_COUNT=1320
```

## Interpretación

Las primitives acotadas no introdujeron regresiones funcionales observables.

El hold de TCP_RX superó levemente el objetivo preferido de 5 ms:

```text
5238 us
```

pero permaneció muy por debajo del techo duro de 10 ms. Este comportamiento ya era conocido en la exploración de chunks y se conserva como REVIEW de performance, no como fallo de NB3.

## Pendientes

1. NB3-D: UDP SEND cooperative engine.
2. NB3-E: hardening de `EthernetUDP::parsePacket()`.
3. NB3-F: bounds restantes del legacy TCP send.
4. Después del cierre NB3: análisis específico de performance UDP RX frente a TCP RX.

## Decisión

No optimizar UDP RX dentro de NB3-D. Se prioriza cerrar las rutas potencialmente bloqueantes antes de optimizar throughput.
