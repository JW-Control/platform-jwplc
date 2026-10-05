# Alpha14 — campaña final de capacidad TCP/UDP/RTU — 2026-10-02

## Objetivo

Cerrar Alpha14 con cifras obtenidas sobre el package actual, evitando que
resultados históricos de candidatos TEMP se confundan con la configuración que
se publicará.

La campaña separa:

1. `PERFORMANCE`: instrumentación mínima, Serial mudo durante la ventana.
2. `PROFILE`: ventanas independientes; los contadores se acumulan en RAM y se
   vuelcan una vez al final.
3. `CAPACITY_PROFILE` RTU: configuración FC03 validada históricamente, pero no
   declarada default universal porque FC0F/FC10 siguen fuera de esa conclusión.

## Tiempos

```text
FULL_RUNTIME_CASE=600 s
SWEEP_CASE=300 s
PROFILE_CASE=300 s
```

## F0 — preflight

Debe confirmar:

```text
W5500=26 MHz
FIFO_REUSE=ON
TCP=C0 POLLING
INT=OFF
D2=OFF
D3=OFF
E1=OFF
tracked tree=clean
index=clean
```

## F1 — ceiling Ethernet RAW

Casos de 300 s:

- TCP RX legacy/product path;
- TCP TX;
- UDP RX legacy;
- UDP TX;
- UDP RX FAST con la extensión aditiva productiva
  `jwplcReadPacketFastDeferred()/jwplcCommitRxFast()`.

El fast UDP usa payload 1016 B y batch2 por la topología W5500 8x2KB validada.

## F2 — ceiling Modbus TCP full-runtime, RTU OFF

```text
FC03/125
TCP=UNPACED
RTU=OFF
DURATION=600 s
```

Entrega el ceiling transaccional de aplicación, no el ceiling RAW Ethernet.

## F3/F4 — coexistencia industrial

```text
F3: TCP1000 + RTU50, 600 s
F4: TCP500  + RTU50, 600 s
```

Display, DataLog, SD, FRAM, RTC, botonera, I/O y resto del runtime permanecen
activos.

## F5 — ceiling RTU FC03

Perfil de capacidad, no default universal:

```text
TCP=OFF
RTU=UNPACED
BAUD=500000
FRAME_GAP_US=100
MASTER_FIFO=9
SLAVE_FIFO=8
RX_MODE=BULK
MASTER_FRAMING=GAP
SLAVE_FRAMING=STRUCTURAL
TX=QUEUED
CRC=BITWISE
DURATION=600 s
```

La elección BITWISE conserva el default del CRC; H3D había medido LOOKUP como
candidato, pero no lo promovió.

## F6 — matriz TCP x RTU

Cada punto dura 300 s con RTU capacity profile unpaced:

| TCP target |
| ---: |
| 100 req/s |
| 250 req/s |
| 500 req/s |
| 750 req/s |
| 1000 req/s |

La salida principal es `TCP_RTU_MATRIX.csv`.

Un punto que no alcance el target TCP pero conserve cero errores se clasifica
`SATURATION_FAIL_CLEAN` y no detiene la campaña. Sí detienen la campaña:

- corrupción/protocolo;
- timeout TCP;
- fallo RTU;
- CRC;
- mismatch Master/Slave;
- fallo SPI reportado;
- fallo de periférico;
- reset/fallo de runtime.

## F7 — PROFILE separado

Tres puntos de 300 s:

- TCP1000 + RTU50;
- TCP500 + RTU FAST;
- TCP1000 + RTU FAST.

No imprime telemetría periódica durante la ventana. Registra al final:

- AVG/P95/P99/MAX TCP;
- loop avg/max;
- RTU service gap max;
- RTU transaction max;
- SPI probe max wait;
- SPI probe >1 ms / >10 ms;
- errores y contabilidad.

El uso SPI RAW/UDP FAST sí registra hold total y occupancy. El full-runtime
reporta contención/probe; no se presenta ese probe como occupancy absoluto.

## F8 — confirmación automática del equilibrio

