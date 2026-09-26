# Alpha14 — RTU-H3B — BYTE RX vs BULK RX

Fecha: 2026-09-26

## Objetivo

Aislar el coste de la recepción byte-a-byte en JWPLC_RS485/JWPLC_ModbusRTU
después de que H3A identificó RX FIFO=1 como el punto más rápido.

H3B compara dos rutas dentro del mismo firmware:

| Modo | Ruta |
|---|---|
| BYTE | `available() + read()` histórico por byte |
| BULK | `readAvailable(buffer, n)` por bloques |

La ruta BULK agrupa la telemetría de actividad RX en una sola actualización
por lectura de bloque.

## Perfil fijo

```txt
BAUD=500000
CONFIG=8N1
CLOCK=APB_FORCED
FRAME_GAP=100 us
RX_FIFO_FULL=1
MOTOR=ASYNC
TX=QUEUED
RTU=UNPACED
W5500=26 MHz
FULL_RUNTIME=ACTIVE
```

Cada modo se prueba con:

- TCP500 FC03/125;
- TCP OFF;
- 60 s por caso por defecto.

## Compatibilidad

H3B agrega APIs aditivas:

```cpp
JWPLC_RS485.readAvailable(buffer, maxSize);
JWPLC_ModbusRTU.setBulkRxEnabled(bool);
JWPLC_ModbusRTU.bulkRxEnabled();
```

El constructor conserva:

```txt
BULK_RX_DEFAULT=false
```

Por tanto el comportamiento histórico BYTE no cambia por defecto durante este
gate.

## Criterio

El control BYTE debe quedar limpio.

Para cada caso se registran:

- RTU tx/s;
- TCP req/s;
- TCP AVG/P95/P99;
- RTU service gap max;
- loop gap max;
- failed/timeouts/CRC;
- SD/periféricos;
- estado físico de TFT.

BULK puede fallar sin convertir el control histórico en fallo de producto; el
resultado se caracteriza antes de decidir adopción.

## Meta exploratoria

H3A dejó:

```txt
FIFO=1
TCP500
RTU=476.854 tx/s
TCP=500.000 req/s
```

La meta H3B es:

```txt
RTU >= 480 tx/s
TCP >= 495 req/s
RUNTIME_CLEAN=YES
```

El runner reporta:

```txt
RTUH3B_DELTA
RTUH3B_TCP480_TARGET_PASS
RTUH3B_BULK_TCP500_RTU_HZ
A14_RTU_H3B
```

## Siguiente decisión

Si BULK supera al control de forma limpia, se evalúa adoptarlo como ruta
interna por defecto del motor ASYNC. Si no aporta, se conserva BYTE y el
siguiente candidato será early-frame dispatch del Slave.
