# Alpha14 — RTU-H3C.2 — Master blocking TX causal

Fecha: 2026-09-26

## Motivo

H3C.1 a 1200 s reprodujo dos timeouts en 929660 transacciones.

La contabilidad de bytes cerró exactamente entre Master TX y Slave RX, pero el
Slave descartó cuatro tails por un total de 16 bytes. El histograma fue:

```txt
LEN1=1
LEN3=1
LEN5=1
LEN7=1
TOTAL=16 bytes
```

El patrón es compatible con dos requests FC03 de 8 bytes fragmentadas en dos
partes.

Además:

```txt
MAX_DISCARDED_AGE_US=15760
RTU_SERVICE_GAP_MAX_US=19307
LOOP_GAP_MAX_US=19301
```

No se incrementa todavía el partial hold del parser.

## Hipótesis

JWPLC_RS485 usa un TX ring buffer de 512 bytes. La ruta queued retorna después de
que HardwareSerial acepta el frame, no después de que todos sus bytes abandonan
físicamente la UART.

H3C.2 prueba si la continuidad del request-side mejora al usar TX bloqueante sólo
en el Master.

## Perfil

```txt
BAUD=500000
CLOCK=APB_FORCED
FRAME_GAP_US=100
RX_FIFO_FULL=1
RX_MODE=BULK
MASTER_SERVER_FRAMING=GAP
SLAVE_SERVER_FRAMING=STRUCTURAL
PARTIAL_HOLD_US=1750
MASTER_TX=BLOCKING
SLAVE_TX=QUEUED
RTU_TIMEOUT_MS=25
TCP=500 req/s
DURATION=1200 s
BUCKET=60 s
FULL_RUNTIME=ACTIVE
W5500_SPI_HZ=26000000
```

Sólo cambia MASTER_TX.

## Criterio causal

Resultado favorable a la hipótesis TX queued:

```txt
RTU_FAILED=0
RTU_TIMEOUTS=0
REQUEST_PATH_GAP=0
REQUEST_BYTE_GAP=0
SLAVE_DISCARDED_TAILS=0
SLAVE_DISCARDED_BYTES=0
MASTER_CRC=0
SLAVE_CRC=0
TCP_TARGET_PCT >= 99
SD_CLEAN=YES
PERIPHERAL_FAILURE_COUNT=0
```

También se medirá el costo de throughput de bloquear sólo el request-side.

PASS esperado:

```txt
A14_RTU_H3C2=PASS_MASTER_BLOCKING_CAUSAL_1200S
```

Este gate no cambia defaults de producto.
