# Alpha14 — RTU-H3B.1 — Repetibilidad Bulk RX + TCP500

Fecha: 2026-09-26

## Contexto

H3B mostró dos resultados importantes:

- BYTE + FIFO=1 superó 480 tx/s con TCP500 y runtime limpio.
- BULK alcanzó 670.637 tx/s con TCP500, pero registró 1 timeout.
- BULK alcanzó 983.969 tx/s con TCP OFF y runtime limpio.

El objetivo de H3B.1 no es aumentar rendimiento todavía. Primero debe
determinarse si el timeout de Bulk RX + TCP500 es reproducible y por qué lado
de la transacción aparece.

## Perfil congelado

```txt
BAUD=500000
CONFIG=8N1
CLOCK=APB_FORCED
FRAME_GAP_US=100
RX_FIFO_FULL=1
MOTOR=ASYNC
TX_MODE=QUEUED
TCP=500 req/s
TCP_FC03_QUANTITY=125
RTU_FC03_QUANTITY=2
RTU_TIMEOUT_MS=25
W5500_SPI_HZ=26000000
FULL_RUNTIME=ACTIVE
```

No se cambia timeout, baud, gap, FIFO ni política de TX para hacer pasar el
gate.

## Matriz

Por defecto:

1. BYTE_CONTROL — 60 s.
2. BULK_R1 — 60 s.
3. BULK_R2 — 60 s.
4. BULK_R3 — 60 s.
5. BULK_R4 — 60 s.
6. BULK_R5 — 60 s.

Las cinco corridas Bulk ocurren en el mismo firmware y sesión para detectar
fallos aleatorios o acumulativos.

## Telemetría qualification-only

El sketch Master añade:

```txt
RTU_TRANSACTION_MAX_US
RTU_TRANSACTIONS_OVER_5MS
RTU_TRANSACTIONS_OVER_10MS
RTU_TRANSACTIONS_OVER_20MS
RTU_LAST_FAILURE_DURATION_US
RTU_MAX_FAILURE_DURATION_US
RTU_LAST_FAILURE_RESULT
```

El runner también compara:

```txt
MASTER_TX
MASTER_RX
SLAVE_RX
SLAVE_TX
SLAVE_OK
REQUEST_PATH_GAP
RESPONSE_PATH_GAP
```

Interpretación:

- REQUEST_PATH_GAP > 0: alguna request iniciada por Master no llegó a ser
  procesada como frame por Slave.
- RESPONSE_PATH_GAP > 0: Slave emitió más respuestas de las que Master llegó a
  procesar como frame.
- ambos gaps en 0 con timeout: investigar scheduling/estado interno alrededor
  del timeout.

## Criterio de cierre

```txt
BYTE_CONTROL=CLEAN
BULK_CLEAN_RUNS=5/5
BULK_FAILED_TOTAL=0
BULK_TIMEOUTS_TOTAL=0
TCP >= 99% target en todas las ventanas
CRC=0
PERIPHERAL_FAILURE_COUNT=0
SD_DATALOG_FAILED_COMMITS=0
```

Si se cumple:

```txt
A14_RTU_H3B1=PASS_BULK_REPEATABILITY
```

Si reaparece un timeout, el gate termina como caracterización para analizar la
telemetría antes de modificar el producto.

## Después de H3B.1

- Si 5/5 Bulk quedan limpias: ejecutar long-run Bulk + TCP500.
- Si el timeout se reproduce: corregir primero su causa.
- Sólo después de estabilidad se continúa hacia throughput adicional, con la
  meta exploratoria posterior de acercarse a RTU800 + TCP500.
