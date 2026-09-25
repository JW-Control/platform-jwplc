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

