# Alpha14 — RTU-H3C.1 — Diagnóstico temporal de tails

Fecha: 2026-09-26

## Contexto

H3C superó la meta de rendimiento:

```txt
RTU_HZ=818.740
TCP_REQ_S=500.000
```

Esto equivale aproximadamente a 102.34 actualizaciones/s por cada uno de 8
módulos, o 9.77 ms por actualización.

Sin embargo, H3C aún registró:

```txt
RTU_TIMEOUTS=3
REQUEST_PATH_GAP=3
SLAVE_DISCARDED_TAILS=6
SLAVE_DISCARDED_BYTES=24
REQUEST_BYTE_GAP=0
RESPONSE_BYTE_GAP=0
```

La ventana de recuperación estructural de 1750 us no fue suficiente para todos
los casos observados.

## Objetivo

No modificar aún la ventana de 1750 us.

Instrumentar exactamente:

```txt
RTU_SERVER_DISCARDED_LAST_LENGTH
RTU_SERVER_DISCARDED_LAST_AGE_US
RTU_SERVER_DISCARDED_MAX_AGE_US
RTU_SERVER_DISCARDED_LEN1
...
RTU_SERVER_DISCARDED_LEN8
RTU_SERVER_DISCARDED_LEN_GT8
```

para determinar:

1. qué longitud tienen realmente los tails descartados;
2. cuánto tiempo llevaba incompleto el tail cuando fue descartado;
3. si los fallos están apenas por encima de 1750 us o requieren una ventana
   considerablemente mayor.

## Perfil congelado

```txt
BAUD=500000
CLOCK=APB_FORCED
FRAME_GAP_US=100
RX_FIFO_FULL=1
RX_MODE=BULK
MASTER_SERVER_FRAMING=GAP
SLAVE_SERVER_FRAMING=STRUCTURAL
PARTIAL_HOLD_US=1750
RTU_TIMEOUT_MS=25
TCP=500 req/s
DURATION=600 s
BUCKET=60 s
FULL_RUNTIME=ACTIVE
W5500_SPI_HZ=26000000
```

## Clasificación

```txt
NO_TAIL_REPRODUCED
DISCARD_BEFORE_HOLD_LIMIT_UNEXPECTED
HOLD_1750_TOO_SHORT_CANDIDATE
LONG_FRAGMENT_GAP_REQUIRES_REVIEW
```

H3C.1 es diagnóstico: si la infraestructura y el perfil son válidos, devuelve
exit 0 aunque reproduzca timeouts, para conservar toda la evidencia.

No se cambia el default de producto.


## Resultado físico H3C.1

La corrida de 600 s no reprodujo tails ni timeouts:

```txt
RTU_HZ=760.059
RTU_STARTED=456177
RTU_SUCCESS=456177
RTU_FAILED=0
RTU_TIMEOUTS=0
REQUEST_PATH_GAP=0
RESPONSE_PATH_GAP=0
REQUEST_BYTE_GAP=0
RESPONSE_BYTE_GAP=0
SLAVE_DISCARDED_TAILS=0
TAIL_HISTOGRAM_ALL_ZERO=YES
MASTER_CRC=0
SLAVE_CRC=0
TCP_REQ_S=500.000
```

La salida mostró `MASTER_DISCARDED_BYTES=-1` y
`SLAVE_DISCARDED_BYTES=-1`. Esto fue un bug del sketch de qualification:
la clave `RTU_SERVER_DISCARDED_BYTES=` se imprimía sin valor y el valor real
quedaba al final de otra línea. El runner interpretó la clave ausente como -1 y
marcó falsamente `RTU_CLEAN=NO`.

En la implementación, `serverDiscardedBytes` sólo se incrementa en el mismo
bloque que `serverDiscardedTails++`. Como:

```txt
SLAVE_DISCARDED_TAILS=0
LEN1..LEN8=0
LEN_GT8=0
```

el valor real de `SLAVE_DISCARDED_BYTES` para esta corrida fue 0. Lo mismo
aplica al Master.

### Decisión

La corrida H3C.1 fue funcionalmente limpia, pero no cierra todavía la causa
intermitente porque no hubo cambios de comportamiento entre H3C y H3C.1:

- H3C: 3 timeouts / 491448 transacciones.
- H3C.1: 0 timeouts / 456177 transacciones.

Por tanto, el defecto observado sigue siendo raro y H3C.1 simplemente no lo
reprodujo.

Se corrige primero el snapshot de `DISCARDED_BYTES` y se repite H3C.1 durante
1200 s con el mismo `PARTIAL_HOLD_US=1750`, sin cambiar timing. El objetivo es
capturar la edad y longitud del próximo tail si reaparece.
