# Alpha14 — A14.3 smoke físico del refresh L2 productivo

Fecha: `2026-09-13`

## Objetivo

Validar físicamente la implementación productiva del refresh L2 periódico integrada en `JWPLC_Ethernet`, antes de consolidar el cambio en source y antes del aging definitivo de 480 s.

La prueba usa el harness `FULL_RUNTIME_REALISTIC` y el código fuente local real de `JWPLC_Ethernet`, sin helper temporal adicional.

## Scope source

Antes y después del gate se conservaron exactamente dos archivos modificados:

```text
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.h
```

Los marcadores productivos esperados estuvieron presentes:

```text
PERIOD_MACRO_PRESENT=TRUE
FIELD_PRESENT=TRUE
SERVICE_CALL_PRESENT=TRUE
HELPER_PRESENT=TRUE
EPHEMERAL_PRESENT=TRUE
PAYLOAD_PRESENT=TRUE
PRODUCTION_SOURCE_MARKERS=PASS
```

## Compile + upload

```text
BUILD_UPLOAD_EXIT=0
WARNING_COUNT=0
FLASH_BYTES=428097
GLOBAL_VARIABLE_BYTES=29804
```

La subida física a COM14 terminó correctamente.

## Runtime inicial

```text
INITIAL_RUNTIME_READY=PASS
INITIAL_PERIPHERAL_FAILURE_COUNT=0
INITIAL_FC03=PASS
INITIAL_CONNECT_MS=11.335
```

## Actividad periférica durante 30 s

```text
DISPLAY_FRAMES_DELTA=298
FRAM_CYCLES_DELTA=119
SD_APPEND_CYCLES_DELTA=29
RTC_SAMPLES_DELTA=119
IO_SAMPLES_DELTA=1458
BUTTON_SAMPLES_DELTA=1458
SPI_PROBE_SAMPLES_DELTA=296
PERIPHERAL_ACTIVITY=PASS
FINAL_PERIPHERAL_FAILURE_COUNT=0
```

Todos los periféricos continuaron activos y sin fallos durante la ventana de smoke.

## Regresión Modbus TCP

Tres conexiones adicionales FC03 después de la ventana de runtime:

```text
TID=2 CONNECT_MS=0.563 FC03=PASS
TID=3 CONNECT_MS=0.807 FC03=PASS
TID=4 CONNECT_MS=23.477 FC03=PASS
FINAL_TCP_PASS=3/3
FINAL_RUNTIME_READY=PASS
```

## Clasificación

```text
A14_3_PRODUCTION_L2_PHYSICAL_SMOKE=PASS_PHYSICAL
PRODUCTION_L2_REFRESH_PERIOD_MS=120000
SOURCE_SCOPE_FINAL=PASS
SOURCE_COMMIT=NOT_YET
```

Este smoke valida que la implementación productiva real no introduce una regresión inmediata en Ethernet, Modbus TCP ni en los periféricos integrados.

No demuestra todavía que el refresh productivo se ejecute a los 120 s ni que prevenga el outage después de 480 s, porque la ventana física de este smoke fue de 30 s.

## Siguiente paso

1. consolidar exclusivamente `JWPLC_Ethernet.cpp` y `JWPLC_Ethernet.h`;
2. repetir aging de 480 s usando la implementación productiva real, sin helper temporal y sin reflashear si el binario no cambia;
3. mantener pendientes obligatorios:

```text
FINAL_FULL_RUNTIME_1000RPS_60S=PENDING_MANDATORY
FINAL_FULL_RUNTIME_1000RPS_30MIN=PENDING_MANDATORY
```
