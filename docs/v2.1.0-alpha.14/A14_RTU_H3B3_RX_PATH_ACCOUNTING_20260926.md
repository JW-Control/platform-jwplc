# Alpha14 — RTU-H3B.3 — Diagnóstico RX-path

Fecha: 2026-09-26

## Motivo

H3B.2 sostuvo el rendimiento y TCP500 durante 600 s, pero registró:

```txt
RTU_STARTED=407868
RTU_SUCCESS=407866
RTU_FAILED=2
RTU_TIMEOUTS=2
MASTER_TX=407868
SLAVE_RX=407866
REQUEST_PATH_GAP=2
RESPONSE_PATH_GAP=0
MASTER_CRC=0
SLAVE_CRC=0
```

Por tanto, el siguiente paso no cambia timing ni timeout. Se instrumenta el camino RX
para distinguir si las dos requests:

1. no llegaron completas al UART del Slave, o
2. llegaron como bytes pero `pollServer()` las descartó antes de
   `processServerFrame()`.

## Perfil congelado

```txt
BAUD=500000
CLOCK=APB_FORCED
FRAME_GAP_US=100
RX_FIFO_FULL=1
RX_MODE=BULK
MOTOR=ASYNC
TX_MODE=QUEUED
RTU_TIMEOUT_MS=25
TCP=500 req/s
DURATION=600 s
BUCKET=60 s
FULL_RUNTIME=ACTIVE
W5500_SPI_HZ=26000000
```

## Nueva telemetría

Se agregan contadores aditivos a `JWPLCModbusRTUStats`:

```txt
rxBytes
txBytes
serverDiscardedTails
serverDiscardedBytes
```

La ruta BULK suma los bytes entregados por HardwareSerial al motor RTU. El
transporte TX suma los bytes aceptados por `writeTransport()`.

Cuando `pollServer()` llega al frame gap pero no puede clasificar el tail como
una trama válida/local, cuenta el tail y sus bytes antes de limpiar el buffer.

## Interpretación

```txt
MASTER_TX_BYTES
SLAVE_RX_BYTES
SLAVE_DISCARDED_TAILS
SLAVE_DISCARDED_BYTES
REQUEST_BYTE_GAP
REQUEST_PATH_GAP
```

- `SLAVE_DISCARDED_BYTES > 0`: evidencia de bytes recibidos pero descartados por
  framing/parser.
- `REQUEST_BYTE_GAP > 0` con descartes cero: candidato a pérdida antes del parser
  (UART/físico/turnaround).
- `REQUEST_PATH_GAP > 0` con bytes completos y sin descartes: revisar
  clasificación/accounting adicional.
- cero timeout: fallo raro no reproducido en esta ventana; no se considera causa
  cerrada.

También se valida contabilidad TX:

```txt
MASTER_TX_BYTES == MASTER_TX_FRAMES * 8
SLAVE_TX_BYTES == SLAVE_TX_FRAMES * 9
```

para este benchmark FC03 fijo.

## Resultado del gate

H3B.3 es un gate de captura diagnóstica. Si la infraestructura y el perfil son
válidos, el runner termina con exit 0 incluso si reaparecen timeouts, de modo que
el gate pueda imprimir y validar toda la telemetría y el estado final.

Resultados posibles:

```txt
RTUH3B3_DIAGNOSIS=NO_FAILURE_REPRODUCED
RTUH3B3_DIAGNOSIS=SERVER_PARSER_DISCARD_OBSERVED
RTUH3B3_DIAGNOSIS=PRE_PARSER_RX_BYTE_LOSS_CANDIDATE
RTUH3B3_DIAGNOSIS=FRAME_ACCOUNTING_GAP_WITH_FULL_RX_BYTES
RTUH3B3_DIAGNOSIS=TIMEOUT_WITHOUT_REQUEST_PATH_GAP
```

No se adopta ninguna corrección hasta observar esta clasificación.
