# Alpha14 — G3B-D3-M0A load characterization — resultado 2026-10-01

## Cierre

```text
HEAD_MEASURED=8826e4009d22aa14cedf5f9a4db90e0ff6b1158d
M0A_STATUS=PASS_CHARACTERIZATION
M0A_FUNCTIONAL_MATRIX=PASS
M0A_PRODUCT_DEFAULT_CHANGED=NO
NEXT=G3B_D3_M0B_RAW_EVIDENCE_REVIEW
```

Las diez corridas fueron funcionales. No hubo pérdida de requests en los puntos
activos.

## Curva de carga

| Carga | P95 POLLING | P95 ADAPTIVE | Delta | P99 POLLING | P99 ADAPTIVE | Delta | Status ratio | Available ratio |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 req/s | 857.605 us | 990.460 us | +15.491 % | 943.776 us | 1148.029 us | +21.642 % | 13.800 % | 14.457 % |
| 500 req/s | 948.505 us | 1089.115 us | +14.824 % | 1085.903 us | 1280.418 us | +17.913 % | 84.607 % | 85.357 % |
| 750 req/s | 1038.365 us | 1063.500 us | +2.421 % | 1223.802 us | 1134.957 us | -7.260 % | 102.818 % | 102.569 % |
| 1000 req/s | 1086.205 us | 1067.600 us | -1.713 % | 1190.107 us | 1193.306 us | +0.269 % | 100.889 % | 100.756 % |

Loop promedio:

```text
100 req/s:  POLLING 230 us -> ADAPTIVE 25 us
500 req/s:  POLLING 307 us -> ADAPTIVE 128 us
750 req/s:  POLLING 387 us -> ADAPTIVE 375 us
1000 req/s: POLLING 525 us -> ADAPTIVE 516 us
```

## IDLE

```text
POLLING_STATUS_CALLS=139932
ADAPTIVE_STATUS_CALLS=1001
STATUS_RATIO=0.715 %
STATUS_REDUCTION=99.285 %

POLLING_AVAILABLE_CALLS=139932
ADAPTIVE_AVAILABLE_CALLS=1001
AVAILABLE_RATIO=0.715 %
AVAILABLE_REDUCTION=99.285 %

LOOP_AVG_US=214 -> 20
```

## Interpretación

D2 de 1500 us ya actúa como un selector implícito por carga:

```text
100 req/s (periodo 10000 us): mayormente INT
500 req/s (periodo 2000 us): mezcla INT/poll
750 req/s (periodo 1333 us): prácticamente polling
1000 req/s (periodo 1000 us): polling sostenido
```

La transición real aparece entre 500 y 750 req/s, coherente con la ventana de
1500 us.

### Región baja

A 100 req/s se obtiene un ahorro de ~86 % en consultas status y ~85.5 % en
available, con costo de latencia aproximado:

```text
P95 +133 us
P99 +204 us
```

El periodo entre requests es 10 ms, por lo que existe gran margen temporal.

### Región media

A 500 req/s el ahorro baja a ~15 %, mientras el costo de P95/P99 todavía ronda
+15/+18 %. Ésta es la región menos atractiva del D2 fijo.

### Región alta

A 750 y 1000 req/s ADAPTIVE converge a POLLING:

```text
status/available ratio ~= 100 %
latencia ~= polling
loop ~= polling
```

El objetivo D3 debe evitar flapping en esta zona y entrar a ACTIVE_POLL de forma
estable cuando la carga sea sostenida.

## Conclusión de diseño

La arquitectura D3 queda reforzada:

```text
IDLE / carga baja -> INT
carga sostenida alta -> ACTIVE_POLL
banda media -> WARM/COOLDOWN con histéresis
```

No se fija todavía el threshold final en este gate. La evidencia sí acota la
frontera útil entre 500 y 750 req/s.

Antes de implementar D3-A se revisa la evidencia RAW TCP existente (M0B) para
confirmar si hace falta o no repetir un gate físico de saturación.
