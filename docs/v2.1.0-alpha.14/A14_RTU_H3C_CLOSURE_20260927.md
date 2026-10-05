# Alpha14 — Cierre RTU-H3C — Framing estructural y RX FIFO

Fecha: 2026-09-27

## Problema

Con Bulk RX + framing estructural y FIFO1 se observaron timeouts raros bajo Full Runtime,
sin pérdida física total de bytes ni CRC. La instrumentación H3C.1 mostró requests FC03
fragmentadas en tails parciales y descartadas por software.

## Evidencia causal

H3C.2 descartó que Master TX=QUEUED fuera la causa principal: usar BLOCKING no resolvió
la inestabilidad y añadió degradación response-side.

H3C.3 cambió sólo Slave RX FIFO 1 -> 8 y eliminó los tails request-side.

H3C.4 probó Master FIFO16 / Slave FIFO8. Request-side siguió limpio, pero apareció un
timeout response-side con pérdida software equivalente a una respuesta FC03 completa de 9 B.

H3C.5 alineó Master FIFO9 / Slave FIFO8 y pasó 1200 s:

```txt
RTU_HZ=785.147
RTU_STARTED=942487
RTU_SUCCESS=942487
RTU_FAILED=0
RTU_TIMEOUTS=0
REQUEST_PATH_GAP=0
RESPONSE_PATH_GAP=0
REQUEST_BYTE_GAP=0
RESPONSE_BYTE_GAP=0
MASTER_CRC=0
SLAVE_CRC=0
TCP_REQ_S=500.000
RUNTIME_CLEAN=YES
```

H3C.6 generalizó el mismo perfil a respuestas FC03 de 7, 9, 13 y 21 bytes,
300 s por caso:

```txt
Q1 / 7 B : 764.832 tx/s, 229517/229517, 0 errores
Q2 / 9 B : 743.484 tx/s, 223127/223127, 0 errores
Q4 / 13 B: 708.263 tx/s, 212547/212547, 0 errores
Q8 / 21 B: 636.157 tx/s, 190921/190921, 0 errores
```

En todos los casos:

```txt
RTU_TIMEOUTS=0
REQUEST_PATH_GAP=0
RESPONSE_PATH_GAP=0
REQUEST_BYTE_GAP=0
RESPONSE_BYTE_GAP=0
MASTER_CRC=0
SLAVE_CRC=0
SLAVE_DISCARDED_TAILS=0
SLAVE_DISCARDED_BYTES=0
TCP_TARGET=PASS
SD_CLEAN=YES
PERIPHERAL_FAILURE_COUNT=0
```

## Conclusión

Para el flujo FC03 ensayado, la causa de los timeouts quedó localizada en la granularidad
de entrega RX UART/FIFO al software bajo carga, no en pérdida física de bytes ni en TX queued.

El perfil candidato queda:

```txt
MASTER_RX_FIFO_FULL=9
SLAVE_RX_FIFO_FULL=8
RX_MODE=BULK
MASTER_TX=QUEUED
SLAVE_TX=QUEUED
MASTER_SERVER_FRAMING=GAP
SLAVE_SERVER_FRAMING=STRUCTURAL
BAUD=500000
FRAME_GAP_US=100
RTU_TIMEOUT_MS=25
```

H3C se considera cerrado para FC03.

## Límite de la conclusión

FIFO9/FIFO8 todavía no se declara política universal de Modbus RTU. FC0F/FC10 tienen
requests de longitud variable y deberán cubrirse antes de convertir estos valores en defaults
generales de producto.

No se incrementó PARTIAL_HOLD_US ni RTU_TIMEOUT_MS para ocultar el defecto.

## Siguiente gate

H3D — CRC lookup.

Objetivo: sustituir de forma seleccionable el CRC16 bit-a-bit por una tabla de 256 entradas,
manteniendo BITWISE como default durante el A/B y sin cambiar framing, FIFO, baudrate,
timeouts, TCP ni carga de periféricos.
