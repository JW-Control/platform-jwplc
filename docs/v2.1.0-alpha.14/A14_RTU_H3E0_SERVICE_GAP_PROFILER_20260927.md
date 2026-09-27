# Alpha14 — RTU-H3E.0 — Full Runtime Service Gap Profiler

Fecha: 2026-09-27

## Objetivo

Dejar de optimizar a ciegas el `RTU_SERVICE_GAP_MAX_US`.

H3E.0 no modifica scheduling, prioridades, periodos, timeouts ni APIs.
Sólo añade instrumentación a un Master de qualification separado del firmware
histórico H3C/H3D.

## Perfil fijo

```txt
BAUD=500000
CLOCK=APB_FORCED
FRAME_GAP_US=100
MASTER_RX_FIFO_FULL=9
SLAVE_RX_FIFO_FULL=8
RX_MODE=BULK
MASTER_TX=QUEUED
SLAVE_TX=QUEUED
MASTER_SERVER_FRAMING=GAP
SLAVE_SERVER_FRAMING=STRUCTURAL
RTU_TIMEOUT_MS=25
RTU_FC03_QTY=2
CRC_MODE=BITWISE
TCP=500 req/s
FULL_RUNTIME=ACTIVE
DURATION=300 s
```

BITWISE se usa para no mezclar el diagnóstico de scheduling con la decisión
pendiente de adoptar CRC LOOKUP como default.

## Instrumentación por bloque

Cada bloque registra:

```txt
CALLS
TOTAL_US
AVG_US
MAX_US
OVER_100US
OVER_500US
OVER_1MS
OVER_5MS
OVER_10MS
```

Bloques principales:

```txt
RTU
SERIAL
WORKLOAD
DISPLAY
READY
```

Subprocesos del realistic workload:

```txt
IO
BUTTONS
SPI
FRAM
RTC
SD_APPEND
SD_VERIFY
```

## Caja negra del peor service gap

Cuando aparece un nuevo máximo de tiempo entre dos entradas consecutivas a
`serviceRtuMaster()`, H3E.0 captura el contexto del intervalo anterior.

```txt
H3E_WORST_GAP_US
H3E_WORST_ACCOUNTED_US
H3E_WORST_UNACCOUNTED_US

H3E_WORST_PREV_VISIBLE_AFTER_RTU_US
H3E_WORST_CURRENT_PRE_RTU_US

H3E_WORST_RTU_US
H3E_WORST_SERIAL_US
H3E_WORST_WORKLOAD_US
H3E_WORST_DISPLAY_US
H3E_WORST_READY_US

H3E_WORST_WORKLOAD_KIND
H3E_WORST_WORKLOAD_ITEM_US
```

El primer intervalo después de cada reset estadístico se descarta porque no
existe contexto completo del loop anterior.

## Significado de ACCOUNTED / UNACCOUNTED

`ACCOUNTED_US` mide el tiempo visible desde la entrada RTU anterior hasta la
salida del loop anterior, más el pequeño tramo desde la entrada al loop actual
hasta la nueva llamada RTU.

`UNACCOUNTED_US` es:

```txt
SERVICE_GAP_US - ACCOUNTED_US
```

Por construcción, ese residuo apunta a tiempo consumido fuera del cuerpo
visible del sketch: servicios automáticos del package-core, tareas FreeRTOS,
drivers u otras preempciones.

No prueba por sí solo qué componente del core fue responsable. Si el residuo es
grande, H3E.0B deberá instrumentar el package-core.

## Interpretación

Caso A:

```txt
WORST_GAP_US=20000
WORST_WORKLOAD_KIND=SD_APPEND
WORST_WORKLOAD_ITEM_US=18000
WORST_UNACCOUNTED_US=1000
```

Prioridad: microSD.

Caso B:

```txt
WORST_GAP_US=20000
WORST_WORKLOAD_ITEM_US=1000
WORST_UNACCOUNTED_US=18000
```

No conviene tocar periféricos del sketch. Siguiente gate: profiler del core.

## Criterio del gate

La instrumentación debe existir y la ventana debe seguir limpia:

```txt
RTU_FAILED=0
RTU_TIMEOUTS=0
REQUEST_PATH_GAP=0
RESPONSE_PATH_GAP=0
REQUEST_BYTE_GAP=0
RESPONSE_BYTE_GAP=0
MASTER_CRC=0
SLAVE_CRC=0
TCP_TARGET_PCT >= 99
SD_CLEAN=YES
PERIPHERAL_FAILURE_COUNT=0
PROFILER_PASS=YES
```

No se exige un valor específico de `WORST_UNACCOUNTED_US`; ese es el resultado
diagnóstico que decidirá qué optimizar después.
