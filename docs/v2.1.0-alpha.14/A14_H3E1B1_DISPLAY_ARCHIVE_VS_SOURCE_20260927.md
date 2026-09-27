# Alpha14 — H3E.1B.1 — Display archive vs source A/B

Fecha: 2026-09-27

## Objetivo

Determinar si la diferencia observada entre:

- H3E.0B: SYS_DISPLAY ~19 ms promedio;
- H3E.1: SYS_DISPLAY ~8.3 ms promedio;

se debe a que el archive precompilado de JWPLC_Display está desalineado respecto
al source actual.

No se modifica la API ni el algoritmo de render durante este gate.

## Diseño A/B

Se usa el Master H3E.0B ya validado para que la telemetría Display provenga del
core, no del profiler interno H3E.1.

Ambos legs usan:

```txt
TFT SPI = 80 MHz
RTU = 500000 / APB_FORCED
Master FIFO = 9
Slave FIFO = 8
RX = BULK
TX = QUEUED
Master framing = GAP
Slave framing = STRUCTURAL
CRC = BITWISE
TCP = 500 req/s
Display telemetry = 100 ms
Full runtime = activo
```

La única variable del Master es el linkage de JWPLC_Display.

### Leg A — ARCHIVE

```txt
libJWPLC_Display.a presente
Display source .o = 0
```

Marker requerido:

```txt
DISPLAY_LINKAGE_PROOF=ARCHIVE_NO_SOURCE_OBJECTS
```

### Leg B — SOURCE

```txt
backup libJWPLC_Display.a
hide libJWPLC_Display.a
compile source actual
restore archive exacto
```

Marker requerido:

```txt
DISPLAY_LINKAGE_PROOF=SOURCE_OBJECTS_PRESENT
```

## Duración

El runner histórico H3E.0B exige >=300 s.

Por tanto:

```txt
Leg A = 300 s
Leg B = 300 s
```

No se reduce la ventana para mantener comparabilidad con H3E.0B/H3E.1.

## Métricas principales

La comparación se centrará en:

```txt
SYS_DISPLAY AVG_US
SYS_DISPLAY MAX_US
RTU_HZ
TCP_AVG_US
TCP_P99_US
TCP_MAX_US
RTU_SERVICE_GAP_MAX_US
LOOP_GAP_MAX_US
```

## Control visual

La HMI del Master debe conservar exactamente el layout ya observado:

```txt
Rol MASTER
TCP OK <contador>
RTU OK <contador>
RTU FAIL 0
SD OK
ETH UP
```

Durante el A/B sólo TCP OK y RTU OK deben cambiar continuamente.

El usuario confirmará:

```txt
misma HMI de referencia
sin cortes
sin parpadeo anómalo
```

## Protección de artefactos

Hashes esperados:

```txt
core.a
4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566

libJWPLC_ModbusRTU.a
444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F

libJWPLC_Display.a
2974D42C847C1B7C7AB3A7B74DA42E2F17969FB852B47A8D434F57F70DA924AF
```

Los tres deben quedar exactamente iguales después de cada leg.

## Secuencia

Por política de un gate por vez:

1. preflight Leg A / ARCHIVE;
2. ejecutar Leg A / ARCHIVE;
3. revisar y registrar;
4. preflight Leg B / SOURCE;
5. ejecutar Leg B / SOURCE;
6. comparar resultados;
7. decidir si el archive Display debe regenerarse antes de H3E.1C.

## Interpretación

Si source es claramente más rápido con la misma HMI y carga:

```txt
archive precompilado necesita regeneración/calificación
```

Si son equivalentes:

```txt
la diferencia H3E.0B vs H3E.1 proviene de otra variable
```

Si archive es más rápido:

```txt
investigar overhead introducido en source actual antes de regenerar archive
```

No se modifica TFT_eSPI/JW_TFT durante H3E.1B.1.


## Resultado físico — Leg A / ARCHIVE

El Leg A fue ejecutado con:

```txt
DISPLAY_LINKAGE=ARCHIVE
TFT_SPI_HZ=80000000
DURATION_S=300
TCP=500 req/s
RTU=500000 baud
CRC=BITWISE
```

Prueba de linkage:

