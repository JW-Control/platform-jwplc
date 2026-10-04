# Alpha14 — G3B-D3 load-adaptive INT/POLL scheduler — PLAN 2026-10-01

## Estado

```text
D3_PRODUCT_MUTATION=NO
D3_THRESHOLDS=UNRESOLVED
D3_IMPLEMENTATION=BLOCKED_BY_C0
PRECONDITION=G3B_D2_C0_RESULT
```

Este documento fija la arquitectura y los gates siguientes, pero
deliberadamente no fija umbrales ni modifica todavía el runtime del producto.

## Motivación

La evidencia acumulada muestra dos comportamientos distintos:

- INT puro reduce drásticamente polling/SPI en idle y baja carga, pero a
  1000 req/s perdió margen temporal en H3E-R.
- D2 con hot-poll fijo de 1500 us recuperó 120000/120000 y 1000 req/s, pero
  mostró una cola P99 mayor que los controles POLLING conocidos.
- En idle, D2 redujo ~99.3 % las consultas status/available.

Por tanto el objetivo D3 no es "usar INT siempre" ni "usar POLLING siempre".
El objetivo es cambiar de estrategia según la actividad real del socket.

## Arquitectura objetivo

Estados internos propuestos:

```text
IDLE_INT
  |
  | RECV / actividad
  v
WARM
  |
  | tráfico suficientemente frecuente
  v
ACTIVE_POLL
  |
  | tráfico se enfría
  v
COOLDOWN
  |
  | RX realmente vacío y estable
  v
IDLE_INT
```

### IDLE_INT

- RECV IRQ habilitada.
- ISR sólo marca pending.
- no SPI dentro de ISR.
- fallback de seguridad acotado.

### ACTIVE_POLL

- no se espera un nuevo IRQ para cada request;
- el scheduler consulta RX directamente en cada pasada cooperativa;
- RECV IRQ puede quedar enmascarada mientras ACTIVE_POLL esté vigente;
- DISCON/TIMEOUT permanecen observables;
- no se crea un while bloqueante: RTU, SD, TFT, FRAM, RTC, I/O y botones deben
  seguir recibiendo servicio.

### COOLDOWN

La salida de ACTIVE_POLL debe tener histéresis. No debe alternar
INT/POLL/INT/POLL por pequeñas variaciones de tráfico.

Antes de volver a IDLE_INT:

1. drenar el RX conocido;
2. confirmar RX vacío;
3. limpiar RECV pendiente;
4. reactivar RECV IRQ;
5. comprobar nuevamente estado/pin para cerrar la carrera de llegada;
6. sólo entonces dormir por INT.

## D3-M0 — caracterización de carga

Antes de elegir thresholds se medirá al menos:

```text
IDLE
100 req/s
500 req/s
1000 req/s
RAW TCP saturation
```

Para cada punto se deben separar:

- request rate;
- AVG/P95/P99/MAX;
- loop avg/max;
- status calls;
- available calls y available-zero;
- SPI occupancy/holds cuando el harness lo exponga;
- integridad funcional;
- periféricos relevantes.

Los thresholds de entrada/salida de ACTIVE_POLL se fijarán a partir de esta
evidencia. No se inventan en este documento.

## D3-A — implementación candidata

Sólo después de C0 y D3-M0:

- estado interno sin API pública nueva;
- histéresis explícita;
- budget cooperativo;
- RECV IRQ masked durante ACTIVE_POLL si la secuencia de liveness queda
  demostrada;
- DISCON/TIMEOUT preservados;
- fallback de seguridad preservado;
- default del package continúa sin promoción.

## D3-B — matriz funcional/performance

A/B contra POLLING en:

```text
IDLE
100 req/s
500 req/s
1000 req/s
RAW TCP
```

Criterio conceptual:

- IDLE/baja carga: ahorro fuerte de polling/SPI;
- alta carga: latencia/rate equivalentes a POLLING;
- transición de estados sin pérdida de requests ni reconnects;
- cero errores de protocolo/transporte;
- no degradar fairness de RTU/periféricos.

No se usará una sola métrica agregada para declarar ganador.

## D3-C — H3E-R full-runtime

Si D3-B pasa:

```text
TCP FC03/125 @1000 req/s
RTU 50 Hz
DataLog
Display/TFT
FRAM
RTC
TCA/I-O
botonera
SPI probe
120 s
```

Debe cerrar 120000/120000, RTU y periféricos limpios y tails dentro de guards.

## D3-D — promoción

Sólo si D3-B y D3-C pasan:

- promover política load-adaptive como default interno;
- mantener APIs públicas;
- post-promotion regression;
- luego continuar con la reconciliación RAW TCP ~14.5 Mbps antes de DIRECT_RX.

## Regla de decisión tras C0

```text
si C0 reproduce cola POLLING baja:
    D2_TAIL_COST=CONFIRMED
    continuar D3-M0

si C0 también muestra cola alta:
    revisar variabilidad antes de fijar thresholds D3
```
