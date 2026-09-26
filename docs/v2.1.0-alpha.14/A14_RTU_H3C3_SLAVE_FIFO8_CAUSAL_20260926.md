# Alpha14 — RTU-H3C.3 — Slave RX FIFO=8 causal

Fecha: 2026-09-26

## Motivo

H3C.2 cambio solamente el TX del Master a BLOCKING. No resolvio el defecto:

```txt
RTU_HZ=731.857
RTU_FAILED=10
RTU_TIMEOUTS=10
REQUEST_PATH_GAP=7
RESPONSE_PATH_GAP=3
SLAVE_DISCARDED_TAILS=14
SLAVE_DISCARDED_BYTES=56
RESPONSE_BYTE_GAP=10
```

Por tanto, Master BLOCKING se descarta como solucion.

El core documenta que `setRxFIFOFull(n)` controla cuantos bytes disparan la
interrupcion que copia RX FIFO al ring buffer y advierte que valores bajos
consumen mas CPU en ISR.

Con FIFO=1, una request FC03 de 8 bytes puede hacerse visible al software en
fragmentos pequenos. Bajo ventanas largas de servicio, el parser puede observar
un prefijo incompleto mucho antes que el resto llegue al ring buffer.

## Hipotesis

Cambiar solamente el RX FIFO del Slave:

```txt
MASTER_RX_FIFO_FULL=1
SLAVE_RX_FIFO_FULL=8
```

Como la request FC03 mide 8 bytes, el Slave deberia recibir la request completa
en una sola transferencia FIFO->ring buffer con mucha mayor frecuencia.

El Master conserva FIFO=1 para no alterar el response-side ni mezclar la
hipotesis.

## Perfil

```txt
BAUD=500000
CLOCK=APB_FORCED
FRAME_GAP_US=100
MASTER_RX_FIFO_FULL=1
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

Solo cambia el FIFO RX del Slave.

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
A14_RTU_H3C3=PASS_SLAVE_FIFO8_CAUSAL_1200S
```

No se modifican defaults de producto.
