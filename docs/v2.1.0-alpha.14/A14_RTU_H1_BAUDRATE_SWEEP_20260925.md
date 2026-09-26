# Alpha14 — RTU-H1 — Sweep de baudrate con firmware optimizado

Fecha: 2026-09-25

## Objetivo

Medir la ganancia debida exclusivamente al baudrate despues de cerrar las
optimizaciones de firmware RTU-F1/F2/F3.

## Configuracion fija

```txt
MOTOR=ASYNC
TX=QUEUED / AutoDirection
FRAME_GAP=500 us
RTU_CONFIG=8N1
RTU_TIMEOUT=25 ms
FULL_RUNTIME=ACTIVE
RS485=A/B/GND
W5500=26 MHz
TCP=FC03/125
```

El motor publico se selecciona con:

```cpp
JWPLC_ModbusRTU.motor(ASYNC);
```

ASYNC es el default del package. SYNC sigue disponible mediante
`JWPLC_ModbusRTU.motor(SYNC)` con los mismos nombres de funciones.

## Baudrates H1

| Comando benchmark | Baud |
|---|---:|
| 7 | 115200 |
| 8 | 230400 |
| 9 | 500000 |

Para cada baud se ejecutan:

1. TCP500 + RTU unpaced, 60 s.
2. TCP OFF + RTU unpaced, 60 s.

## Aislamiento

H1 no optimiza el frame gap por baudrate. Se mantiene 500 us en los tres puntos
para atribuir la diferencia observada al cambio de velocidad fisica.

Si 500000 resulta limpio, un gate posterior puede estudiar si el gap puede
reducirse de forma segura a esa velocidad.

## Criterio

115200 es el control y debe quedar completamente limpio.

230400 y 500000 son puntos de caracterizacion: si uno falla se conserva el
resultado y se identifica el baud mas alto que complete ambos casos sin
CRC/timeouts/fallos y mantenga TCP500 >=99 %.

No se cambia aun el baudrate default publico del package.

## Gate versionado

El gate fuerza una sola compilacion/upload desde las fuentes actuales,
ocultando temporalmente el archive precompilado anterior de Modbus RTU.

Verifica en Master y Slave los objetos:

```txt
JWPLC_ModbusRTU.cpp.o
JWPLC_RS485.cpp.o
```

Antes del sweep restaura el archive previo con su mismo SHA-256.

Durante la medicion el baudrate cambia por USB de diagnostico, con el trafico
RTU detenido. El Slave se reconfigura primero y el Master despues; no se emite
ninguna transaccion mientras los extremos tienen baudrates distintos.

Cada snapshot debe confirmar:

```txt
RTU_MOTOR=ASYNC
RTU_TX_MODE=QUEUED
RTU_FRAME_GAP_US=500
RS485_AUTO_DIRECTION=YES
RS485_QUEUED_TX_SUPPORTED=YES
```

El runner muestra la salida en vivo y simultaneamente la guarda con
`Tee-Object`, usando explicitamente `python.exe -u` para evitar buffering
en pruebas largas.

Si 115200 pasa, el gate conserva los resultados de 230400 y 500000 incluso si
alguno falla. Al final reporta el baud mas alto limpio en TCP500 y TCP OFF.

## Resultado fisico RTU-H1

| Baud | TCP500 RTU | TCP OFF RTU | Fallos/CRC | Estado |
|---:|---:|---:|---|---|
| 115200 | 248.696 Hz | 280.367 Hz | 0 | limpio |
| 230400 | 240.405 Hz | 276.101 Hz | miles | FALLA |
| 500000 | 394.137 Hz | 452.040 Hz | 0 | limpio |

A 230400 se observaron 8321 fallos con TCP500 y 9433 con TCP OFF, dominados
por CRC. A 500000 hubo cero fallos, timeout y CRC en ambos casos.

```txt
RTUH1_CLEAN_BAUDS=115200,500000
RTUH1_HIGHEST_CLEAN_BAUD=500000
TCP500_GAIN_500K_VS_115200=+58.481 %
OFF_GAIN_500K_VS_115200=+61.231 %
```

La discontinuidad 230400 FAIL / 500000 PASS obliga a diagnosticar reloj/divisor
antes de fijar una politica de baudrate.

