# v2.1.0-alpha.14 — Estado

Actualizado: `2026-09-11`

## Identidad

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
BASE_BRANCH=release/v2.1.x
BASE_SHA=165f94fb25617d3a6035fdfd9899bd38a1331583
ALPHA14_STATUS=IN_PROGRESS
```

## Gate actual

```text
CURRENT_GATE=A14.2_CLIENT_COOPERATIVE_BACKEND
A14_1_IMPLEMENTATION=PASS_SOURCE_COMPLETE
A14_1_COMPILE=PASS
A14_1_EMPTY_SKETCH_REGRESSION=PASS
A14_1_SERVER_LISTEN_SMOKE=PASS
A14_1_FC01=PASS
A14_1_FC02=PASS
A14_1_FC03=PASS
A14_1_FC04=PASS
A14_1_FC05=PASS
A14_1_FC06=PASS
A14_1_FC15=PASS
A14_1_FC16=PASS
A14_1_SERVER_FUNCTION_MATRIX=PASS
A14_1_EXCEPTION_RECOVERY=PASS
A14_1_INVALID_MBAP_RECONNECT=PASS
A14_1_SERVER_RUNTIME=PASS
A14_1=PASS
A14_2_CLIENT=IN_PROGRESS
RTU_TCP_SIMULTANEOUS=NOT_EXECUTED
```

## Build / regresión

```text
FQBN=jwplc_local:esp32:jwplcbasic
PLATFORM=jwplc_local:esp32 2.1.0-dev
JWPLC_ModbusTCP=0.1.0
COMPILE_EXIT_CODE=0
PROGRAM_BYTES=417893
GLOBAL_VARIABLE_BYTES=28452
LOCAL_PACKAGE_RESOLUTION=PASS
```

Run de regresión `Basic / 01_empty`:

```text
RUN=20260911_161448
LABEL=alpha14-empty-regression
ALPHA11_COLD_COMPILERS=15
ALPHA14_COLD_COMPILERS=15
ALPHA11_WARM_COMPILERS=1
ALPHA14_WARM_COMPILERS=1
ALPHA11_EXPLICIT_BINARY_BYTES=4618720
ALPHA14_EXPLICIT_BINARY_BYTES=4619056
DELTA_BYTES=+336
DELTA_PERCENT≈+0.0073%
A14_1_COMPILER_STRUCTURE_PARITY=PASS
A14_1_EMPTY_SKETCH_BINARY_REGRESSION=MATERIAL_NO
A14_1_EMPTY_SKETCH_REGRESSION=PASS
```

`JWPLC_ModbusTCP` permanece opt-in durante Alpha14 y no se añade todavía a `JWPLC_GlobalPeripherals` ni al autoload global.

## A14.1 — Foundation + Server

### Smoke físico

```text
BOOT=POWERON_RESET
DHCP_READY=PASS
IP=192.168.0.31
TCP_PORT=502
UNIT_ID=1
SERVER_STATE=READY
LAST_ERROR=OK
UNEXPECTED_RESET=0
```

### FC03 / FC06 individuales

```text
FC03_READ_HOLDING_0=PASS
FC06_WRITE_HOLDING_0_12345=PASS
FC06_EXACT_ECHO=PASS
HOLDING_0=12345
```

### Matriz completa

Evidencia: `docs/v2.1.0-alpha.14/A14_1_SERVER_FUNCTION_MATRIX_20260911.md`.

```text
FC01=PASS
FC02=PASS
FC03=PASS
FC04=PASS
FC05=PASS
FC06=PASS
FC15=PASS
FC16=PASS
PERSISTENT_TCP_CONNECTION=PASS
READBACK_WRITES=PASS
RX_FRAMES=11
TX_FRAMES=11
REQUESTS_OK=11
EXCEPTIONS=0
LAST_ERROR=OK
```

### Excepciones + recuperación en la misma sesión

```text
EX01_ILLEGAL_FUNCTION=PASS
EX02_ILLEGAL_DATA_ADDRESS=PASS
EX03_ILLEGAL_DATA_VALUE=PASS
VALID_REQUEST_AFTER_EXCEPTIONS=PASS
SAME_TCP_CONNECTION_AFTER_EXCEPTIONS=PASS
RX_FRAMES=15
TX_FRAMES=15
REQUESTS_OK=12
EXCEPTIONS=3
LAST_ERROR=OK
```

### Framing fatal + reconexión

Evidencia: `docs/v2.1.0-alpha.14/A14_1_INVALID_MBAP_RECONNECT_20260911.md`.

```text
INVALID_MBAP_PROTOCOL_ID_1=DETECTED
CLIENT_DROP_AFTER_FATAL_MBAP=PASS
NEW_TCP_CONNECTION=PASS
VALID_FC03_AFTER_RECONNECT=PASS
MAP_STATE_PRESERVED=PASS
RX_FRAMES=16
TX_FRAMES=16
REQUESTS_OK=13
EXCEPTIONS=3
LAST_ERROR=OK
UNEXPECTED_RESET=0
A14_1_INVALID_MBAP_RECONNECT=PASS
```

Conclusión:

```text
MODBUS_TCP_SERVER_A14_1=PASS
A14_1_SERVER_RUNTIME=PASS
A14_1=PASS
```

Esto cierra el alcance de A14.1 definido en `ALPHA14_PLAN.md`: librería, mapas, parser MBAP incremental, Server FC01/02/03/04/05/06/15/16, excepciones/estadísticas, ejemplo y compile/runtime físico.

## A14.2 — Client cooperativo

### Hallazgo confirmado

El backend actual `EthernetClient::connect(IPAddress, port)` abre el socket y ejecuta `socketConnect()`, pero después espera síncronamente en un bucle hasta `ESTABLISHED`, `CLOSE_WAIT`, `CLOSED` o timeout.

`socketConnect()` del backend W5500 sólo configura destino, ejecuta `Sock_CONNECT` y retorna; por tanto, la espera bloqueante puede separarse sin modificar `socket.cpp`.

### Decisión

A14.2 añadirá una extensión mínima compatible a `EthernetClient` para conexión TCP por estados:

```text
beginConnectAsync(IP, port)
pollConnectAsync()
connectAsyncInProgress()
cancelConnectAsync()
```

Contrato previsto:

```text
beginConnectAsync: -1=falló al iniciar, 0=pending, 1=connected
pollConnectAsync : -1=failed/closed, 0=pending, 1=connected
```

La API `connect()` existente se conserva sin cambios de semántica para compatibilidad Arduino. El nuevo Client Modbus TCP usará exclusivamente el camino cooperativo y mantendrá el mutex SPI JWPLC sólo alrededor de pasos W5500 cortos.

## Siguientes pasos A14.2

1. añadir primitiva TCP connect asíncrona al backend Ethernet;
2. compilar regresión de Server existente;
3. implementar state machine Client/Master en `JWPLC_ModbusTCP`;
4. exponer FC01/02/03/04/05/06/15/16 cooperativas;
5. añadir ejemplo Client;
6. validar timeout, reconexión y matriz Client contra servidor de prueba.

## Pendientes posteriores

```text
MODBUS_TCP_CLIENT=PASS            -> NO
MODBUS_TCP_RECONNECT=PASS         -> PARCIAL: Server sí; Client pendiente
MODBUS_RTU_TCP_SIMULTANEOUS=PASS -> NO
ROBOT_INTEROPERABILITY=PASS       -> NO
```
