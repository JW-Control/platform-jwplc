# Alpha14 — RTU-F2 — Sweep de frame gap exacto

Fecha: 2026-09-25

## Objetivo

Caracterizar el impacto del frame gap exacto con el motor de microsegundos de
RTU-F1, sin cambiar baudrate, timeout, TX, autoservice ni scheduler.

## Configuracion fija

```txt
RTU=115200 8N1
RTU_MODE=UNPACED
RS485=A/B/GND
FULL_RUNTIME=ACTIVE
TCP=FC03/125
W5500=26 MHz
```

## Sweep

| Comando | Gap |
|---|---:|
| H | 2000 us |
| I | 1750 us |
| J | 1500 us |
| K | 1250 us |
| L | 1000 us |

Para cada gap se mediran dos ventanas de 60 s:

1. TCP500 + RTU unpaced.
2. TCP OFF + RTU unpaced.

1750 us queda como referencia de interoperabilidad a estudiar. Los gaps menores
son candidatos de investigacion para un futuro perfil JWPLC rapido; este gate no
adopta aun ningun valor como default de producto.

## Baseline F1

```txt
2000 us exactos
TCP500: 170.432 Hz RTU
TCP OFF: 190.275 Hz RTU
```

## Gate versionado

El gate fuerza una unica compilacion/upload desde fuente con RTU-F1, restaura
el archive precompilado previo y luego cambia los gaps por Serial sin volver a
compilar.

El sweep conserva toda la evidencia aunque un gap agresivo falle. Para cerrar
como PASS_CHARACTERIZED, 2000 us y 1750 us deben quedar limpios y el caso
TCP500 a 1750 us debe alcanzar al menos 99 % del target.

Los puntos 1500, 1250 y 1000 us son de caracterizacion. Su fallo no descarta el
gate; su resultado define el limite del futuro perfil rapido.

## Resultado fisico RTU-F2

| Gap | TCP500 req/s | RTU con TCP500 | RTU TCP OFF | Estado |
|---:|---:|---:|---:|---|
| 2000 us | 500.000 | 165.395 Hz | 181.642 Hz | limpio |
| 1750 us | 500.000 | 177.382 Hz | 192.024 Hz | limpio |
| 1500 us | 500.000 | 189.093 Hz | 204.556 Hz | limpio |
| 1250 us | 500.000 | 195.188 Hz | 218.730 Hz | limpio |
| 1000 us | 500.000 | 203.256 Hz | 233.073 Hz | limpio |

En todos los puntos: RTU_FAILED=0, RTU_TIMEOUTS=0, RTU_CRC=0,
SLAVE_CRC=0, TCP_CLEAN=YES, RTU_CLEAN=YES y RUNTIME_CLEAN=YES.

Ganancia contra 2000 us dentro de la misma corrida:

| Gap | TCP500 | TCP OFF |
|---:|---:|---:|
| 1750 us | +7.247 % | +5.716 % |
| 1500 us | +14.328 % | +12.615 % |
| 1250 us | +18.013 % | +20.418 % |
| 1000 us | +22.891 % | +28.314 % |

1000 us fue el menor gap probado y siguio completamente limpio, por lo que
RTU-F2 no encontro aun el limite inferior.

