# Alpha14 — G3B-D3-C3 SLOW_STREAK=4 — resultado 2026-10-02

## Clasificación

```text
HEAD=95e566f5aed308880361efad9df186f743323124
D3C3_TCP=SATURATION_FAIL_CLEAN
D3C3_RTU=PASS
D3C3_PERIPHERALS=PASS
D3C3_SLOW_STREAK4=REJECT_PERFORMANCE
PRODUCT_DEFAULT_CHANGED=NO
```

## Resultado

```text
REQUESTS=51228/60000
REQ_S=853.791
AVG=1166.2 us
P95=1898.3 us
P99=4131.1 us
MAX=15540.9 us
LOOP_AVG=712 us
LOOP_MAX=8299 us
TCP_ERRORS=0
RTU=3001/3001 @ 50.010 Hz
```

## Scheduler

```text
TO_WARM=5
TO_ACTIVE_POLL=11
TO_COOLDOWN=10
TO_IDLE_INT=5
ACTIVE_POLL_PASSES=161033
ACTIVE_IDLE_EXITS=1
NONACTIVE_IDLE_EXITS=4
```

Frente a C2, `TO_COOLDOWN` cayó 45→10 (-77.8 %), pero el throughput cayó ~14.6 % y P95/P99 empeoraron materialmente. No continuar elevando streaks ni ajustando thresholds D3.

## Decisión

Cerrar la ruta de tuning D3. El siguiente y último candidato arquitectónico INT para v2 será E1: INT sólo como wake-up y drenaje determinado por `Sn_RX_RSR`, sin D2/D3.
