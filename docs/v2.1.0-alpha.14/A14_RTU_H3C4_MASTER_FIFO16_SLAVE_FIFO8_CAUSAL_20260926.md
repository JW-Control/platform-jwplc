# Alpha14 — RTU-H3C.4 — Master FIFO16 / Slave FIFO8 causal

Fecha: 2026-09-26

## Motivo

H3C.3 corrigio el request-side con Slave RX FIFO=8:

```txt
REQUEST_PATH_GAP=0
REQUEST_BYTE_GAP=0
SLAVE_DISCARDED_TAILS=0
SLAVE_DISCARDED_BYTES=0
```

En 936681 transacciones quedo un unico timeout response-side:

```txt
RESPONSE_PATH_GAP=1
RESPONSE_BYTE_GAP=3
```

La respuesta FC03 qty=2 mide 9 bytes. Con Master FIFO=1 el driver puede exponer
la respuesta al software en fragmentos pequenos. H3C.4 mantiene Slave FIFO=8 y
cambia solamente Master FIFO 1->16.

Con FIFO16, la respuesta de 9 bytes no alcanza el umbral FIFO full. El driver
puede moverla al ring buffer mediante RX timeout una vez completado el frame,
evitando un umbral intermedio dentro de la respuesta.

## Perfil

```txt
BAUD=500000
CLOCK=APB_FORCED
FRAME_GAP_US=100
MASTER_RX_FIFO_FULL=16
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
A14_RTU_H3C4=PASS_MASTER_FIFO16_SLAVE_FIFO8_1200S
```

No se modifican defaults de producto.
