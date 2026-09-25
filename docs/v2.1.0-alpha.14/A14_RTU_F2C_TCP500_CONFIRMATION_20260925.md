# Alpha14 — RTU-F2C — Confirmacion de gaps rapidos con TCP500

Fecha: 2026-09-25

## Objetivo

Confirmar bajo carga Ethernet sostenida los tres candidatos seleccionados tras
RTU-F2B antes de modificar la ruta TX de RS-485.

Este gate no cambia firmware de producto ni adopta todavia un gap definitivo.

## Firmware ya cargado

Se reutiliza el firmware RTU-F2B cargado fisicamente en Master y Slave. Dicho
firmware ya expone por consola los gaps exactos requeridos:

| Comando | Gap |
|---|---:|
| N | 600 us |
| O | 500 us |
| W | 300 us |

El gate F2C no compila ni sube firmware. Cada punto se valida por ACK y snapshot
de Master/Slave antes de medir.

## Configuracion fija

```txt
RTU=115200 8N1
RTU_MODE=UNPACED
TCP_TARGET=500 req/s
TCP_FC03_QUANTITY=125
FULL_RUNTIME=ACTIVE
RS485=A/B/GND
W5500=26 MHz
DURATION_PER_CASE=60 s
```

## Casos

1. 600 us + TCP500.
2. 500 us + TCP500.
3. 300 us + TCP500.

## Criterios

Para considerar un punto limpio:

- TCP >= 99 % del target.
- cero errores/timeout/protocolo TCP;
- cero fallos, timeout y CRC RTU;
- igualdad entre requests Master y frames procesados por Slave;
- SD/perifericos limpios;
- gap efectivo correcto en ambos equipos.

El runner reporta el mejor punto limpio, pero la decision de gap para F3 debe
considerar margen y no solo el maximo throughput.

## Referencia RTU-F2B con TCP OFF

```txt
600 us -> 287.899 Hz
500 us -> 291.722 Hz
300 us -> 311.761 Hz
```

## Siguiente etapa

Tras F2C se selecciona el gap candidato para RTU-F3. F3 mantiene 115200 y el
mismo gap mientras compara la TX bloqueante actual frente a una estrategia que
evite mantener la CPU esperando en `flush()`. El baudrate se cambia despues.
