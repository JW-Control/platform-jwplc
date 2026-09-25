# Alpha14 — RTU-F3 — TX encolada sobre AutoDirection

Fecha: 2026-09-25

## Objetivo

Medir el efecto de eliminar la espera activa de `flush()` en Modbus RTU,
manteniendo fijo:

```txt
RTU=115200 8N1
FRAME_GAP=500 us
TCP_TARGET=500 req/s o OFF
FULL_RUNTIME=ACTIVE
RS485=A/B/GND
W5500=26 MHz
```

F2C selecciono 500 us como baseline con margen:

```txt
600 us + TCP500 -> 248.238 Hz
500 us + TCP500 -> 252.573 Hz
300 us + TCP500 -> 266.483 Hz
500 vs 600 = +1.746 %
300 vs 500 = +5.507 %
```

## Cambio de package

`JWPLC_RS485.write()` conserva su comportamiento historico y ejecuta
`flush()`.

Se anade `JWPLC_RS485.writeQueued()`. En JWPLC Basic v2, el hardware declara
`JWPLC_RS485_AUTO_DIRECTION=1` por el MAX13487E y se configura un TX ring
buffer de 512 bytes antes de iniciar Serial2.

La ruta queued no ejecuta los hooks manuales DE/RE porque solo se considera
activa cuando el hardware confirma AutoDirection. En hardware sin soporte cae
al write bloqueante.

`JWPLC_ModbusRTU` anade:

```cpp
setQueuedTxEnabled(bool)
queuedTxEnabled()
queuedTxActive()
```

El default continua siendo bloqueante.

## A/B

El mismo firmware permite:

| Comando | Modo |
|---|---|
| Y | BLOCKING |
| Z | QUEUED |

Se mediran cuatro ventanas de 60 s:

1. BLOCKING + TCP500.
2. BLOCKING + TCP OFF.
3. QUEUED + TCP500.
4. QUEUED + TCP OFF.

Ambos modos usan el mismo TX ring buffer. La unica diferencia A/B es esperar o
no a que la UART quede fisicamente idle tras cada write.

## Criterio

Todos los casos deben mantener:

- cero CRC y timeout RTU;
- cero errores TCP;
- TCP500 >= 99 %;
- SD/perifericos limpios;
- gap efectivo de 500 us;
- AutoDirection detectado;
- buffer TX >= 257 bytes;
- soporte queued activo cuando se selecciona QUEUED.

La ganancia de rendimiento se caracteriza; no se exige una mejora minima para
considerar valida la corrida.
