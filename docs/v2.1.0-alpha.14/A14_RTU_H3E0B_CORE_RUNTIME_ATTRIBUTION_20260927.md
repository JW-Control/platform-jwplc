# Alpha14 — RTU-H3E.0B — Core Runtime Attribution

Fecha: 2026-09-27

## Objetivo

Identificar la causa de los service gaps de aproximadamente 20 ms observados en H3E.0
sin volver a introducir el profiler invasivo dentro del sketch.

H3E.0B mide específicamente el tiempo que transcurre desde que `loop()` retorna
hasta que el core vuelve a entrar al siguiente `loop()`.

## Motivación

H3E.0 obtuvo:

```txt
RTU_SERVICE_GAP_MAX_US=20659
H3E_WORST_ACCOUNTED_US=81
H3E_WORST_UNACCOUNTED_US=20578
H3E_WORST_UNACCOUNTED_PCT=99.608
```

Por lo tanto el cuello principal no está en los bloques visibles del sketch.

## Tramo instrumentado

El core JWPLC ejecuta:

```txt
jwplcModbusTCPLoopServiceCallback()   <- TCP_PRE
loop()
jwplcModbusTCPLoopServiceCallback()   <- TCP_POST
serialEventRun()                      <- SERIAL_EVENT
taskYIELD()                           <- TASK_YIELD
```

H3E.0B registra:

```txt
CALLS
TOTAL_US
AVG_US
MAX_US
```

para:

```txt
TCP_PRE
TCP_POST
SERIAL_EVENT
TASK_YIELD
OUTSIDE_TOTAL
```

## System task

`jwplcSystemTask` corre en el mismo core que `loopTask` con prioridad superior.

Se perfilan:

```txt
SYS_ACTIVE
SYS_IO
SYS_RTC
SYS_ETH
SYS_DATALOG
SYS_DISPLAY
```

## Caja negra del peor intervalo

Cuando aparece un nuevo peor intervalo fuera de `loop()`, se guardan:

```txt
WORST_OUTSIDE_US
WORST_TCP_PRE_US
WORST_TCP_POST_US
WORST_SERIAL_EVENT_US
WORST_TASK_YIELD_US
WORST_DIRECT_SUM_US
WORST_RESIDUAL_US

WORST_SYS_ACTIVE_DELTA_US
WORST_SYS_IO_DELTA_US
WORST_SYS_RTC_DELTA_US
WORST_SYS_ETH_DELTA_US
WORST_SYS_DATALOG_DELTA_US
WORST_SYS_DISPLAY_DELTA_US
```

El runner calcula además:

```txt
DOMINANT_DIRECT
DOMINANT_DIRECT_US
DOMINANT_DIRECT_PCT

DOMINANT_SYSTEM
DOMINANT_SYSTEM_US
SYSTEM_ACTIVE_PCT
```

## Instrumentación ligera

A diferencia de H3E.0:

- no se generan histogramas por vuelta;
- no se cronometra cada función del sketch;
- sólo se usan count/total/max;
- el profiler está deshabilitado por default;
- el sketch de qualification lo activa mediante un hook fuerte.

Reset, snapshot y captura de un nuevo récord se protegen contra preempción del
`jwplcSystemTask`.

## Protección del core precompilado

El Master H3E.0B se compila temporalmente desde:

```txt
JWPLC/2.1.0/cores/jwcontrol
```

mediante un override temporal de `boards.local.txt`.

El archive candidato local:

```txt
JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
```

NO se reemplaza para la qualification.

El gate comprueba el SHA del archive antes y después de compilar/subir el Master.

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
DURATION=300 s
FULL_RUNTIME=ACTIVE
```

## Política de resultado

El gate distingue:

```txt
PASS_CORE_ATTRIBUTION_CLEAN
PASS_CORE_ATTRIBUTION_WITH_RTU_FAILURE
REVIEW_CORE_ATTRIBUTION_INVALID
```

Los timeouts RTU no invalidan automáticamente la captura diagnóstica. Si TCP,
SD, periféricos, perfil y telemetría core son válidos, se conserva el resultado
para identificar qué runtime ocupó los ~20 ms.

## Decisión posterior

No se optimiza ninguna tarea hasta ver la atribución.

Ejemplos:

```txt
DOMINANT_DIRECT=TCP_POST
-> atacar servicio Modbus TCP

DOMINANT_DIRECT=TASK_YIELD
DOMINANT_SYSTEM=SYS_DATALOG
-> atacar DataLog/system task

DOMINANT_DIRECT=TASK_YIELD
SYS_ACTIVE bajo
-> investigar scheduler/otras tareas

DOMINANT_DIRECT=RESIDUAL
-> ampliar profiler en el core sólo alrededor del tramo residual
```


## Incidencia de setup 1

El primer intento físico no llegó a upload ni a la ventana de 300 s.

Fallo:

```txt
a14_h3e0b_core_profiler_master.ino:499:62:
error: 'SD_RECORD_BYTES' was not declared in this scope
H3E0B_SETUP_MASTER_COMPILE_FAILED
```

Causa:

El hook fuerte `jwplcH3E0BProfilerEnabled()` se había definido inmediatamente
después de los includes del sketch. Esa nueva primera función desplazó el punto
donde el preprocesador Arduino inserta prototipos automáticos y provocó que el
prototipo de `buildSdRecord(... record[SD_RECORD_BYTES])` apareciera antes de
la declaración global de `SD_RECORD_BYTES`.

Corrección:

- mover el hook H3E.0B al bloque de funciones, después de constantes, tipos y
  estado global;
- conservar la firma original de `buildSdRecord`;
- retirar `--verbose` del compile source-core;
- limitar el volcado de logs en caso de error.

La incidencia no cambia el objetivo ni la hipótesis H3E.0B.
