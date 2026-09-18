# Alpha14 — Cierre A14.2 Client Modbus TCP

Fecha: `2026-09-12`

## Resultado

```text
A14_2=PASS
A14_2_CLIENT_FUNCTION_MATRIX=PASS_PHYSICAL
A14_2_CLIENT_TIMEOUT_RECONNECT=PASS_PHYSICAL
A14_2_ETHERNET_BACKEND_COMPAT_REVIEW=PASS
A14_2_CLIENT_EXAMPLE=PASS_COMPILE
A14_2_UMBRELLA_API=PASS
A14_2_EMPTY_REGRESSION=PASS
A14_2_SYNC_API_DECISION=DEFERRED_POST_ALPHA14
```

## Alcance validado

El Client cooperativo de `JWPLC_ModbusTCP` queda cerrado para Alpha14 con soporte validado de:

```text
FC01 Read Coils
FC02 Read Discrete Inputs
FC03 Read Holding Registers
FC04 Read Input Registers
FC05 Write Single Coil
FC06 Write Single Register
FC15 Write Multiple Coils
FC16 Write Multiple Registers
```

La matriz completa se ejecutó físicamente sobre una única conexión TCP persistente con TID incremental `1..8`, sin excepciones, timeouts, errores de transporte, errores de protocolo ni timeouts de lock SPI.

Evidencia principal:

- `A14_2_CLIENT_FULL_MATRIX_RUNTIME_20260912.md`

## Timeout y reconexión

Se validó físicamente la recuperación ante una petición FC03 sin respuesta:

```text
FIRST_TID=1
FIRST_TIMEOUT_OBSERVED=YES
SESSION_CLOSED_AFTER_TIMEOUT=YES
SECOND_TID=2
SECOND_VALUE=9320
CONNECTIONS=2
TX_FRAMES=2
RX_FRAMES=1
REQUESTS_OK=1
TIMEOUTS=1
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
A14_2_GRACEFUL_CLOSE_RETEST_RUNTIME=PASS
A14_2_GRACEFUL_CLOSE_RETEST_PC=PASS
```

Evidencia:

- `A14_2_CLIENT_TIMEOUT_RECONNECT_RUNTIME_20260912.md`

La segunda request se completó correctamente sin volver a ejecutar `begin()`.

## Backend Ethernet cooperativo

La revisión de compatibilidad del backend quedó cerrada con dos decisiones:

1. una sesión `ESTABLISHED` o `CLOSE_WAIT` se cierra mediante `socketDisconnect()` para notificar el cierre al peer sin bloquear la state machine cooperativa;
2. se eliminó el rechazo temprano de `port == 0` introducido durante el desarrollo async para conservar la semántica histórica de `EthernetClient`.

Commit:

```text
34e4762adfacecccf44f125daf8ae7dabc836ba5
fix(ethernet): cerrar sesiones async de forma graceful
```

Resultado:

```text
A14_2_ETHERNET_BACKEND_COMPAT_REVIEW=PASS
```

## Ejemplo oficial y API umbrella

Se añadió:

```text
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/examples/02.ModbusTCP_Client/02.ModbusTCP_Client.ino
```

El ejemplo usa la API cooperativa, conexión lazy, sesión persistente, polling periódico, reporte de errores y reconexión automática tras pérdida de sesión.

Además, `JWPLC_ModbusTCP.h` expone ahora también `JWPLC_ModbusTCPClient`, evitando que el usuario tenga que incluir manualmente el header interno del Client.

Commit:

```text
3f55d80c436b601d326bdead34a7e71dd5b5af39
feat(modbus-tcp): añadir ejemplo Client oficial
```

Regresiones de compilación:

```text
CLIENT_EXAMPLE_COMPILE_EXIT=0
SERVER_EXAMPLE_REGRESSION_EXIT=0
EMPTY_REGRESSION_EXIT=0
A14_2_CLIENT_EXAMPLE_COMPILE=PASS
A14_2_EMPTY_REGRESSION=PASS
```

## Decisión sobre API Sync

Para Alpha14 no se expondrán wrappers síncronos adicionales.

```text
A14_2_SYNC_API_DECISION=DEFERRED_POST_ALPHA14
```

Motivos:

- el motor cooperativo ya cubre el alcance obligatorio de Alpha14;
- es la API que se necesita para medir rendimiento y coexistencia sin introducir esperas bloqueantes;
- añadir wrappers Sync en este punto ampliaría superficie API sin ser requisito de cierre;
- una API Sync podrá evaluarse después de Alpha14 como capa de conveniencia, sin modificar el motor cooperativo validado.

## Conclusión

A14.2 queda cerrado como `PASS`.

El siguiente gate es A14.3: benchmark físico de rendimiento Server/Client conforme a `A14_MODBUS_TCP_PERFORMANCE_BENCHMARK_PLAN.md`.
