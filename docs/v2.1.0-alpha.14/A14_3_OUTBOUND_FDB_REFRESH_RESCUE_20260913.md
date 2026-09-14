# Alpha14 — A14.3 refresh L2/FDB saliente tras 480 s

Fecha: `2026-09-13`

## Objetivo

Distinguir entre un problema del neighbor/ARP local de Windows y un problema de aging del forwarding L2/FDB de la infraestructura de red.

La prueba parte del perfil `FULL_RUNTIME_REALISTIC` con Ethernet en modo STATIC equivalente a la configuración obtenida previamente por DHCP. Tras `480 s` sin tráfico Ethernet desde el PC hacia el JWPLC, el JWPLC transmite una única trama UDP hacia el gateway `192.168.0.1` por orden Serial. El PC conserva intacta su entrada neighbor para `192.168.0.31`.

## Preparación

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
WORKTREE_BEFORE=PASS
REFLASH=YES_TEMP_HARNESS
ORIGINAL_HARNESS_MODIFIED=NO
LIBRARY_SOURCE_CHANGES=0
PRECOMPILED_OVERRIDE_REQUIRED=NO
BUILD_UPLOAD_EXIT=0
COMPILE_WARNINGS=0
```

El harness temporal añadió únicamente:

- configuración STATIC equivalente ya validada;
- telemetría Ethernet en snapshots;
- comando Serial `U`;
- una transmisión UDP única hacia el gateway para provocar actividad L2 saliente del JWPLC.

No hubo cambios en código productivo ni en librerías.

## Precondición

```text
INITIAL_PRECONDITION=PASS
WARM_TCP=PASS
WARM_FC03=PASS
NEIGHBOR_MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE=5
```

## Aging

Durante `480 s` no hubo tráfico Ethernet de prueba. Sólo se consultaron snapshots por USB/Serial.

En todo el periodo:

```text
AGING_ETH_READY=YES
AGING_SERVER_READY=YES
AGING_FULL_RUNTIME_READY=YES
AGING_PERIPHERAL_FAILURE_COUNT=0
```

Al terminar:

```text
AGED_ETH_READY=YES
AGED_SERVER_READY=YES
AGED_FULL_RUNTIME_READY=YES
AGED_PERIPHERAL_FAILURE_COUNT=0
NEIGHBOR_MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE=4
```

La entrada neighbor del PC seguía presente con la MAC correcta, pero en estado envejecido.

## Refresh saliente del JWPLC

Sin ejecutar ping, TCP ni borrar el neighbor del PC, se ordenó al JWPLC una única transmisión UDP al gateway:

```text
A14_L2_REFRESH_GATEWAY=192.168.0.1
A14_L2_REFRESH_UDP_BEGIN=PASS
A14_L2_REFRESH_PACKET_BEGIN=PASS
A14_L2_REFRESH_WRITE=PASS
A14_L2_REFRESH_SEND=PASS
A14_L2_REFRESH_US=786
JWPLC_REFRESH_RESULT=PASS
```

Después del envío y antes de iniciar TCP, el neighbor del PC permaneció sin cambios visibles:

```text
NEIGHBOR_MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE=4
```

## Primer TCP posterior

```text
FIRST_TCP_AFTER_JWPLC_REFRESH=PASS
FIRST_TCP_CONNECT_MS=19.882
FIRST_FC03=PASS
```

Después de la conexión:

```text
NEIGHBOR_MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE=5
```

Estado final:

```text
JWPLC_REFRESH_SEND=PASS
FIRST_TCP_CONNECTED=YES
FIRST_FC03_PASS=YES
FINAL_ETH_READY=YES
FINAL_PERIPHERAL_FAILURE_COUNT=0
```

## Clasificación

```text
A14_3_OUTBOUND_FDB_REFRESH=PASS_RESCUED_WITHOUT_HOST_ARP_DELETE
ROOT_LAYER_SUSPECT=NETWORK_FDB_OR_UNKNOWN_UNICAST_AGING
A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED
LONG_RUN=ON_HOLD
```

## Interpretación

La comunicación se recuperó sin modificar la entrada ARP/neighbor del PC. La única acción previa al primer TCP fue una transmisión saliente del JWPLC hacia el gateway, suficiente para que la infraestructura de red volviera a observar la MAC origen del JWPLC.

Esto refuerza la hipótesis de aging del forwarding L2/FDB o de tratamiento temporal del tráfico unknown-unicast tras inactividad, y reduce la probabilidad de que la causa raíz esté en Modbus TCP, DHCP o una entrada neighbor incorrecta en Windows.

No se considera todavía causa raíz demostrada. Falta distinguir entre comportamiento de la infraestructura y una posible interacción específica del W5500 con tráfico unicast envejecido.

## Siguiente paso

```text
NEXT=VERIFY_WITH_PACKET_CAPTURE_OR_KEEPALIVE_AB
```

Antes de reanudar el soak largo de rendimiento se debe cerrar o mitigar explícitamente este punto, y luego repetir la qualification integrada de `1000 req/s` con la configuración final del harness y ejecutar el soak integrado de larga duración.
