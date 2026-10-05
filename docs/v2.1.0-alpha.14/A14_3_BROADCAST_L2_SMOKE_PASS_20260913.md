# Alpha14 — Smoke físico de refresh L2 por broadcast dirigido

Fecha: `2026-09-13`

## Objetivo

Validar una alternativa de refresh L2 que no dependa de disponer de gateway/router válido: emitir una trama UDP mínima al broadcast dirigido de la subred calculado a partir de `localIP` y `subnetMask`.

La intención es refrescar el aprendizaje de la MAC origen del JWPLC en la infraestructura L2 sin tocar la tabla neighbor del PC y sin reservar un socket permanente.

## Condiciones

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
FQBN=jwplc_local:esp32:jwplcbasic
PLATFORM=jwplc_local:esp32 2.1.0-dev
REFLASH=YES_TEMP_HARNESS
PRODUCTION_SOURCE_CHANGES=0
ORIGINAL_HARNESS_MODIFIED=NO
```

El helper temporal usa `EthernetUDP`, puerto local `0` (efímero) y un payload de 4 bytes.

## Build / upload

```text
BUILD_UPLOAD_EXIT=0
WARNING_COUNT=0
SKETCH_FLASH_BYTES=428833
GLOBAL_RAM_BYTES=29796
```

## Precondición física

```text
RUNTIME_PRECONDITION=PASS
WARM_FC03=PASS
```

## Broadcast calculado

```text
A14_BCAST_LOCAL_IP=192.168.0.31
A14_BCAST_SUBNET=255.255.255.0
A14_BCAST_TARGET=192.168.0.255
A14_BCAST_NETWORK_VALID=YES
A14_BCAST_TARGET_EXPECTED=PASS
```

## Envío

```text
A14_BCAST_UDP_BEGIN=PASS
A14_BCAST_PACKET_BEGIN=PASS
A14_BCAST_WRITE=PASS
A14_BCAST_SEND=PASS
A14_BCAST_SEND_US=507
BCAST_COMMAND_MS=28.386
BCAST_SEND_RESULT=PASS
```

## Regresión inmediata

```text
POST_BCAST_FC03=PASS
FINAL_FULL_RUNTIME_READY=YES
FINAL_PERIPHERAL_FAILURE_COUNT=0
```

## Clasificación

```text
A14_3_BROADCAST_L2_SMOKE=PASS_PHYSICAL
```

La alternativa por broadcast dirigido compila, transmite correctamente y no introduce una regresión funcional inmediata en Modbus TCP ni en el runtime integrado.

## Interpretación

Este smoke no demuestra todavía que el broadcast dirigido prevenga el fallo de reaccept tras envejecimiento. Sólo valida que el mecanismo propuesto es ejecutable y compatible con el runtime normal.

La siguiente prueba obligatoria es un aging de 480 s con refresh por broadcast cada 120 s, sin TCP ni ping desde el PC durante la ventana, seguido del primer TCP/FC03 al finalizar.

## Pendientes relacionados

```text
A14_3_BROADCAST_L2_AGING_480S=PENDING
FINAL_FULL_RUNTIME_1000RPS_60S=PENDING_MANDATORY
FINAL_FULL_RUNTIME_1000RPS_30MIN=PENDING_MANDATORY
LONG_RUN_1000RPS=ON_HOLD
```