```txt
DISPLAY_ARCHIVE_HIDDEN=NO
MASTER_DISPLAY_OBJECT JWPLC_Display.cpp.o=0
MASTER_DISPLAY_OBJECT JWPLC_UI.cpp.o=0
MASTER_DISPLAY_OBJECT JWPLC_UI_API.cpp.o=0
MASTER_DISPLAY_OBJECT JWPLC_UI_Pages.cpp.o=0
MASTER_DISPLAY_SOURCE_OBJECT_COUNT=0
DISPLAY_LINKAGE_PROOF=ARCHIVE_NO_SOURCE_OBJECTS
```

Por tanto este leg utilizó inequívocamente `libJWPLC_Display.a`.

### Display

```txt
SYS_DISPLAY_CALLS=2953
SYS_DISPLAY_TOTAL_US=56252937
SYS_DISPLAY_AVG_US=19049
SYS_DISPLAY_MAX_US=23629
```

El peor gap mostró:

```txt
WORST_OUTSIDE_US=24538
WORST_TASK_YIELD_US=23699
WORST_SYS_ACTIVE_US=23674
WORST_SYS_DISPLAY_US=23629
DOMINANT_DIRECT=TASK_YIELD
DOMINANT_SYSTEM=SYS_DISPLAY
```

Esto reproduce el patrón lento original de H3E.0B.

### TCP

```txt
TCP_REQ_S=499.988
TCP_TARGET_PCT=99.998
TCP_AVG_US=1429.5
TCP_P95_US=2042.2
TCP_P99_US=19719.0
TCP_MAX_US=35451.9
```

### RTU

```txt
RTU_HZ=756.558
RTU_STARTED=227043
RTU_COMPLETED=227043
RTU_SUCCESS=227042
RTU_FAILED=1
RTU_TIMEOUTS=1
REQUEST_PATH_GAP=0
RESPONSE_PATH_GAP=1
RESPONSE_BYTE_GAP=9
MASTER_CRC=0
SLAVE_CRC=0
```

El único fallo corresponde a una respuesta FC03 Q2 completa de 9 bytes no
recibida/procesada por el Master dentro de la ventana de timeout.

```txt
LAST_FAILURE_DURATION_US=32334
MAX_FAILURE_DURATION_US=32334
RTU_SERVICE_GAP_MAX_US=24600
LOOP_GAP_MAX_US=24598
```

El Slave reportó todas las requests recibidas y todas las responses
transmitidas, sin tails descartadas ni errores CRC.

### Runtime

```txt
PROFILER_PASS=YES
RTU_FLOOR_PASS=YES
TCP_CLEAN=YES
TCP_TARGET_PASS=YES
BUCKET_TARGET_PASS=YES
SD_CLEAN=YES
RTU_CLEAN=NO
RUNTIME_CLEAN=NO
DIAGNOSTIC_CAPTURE_PASS=YES
```

El runner cerró:

```txt
A14_RTU_H3E0B=PASS_CORE_ATTRIBUTION_WITH_RTU_FAILURE
H3E1B1_RUNNER_EXIT=0
```

La observación física fue PASS para Master y Slave, con la misma HMI de
referencia y sin parpadeo/cortes visibles.

El leg completo cerró:

```txt
H3E1B1_DISPLAY_LINKAGE_FINAL=ARCHIVE
A14_H3E1B1_DISPLAY_LINKAGE_LEG_GATE=PASS
```

### Comparación provisional contra H3E.1 source

La corrida H3E.1 previa, que forzó source actual de `JWPLC_Display`, obtuvo:

```txt
SYS_DISPLAY_AVG_US ~= 8329
SYS_DISPLAY_MAX_US ~= 9611
```

Frente al archive:

```txt
SYS_DISPLAY_AVG_US = 19049
SYS_DISPLAY_MAX_US = 23629
```

Esto equivale provisionalmente a:

```txt
source / archive AVG ratio ~= 0.4372
source AVG improvement ~= 56.276 %
archive AVG ~= 2.287x source

source / archive MAX ratio ~= 0.4067
source MAX improvement ~= 59.325 %
archive MAX ~= 2.459x source
```

Esta comparación todavía NO cierra la decisión porque H3E.1 incluía
instrumentación interna Display adicional. El Leg B / SOURCE debe ejecutarse
con el mismo Master H3E.0B y el mismo gate para obtener el A/B controlado.

## Estado H3E.1B.1

```txt
LEG_A_ARCHIVE=PASS_DIAGNOSTIC_VALID
LEG_B_SOURCE=PENDING
A_B_DECISION=PENDING
```
