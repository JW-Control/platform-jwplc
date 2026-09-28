# Alpha14 — Requalification post-H3E de límites TCP / RTU

Fecha: `2026-09-28`

## Motivo

H3E demostró que el Display precompilado histórico introducía pausas largas en
`jwplcSystemTask` y que el renderer Display/TFT dominaba el peor gap del loop.

Evidencia previa RTU-H3C.1:

```text
RTU_SERVICE_GAP_MAX_US=19307
LOOP_GAP_MAX_US=19301
```

Evidencia H3E de atribución:

```text
SYS_DISPLAY ~= 23.6 ms en el peor evento del baseline histórico
```

Después de regenerar Display/TFT y cerrar H3E.5:

```text
LOOP_MAX_US=8212
TCP=1000.0 req/s
RTU=50.004 Hz
RTU_FAILED=0
TCP_ERRORS=0
```

Por tanto algunos umbrales obtenidos antes de H3E deben reinterpretarse como
límites del runtime anterior, no necesariamente del runtime final.

## Estado

```text
A14_POST_H3E_THRESHOLD_REQUALIFICATION=OPEN
R0_STATIC_CLI=PASS_RETAINED
R1_ARDUINO_IDE_PHYSICAL=PASS_RETAINED
PR99_MERGE_READINESS=PAUSED
```

## H4A0 — RAW Ethernet post-H3E

Objetivo: volver a medir el techo bruto de transporte Ethernet con el package
final, separando claramente transporte RAW de Modbus TCP.

Firmware:

```text
tools/modbus-tcp-benchmark/firmware/
eth14_raw_transport_server/
eth14_raw_transport_server.ino
```

Runner:

```text
tools/modbus-tcp-benchmark/pc/
eth14_raw_transport_benchmark.py
```

Modos:

```text
TCP_RX
TCP_TX
UDP_RX
UDP_TX
```

Baseline histórica a 26 MHz:

```text
G2:
TCP_RX ~= 13.798 Mbps
TCP_TX ~= 4.900 Mbps
UDP_RX ~= 11.411 Mbps
UDP_TX ~= 5.178 Mbps
```

Otra sesión optimizada RAW dejó:

```text
TCP_RX median ~= 13.412507 Mbps
UDP_RX median ~= 13.866349 Mbps
UDP_RX vs TCP_RX ~= +3.38 %
```

Estas cifras pertenecen a gates/direcciones/workloads distintos; por tanto no
se mezclan como un único techo universal.

La repetición post-H3E usará:

```text
W5500 SPI=26 MHz
autoload normal
JWPLC_Display + JWPLC_TFT finales
external TFT_eSPI=NO
TCP chunk comparable
UDP payload comparable
```

Se medirán primero los cuatro modos con el mismo runner histórico y luego, si
alguno muestra una mejora clara, se hará una pequeña exploración de chunk/batch
sin cambiar librería Ethernet de producto.

El dato de datasheet debe declarar dirección y protocolo, por ejemplo:

```text
RAW TCP RX throughput = X Mbps
RAW TCP TX throughput = Y Mbps
RAW UDP RX throughput = Z Mbps
RAW UDP TX throughput = W Mbps
```

## H4A1 — Modbus TCP-only post-H3E

Objetivo: redescubrir el techo de Modbus TCP con el runtime final y RTU
deshabilitado, manteniendo autoload normal y artifacts H3E finales.

Se repite primero la forma de la caracterización histórica:

```text
FC03 quantity=1,16,64,125
modo no-wait
ventana corta
```

Referencias históricas pre-H3E:

| FC03 | Techo corto observado |
|---:|---:|
| 1 reg | 2981.1 req/s |
| 16 reg | 2646.8 req/s |
| 64 reg | 1684.0 req/s |
| 125 reg | 1142.4 req/s |

Después se acota FC03/125 con targets crecientes y criterio:

```text
ACHIEVED >= 95% requested
timeouts=0
transport_errors=0
protocol_errors=0
bus_lock_timeouts=0
unexpected_resets=0
```

No se reutiliza como techo final el límite histórico ~1.10-1.14 kreq/s hasta
repetirlo post-H3E.

## H4B — TCP + RTU 50 Hz post-H3E

Perfil:

