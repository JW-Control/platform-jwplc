# Alpha14.3 — Diagnóstico RAW de reaccept sobre W5500

Fecha: 2026-09-13

## Objetivo

Investigar el pendiente `A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED` observando directamente los estados físicos de los sockets del W5500 durante ciclos repetidos de conexión/desconexión Modbus TCP.

La prueba se ejecutó con un firmware diagnóstico temporal que:

- mantiene `JWPLC_ModbusTCP` Server en puerto 502;
- expone por serial el estado lógico de Ethernet y Modbus TCP;
- lee directamente los 8 sockets físicos del W5500 (`SnSR`, puerto local/remoto, RX, TX free y asociación de `EthernetServer::server_port[]`);
- no modifica librerías ni archivos del repositorio;
- no requiere desactivar precompilados.

## Resultado

```text
CYCLES=12
PASS_CYCLES=12
CONNECT_FAILURES=0
PROTOCOL_FAILURES=0
FAIL_WITH_HW_LISTEN=0
FAIL_WITHOUT_HW_LISTEN=0
FAIL_WITH_PING_PASS=0
FAIL_WITH_PING_FAIL=0
A14_3_RAW_W5500_REACCEPT=PASS_NOT_REPRODUCED
```

Compilación y ejecución:

```text
BUILD_UPLOAD_EXIT=0
COMPILE_WARNINGS=0
RAW_PROBE_EXIT=0
WORKTREE_AFTER=PASS
LIBRARY_SOURCE_CHANGES=0
PRECOMPILED_OVERRIDE_REQUIRED=NO
```

## Evidencia de socket físico

Antes de cada conexión se observó consistentemente un socket físico en `LISTEN` sobre el puerto 502:

```text
LISTEN_502_COUNT=1
ESTABLISHED_502_COUNT=0
```

Durante la conexión Modbus TCP se observó simultáneamente:

```text
LISTEN_502_COUNT=1
ESTABLISHED_502_COUNT=1
```

Esto confirma que el backend abre un socket adicional de escucha mientras el socket activo atiende al cliente.

A los ~100 ms del cierre del cliente se recuperó de forma consistente:

```text
LISTEN_502_COUNT=1
ESTABLISHED_502_COUNT=0
```

El socket utilizado por la conexión anterior queda `CLOSED` y pierde su `SERVER_PORT`, mientras otro socket permanece escuchando en 502.

## Estado físico del W5500 observado

Durante las capturas:

```text
W5500_CHIP=55
W5500_PHYCFGR=0xBF
W5500_SIPR=192.168.0.31
ETH_LINK=ON
ETH_READY=YES
MODBUS_SERVER_READY=YES
```

No se observaron sockets atascados en `CLOSE_WAIT`, `TIME_WAIT`, `LAST_ACK`, `FIN_WAIT` ni otros estados transitorios durante esta prueba aislada.

## Latencia de conexión observada

Las conexiones exitosas mostraron dos grupos visibles:

- conexiones rápidas de aproximadamente 0.4–0.5 ms;
- algunos ciclos con aproximadamente 4.9 ms, 13.7 ms, 18.7 ms y 22.3 ms.

No hubo timeout ni fallo funcional en ninguno de los 12 ciclos.

## Conclusión

El problema previo de reaccept **no se reprodujo en el firmware diagnóstico aislado**.

Por tanto, esta prueba descarta como causa general un fallo determinista del mecanismo básico de `EthernetServer`/W5500 al cerrar y reabrir conexiones consecutivas.

La evidencia actual apunta a que la incidencia previa depende de alguna condición adicional presente en el perfil `FULL_RUNTIME_REALISTIC`, del arranque/estado temporal de la red, o de la combinación de carga/periféricos/tareas concurrentes.

No se cierra todavía el pendiente:

```text
A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED
```

La siguiente prueba debe repetir la instrumentación RAW **dentro del firmware FULL_RUNTIME_REALISTIC**, manteniendo TFT, FRAM, microSD, RTC, TCA/I/O y botonera activos, para capturar los estados del W5500 exactamente cuando vuelva a ocurrir un timeout de conexión.

## Decisiones

```text
A14_3_RAW_W5500_REACCEPT=PASS_NOT_REPRODUCED
ISOLATED_W5500_LISTENER_RECOVERY=PASS_PHYSICAL
RAW_SOCKET_STUCK_STATE_OBSERVED=NO
LIBRARY_SOURCE_CHANGE=NO
PRECOMPILED_OVERRIDE_REQUIRED=NO
LONG_RUN=ON_HOLD
NEXT_GATE=A14.3_FULL_RUNTIME_RAW_W5500_REACCEPT_DIAGNOSTIC
```
