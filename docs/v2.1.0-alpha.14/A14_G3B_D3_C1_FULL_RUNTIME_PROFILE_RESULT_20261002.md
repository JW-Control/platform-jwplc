# Alpha14 — G3B-D3-C1 full-runtime profile — resultado 2026-10-02

## Clasificación

```text
HEAD=cac466c2423689613e5b35a5ee8204cb654c2e89
D3C1_STATUS=PASS_CHARACTERIZATION
D3C1_TCP=PASS
D3C1_RTU=PASS
D3C1_PERIPHERALS=PASS
D3C1_ACTIVE_STABLE=FALSE
D3C1_TRANSITION_FLAPPING=TRUE
PRODUCT_DEFAULT_CHANGED=NO
NEXT=G3B_D3_C2_ACTIVE_IDLE_20MS_PROFILE
```

## TCP 60 s

```text
REQUESTS=59998/60000
ACHIEVED_REQ_S=999.965
AVG=935.1 us
P95=1374.1 us
P99=2622.3 us
MAX=8065.8 us
LOOP_AVG=905 us
LOOP_MAX=8945 us
TCP_ERRORS=0
```

## Scheduler

```text
COMPLETE_FRAMES=59998
TO_WARM=115
TO_ACTIVE_POLL=144
TO_COOLDOWN=29
TO_IDLE_INT=115
ACTIVE_POLL_PASSES=125259
FALLBACK_PASSES=10
IDLE_FALLBACK_REALIGNS=115
FINAL_STATE=IDLE_INT
LAST_FRAME_GAP_US=785
```

En 60 s hubo 2.40 entradas/s a ACTIVE_POLL, 1.92 retornos/s a IDLE_INT y 0.48 entradas/s a COOLDOWN.

El source actual posee una única ruta runtime hacia IDLE_INT: el hard idle-exit de 5 ms en shouldServiceRxInt(). Por tanto, las 115 transiciones a IDLE_INT demuestran que ese timeout se disparó 115 veces durante la ventana.

## Comparación

Contra C0 POLLING matched:

```text
REQ_S=-0.004 %
AVG=+5.769 %
P95=+8.831 %
P99=+8.647 %
MAX=-10.101 %
LOOP_AVG=+53.130 %
```

Contra D2 fixed 1500:

```text
REQ_S=-0.004 %
AVG=-0.288 %
P95=+0.977 %
P99=-11.220 %
MAX=-53.059 %
LOOP_AVG=+2.958 %
```

D3-C1 reproduce throughput completo pero confirma flapping. Esto demuestra que el flapping existe, aunque una sola corrida no permite atribuirle por sí solo el SATURATION_FAIL de D3-C.

## Decisión

No modificar FAST_GAP, FAST_STREAK, SLOW_GAP ni SLOW_STREAK todavía.

El siguiente gate prueba una sola hipótesis: conservar el idle-exit de 5 ms para WARM/COOLDOWN, pero extender únicamente ACTIVE_POLL a 20 ms mediante build override. El default sigue siendo 5 ms.
