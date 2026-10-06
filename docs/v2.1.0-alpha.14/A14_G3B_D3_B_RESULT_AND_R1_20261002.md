# Alpha14 — G3B-D3-B resultado y R1 — 2026-10-02

## Ejecución

```text
HEAD=777d4d1b5ce7f79b5b1d7383309087d26f034a5e
D3B_STATUS=PASS_CHARACTERIZATION
D3B_FUNCTIONAL_MATRIX=PASS
PRODUCT_DEFAULT_CHANGED=NO
```

Todas las ventanas IDLE/100/500/750/1000 fueron funcionales y sin pérdida de
requests.

## Curva observada

| Carga | Estado D3 | P95 vs polling | P99 vs polling | status ratio | available ratio |
| --- | --- | ---: | ---: | ---: | ---: |
| 100 req/s | IDLE_INT | +64.687 % | +49.924 % | 1.301 % | 2.002 % |
| 500 req/s | WARM | +1.827 % | +3.712 % | 4.773 % | 9.111 % |
| 750 req/s | ACTIVE_POLL | +4.528 % | +5.239 % | 86.671 % | 87.777 % |
| 1000 req/s | ACTIVE_POLL | +4.881 % | +0.551 % | 81.677 % | 84.252 % |

IDLE mantuvo:

```text
status ratio=0.667 %
available ratio=0.667 %
state=IDLE_INT
functional=PASS
```

## Clasificación

El selector de carga sí cruza en la región prevista:

```text
100 -> IDLE_INT
500 -> WARM
750 -> ACTIVE_POLL
1000 -> ACTIVE_POLL
```

Por tanto la frontera 500..750 req/s queda confirmada también con D3.

Sin embargo D3-B R0 no habilita todavía H3E-R porque quedan dos costos
atribuibles al scheduler.

### 1. Alias del fallback a 100 req/s

A 100 req/s:

```text
complete_frames=1500
status_calls=2732
available_zero=1232
```

La diferencia:

```text
2732 - 1500 = 1232
```

coincide exactamente con los 1232 probes vacíos. El fallback de seguridad es
10 ms y la carga de 100 req/s también tiene período nominal de 10 ms. Después
de salir de WARM a IDLE_INT a los 5 ms, el scheduler conserva la fase del
último servicio y el fallback puede competir con la siguiente request.

### 2. Costo por pasada en ACTIVE_POLL

La primera implementación ejecuta `micros()` en cada llamada a
`shouldServiceRxInt()` para comprobar el idle-exit incluso en ACTIVE_POLL.

Eso no forma parte de la estimación de gap por trama completa y añade trabajo
a cada pasada de alta carga.

## D3-B-R1

R1 mantiene sin cambios:

```text
FAST_GAP_US=1600
FAST_STREAK=3
SLOW_GAP_US=1800
SLOW_STREAK=2
IDLE_EXIT_US=5000
INT_FALLBACK_MS=10
PRODUCT_DEFAULT_INT=0
PRODUCT_DEFAULT_HOT_POLL=0
PRODUCT_DEFAULT_LOAD_ADAPTIVE=0
```

Cambios exclusivos del candidato:

1. al entrar deliberadamente en IDLE_INT, reiniciar la fase del fallback desde
   ese instante;
2. usar el `nowMs` ya disponible para idle-exit;
3. reservar `micros()` para la medición de gaps en ADUs completas;
4. añadir contadores de benchmark de fallback y realineación.

No se cambia el intervalo global del fallback ni se relaja liveness.

## Siguiente gate

Repetir la misma matriz completa como:

```text
G3B-D3-B-R1
IDLE / 100 / 500 / 750 / 1000
POLLING vs D3
```

Sólo después de revisar R1 se decide H3E-R D3-C.
