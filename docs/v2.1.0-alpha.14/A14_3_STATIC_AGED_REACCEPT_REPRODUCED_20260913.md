# Alpha14.3 — Reaccept envejecido también reproducido con IP estática

Fecha: 2026-09-13

## Objetivo

Comprobar si la pérdida temporal de conectividad observada tras varios minutos sin tráfico Ethernet dependía del modo DHCP o también podía reproducirse con una configuración IPv4 estática equivalente.

El ensayo reutilizó el mismo firmware `FULL_RUNTIME_REALISTIC` que había pasado previamente la precondición física en modo STATIC, sin reflashear ni reiniciar antes del período de aging.

## Precondición

Antes del aging:

- `ETH_MODE=STATIC`
- `ETH_READY=YES`
- `ETH_IP=192.168.0.31`
- `SERVER_READY=YES`
- `FULL_RUNTIME_READY=YES`
- `PERIPHERAL_FAILURE_COUNT=0`
- neighbor Windows para `192.168.0.31`: MAC `02-4A-57-2F-56-28`, estado `Stale`

Resultado de precondición: `PASS`.

## Aging

Se mantuvo el runtime durante 480 s sin tráfico Ethernet generado por el gate. Únicamente se tomaron snapshots por USB/Serial cada 60 s.

Durante todo el período se mantuvo:

- `ETH_MODE=STATIC`
- `ETH_READY=YES`
- `ETH_IP=192.168.0.31`
- `ETH_DIAG=---`
- `SERVER_READY=YES`
- `FULL_RUNTIME_READY=YES`
- `PERIPHERAL_FAILURE_COUNT=0`

Al finalizar los 480 s, el neighbor de Windows seguía asociado a la MAC correcta `02-4A-57-2F-56-28` y permanecía en estado `Stale`.

## Reaccept posterior al aging

Se realizaron 12 intentos TCP/FC03 consecutivos.

### Intento 1

- TCP: `FAIL`
- timeout: `2008.946 ms`
- `ETH_MODE=STATIC`
- `ETH_READY=YES`
- `ETH_IP=192.168.0.31`
- `ETH_DIAG=---`
- `SERVER_READY=YES`
- `FULL_RUNTIME_READY=YES`
- neighbor Windows después del fallo: `Probe`
- ping inmediatamente posterior: `FAIL`

### Intento 2

- TCP: `FAIL`
- timeout: `2011.771 ms`
- estado lógico Ethernet/Modbus sin degradación visible
- neighbor Windows: `Probe`
- ping inmediatamente posterior: `FAIL`

### Intentos 3..12

Todos pasaron TCP + FC03.

Latencias de las conexiones exitosas:

- mínimo: `0.856 ms`
- promedio: `15.842 ms`
- máximo: `24.703 ms`

## Resumen

```text
AGING_S=480
ATTEMPTS=12
PASS_CYCLES=10
CONNECT_FAILURES=2
PROTOCOL_FAILURES=0
PING_FAILURES_AFTER_TCP_FAIL=2
FINAL_ETH_MODE=STATIC
FINAL_ETH_READY=YES
FINAL_ETH_IP=192.168.0.31
FINAL_ETH_DIAG=---
FINAL_PERIPHERAL_FAILURE_COUNT=0
A14_3_STATIC_AGED_REACCEPT=REPRODUCED_STATIC_TOO
```

## Conclusión

La anomalía se reproduce también con IP estática. Por tanto:

```text
DHCP_AS_ROOT_CAUSE=DISCARDED
MODBUS_PROTOCOL_FAILURE=NO
LOGICAL_SERVER_READY_DURING_OUTAGE=YES
FULL_RUNTIME_PERIPHERAL_FAILURES=0
IDLE_NETWORK_PATH_ISSUE=CONFIRMED_FOR_FURTHER_DIAGNOSIS
```

Esto refuerza que el problema está por debajo de la lógica Modbus TCP y no depende del mantenimiento DHCP. Durante la ventana de fallo, incluso ICMP desde el PC falla mientras el firmware sigue reportando Ethernet y servidor listos.

El cambio de neighbor Windows `Stale -> Probe` durante los primeros fallos es una pista relevante, pero todavía no permite atribuir la causa a ARP del host, al aprendizaje/aging L2 del equipo intermedio o al propio W5500. Una eliminación forzada del neighbor en un ensayo anterior reconstruyó ARP inmediatamente cuando la ruta estaba activa, por lo que el siguiente diagnóstico debe ejecutarse después de un nuevo período de inactividad.

## Siguiente gate

Después de un nuevo aging sin tráfico Ethernet, y antes de cualquier TCP/ping normal:

1. registrar el estado neighbor;
2. eliminar deliberadamente la entrada neighbor del PC para forzar resolución ARP por broadcast;
3. ejecutar inmediatamente el primer TCP/FC03;
4. comparar contra la secuencia natural `Stale -> Probe` observada en este ensayo.

Interpretación prevista:

- si el rebuild forzado después del aging recupera inmediatamente TCP, la sospecha principal pasa a neighbor/FDB/unknown-unicast/L2 idle path;
- si incluso el rebuild ARP forzado falla, la investigación debe bajar al W5500/PHY/ruta física de recepción.

`LONG_RUN=ON_HOLD` hasta cerrar esta anomalía.