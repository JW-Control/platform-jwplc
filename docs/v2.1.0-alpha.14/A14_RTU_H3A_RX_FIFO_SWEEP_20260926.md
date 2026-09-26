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


## Resultado físico H3A

La matriz completa quedó limpia en los diez escenarios.

| RX FIFO | TCP | RTU tx/s | TCP req/s | TCP AVG us | TCP P95 us | Estado |
|---:|---|---:|---:|---:|---:|---|
| 120 | TCP500 | 428.938 | 500.000 | 1113.7 | 1777.6 | limpio |
| 120 | OFF | 582.422 | 0 | 0 | 0 | limpio |
| 32 | TCP500 | 425.847 | 500.000 | 1119.5 | 1770.0 | limpio |
| 32 | OFF | 577.268 | 0 | 0 | 0 | limpio |
| 16 | TCP500 | 429.833 | 500.000 | 1113.8 | 1768.0 | limpio |
| 16 | OFF | 580.473 | 0 | 0 | 0 | limpio |
| 8 | TCP500 | 456.269 | 500.000 | 1153.3 | 1812.3 | limpio |
| 8 | OFF | 596.187 | 0 | 0 | 0 | limpio |
| 1 | TCP500 | 476.854 | 500.000 | 1262.8 | 1914.1 | limpio |
| 1 | OFF | 674.493 | 0 | 0 | 0 | limpio |

Todos los puntos quedaron con:

```txt
FAILED=0
TIMEOUTS=0
MASTER_CRC=0
SLAVE_CRC=0
RUNTIME_CLEAN=YES
```

Resumen:

```txt
RTUH3A_FASTEST_CLEAN_TCP500_FIFO_BYTES=1
RTUH3A_FASTEST_CLEAN_TCP500_RTU_HZ=476.854
RTUH3A_FASTEST_CLEAN_OFF_FIFO_BYTES=1
RTUH3A_FASTEST_CLEAN_OFF_RTU_HZ=674.493
A14_RTU_H3A=PASS_CHARACTERIZED
```

FIFO=1 mejora RTU +11.171 % con TCP500 y +15.808 % con TCP OFF frente al
threshold 120. TCP mantiene 500 req/s, aunque su latencia media aumenta.

### Decisión

FIFO=1 pasa a candidato experimental para H3B, pero todavía no se adopta como
default de producto. H3B aislará el coste de la lectura RX byte-a-byte mediante
una ruta BULK RX con actividad agrupada, manteniendo FIFO=1 y el resto del
perfil sin cambios.
