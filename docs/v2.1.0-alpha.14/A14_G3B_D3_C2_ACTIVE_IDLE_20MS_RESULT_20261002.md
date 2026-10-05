# Alpha14 — G3B-D3-C2 ACTIVE idle 20 ms — resultado 2026-10-02

## Clasificación

```text
HEAD=e814f96c028e4bc9f23631acce0d8efa0c177148
D3C2_STATUS=PASS_CHARACTERIZATION
D3C2_TCP=PASS
D3C2_RTU=PASS
D3C2_PERIPHERALS=PASS
D3C2_ACTIVE_IDLE_5MS_PROBLEM=CONFIRMED
D3C2_ACTIVE_IDLE_20MS_EFFECTIVE=YES
D3C2_ACTIVE_COOLDOWN_FLAPPING=REMAINS
PRODUCT_DEFAULT_CHANGED=NO
NEXT=G3B_D3_C3_SLOW_STREAK4_PROFILE
```

## TCP 60 s

```text
REQUESTS=59997/60000
ACHIEVED_REQ_S=999.949
AVG=944.0 us
P95=1415.6 us
P99=2727.5 us
MAX=8073.6 us
LOOP_AVG=1096 us
LOOP_MAX=8809 us
TCP_ERRORS=0
```

## Scheduler

```text
COMPLETE_FRAMES=59997
TO_WARM=1
TO_ACTIVE_POLL=46
TO_COOLDOWN=45
TO_IDLE_INT=1
ACTIVE_POLL_PASSES=103793
FALLBACK_PASSES=8
IDLE_FALLBACK_REALIGNS=1
ACTIVE_IDLE_EXITS=1
NONACTIVE_IDLE_EXITS=0
FINAL_STATE=IDLE_INT
```

## Comparación contra C1

| Métrica | C1 5 ms | C2 20 ms | cambio |
| --- | ---: | ---: | ---: |
| TO_WARM | 115 | 1 | -99.13 % |
| TO_ACTIVE_POLL | 144 | 46 | -68.06 % |
| TO_COOLDOWN | 29 | 45 | +55.17 % |
| TO_IDLE_INT | 115 | 1 | -99.13 % |
| FALLBACK | 10 | 8 | -20.00 % |

El cambio 5→20 ms elimina prácticamente el hard-idle flapping, pero deja visible el segundo mecanismo de salida: ACTIVE_POLL→COOLDOWN por `SLOW_GAP>=1800 us` durante dos frames consecutivos.

## Decisión

No cambiar FAST_GAP, FAST_STREAK ni SLOW_GAP. El gate C3 conserva ACTIVE idle 20 ms y cambia únicamente el streak lento de 2 a 4 mediante build override.
