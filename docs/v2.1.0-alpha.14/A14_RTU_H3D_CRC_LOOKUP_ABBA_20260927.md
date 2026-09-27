# Alpha14 — RTU-H3D — CRC lookup ABBA

Fecha: 2026-09-27

## Objetivo

Medir si reemplazar el CRC16 Modbus bit-a-bit por una lookup table de 256 entradas
aporta rendimiento real bajo Full Runtime sin degradar estabilidad.

## Estado base

H3C queda cerrado para FC03 con el perfil:

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
TCP=500 req/s
```

H3D no modifica ningún parámetro anterior.

## Implementación CRC

Se añade una implementación lookup de 256 entradas y un selector estático:

```txt
BITWISE
LOOKUP
```

BITWISE sigue siendo el default mientras la qualification esté abierta.

La API histórica `crc16()`, `checkCRC()` y `appendCRC()` se conserva.

## Corrección

Master y Slave exponen:

```txt
RTU_CRC_MODE=BITWISE|LOOKUP
RTU_CRC_SELFTEST=PASS|FAIL
```

La autoprueba usa el vector estándar:

```txt
"123456789" -> 0x4B37
```

Cada corrida exige self-test PASS en ambos dispositivos.

## Diseño experimental

Secuencia ABBA:

```txt
RUN1 BITWISE 180 s
RUN2 LOOKUP  180 s
RUN3 LOOKUP  180 s
RUN4 BITWISE 180 s
```

Se usa FC03 qty=2:

```txt
request = 8 B
response = 9 B
```

El gate calcula:

```txt
RTUH3D_BITWISE_AVG_HZ
RTUH3D_LOOKUP_AVG_HZ
RTUH3D_LOOKUP_GAIN_PCT
```

No se exige una ganancia mínima para declarar válida la prueba. El gate PASS significa
que los cuatro casos fueron correctos y comparables. La decisión de adoptar LOOKUP se toma
después de revisar la mejora medida y el coste de 512 B de tabla.

## Criterio de limpieza por corrida

```txt
RTU_FAILED=0
RTU_TIMEOUTS=0
REQUEST_PATH_GAP=0
RESPONSE_PATH_GAP=0
REQUEST_BYTE_GAP=0
RESPONSE_BYTE_GAP=0
MASTER_CRC=0
SLAVE_CRC=0
SLAVE_DISCARDED_TAILS=0
SLAVE_DISCARDED_BYTES=0
TCP_TARGET_PCT >= 99
cada bucket TCP >= 495 req/s
SD_CLEAN=YES
PERIPHERAL_FAILURE_COUNT=0
```

## Límite

H3D evalúa coste/beneficio de CRC lookup. No convierte LOOKUP en default de producto
automáticamente.