Se busca primero el mayor target TCP limpio de F6 que conserve al menos 90 % del
ceiling RTU F5. Si no existe, se selecciona el punto limpio con mayor RTU entre
los targets TCP que sí cumplen.

Ese punto se repite 600 s.

## Evidencia

Raíz:

```text
tools/modbus-tcp-benchmark/results/a14_final_capability_YYYYMMDD_HHMMSS/
```

Incluye:

- `SESSION.log`;
- `00_preflight/MANIFEST.txt`;
- compile/upload logs;
- logs de runners;
- snapshots Master/Slave por caso;
- JSON por caso;
- `FULL_RUNTIME_SUMMARY.csv`;
- `TCP_RTU_MATRIX.csv`;
- `PROFILE_SUMMARY.csv`;
- `F8_SELECTION.json`;
- `FINAL_STATUS.txt`.

## Regla de publicación

Los números PERFORMANCE se obtienen con source/product package actual y sin
overrides de INT. El perfil FAST RTU debe publicarse siempre etiquetado con su
configuración exacta y alcance FC03; no presentarlo como default universal de
Modbus RTU.


## Tiempo esperado de la campaña

Ventanas puras con la configuración actual:

```text
F1 legacy RAW 4 x 300 s  = 20 min
F1 UDP FAST 1 x 300 s    =  5 min
F2 TCP ceiling 600 s      = 10 min
F3 TCP1000+RTU50 600 s    = 10 min
F4 TCP500+RTU50 600 s     = 10 min
F7 profile industrial     =  5 min
F5 RTU ceiling 600 s      = 10 min
F6 matrix 5 x 300 s       = 25 min
F7 profile FAST 2 x 300 s = 10 min
F8 confirmation 600 s     = 10 min
----------------------------------
MEASUREMENT_WINDOWS         = 115 min
```

Con tres etapas de compile/upload, boot/DHCP, snapshots y persistencia de logs,
se espera aproximadamente 2.5–3.5 h en una ejecución limpia. Dejarlo toda la
noche proporciona margen amplio para variaciones de compilación o entorno.


## Guards endurecidos

Cada firmware final expone `BOOT_MARKER` y `UPTIME_MS` sólo en snapshots
fuera de la ventana medida. En full-runtime se guarda un snapshot pre/post por
caso y se exige:

```text
BOOT_MARKER estable Master/Slave
UPTIME monotónico con delta >= 90 % de la ventana
SPI_PROBE_FAILS=0
FULL_RUNTIME_READY=YES
SLAVE_READY=YES
RTU_READY=YES
DISPLAY_READY=YES
```

La campaña verifica además por ancestry que el HEAD contiene las promociones
de FIFO_REUSE, DLEN_REUSE, COPY_OUT_64 y UDP FAST, y comprueba los defaults de
source antes de compilar.

## Nota sobre uso SPI

PERFORMANCE no añade instrumentación global al mutex SPI. En full-runtime se
reporta contención mediante `SPI_PROBE_MAX_WAIT_US`,
`SPI_PROBE_OVER_1MS`, `SPI_PROBE_OVER_10MS` y `SPI_PROBE_FAILS`.
RAW TCP y UDP FAST sí reportan occupancy de su propio ownership SPI.

No se debe interpretar el probe full-runtime como porcentaje absoluto de
ocupación global del bus.

## F5A y F5B

Antes del perfil FAST se añade:

```text
F5A_RTU_ONLY_115200_UNPACED = 600 s
```

Luego:

```text
F5B_RTU_ONLY_FAST_CEILING = 600 s
```

Esto separa la capacidad del perfil industrial 115200 de la capacidad extrema
FC03 con 500 kbaud/FIFO9-FIFO8/BULK.

Con F5A el total de ventanas de medición pasa de 115 a aproximadamente
125 minutos. Incluyendo compilaciones/uploads, boot/DHCP y persistencia de
evidencia, la ejecución limpia debería quedar alrededor de 2.5–4 horas; dejarla
toda la noche da margen suficiente.
