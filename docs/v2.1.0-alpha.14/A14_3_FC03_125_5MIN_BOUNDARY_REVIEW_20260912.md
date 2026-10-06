# Alpha14 — A14.3 FC03/125 frontera 5 min — REVIEW

Fecha: `2026-09-12`

## Objetivo

Confirmar durante 5 minutos por punto la frontera observada previamente en ventanas de 30 s para `FC03` con `125` registros.

La hipótesis previa era:

```text
1200 req/s -> STABLE_PASS
1225 req/s -> SATURATION_FAIL
```

El criterio de estabilidad sigue siendo:

```text
clean_case = true
achieved_req_s >= 95% de requested_req_s
unexpected_resets = 0
timeouts = 0
protocol_errors = 0
transport_errors = 0
spi_bus_lock_timeouts = 0
```

## Resultado físico

### 1200 req/s — 300 s

```text
REQUESTED_REQ_S=1200
ACHIEVED_REQ_S=1096.2
ACHIEVED_PCT=91.35
TOTAL_TCP_PAYLOAD_MBPS=2.377
USEFUL_REGISTER_DATA_MBPS=2.192
LATENCY_P95_US=1209.8
LATENCY_P99_US=1670.6
LATENCY_MAX_US=49212.8
LOOP_GAP_AVG_US=287
LOOP_GAP_MAX_US=4114
REQUESTS_OK=328866/360000
CONNECT_ATTEMPTS=1
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES
RESULT=SATURATION_FAIL
```

### 1225 req/s — 300 s

```text
REQUESTED_REQ_S=1225
ACHIEVED_REQ_S=1102.4
ACHIEVED_PCT=89.99
TOTAL_TCP_PAYLOAD_MBPS=2.390
USEFUL_REGISTER_DATA_MBPS=2.205
LATENCY_P95_US=1207.8
LATENCY_P99_US=1518.0
LATENCY_MAX_US=49456.1
LOOP_GAP_AVG_US=291
LOOP_GAP_MAX_US=4275
REQUESTS_OK=330709/367500
CONNECT_ATTEMPTS=1
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES
RESULT=SATURATION_FAIL
```

## Interpretación

La frontera de 30 s no se confirmó a 5 min. El punto de `1200 req/s`, que había alcanzado `97.81%` durante 30 s, cayó a `91.35%` durante 300 s.

No aparecieron errores de transporte, protocolo, SPI ni reset. Por tanto, el resultado no corresponde a una falla funcional del Server, sino a saturación de capacidad sostenida.

Los dos puntos solicitados por encima de la capacidad convergieron a un throughput parecido:

```text
1200 requested -> 1096.2 achieved req/s
1225 requested -> 1102.4 achieved req/s
```

Esto sugiere un techo sostenido de corto/medio plazo alrededor de `1.10 kreq/s` para `FC03/125`, equivalente aproximadamente a:

```text
TOTAL_TCP_PAYLOAD ≈ 2.38 - 2.39 Mbps
USEFUL_REGISTER_DATA ≈ 2.19 - 2.21 Mbps
```

Esta observación aún no fija `SERVER_MAX_STABLE_REQ_S`; hace falta acotar la frontera de 5 min por debajo de `1200 req/s`.

## Comparación con ventanas anteriores

```text
No-wait 10 s:
  1142.4 req/s
  2.477 Mbps total payload
  2.285 Mbps útiles

1200 req/s / 30 s:
  1173.8 req/s
  97.81%
  2.545 Mbps total payload
  STABLE_PASS según criterio de 30 s

1200 req/s / 300 s:
  1096.2 req/s
  91.35%
  2.377 Mbps total payload
  SATURATION_FAIL
```

La diferencia confirma que una ventana breve sobreestima la capacidad sostenible cerca de saturación. El resultado de 30 s se conserva como evidencia de pico temporal, pero no como tasa estable final.

## Clasificación

```text
A14_3_FC03_125_5MIN_BOUNDARY=REVIEW
RATE_1200_CLEAN=YES
RATE_1200_STABLE=NO
RATE_1225_CLEAN=YES
RATE_1225_SATURATED=YES
FC03_125_5MIN_BOUNDARY_CONFIRMED=NO
PERFORMANCE_FAILURE_TYPE=CAPACITY_SATURATION
FUNCTIONAL_ERROR=NO
SERVER_MAX_STABLE_REQ_S=NOT_DECIDED
```

## Siguiente gate

Acotar la frontera de 5 min por debajo de `1200 req/s`, usando puntos cercanos al techo sostenido observado. No se modifica código de producción a partir de este resultado.
