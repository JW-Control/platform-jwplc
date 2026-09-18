# Alpha14.3 — Precondición física FULL_RUNTIME_REALISTIC en IP estática

Fecha: 2026-09-13

## Objetivo

Validar que el perfil `FULL_RUNTIME_REALISTIC` puede pasar de la configuración DHCP obtenida durante el arranque a una configuración estática equivalente, sin degradar periféricos ni el servidor Modbus TCP, antes de ejecutar el A/B de aging de 480 s.

## Alcance

- Rama: `v2.1.0-alpha.14/feature/modbus-tcp`
- Harness base: `tools/modbus-tcp-benchmark/firmware/a14_perf_full_runtime_realistic/a14_perf_full_runtime_realistic.ino`
- Modificación únicamente temporal en copia bajo `%TEMP%`.
- Sin cambios de fuentes de librerías.
- Sin cambios al harness versionado.
- Sin override de librerías precompiladas.

## Resultado de build

- `BUILD_UPLOAD_EXIT=0`
- `COMPILE_WARNINGS=0`
- Flash: 430757 bytes (10 %)
- RAM global: 29796 bytes (9 %)

## Resultado físico

La precondición llegó inmediatamente a estado sano:

```text
MODE=STATIC
ETH_READY=YES
IP=192.168.0.31
SERVER_READY=YES
FULL_RUNTIME_READY=YES
FRAM_READY=YES
SD_READY=YES
RTC_PRESENT=YES
IO_INITIALIZED=YES
BUTTONS_READY=YES
PERIPHERAL_FAILURE_COUNT=0
```

La prueba FC03 sobre el servidor Modbus TCP también pasó:

```text
FC03_STATIC_PRECONDITION=PASS
```

Estado final:

```text
FINAL_ETH_MODE=STATIC
FINAL_ETH_READY=YES
FINAL_ETH_IP=192.168.0.31
FINAL_ETH_DIAG=---
FINAL_PERIPHERAL_FAILURE_COUNT=0
A14_3_STATIC_PRECONDITION=PASS_PHYSICAL
```

## Conclusión

La ruta de conversión DHCP -> STATIC equivalente queda validada físicamente para el experimento A/B de aging. La configuración estática no rompe el runtime integrado ni el servidor Modbus TCP.

El firmware actualmente cargado queda apto para el siguiente gate y debe conservarse sin reflashear ni reiniciar:

```text
NEXT_GATE=A14.3.STATIC_FULL_RUNTIME_AGED_REACCEPT_480S
FIRMWARE_READY_FOR_AGING=YES
LONG_RUN=ON_HOLD
```

Este gate no concluye todavía si DHCP participa o no en el fallo de reaccept; únicamente valida la precondición STATIC para realizar la comparación controlada.
