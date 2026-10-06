# Alpha14 — G2 INT GPIO15 TCP A/B — resultado 2026-10-01

## Cierre

```text
HEAD=a416155128c2a4ae035306a91db3a6fee932a88e
G2_STATUS=PASS
G2_PRODUCT_POLICY_CHANGE=NO
G2_PRODUCT_PROMOTION=NOT_YET
G2_INT_RECONNECT_LIVENESS=PASS
G2_INT_TCP_BUS_EFFICIENCY_SIGNAL=STRONG
G2_INT_TCP_RAW_THROUGHPUT_GAIN=NEEDS_REPEATABILITY
NEXT=G2R1_INT_TCP_REPEATABILITY
```

El A/B comparó POLLING contra INT_GUIDED con `PROFILE_HOOKS=ON` y
`SINGLE_STATUS_SAME_PASS=ON` en ambas variantes. El pin INTn fue GPIO15 y la
ISR no ejecutó SPI.

## IDLE

| Métrica | POLLING | INT_GUIDED | Delta |
|---|---:|---:|---:|
| SPI occupancy | 62.963 % | 0.000 % | -62.963 pp |
| SPI hold count | 170341 | 0 | -100.000 % |
| status calls | 170341 | 0 | -100.000 % |
| available zero | 100.000 % | 0.000 % | polling eliminado |

INT ejecutó 806913 skips sin adquirir el shared SPI y cero wakes/ISR, como se
espera para una conexión estable sin payload.

## CONTROLLED

30 s, RAW TCP de 12 B a ~1000 Hz.

| Métrica | POLLING | INT_GUIDED | Delta |
|---|---:|---:|---:|
| PC rate | 999.991 Hz | 999.997 Hz | equivalente |
| DUT RX | 0.095987 Mbps | 0.095988 Mbps | +0.001 % |
| SPI occupancy | 65.883 % | 16.681 % | -49.202 pp |
| SPI hold count | 302605 | 29839 | -90.139 % |
| service empty | 90.150 % | 0.000 % | eliminado |
| status calls | 302605 | 29839 | -90.139 % |
| available zero | 91.014 % | 49.982 % | -41.032 pp |

El ~50 % de `available()==0` restante en INT_GUIDED no representa polling
continuo previo al evento: aparece principalmente como comprobación terminal
tras drenar el RX durante una wake activa.

Telemetría INT:

```text
ISR_COUNT=29831
WAKE_COUNT=29839
LOW_FALLBACK=0
RSR_REARM=9
PIN_REARM=7
```

No hubo pérdida de payload, errores de transporte ni locks SPI.

## Reconnect/liveness

El probe físico cerró la conexión medida, liberó el freeze, verificó retorno a
IDLE, deshabilitación de la máscara del socket anterior, reconexión,
reconfiguración de INT y nueva recepción.

```text
G2_RECONNECT_RX_BYTES=1024
G2_RECONNECT_PASS=YES
G2_INT_RECONNECT_LIVENESS=PASS
```

## SATURATED

30 s RAW TCP continuo.

| Métrica | POLLING | INT_GUIDED | Delta |
|---|---:|---:|---:|
| DUT RX | 12.043156 Mbps | 12.798952 Mbps | +6.276 % |
| SPI occupancy | 93.722 % | 89.021 % | -4.701 pp |
| SPI hold count | 46820 | 12033 | -74.299 % |
| service empty | 77.362 % | 6.432 % | fuerte reducción |
| status calls | 46820 | 12033 | -74.299 % |
| available zero | 44.011 % | 11.781 % | fuerte reducción |

El A/B muestra una señal favorable también bajo saturación, pero el incremento
de +6.276 % no se promueve todavía como ganancia de throughput: ambos valores
absolutos quedaron por debajo del G1 SATURATED (~14.5 Mbps), por lo que hace
falta una repetición balanceada antes de convertir esa observación en una
conclusión causal.

## Comparación con G1

G1 ya había mostrado:

- IDLE: ~66 % de ocupación SPI sin payload;
- CONTROLLED: ~68 % de ocupación y ~89 % de available-zero;
- SATURATED: ~98 % de ocupación y <1 % de available-zero en aquella sesión.

G2 confirma la oportunidad de INT en IDLE/CONTROLLED con una separación muy
grande y sin pérdida funcional. La variabilidad absoluta del caso SATURATED
entre sesiones obliga a usar A/B repetido y balanceado para cualquier claim de
Mbps.

## Comparación con UDP P3G

P3G UDP ya había mostrado que INT reduce fuertemente service holds vacíos sin
pérdida material de throughput. G2 extiende físicamente el mismo principio a
TCP y además valida el lifecycle de disconnect/reconnect.

No se copian semánticas UDP específicas de commit/rearm al TCP; el candidato
TCP conserva su propio tratamiento de stream/lifecycle.

## Decisión

```text
G2_IDLE_BUS_EFFICIENCY=PASS_STRONG
G2_CONTROLLED_BUS_EFFICIENCY=PASS_STRONG
G2_CONTROLLED_THROUGHPUT_NONREGRESSION=PASS
G2_TCP_RECONNECT_LIVENESS=PASS
G2_SATURATED_SINGLE_PAIR_SIGNAL=POSITIVE
G2_SATURATED_REPEATABILITY=REQUIRED
G2_INT_TCP_CANDIDATE=KEEP
G2_PRODUCT_PROMOTION=NOT_YET
NEXT=G2R1_INT_TCP_REPEATABILITY
```
