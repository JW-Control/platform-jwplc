# Alpha14 — A14.3 FC03/125 rebracket inferior a 5 min

Fecha: `2026-09-12`

## Objetivo

Acotar la frontera sostenible de `PERF-S1` para FC03 leyendo 125 registros después de que `1150 req/s` y `1175 req/s` fallaran limpiamente por saturación durante 5 minutos.

Se evaluaron dos puntos inferiores:

```text
1125 req/s x 300 s
1140 req/s x 300 s
```

Criterio estable:

```text
CLEAN_CASE=YES
ACHIEVED_REQ_S >= 95% de REQUESTED_REQ_S
```

## Precondición de conexión

El primer caso requirió cuatro intentos de establecimiento TCP antes de iniciar la ventana medida:

```text
RATE=1125
TCP_CONNECT_ATTEMPTS=4
```

El segundo caso conectó al primer intento:

```text
RATE=1140
TCP_CONNECT_ATTEMPTS=1
```

Los retries de preparación ocurrieron antes del reset de estadísticas y no forman parte de la ventana de throughput. La latencia de reaceptación TCP permanece como observación separada de revisión.

## Resultados

| Requested req/s | Achieved req/s | Achieved % | Total TCP payload Mbps | Useful data Mbps | p95 us | p99 us | max us | Resultado |
|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 1125 | 1118.8 | 99.45 | 2.426 | 2.238 | 1200.5 | 1508.6 | 20301.7 | STABLE_PASS |
| 1140 | 1140.0 | 100.00 | 2.472 | 2.280 | 1195.9 | 1309.1 | 25549.9 | STABLE_PASS |

Conteos:

```text
RATE=1125
REQUESTS_OK=335646/337500
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES

RATE=1140
REQUESTS_OK=342000/342000
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES
```

Servidor:

```text
RATE=1125
LOOP_GAP_AVG_US=304
LOOP_GAP_MAX_US=4013

RATE=1140
LOOP_GAP_AVG_US=322
LOOP_GAP_MAX_US=4081
```

## Frontera actual

El resultado previo conocido para `1150 req/s` fue:

```text
RATE_1150_5MIN=SATURATION_FAIL_CLEAN
ACHIEVED_REQ_S=1089.1
ACHIEVED_PCT=94.71
```

Con el nuevo resultado:

```text
RATE_1140_5MIN=STABLE_PASS
RATE_1150_5MIN=SATURATION_FAIL_CLEAN
BOUNDARY_BRACKET=1140_PASS_1150_FAIL
CANDIDATE_FINAL_STABLE_REQ_S=1140
```

La frontera sostenible de 5 minutos queda acotada a un intervalo de `10 req/s` entre el último PASS y el primer FAIL observado.

## Throughput candidato TCP-only

En `1140 req/s` durante 5 minutos:

```text
ACHIEVED_REQ_S=1140.0
TOTAL_TCP_PAYLOAD_MBPS=2.472
USEFUL_REGISTER_DATA_MBPS=2.280
P99_US=1309.1
```

Este punto todavía no se declara como `SERVER_MAX_STABLE_REQ_S` definitivo. Debe superar el soak TCP-only de 30 minutos establecido en el plan.

## Clasificación

```text
A14_3_FC03_125_5MIN_LOWER_REBRACKET=PASS_PHYSICAL
RATE_1125_STABLE=YES
RATE_1140_STABLE=YES
BOUNDARY_BRACKET=1140_PASS_1150_FAIL
CANDIDATE_FINAL_STABLE_REQ_S=1140
CANDIDATE_FINAL_STABLE_TCP_PAYLOAD_MBPS=2.472
CANDIDATE_FINAL_STABLE_USEFUL_MBPS=2.280
SERVER_MAX_STABLE_REQ_S=NOT_DECIDED_YET
SERVER_MAX_STABLE_TCP_MBPS=NOT_DECIDED_YET
A14_3_TCP_REACCEPT_LATENCY=REVIEW
```

## Siguiente paso

Ejecutar un soak TCP-only de 30 minutos en `FC03/125 @ 1140 req/s`.

Si el caso permanece limpio y cumple el 95% durante los 30 minutos, `1140 req/s` podrá cerrarse como máximo sostenible observado para este perfil TCP-only. Después se repetirá la referencia bajo carga integrada de periféricos (`FULL_RUNTIME_REALISTIC` / `FULL_RUNTIME_STRESS`).
