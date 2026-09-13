# Alpha14 — Implementación productiva de refresh L2: compile gate

Fecha: `2026-09-13`

## Objetivo

Introducir en `JWPLC_Ethernet` una mitigación productiva mínima para el problema de reacceso TCP tras largos periodos sin tráfico Ethernet, manteniendo compatibilidad de API y sin modificar el comportamiento funcional salvo por un refresh L2 best-effort cada 120 s.

La mitigación usa un datagrama UDP de 4 bytes dirigido al broadcast calculado de la subred local. No depende del gateway y usa un socket local efímero.

## Cambios fuente locales

Se modificaron únicamente:

```text
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.h
```

Configuración introducida:

```text
JWPLC_ETH_L2_REFRESH_PERIOD_MS=120000
JWPLC_ETH_L2_REFRESH_UDP_PORT=9
L2_REFRESH_PAYLOAD_BYTES=4
L2_REFRESH_TARGET=DIRECTED_BROADCAST
L2_REFRESH_FAILURE_POLICY=BEST_EFFORT_NON_FATAL
```

Detalles de diseño:

- el refresh se ejecuta únicamente con Ethernet en estado `READY` y link `ON`;
- reutiliza la ventana de ownership del mutex SPI ya existente en `JWPLC_Ethernet::service()`;
- no se ejecuta mientras hay mantenimiento DHCP cooperativo en progreso;
- `0` en `JWPLC_ETH_L2_REFRESH_PERIOD_MS` desactiva el mecanismo en compilación;
- el helper marca el intento antes de transmitir para evitar retries cerrados ante fallo;
- una falta de socket libre o fallo de envío no degrada `READY` ni sustituye el diagnóstico de red vigente;
- se calcula el broadcast como `local | ~subnet`;
- se rechazan configuraciones inválidas `/0`, `/32`, `0.0.0.0` y `255.255.255.255`;
- `udp.begin(0)` usa puerto local efímero;
- el payload es `JWL2`.

## Validación de parche

```text
HEADER_MACRO_ANCHOR_MATCHES=1
HEADER_FIELD_ANCHOR_MATCHES=1
HEADER_METHOD_ANCHOR_MATCHES=1
CTOR_ANCHOR_MATCHES=1
SERVICE_READY_ANCHOR_MATCHES=1
FINISH_READY_ANCHOR_MATCHES=1
HELPER_INSERT_ANCHOR_MATCHES=1
PERIOD_MACRO_COUNT=1
PRIVATE_FIELD_COUNT=1
PRIVATE_METHOD_COUNT=1
SERVICE_CALL_COUNT=1
HELPER_IMPL_COUNT=1
BCAST_PAYLOAD_COUNT=1
EPHEMERAL_SOCKET_COUNT=1
PRODUCTION_PATCH=PASS
SOURCE_SCOPE=PASS
DIFF_CHECK=PASS
```

Los avisos de Git sobre conversión futura `LF -> CRLF` corresponden a política de line endings del worktree y no fueron warnings de compilación. `git diff --check` pasó sin errores.

## Compile gates

### Empty sketch

```text
EMPTY_EXIT=0
EMPTY_WARNINGS=0
EMPTY=PASS_COMPILE
FLASH_BYTES=397401
GLOBAL_BYTES=27732
```

### FULL_RUNTIME_REALISTIC

```text
FULL_RUNTIME_EXIT=0
FULL_RUNTIME_WARNINGS=0
FULL_RUNTIME=PASS_COMPILE
FLASH_BYTES=428097
GLOBAL_BYTES=29804
```

## Estado

```text
PRODUCTION_L2_SOURCE_IMPLEMENTED=YES_UNCOMMITTED
A14_3_PRODUCTION_L2_REFRESH_COMPILE=PASS
PHYSICAL_PRODUCTION_MITIGATION=NOT_TESTED_YET
SOURCE_COMMIT=NOT_YET
FINAL_1000RPS_60S=PENDING_MANDATORY
FINAL_1000RPS_30MIN=PENDING_MANDATORY
```

La implementación fuente queda deliberadamente sin commit hasta completar un smoke físico del código productivo. Si el smoke pasa, se podrá consolidar el commit fuente y ejecutar el aging de 480 s usando la implementación real, no el helper temporal del harness.

## Siguiente gate

```text
NEXT=A14.3_PRODUCTION_L2_REFRESH_PHYSICAL_SMOKE
```