```text
FULL_RUNTIME=ACTIVE
DISPLAY=DIRTY/ON_DEMAND
RTU=115200 8N1
RTU_TARGET=50 Hz
W5500=26 MHz
```

H3E.5 ya demuestra:

```text
TCP=1000 req/s
RTU=50.004 Hz
TCP=120000/120000
RTU=6001/6001
```

H4B buscará la frontera TCP por encima de 1000 req/s manteniendo:

```text
RTU >= 99% de 50 Hz
RTU_FAILED=0
RTU_TIMEOUTS=0
CRC=0
TCP clean
peripheral_failure_count=0
```

## H4C — RTU FAST post-H3E

El perfil FAST histórico cualificado:

```text
BAUD=500000
MOTOR=ASYNC
TX=QUEUED
W5500=26 MHz
```

H2B pre-H3E:

| Gap | TCP500 | RTU |
|---:|---:|---:|
| 100 us | 500.000 req/s | 421.042 Hz |
| 75 us | 500.000 req/s | 422.313 Hz |
| 50 us | 499.977 req/s | 431.470 Hz |

Decisión post-H3E:

```text
100 us = baseline FAST
75 us = candidato a revalidar
50 us = fuera de esta segunda pasada
```

Cada punto se medirá con:

1. TCP500.
2. TCP OFF.

## H4D — SFIFO/BULK post-H3E

H3C.5 pre-H3E cerró:

```text
MASTER_RX_FIFO_FULL=9
SLAVE_RX_FIFO_FULL=8
RX_MODE=BULK
FRAME_GAP_US=100
TCP=500 req/s
RTU=785.147 tx/s
DURATION=1200 s
FAILED=0
TIMEOUTS=0
CRC=0
```

La revisión post-H3E debe medir de nuevo el throughput con ese perfil estable.

Como diagnóstico causal se puede comparar contra FIFO1/FIFO1, porque los tails
históricos coincidieron con ventanas de servicio de ~19 ms que H3E atribuyó
posteriormente al Display.

No se adoptará FIFO1 como default aunque un gate corto quede limpio.


## H4E — Coexistencia extrema TCP + RTU

Objetivo: construir una frontera 2D de rendimiento simultáneo para obtener un
dato defendible de datasheet.

No se fija de antemano un target como PASS obligatorio. Se exploran pares:

```text
TCP req/s x RTU tx/s
```

partiendo de puntos ya demostrados y subiendo gradualmente.

Ejemplos de zonas objetivo:

```text
TCP 1000 + RTU 50
TCP 1000 + RTU 200
TCP 1000 + RTU 500
TCP 1000 + RTU 800
TCP 1000 + RTU 1000   <-- sólo si el hardware/runtime lo sostiene
```

Si el techo RTU con TCP=1000 queda por debajo, se construye la frontera inversa:

```text
RTU target alto + TCP variable
```

Cada candidato de datasheet exige:

```text
duration >= 600 s
TCP achieved >= 99% target
RTU achieved >= 99% target
TCP errors=0
RTU failed=0
RTU timeouts=0
CRC=0
SD failed commits=0
peripheral failures=0
TFT physical=PASS
unexpected resets=0
```

La cifra publicada debe incluir el perfil exacto:

```text
Modbus TCP:
  FC/function
  quantity
  req/s

Modbus RTU:
  baud
  frame gap
  FIFO
  RX mode
  tx/s

Runtime:
  Display mode
  Ethernet SPI
  periféricos activos
```

Esto evita publicar un número ambiguo o no reproducible.

## Reglas de adopción

- No cambiar defaults por un único pico corto.
- Primero 60-120 s para localizar frontera.
- Sólo el candidato final se confirma 5-10 min.
- 50 us queda excluido.
- W5500 30 MHz no se reabre.
- No se retiran periféricos del autoload.
- No se modifican APIs públicas para esta requalification.
- Si un perfil mejora poco frente a su pérdida de margen, se conserva el valor
  más robusto.

## Orden

```text
H4A0 RAW Ethernet TCP/UDP Mbps
-> H4A1 Modbus TCP-only req/s
-> H4B TCP + RTU50
-> H4C RTU FAST 100/75 us
-> H4D SFIFO/BULK
-> H4E coexistencia extrema TCP+RTU / datasheet
-> decisión final
-> R0/R1 siguen válidos
-> PR99 + CI
```
