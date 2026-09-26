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

## Gate versionado

El gate oculta temporalmente el archive precompilado de Modbus RTU, fuerza una
unica compilacion/upload desde las fuentes actuales y verifica en los build
paths la presencia de:

```txt
JWPLC_ModbusRTU.cpp.o
JWPLC_RS485.cpp.o
```

Luego restaura el archive previo con el mismo SHA-256 y ejecuta el A/B sin
recompilar entre modos.

El runner valida en Master y Slave:

```txt
RTU_FRAME_GAP_US=500
RS485_AUTO_DIRECTION=YES
RS485_TX_BUFFER_BYTES>=257
RS485_QUEUED_TX_SUPPORTED=YES
```

En BLOCKING, `RTU_TX_QUEUED_ACTIVE=NO`. En QUEUED debe ser `YES`.

F3 es de caracterizacion: si ambos modos son estables se calcula la ganancia
RTU y el cambio de latencia TCP antes de decidir si QUEUED pasa a ser el
comportamiento recomendado del package.

## Resultado fisico RTU-F3

| Modo | TCP | RTU | TCP AVG | P95 | P99 | Estado |
|---|---:|---:|---:|---:|---:|---|
| BLOCKING | 500.000 req/s | 245.091 Hz | 1423.9 us | 2478.2 us | 11447.9 us | limpio |
| BLOCKING | OFF | 279.353 Hz | - | - | - | limpio |
| QUEUED | 499.907 req/s | 243.279 Hz | 1077.0 us | 1630.7 us | 10514.0 us | limpio |
| QUEUED | OFF | 277.063 Hz | - | - | - | limpio |

```txt
QUEUED vs BLOCKING, TCP500 RTU = -0.739 %
QUEUED vs BLOCKING, TCP OFF RTU = -0.820 %
TCP500 AVG = -346.9 us
TCP500 P95 = -847.5 us
TCP500 P99 = -933.9 us
```

Interpretacion: la TX encolada no aumenta el techo RTU a 115200, pero reduce de
forma clara el tiempo que Ethernet espera por CPU. Se conserva como transporte
del motor ASYNC sobre hardware AutoDirection. El motor SYNC mantiene el
transporte bloqueante por compatibilidad.

