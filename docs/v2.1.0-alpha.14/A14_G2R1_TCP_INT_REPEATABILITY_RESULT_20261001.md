# Alpha14 — G2-R1 INT TCP repeatability — resultado 2026-10-01

## Cierre

```text
HEAD=ed818714d7600f843739736464b75ffdcfcdd478
G2R1_STATUS=PASS
G2R1_PRODUCTIZATION_DESIGN_READY=YES
G2R1_PRODUCT_POLICY_CHANGE=NO
G2R1_INT_RECONNECT_LIVENESS=PASS
NEXT=G3_INT_PRODUCTIZATION_DESIGN
```

G2-R1 repitió el A/B TCP POLLING vs INT_GUIDED con tres corridas por variante
en CONTROLLED y SATURATED, orden balanceado y fresh upload por caso.

Ambas variantes conservaron:

```text
PROFILE_HOOKS=ON
SINGLE_STATUS_SAME_PASS=ON
W5500_SPI_HZ=26000000
FIFO_REUSE=ON
DLEN_REUSE=ON
COPY_OUT_64=ON
DIRECT_RX=OFF
RX_COMMIT=IMMEDIATE
```

## CONTROLLED — repetibilidad

20 s por corrida, RAW TCP de 12 B a ~1000 Hz.

| Métrica mediana | POLLING | INT_GUIDED | Delta |
|---|---:|---:|---:|
| DUT RX | 0.095980 Mbps | 0.095979 Mbps | -0.001 % |
| PC rate | 999.978 Hz | 999.970 Hz | equivalente |
| SPI occupancy | 65.886 % | 16.555 % | -49.331 pp |
| hold count | 201857 | 19771 | -90.205 % |
| status calls | 201857 | 19771 | -90.205 % |
| service empty | 90.213 % | 0.000 % | eliminado |
| available zero | 91.065 % | 49.986 % | -41.079 pp |

Dispersión de throughput:

```text
POLLING_SPREAD=0.003 %
INT_GUIDED_SPREAD=0.002 %
```

Resultado:

```text
CONTROLLED_BUS_SEPARATION=PASS
CONTROLLED_THROUGHPUT_NONREGRESSION=PASS
```

La reducción de ocupación/holds es grande, estable y no cambia la tasa útil.

## SATURATED — repetibilidad

30 s por corrida.

| Métrica mediana | POLLING | INT_GUIDED | Delta |
|---|---:|---:|---:|
| DUT RX | 11.711949 Mbps | 11.886347 Mbps | +1.489 % |
| SPI occupancy | 92.960 % | 82.615 % | -10.345 pp |
| hold count | 54401 | 10665 | -80.396 % |
| status calls | 54401 | 10665 | -80.396 % |
| service empty | 81.158 % | 7.019 % | fuerte reducción |
| available zero | 48.359 % | 10.340 % | fuerte reducción |

Dispersión de throughput:

```text
POLLING_SPREAD=7.136 %
INT_GUIDED_SPREAD=6.816 %
```

La mediana INT fue +1.489 %, por debajo del umbral de +2 % definido previamente
para declarar una mejora causal de throughput.

Resultado:

```text
SATURATED_THROUGHPUT_NONREGRESSION=PASS
SATURATED_THROUGHPUT_GAIN=NOT_CONFIRMED
```

Por tanto, INT se conserva por eficiencia de bus y no se promociona usando un
claim de mayor ceiling RAW.

## Liveness

La primera corrida CONTROLLED INT repitió el probe de cierre/reconexión:

```text
G2_RECONNECT_RX_BYTES=1024
G2_RECONNECT_PASS=YES
G2R1_INT_RECONNECT_LIVENESS=PASS
```

No hubo errores de transporte ni timeouts del mutex SPI en las doce corridas.

## Decisión técnica

La evidencia acumulada G1 + G2 + G2-R1 soporta:

```text
INT_TCP_BUS_EFFICIENCY=CONFIRMED
INT_TCP_CONTROLLED_NONREGRESSION=CONFIRMED
INT_TCP_SATURATED_NONREGRESSION=CONFIRMED
INT_TCP_RAW_THROUGHPUT_GAIN=NOT_CLAIMED
INT_TCP_RECONNECT_LIVENESS=CONFIRMED
```

El siguiente paso no debe activar INT globalmente sobre `EthernetClient`.
La política debe integrarse en un scheduler JWPLC que conoce el lifecycle del
socket, dejando intactas las APIs Arduino legacy.

G3 preparará un candidato interno para el servidor `JWPLC_ModbusTCP`, detrás
de flag OFF por defecto. Primero se validará con Modbus TCP real FC03/125
@1000 req/s y, sólo si pasa, con H3E-R full-runtime antes de promover el default.
