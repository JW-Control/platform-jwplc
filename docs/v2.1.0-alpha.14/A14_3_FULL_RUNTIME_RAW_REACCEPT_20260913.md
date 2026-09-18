# Alpha14.3 — Diagnóstico RAW W5500 de reaccept bajo FULL_RUNTIME_REALISTIC

Fecha: 2026-09-13

## Objetivo

Repetir el diagnóstico de reconexión TCP sobre el perfil `FULL_RUNTIME_REALISTIC`, manteniendo activos TFT, FRAM, microSD, RTC, botonera y TCA/I/O, e inspeccionando en paralelo los estados físicos de los sockets del W5500.

El objetivo era determinar si el problema previamente observado de varios intentos TCP antes de reconectar correspondía a:

- ausencia real de un socket `LISTEN` en puerto 502;
- un socket W5500 atascado en `CLOSE_WAIT`, `TIME_WAIT`, `LAST_ACK` u otro estado transitorio;
- pérdida temporal de disponibilidad IP/W5500 pese a que el runtime declaraba `SERVER_READY=YES`;
- o una condición dependiente de una precondición todavía no reproducida.

## Metodología

Se usó una copia temporal del harness `FULL_RUNTIME_REALISTIC` con un comando serial `W` adicional para capturar:

- `ETH_READY`;
- estado runtime Ethernet;
- link PHY;
- IP lógica y `SIPR` física del W5500;
- estado lógico del servidor Modbus TCP;
- `SnSR`, puerto local, puerto remoto, RX, TXFREE y `server_port` de los ocho sockets W5500;
- conteo de sockets `LISTEN` y `ESTABLISHED` en puerto 502.

La prueba realizó 12 ciclos de conexión TCP, una transacción FC03 válida, cierre y verificación 100 ms después.

No se modificó el harness versionado ni las librerías del package. No fue necesario desactivar precompilados.

## Resultado físico

```text
CYCLES=12
PASS_CYCLES=12
CONNECT_FAILURES=0
PROTOCOL_FAILURES=0
FAIL_WITH_HW_LISTEN=0
FAIL_WITHOUT_HW_LISTEN=0
FAIL_WITH_PING_PASS=0
FAIL_WITH_PING_FAIL=0
CONNECT_MIN_MS=0.397
CONNECT_AVG_MS=4.638
CONNECT_MAX_MS=19.033
A14_3_FULL_RUNTIME_RAW_REACCEPT=PASS_NOT_REPRODUCED
```

Compilación y entorno:

```text
BUILD_UPLOAD_EXIT=0
COMPILE_WARNINGS=0
RAW_PROBE_EXIT=0
WORKTREE_AFTER=PASS
ORIGINAL_HARNESS_MODIFIED=NO
LIBRARY_SOURCE_CHANGES=0
PRECOMPILED_OVERRIDE_REQUIRED=NO
```

## Evidencia de sockets

En todos los ciclos, antes de la conexión existió exactamente un socket físico W5500 en:

```text
SR=LISTEN
PORT=502
SERVER_PORT=502
```

Durante cada conexión apareció un socket `ESTABLISHED` en 502 y se mantuvo en paralelo otro socket `LISTEN` en 502.

Aproximadamente 100 ms después del cierre:

- el socket de la sesión anterior quedó `CLOSED`;
- permaneció exactamente un socket `LISTEN` en 502;
- no quedaron sockets atascados en `CLOSE_WAIT`, `TIME_WAIT`, `LAST_ACK`, `FIN_WAIT` o estados equivalentes.

Durante toda la prueba:

```text
FULL_RUNTIME_READY=YES
ETH_READY=YES
ETH_RUNTIME_STATE=5
ETH_LINK_STATUS=1
ETH_LOGICAL_IP=192.168.0.31
W5500_CHIP=55
W5500_PHYCFGR=0xBF
W5500_SIPR=192.168.0.31
```

## Clasificación

```text
FULL_RUNTIME_RAW_W5500_REACCEPT=PASS_PHYSICAL
REACCEPT_FAILURE_IN_FRESH_FULL_RUNTIME=NOT_REPRODUCED
A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED
ROOT_CAUSE=NOT_DEMONSTRATED
```

Este resultado descarta un fallo determinista del listener o del mecanismo básico de reapertura bajo un `FULL_RUNTIME_REALISTIC` recién iniciado.

## Interpretación

El problema original sigue siendo real porque se reprodujo previamente con tres timeouts consecutivos de conexión; dos de ellos también mostraron fallo de ping y el tercero mostró ping recuperado antes de que TCP conectara.

Sin embargo, no volvió a aparecer después de reflashear/reiniciar el equipo, ni en el servidor aislado ni en un `FULL_RUNTIME_REALISTIC` fresco.

La evidencia cambia la hipótesis principal: el fallo parece depender de una precondición acumulada o temporal que no está presente inmediatamente después de un reboot. Candidatos a revisar en el siguiente gate:

1. tiempo de actividad previo al intento de conexión;
2. mantenimiento DHCP renew/rebind cooperativo;
3. estado de red/ARP después de varios minutos;
4. secuencia previa de benchmarks o tráfico sostenido;
5. interacción entre un estado de red envejecido y el servidor Modbus TCP.

## Siguiente gate

Antes de liberar el long run de 1000 req/s se debe intentar reproducir el fallo sin reboot, llevando el equipo a una condición de runtime envejecido comparable con la observada originalmente y capturando estados RAW W5500 si la conexión vuelve a fallar.

El long run permanece en `HOLD` hasta clasificar esta condición.