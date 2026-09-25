# Alpha14 — RTU-F2B — Busqueda del piso de frame gap

Fecha: 2026-09-25

## Objetivo

Continuar RTU-F2 por debajo de 1000 us para localizar el limite inferior
practico del parser RTU a 115200 8N1 antes de modificar TX o baudrate.

Este gate es diagnostico. No adopta el menor valor limpio como default.

## Configuracion fija

```txt
RTU=115200 8N1
RTU_MODE=UNPACED
TCP=OFF
FULL_RUNTIME=ACTIVE
RS485=A/B/GND
W5500=26 MHz
```

## Sweep exploratorio

| Comando | Gap |
|---|---:|
| L | 1000 us |
| M | 750 us |
| N | 600 us |
| O | 500 us |
| Q | 400 us |
| V | 350 us |
| W | 300 us |

Duracion: 30 s por punto.

El piso de 300 us se elige para esta exploracion porque a 115200 8N1 esta
aproximadamente en la zona de 3.5 tiempos de caracter. Valores inferiores
quedarian fuera del alcance de este gate y requeririan una decision explicita
de protocolo/perfil rapido.

## Criterio

- 1000 us debe permanecer limpio como control.
- Los puntos inferiores son de caracterizacion y pueden fallar.
- El runner debe continuar despues de un punto fallido para conservar la curva.
- Se reportan RTU_HZ, failures, timeouts, CRC y estado del runtime.
- Al final se identifica el menor gap limpio observado y el mayor RTU limpio.

Despues de F2B, el siguiente cambio de firmware sera estudiar TX/flush a
115200 con un gap seleccionado con margen. El baudrate se cambia despues para
no mezclar optimizacion de CPU con velocidad fisica del enlace.

## Gate versionado

El gate fuerza una unica compilacion/upload desde fuente, comprueba que
`JWPLC_ModbusRTU.cpp.o` exista tanto para Master como para Slave y restaura
el archive precompilado previo con su mismo SHA-256 antes de medir.

La medicion usa TCP OFF y 30 s por gap para localizar rapidamente el piso del
framing. Un punto agresivo fallido no detiene el sweep; solo el control de
1000 us debe permanecer limpio para considerar valida la corrida.

Si 300 us continua limpio, el gate declara que el piso no fue encontrado dentro
del rango con sentido elegido para F2B y no baja automaticamente por debajo de
300 us.

## Resultado fisico RTU-F2B

| Gap | RTU TCP OFF | Fallos | Timeouts | CRC Master | CRC Slave | Runtime |
|---:|---:|---:|---:|---:|---:|---|
| 1000 us | 252.374 Hz | 0 | 0 | 0 | 0 | limpio |
| 750 us | 269.846 Hz | 0 | 0 | 0 | 0 | limpio |
| 600 us | 287.899 Hz | 0 | 0 | 0 | 0 | limpio |
| 500 us | 291.722 Hz | 0 | 0 | 0 | 0 | limpio |
| 400 us | 301.373 Hz | 0 | 0 | 0 | 0 | limpio |
| 350 us | 304.665 Hz | 0 | 0 | 0 | 0 | limpio |
| 300 us | 311.761 Hz | 0 | 0 | 0 | 0 | limpio |

El menor gap probado, 300 us, siguio limpio y fue tambien el punto mas rapido.
El piso no se encontro dentro del alcance seleccionado:

```txt
RTUF2B_LOWEST_CLEAN_GAP_US=300
RTUF2B_FASTEST_CLEAN_GAP_US=300
RTUF2B_FLOOR_WITHIN_SCOPE=NOT_FOUND_AT_OR_ABOVE_300US
A14_RTU_F2B=PASS_CHARACTERIZED
```

No se baja automaticamente de 300 us. Antes de modificar TX se confirma bajo
TCP500 un conjunto reducido de candidatos: 600, 500 y 300 us.

