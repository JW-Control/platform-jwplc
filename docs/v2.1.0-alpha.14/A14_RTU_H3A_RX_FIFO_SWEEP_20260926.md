# Alpha14 — RTU-H3A — Sweep RX FIFO a 500 kbaud

Fecha: 2026-09-26

## Objetivo

Aislar cuánto del costo de convivencia RTU/TCP proviene de la latencia con la
que HardwareSerial hace visibles los bytes RX al runtime.

Configuración fija:

```txt
BAUD=500000
CONFIG=8N1
CLOCK=APB_FORCED
FRAME_GAP=100 us
MOTOR=ASYNC
TX=QUEUED
RTU=UNPACED
FULL_RUNTIME=ACTIVE
W5500=26 MHz
```

Única variable:

| Caso | RX FIFO full threshold |
|---|---:|
| control | 120 bytes |
| A | 32 bytes |
| B | 16 bytes |
| C | 8 bytes |
| D | 1 byte |

Cada threshold se prueba con TCP500 FC03/125 y con TCP OFF.

## Hipótesis

A baudrates altos HardwareSerial usa 120 bytes como threshold por defecto.
Nuestras tramas RTU cortas normalmente llegan al ring buffer por RX timeout.
Un threshold menor puede reducir esa latencia, pero también incrementar la
carga de interrupciones. H3A mide ambos efectos antes de adoptar nada.

## Alcance

H3A sólo añade instrumentación al firmware de qualification. No cambia todavía
el default de producto en JWPLC_RS485.

No se modifican simultáneamente:

- timeout RTU de 25 ms;
- frame gap de 100 us;
- APB;
- baudrate de 500 kbaud;
- W5500 a 26 MHz;
- parser RTU;
- CRC;
- scheduler;
- backend Ethernet.

## Métricas

Por caso se registran RTU tx/s, TCP req/s y latencias AVG/P95/P99, máximo gap
de servicio RTU, máximo gap de loop, CRC, timeout, fallos, SD/periféricos y
estabilidad física de ambas TFT.

El control FIFO=120 debe quedar limpio. Los thresholds menores pueden fallar
sin invalidar la caracterización.

## Meta

H2C dejó 420.916 tx/s a 500k + 100 us + TCP500 durante 600 s. H2B alcanzó
431.470 tx/s con 50 us, y H2 alcanzó 614.577 tx/s sin TCP a 50 us.

La meta exploratoria es acercarse a 480 tx/s bajo TCP500. Ese nivel permitiría,
si cada muestra usa una sola transacción, 8 módulos x 60 transacciones/s.
