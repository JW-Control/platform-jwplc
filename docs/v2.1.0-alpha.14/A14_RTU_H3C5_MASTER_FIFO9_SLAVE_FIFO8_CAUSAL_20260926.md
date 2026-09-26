# Alpha14 — RTU-H3C.5 — Master FIFO9 / Slave FIFO8 causal

Fecha: 2026-09-26

## Motivo

H3C.4 mantuvo request-side limpio con Slave FIFO8, pero quedo un timeout
response-side:

```txt
REQUEST_PATH_GAP=0
REQUEST_BYTE_GAP=0
SLAVE_DISCARDED_TAILS=0
SLAVE_DISCARDED_BYTES=0

RESPONSE_PATH_GAP=1
RESPONSE_BYTE_GAP=9
```

La respuesta FC03 qty=2 mide exactamente 9 bytes.

Con Master FIFO16, la respuesta nunca alcanza el threshold FIFO full y depende
del RX timeout del UART para pasar completa al ring buffer. H3C.4 perdio una
respuesta completa de 9 bytes.

## Hipotesis

Alinear el threshold del Master con el tamaño real de la respuesta:

```txt
MASTER_RX_FIFO_FULL=9
SLAVE_RX_FIFO_FULL=8
```

La request de 8 bytes dispara FIFO full en el Slave al completar el frame y la
respuesta de 9 bytes hace lo mismo en el Master.

No se modifica ningun otro parametro.

## Perfil

```txt
BAUD=500000
CLOCK=APB_FORCED
FRAME_GAP_US=100
MASTER_RX_FIFO_FULL=9
SLAVE_RX_FIFO_FULL=8
RX_MODE=BULK
MASTER_SERVER_FRAMING=GAP
SLAVE_SERVER_FRAMING=STRUCTURAL
PARTIAL_HOLD_US=1750
MASTER_TX=QUEUED
SLAVE_TX=QUEUED
RTU_TIMEOUT_MS=25
TCP=500 req/s
DURATION=1200 s
BUCKET=60 s
FULL_RUNTIME=ACTIVE
W5500_SPI_HZ=26000000
```

## Criterio

```txt
RTU_HZ >= 650
RTU_FAILED=0
RTU_TIMEOUTS=0
REQUEST_PATH_GAP=0
RESPONSE_PATH_GAP=0
REQUEST_BYTE_GAP=0
RESPONSE_BYTE_GAP=0
SLAVE_DISCARDED_TAILS=0
SLAVE_DISCARDED_BYTES=0
MASTER_CRC=0
SLAVE_CRC=0
TCP_TARGET_PCT >= 99
cada bucket TCP >= 495 req/s
SD_CLEAN=YES
PERIPHERAL_FAILURE_COUNT=0
```

PASS esperado:

```txt
A14_RTU_H3C5=PASS_MASTER_FIFO9_SLAVE_FIFO8_1200S
```

No se modifican defaults de producto.
