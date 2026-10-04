# Alpha14 — G3B-D2 scheduler INT adaptativo — resultado 2026-10-01

## Cierre

```text
HEAD=b3f5867a7d20f2ab4e647eb87a78c042d68ef0b5
D2_STATUS=PASS
D2_READY_FOR_H3ER=YES
D2_HIGH_RATE_LATENCY_RECOVERY=PASS
D2_IDLE_BUS_EFFICIENCY=PASS
D2_PRODUCT_DEFAULT_CHANGED=NO
NEXT=G3B_D2_H3ER
```

## HIGH — FC03/125 @1000 req/s

| Métrica | POLLING | ADAPTIVE | Delta |
|---|---:|---:|---:|
| req/s mediana | 999.992 | 999.988 | ~0 % |
| AVG | 744.613 us | 735.672 us | -1.201 % |
| P95 | 1082.350 us | 1073.500 us | -0.818 % |
| P99 | 1148.153 us | 1146.902 us | -0.109 % |
| status calls | 171154 | 172880 | +1.0 % aprox. |
| available calls | 201154 | 202880 | +0.9 % aprox. |

Las cuatro corridas completaron 30000/30000 requests con
`G3A_FUNCTIONAL_PASS=YES`.

La política adaptativa recupera completamente la latencia de POLLING bajo
carga sostenida de 1 kHz. No se interpreta la pequeña mejora de AVG/P95/P99
como ganancia causal; el resultado importante es no-regresión.

## IDLE conectado — 15 s sin payload

| Métrica | POLLING | ADAPTIVE | Reducción |
|---|---:|---:|---:|
| status calls | 210045 | 1501 | 99.285 % |
| available calls | 210045 | 1501 | 99.285 % |
| available-zero | 210045 | 1501 | 99.285 % |
| loop avg | 214 us | 20 us | gran liberación |

Ambas variantes terminaron `D2_IDLE_FUNCTIONAL_PASS=YES`.

## Interpretación

D2 resuelve el tradeoff observado en G3/G3B-D1:

- con tráfico sostenido, la ventana hot de 1500 us mantiene el scheduler cerca
  del comportamiento POLLING y recupera latencia/headroom;
- al quedar el socket idle, la ventana expira y el runtime vuelve a INT-guided,
  eliminando >99 % de consultas status/available.

En HIGH no se busca ahorro de bus porque la estrategia deliberadamente se
mantiene hot mientras la carga de 1 kHz continúa. El ahorro aparece cuando el
tráfico se enfría.

## Estado del producto

El package conserva:

```text
JWPLC_MODBUS_TCP_INT_GUIDED_RX=0
JWPLC_MODBUS_TCP_INT_HOT_POLL_US=0
```

D2 fue probado mediante build override:

```text
JWPLC_MODBUS_TCP_INT_GUIDED_RX=1
JWPLC_MODBUS_TCP_INT_HOT_POLL_US=1500
```

No se promueve todavía.

## Próximo gate

Repetir H3E-R full-runtime de 120 s con ambos overrides:

```text
-DJWPLC_MODBUS_TCP_INT_GUIDED_RX=1
-DJWPLC_MODBUS_TCP_INT_HOT_POLL_US=1500
```

El objetivo es comprobar 120000/120000 @1000 req/s junto a RTU 50 Hz,
DataLog, Display/TFT, FRAM, RTC, TCA/I-O, botonera y SPI probe.

Sólo si ese full-runtime pasa se podrá abrir G3C de promoción a default.
