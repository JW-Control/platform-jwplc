# Alpha14.3 - Rescate inmediato tras reconstrucción ARP envejecida

Fecha: 2026-09-13

## Objetivo

Separar un posible fallo del JWPLC/W5500 de un problema de resolución/forwarding L2 después de un periodo prolongado sin tráfico Ethernet.

El firmware usado fue el perfil `FULL_RUNTIME_REALISTIC` en modo IP estática, ya validado previamente con Ethernet, Modbus TCP y periféricos integrados operativos.

No se reflasheó ni reinició el equipo durante este gate.

## Precondición

- `ETH_MODE=STATIC`
- `ETH_READY=YES`
- `SERVER_READY=YES`
- `FULL_RUNTIME_READY=YES`
- `PERIPHERAL_FAILURE_COUNT=0`
- IP: `192.168.0.31`
- MAC esperada del JWPLC en Windows: `02-4A-57-2F-56-28`

## Procedimiento

1. Mantener el sistema 480 s sin tráfico Ethernet.
2. Consultar únicamente por USB/Serial el estado del runtime.
3. Verificar el estado del neighbor de Windows después del aging.
4. Eliminar explícitamente la entrada neighbor/ARP de `192.168.0.31`.
5. Sin hacer ping previo, iniciar una conexión TCP al puerto 502 y ejecutar FC03.
6. Verificar la MAC aprendida nuevamente por Windows.

## Resultado físico

Antes del borrado, después de 480 s:

```text
AGED_ETH_MODE=STATIC
AGED_ETH_READY=YES
AGED_SERVER_READY=YES
AGED_FULL_RUNTIME_READY=YES
AGED_PERIPHERAL_FAILURE_COUNT=0
NEIGHBOR_IP=192.168.0.31
NEIGHBOR_MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE=4
```

Tras `Remove-NetNeighbor`:

```text
NEIGHBOR_MAC=00-00-00-00-00-00
NEIGHBOR_STATE=0
```

Primer TCP inmediatamente después del borrado, sin ping previo:

```text
TCP_CONNECT=PASS
TCP_CONNECT_MS=2.655
FC03_PROBE=PASS
```

La resolución ARP se reconstruyó correctamente:

```text
NEIGHBOR_MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE=5
EXPECTED_MAC_RESTORED=YES
```

Estado final:

```text
FINAL_ETH_MODE=STATIC
FINAL_ETH_READY=YES
FINAL_SERVER_READY=YES
FINAL_FULL_RUNTIME_READY=YES
FINAL_PERIPHERAL_FAILURE_COUNT=0
```

Resultado del gate:

```text
A14_3_AGED_ARP_REBUILD=PASS_RESCUED_IMMEDIATELY
ROOT_LAYER_SUSPECT=HOST_NEIGHBOR_OR_L2_FDB_IDLE_PATH
```

## Interpretación

La reconstrucción ARP desde cero después del aging recuperó la conectividad de forma inmediata, con conexión TCP en 2.655 ms y FC03 correcto.

Esto contrasta con el comportamiento natural observado previamente tras el mismo orden de magnitud de inactividad, donde Windows conservaba la MAC correcta en estado `Stale`, pasaba a `Probe`, los primeros intentos TCP expiraban y los pings iniciales también fallaban antes de la recuperación.

Por tanto:

- no hay evidencia de que el servidor Modbus TCP esté caído;
- no hay evidencia de que el listener TCP lógico sea la causa primaria;
- DHCP ya había sido descartado porque el problema también se reprodujo con IP estática;
- una resolución ARP broadcast desde cero sí restablece inmediatamente el camino de red;
- la hipótesis principal pasa a ser el camino L2 después de inactividad: neighbor/NDP equivalente en el host, aging de FDB/forwarding en la infraestructura, o diferencias entre tráfico unicast hacia una MAC aprendida y broadcast ARP.

Aún no se considera causa raíz cerrada. Falta separar si el problema está del lado del host Windows, en el forwarding/FDB de la infraestructura de red o en cómo el W5500 recibe tráfico unicast tras un periodo de inactividad.

## Próximo diagnóstico

Usar una prueba controlada donde, después del aging, el JWPLC emita una única trama/paquete saliente hacia el PC antes del primer TCP entrante. Si esa transmisión saliente hace que el primer TCP vuelva a funcionar sin borrar el neighbor de Windows, se fortalecería significativamente la hipótesis de aging de FDB/forwarding L2.

La siguiente instrumentación puede realizarse temporalmente con `EthernetUDP`, disponible en la librería `JWPLC_Ethernet`, evitando cambios de API productiva.

## Estado

```text
A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED
DHCP_ROOT_CAUSE=NO
AGED_ARP_REBUILD=PASS_RESCUED_IMMEDIATELY
ROOT_LAYER_SUSPECT=HOST_NEIGHBOR_OR_L2_FDB_IDLE_PATH
LONG_RUN=ON_HOLD
```
