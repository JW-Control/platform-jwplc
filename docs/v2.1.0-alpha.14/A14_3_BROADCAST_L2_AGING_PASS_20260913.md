# Alpha14 — A14.3 refresh L2 por broadcast dirigido durante aging

Fecha: `2026-09-13`

## Objetivo

Validar si una transmisión saliente mínima y periódica del JWPLC hacia el broadcast dirigido de su propia subred evita la latencia de reaceptación TCP observada después de varios minutos de inactividad Ethernet.

La prueba usa el mismo `FULL_RUNTIME_REALISTIC` y mantiene activos los periféricos normales. No se modifica código productivo: el helper de broadcast existe sólo en un sketch temporal de diagnóstico.

## Condiciones

```text
AGING_S=480
BCAST_PERIOD_S=120
BCAST_EXPECTED=3
PC_ETHERNET_TRAFFIC_DURING_AGING=NONE
TARGET_BROADCAST=192.168.0.255
PAYLOAD_BYTES=4
```

Antes del aging:

```text
INITIAL_FULL_RUNTIME_READY=YES
INITIAL_SERVER_READY=YES
INITIAL_FRAM_READY=YES
INITIAL_SD_READY=YES
INITIAL_RTC_PRESENT=YES
INITIAL_IO_INITIALIZED=YES
INITIAL_BUTTONS_READY=YES
INITIAL_PERIPHERAL_FAILURE_COUNT=0
WARM_TCP=PASS
WARM_FC03=PASS
WARM_CONNECT_MS=2.805
```

La entrada neighbor inicial del PC correspondía al JWPLC:

```text
NEIGHBOR_MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE=5
NEIGHBOR_IFINDEX=15
```

## Refreshes salientes

Los tres broadcast dirigidos se enviaron correctamente:

```text
BCAST_1_SEND_US=607
BCAST_2_SEND_US=1033
BCAST_3_SEND_US=608
BCAST_PASS=3/3
```

En los tres casos:

```text
A14_BCAST_LOCAL_IP=192.168.0.31
A14_BCAST_SUBNET=255.255.255.0
A14_BCAST_TARGET=192.168.0.255
A14_BCAST_NETWORK_VALID=YES
A14_BCAST_UDP_BEGIN=PASS
A14_BCAST_PACKET_BEGIN=PASS
A14_BCAST_WRITE=PASS
A14_BCAST_SEND=PASS
```

La entrada neighbor del PC permaneció en estado stale durante el aging y no fue forzada ni eliminada:

```text
NEIGHBOR_MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE=4
```

## Salud del runtime durante los 480 s

En todos los snapshots de 60 s a 420 s:

```text
AGING_FULL_RUNTIME_READY=YES
AGING_SERVER_READY=YES
AGING_PERIPHERAL_FAILURE_COUNT=0
```

Al finalizar:

```text
AGED_FULL_RUNTIME_READY=YES
AGED_SERVER_READY=YES
AGED_PERIPHERAL_FAILURE_COUNT=0
```

## Primer TCP después del aging

Sin ping, sin borrado ARP y sin tráfico TCP previo desde el PC:

```text
FIRST_TCP_CONNECTED=YES
FIRST_TCP_CONNECT_MS=10.951
FIRST_FC03_PASS=YES
```

Después del primer TCP la entrada neighbor pasó naturalmente a reachable:

```text
NEIGHBOR_MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE=5
```

Dos conexiones adicionales también pasaron:

```text
FOLLOWUP_TID=2
FOLLOWUP_TCP=PASS
FOLLOWUP_FC03=PASS
FOLLOWUP_CONNECT_MS=21.364

FOLLOWUP_TID=3
FOLLOWUP_TCP=PASS
FOLLOWUP_FC03=PASS
FOLLOWUP_CONNECT_MS=14.009
```

## Resultado

```text
A14_3_BROADCAST_L2_AGING=PASS_PREVENTED_REACCEPT_OUTAGE
PRODUCTION_MITIGATION_CANDIDATE=DIRECTED_BROADCAST_120S
FULL_RUNTIME_READY=YES
SERVER_READY=YES
PERIPHERAL_FAILURE_COUNT=0
```

## Interpretación

El fallo de reaceptación observado anteriormente tras 480 s de inactividad no reapareció cuando el JWPLC emitió un broadcast dirigido mínimo cada 120 s. El PC conservó su neighbor en estado stale durante el aging, por lo que el resultado no depende de refrescar deliberadamente la entrada ARP/neighbor del host.

La evidencia acumulada es consistente con aging del camino L2/FDB/unknown-unicast de la red, no con DHCP, Modbus TCP ni una pérdida lógica del listener del W5500.

Este gate valida el mecanismo como candidato de mitigación, pero todavía no autoriza cerrar A14.3. Debe implementarse de forma mínima dentro de `JWPLC_Ethernet`, respetando el mutex SPI, sin romper APIs ni readiness, y después ejecutar regresiones funcionales y de rendimiento.

## Pendientes obligatorios posteriores

```text
PRODUCTION_L2_MITIGATION=NOT_IMPLEMENTED_YET
FINAL_FULL_RUNTIME_1000RPS_60S=PENDING_MANDATORY
FINAL_FULL_RUNTIME_1000RPS_30MIN=PENDING_MANDATORY
LONG_RUN_1000RPS=ON_HOLD
```
