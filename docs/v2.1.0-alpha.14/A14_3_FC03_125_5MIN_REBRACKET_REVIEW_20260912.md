# Alpha14 — A14.3 FC03/125 rebracket de 5 min — REVIEW

Fecha: `2026-09-12`

## Objetivo

Reubicar la frontera sostenible de `PERF-S1` para FC03 de 125 registros después de que los puntos de 1200 y 1225 req/s fallaran por saturación limpia en la ventana de 5 minutos.

Se probaron:

```text
1150 req/s x 300 s
1175 req/s x 300 s
```

Criterio formal:

```text
STABLE = CLEAN && ACHIEVED_REQ_S >= 95% de REQUESTED_REQ_S
```

Por lo tanto:

```text
1150 req/s -> mínimo PASS = 1092.5 req/s
1175 req/s -> mínimo PASS = 1116.25 req/s
```

## Resultado físico

### 1150 req/s

```text
REQUESTED_REQ_S=1150
ACHIEVED_REQ_S=1089.1
ACHIEVED_PCT=94.71
TOTAL_TCP_PAYLOAD_MBPS=2.361
USEFUL_REGISTER_DATA_MBPS=2.178
P95_US=1207.6
P99_US=1696.1
MAX_US=44883.7
LOOP_AVG_US=282
LOOP_MAX_US=4187
REQUESTS_OK=326734/345000
CONNECT_ATTEMPTS=4
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES
RESULT=SATURATION_FAIL
```

### 1175 req/s

```text
REQUESTED_REQ_S=1175
ACHIEVED_REQ_S=1108.7
ACHIEVED_PCT=94.36
TOTAL_TCP_PAYLOAD_MBPS=2.404
USEFUL_REGISTER_DATA_MBPS=2.217
P95_US=1205.9
P99_US=1538.5
MAX_US=53998.5
LOOP_AVG_US=296
LOOP_MAX_US=4097
REQUESTS_OK=332614/352500
CONNECT_ATTEMPTS=1
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES
RESULT=SATURATION_FAIL
```

## Clasificación

Ambos puntos son fallos de capacidad, no fallos funcionales.

```text
RATE_1150_5MIN=SATURATION_FAIL_CLEAN
RATE_1175_5MIN=SATURATION_FAIL_CLEAN
FUNCTIONAL_ERROR=NO
CAPACITY_SATURATION=YES
A14_3_FC03_125_5MIN_REBRACKET=REVIEW
```

El punto de 1150 req/s quedó muy cerca del umbral formal: `94.71%`, sólo `0.29 puntos porcentuales` por debajo del criterio de 95%.

No se debe redondear este resultado a PASS.

## Observación sobre capacidad sostenida

Los ensayos largos realizados hasta este punto muestran que la capacidad efectiva de FC03/125 bajo saturación sostenida se mueve aproximadamente en la zona de `1.09–1.11 kreq/s`, con payload total de aplicación alrededor de `2.36–2.40 Mbps`.

Esto sigue siendo una observación de capacidad, no el valor final de `SERVER_MAX_STABLE_REQ_S`, porque falta validar un punto solicitado que cumpla el 95% durante la ventana larga y posteriormente superar el soak de 30 minutos.

## Observación sobre reapertura TCP

En el primer caso de este rebracket se necesitaron cuatro intentos para establecer la conexión TCP antes de comenzar la ventana medida:

```text
TCP_CONNECT_ATTEMPT_1=TIMEOUT
TCP_CONNECT_ATTEMPT_2=TIMEOUT
TCP_CONNECT_ATTEMPT_3=TIMEOUT
TCP_CONNECT_ATTEMPT_4=PASS
```

La preparación ocurre antes del reset de estadísticas y no contamina el throughput medido. Sin embargo, el patrón ya se ha observado en más de una secuencia de benchmark y debe conservarse como observación independiente de latencia de reapertura/listener:

```text
A14_3_TCP_REACCEPT_LATENCY=REVIEW
THROUGHPUT_WINDOW_AFFECTED=NO
```

No se modifica producción por este dato durante el benchmark; se revisará con un gate específico si persiste tras cerrar la caracterización de throughput.

## Siguiente gate

Para evitar otra iteración demasiado amplia y acercarnos a la frontera de forma eficiente, probar dos puntos durante 5 minutos:

```text
1125 req/s
1140 req/s
```

Interpretación prevista:

- si `1140` pasa, será el candidato superior para soak de 30 min;
- si `1125` pasa y `1140` falla, `1125` será el candidato conservador para soak;
- si ambos fallan, se reubica nuevamente la frontera por debajo de 1125.

```text
NEXT_GATE=A14.3_FC03_125_5MIN_REBRACKET_1125_1140
```
