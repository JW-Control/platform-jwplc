# Alpha14 — G1 TCP SPI Waste Baseline — resultado 2026-10-01

## Cierre

```text
G1_STATUS=PASS
G1_PHYSICAL_RUN=COMPLETE
G1_PRODUCT_RUNTIME_POLICY_CHANGE=NO
G1_INT_IMPLEMENTED=NO
G1_INT_TCP_OPPORTUNITY_FOR_BUS_EFFICIENCY=HIGH
G1_INT_TCP_OPPORTUNITY_FOR_RAW_THROUGHPUT=LOW
G1_INT_TCP_FULL_RUNTIME_EFFECT=UNPROVEN
NEXT_GATE=G2_INT_GPIO15_TCP_AB
```

G1 caracterizó el uso SPI del TCP actual en tres regímenes sin activar INT ni
cambiar la política productiva del package.

## Integridad del gate

```text
HEAD=9b9882dc048a0e3fe3c161bf532ebe76aff49578
PROFILE_DEFAULT=OFF
NORMAL_PRODUCT_POLICY_CHANGED=NO
INT_IMPLEMENTED=NO
TRANSPORT_ERRORS=0
TCP_SPI_LOCK_ERRORS=0
```

BASE y PROFILE compilaron desde source canónico para Ethernet, Display y TFT.

## IDLE

15 s con conexión TCP establecida y cero payload.

| Métrica | BASE | PROFILE |
|---|---:|---:|
| RX | 0 Mbps | 0 Mbps |
| SPI occupancy | 66.811 % | 66.077 % |
| service empty | 100.000 % | 100.000 % |
| SPI hold avg | 69.276 us | 71.964 us |
| SPI hold max | 290 us | 701 us |

PROFILE observó:

```text
STATUS_CALLS=275544
AVAILABLE_CALLS=137772
AVAILABLE_ZERO=137772
AVAILABLE_NONZERO=0
AVAILABLE_ZERO_PCT=100.000
```

Interpretación: el scheduler polling conserva ~66 % de ocupación SPI incluso
sin recibir payload. Todo `available()` medido devolvió cero.

## CONTROLLED

30 s de RAW TCP con ingress de 12 B a 1000 Hz. Este caso modela la esparsidad
temporal de una solicitud FC03, pero no ejecuta Modbus TCP completo.

| Métrica | BASE | PROFILE |
|---|---:|---:|
| PC rate | 1000.000 Hz | 999.996 Hz |
| DUT RX | 0.095987 Mbps | 0.095987 Mbps |
| SPI occupancy | 68.504 % | 67.579 % |
| service empty | 88.668 % | 88.174 % |
| SPI hold avg | 77.855 us | 80.240 us |
| SPI hold max | 821 us | 948 us |

PROFILE observó:

```text
STATUS_CALLS=505398
AVAILABLE_CALLS=282620
AVAILABLE_ZERO=252699
AVAILABLE_NONZERO=29921
AVAILABLE_ZERO_PCT=89.413
PROFILE_VS_BASE_DUT_MBPS_PCT=0.000
```

Interpretación: a 1000 eventos útiles/s persiste una fracción muy alta de
polling sin RX útil. El profiling no alteró el throughput medido.

## SATURATED

30 s de RAW TCP continuo con writes PC de 4096 B.

| Métrica | BASE | PROFILE |
|---|---:|---:|
| DUT RX | 14.535333 Mbps | 14.516615 Mbps |
| SPI occupancy | 98.167 % | 98.121 % |
| service empty | 1.578 % | 6.475 % |
| SPI hold avg | 4344.816 us | 4135.278 us |
| SPI hold max | 5174 us | 5212 us |

PROFILE observó:

```text
AVAILABLE_CALLS=53698
AVAILABLE_ZERO=470
AVAILABLE_NONZERO=53228
AVAILABLE_ZERO_PCT=0.875
SPI_US_PER_RX_BYTE=0.540735269
PROFILE_VS_BASE_DUT_MBPS_PCT=-0.129
```

El resultado reproduce el orden de magnitud del RAW LR600 post-P4.2
(~14.494 Mbps, ~98.112 % de ocupación SPI). Bajo saturación casi todo el bus
corresponde a trabajo útil inevitable y queda poco polling vacío que eliminar.

## Relación con P8 SINGLE_STATUS

G1 se ejecutó con el profiler RAW histórico, cuyo flag de P8 permanece OFF por
default. Por eso se observaron dos status calls por service pass.

La decisión de producto de P8 sigue vigente:

```text
PERSISTENT_SOCKET_STATUS_CACHE=FORBIDDEN
SCHEDULER_SAME_PASS_REUSE_POLICY=ADOPT
```

G2 comparará POLLING vs INT con esa política same-pass habilitada en ambas
variantes, evitando atribuir a INT una ganancia causada por la redundancia P8.

## Relación con UDP P3G

UDP ya demostró que INTn GPIO15 puede reducir fuertemente service holds vacíos
sin pérdida material de throughput. G1 no extrapola ese resultado a TCP, pero
confirma que el TCP polling actual tiene una oportunidad medible especialmente
en IDLE y CONTROLLED.

## Decisión

```text
G1_BUS_EFFICIENCY_SIGNAL=STRONG
G1_RAW_CEILING_SIGNAL=INT_NOT_EXPECTED_TO_RAISE_CEILING_MATERIALLY
G1_PRODUCT_CHANGE=NONE
G1_CLOSED=YES
NEXT=G2_INT_GPIO15_TCP_AB
```
