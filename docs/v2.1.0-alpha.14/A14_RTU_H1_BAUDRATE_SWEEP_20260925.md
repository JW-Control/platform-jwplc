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
