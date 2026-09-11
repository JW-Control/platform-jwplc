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
CURRENT_GATE=A14.1_INVALID_MBAP_RECONNECT
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
A14_1_INVALID_MBAP_RECONNECT=NOT_EXECUTED
A14_1_SERVER_RUNTIME=PARTIAL_PASS
A14_2_CLIENT=NOT_STARTED
RTU_TCP_SIMULTANEOUS=NOT_EXECUTED
```

## Evidencia de compilación

```text
FQBN=jwplc_local:esp32:jwplcbasic
PLATFORM=jwplc_local:esp32 2.1.0-dev
JWPLC_ModbusTCP=0.1.0
COMPILE_EXIT_CODE=0
PROGRAM_BYTES=417893
GLOBAL_VARIABLE_BYTES=28452
A14_1_COMPILE=PASS
LOCAL_PACKAGE_RESOLUTION=PASS
```

## Regresión de sketch vacío

Run `20260911_161448`, label `alpha14-empty-regression`.

```text
ALPHA11_BASIC_EMPTY_COLD_COMPILERS=15
ALPHA14_BASIC_EMPTY_COLD_COMPILERS=15
ALPHA11_BASIC_EMPTY_WARM_COMPILERS=1
ALPHA14_BASIC_EMPTY_WARM_COMPILERS=1
ALPHA11_EXPLICIT_BINARY_BYTES=4618720
ALPHA14_EXPLICIT_BINARY_BYTES=4619056
DELTA_BYTES=+336
DELTA_PERCENT≈+0.0073%
A14_1_COMPILER_STRUCTURE_PARITY=PASS
A14_1_EMPTY_SKETCH_BINARY_REGRESSION=MATERIAL_NO
A14_1_EMPTY_SKETCH_REGRESSION=PASS
```

`JWPLC_ModbusTCP` permanece opt-in; no se añade todavía a `JWPLC_GlobalPeripherals` ni al autoload global.

## Smoke físico del Server

Con `01.ModbusTCP_Server` sobre JWPLC Basic físico:

```text
BOOT=POWERON_RESET
DHCP_READY=PASS
IP=192.168.0.31
TCP_PORT=502
UNIT_ID=1
SERVER_STATE=READY
LAST_ERROR=OK
UNEXPECTED_RESET=0
A14_1_SERVER_LISTEN_SMOKE=PASS
```

## Gates físicos individuales

### FC03

```text
TX=00 01 00 00 00 06 01 03 00 00 00 01
RX=00 01 00 00 00 05 01 03 02 00 00
HOLDING_0=0
A14_1_FC03=PASS
```

### FC06

```text
TX=00 02 00 00 00 06 01 06 00 00 30 39
RX=00 02 00 00 00 06 01 06 00 00 30 39
EXACT_ECHO=PASS
HOLDING_0=12345
A14_1_FC06=PASS
```

## Matriz física completa del Server

Evidencia detallada: `docs/v2.1.0-alpha.14/A14_1_SERVER_FUNCTION_MATRIX_20260911.md`.

La matriz se ejecutó sobre una única conexión TCP persistente y validó:

```text
FC01 Read Coils=PASS
FC02 Read Discrete Inputs=PASS
FC04 Read Input Registers=PASS
FC05 Write Single Coil=PASS
FC05 -> FC01 readback=PASS
FC15 Write Multiple Coils=PASS
FC15 -> FC01 readback=PASS
FC16 Write Multiple Registers=PASS
FC16 -> FC03 readback=PASS
PASS_COUNT=9
FAIL_COUNT=0
```

Estado interno posterior:

```text
SERVER_STATE=READY
CLIENT=NONE
LAST_ERROR=OK
RX_FRAMES=11
TX_FRAMES=11
REQUESTS_OK=11
EXCEPTIONS=0
HOLDING_0=12345
COILS_RAW=0xA5
UNEXPECTED_RESET=0
```

Conclusión funcional:

```text
A14_1_FC01_02_03_04_05_06_15_16=PASS
A14_1_PERSISTENT_CONNECTION_MATRIX=PASS
A14_1_SERVER_FUNCTION_MATRIX=PASS
```

## Gate físico de excepciones y recuperación

Se ejecutó sobre una única conexión TCP persistente contra `192.168.0.31:502`, Unit ID `1`.

### EX01 — Illegal Function

```text
TX=00 14 00 00 00 02 01 45
RX=00 14 00 00 00 03 01 C5 01
EXPECTED_EXCEPTION=01
RESULT=PASS
```

### EX02 — Illegal Data Address

Lectura FC03 desde Holding Register `16`, fuera del mapa válido `0..15`:

```text
TX=00 15 00 00 00 06 01 03 00 10 00 01
RX=00 15 00 00 00 03 01 83 02
EXPECTED_EXCEPTION=02
RESULT=PASS
```

### EX03 — Illegal Data Value

FC03 con `quantity=0`:

```text
TX=00 16 00 00 00 06 01 03 00 00 00 00
RX=00 16 00 00 00 03 01 83 03
EXPECTED_EXCEPTION=03
RESULT=PASS
```

### Request válido posterior sobre la misma conexión

```text
TX=00 17 00 00 00 06 01 03 00 00 00 01
RX=00 17 00 00 00 05 01 03 02 30 39
HOLDING_0=12345
SAME_TCP_CONNECTION=PASS
VALID_AFTER_EXCEPTIONS=PASS
```

Diagnóstico interno posterior del JWPLC:

```text
SERVER_STATE=READY
CLIENT=NONE
LAST_ERROR=OK
RX_FRAMES=15
TX_FRAMES=15
REQUESTS_OK=12
EXCEPTIONS=3
HOLDING_0=12345
COILS_RAW=0xA5
UNEXPECTED_RESET=0
```

Conclusión:

```text
EX01_ILLEGAL_FUNCTION=PASS
EX02_ILLEGAL_DATA_ADDRESS=PASS
EX03_ILLEGAL_DATA_VALUE=PASS
VALID_REQUEST_AFTER_EXCEPTIONS=PASS
SAME_TCP_CONNECTION_AFTER_EXCEPTIONS=PASS
A14_1_EXCEPTION_RECOVERY=PASS
```

El contrato observado coincide con source: las excepciones Modbus válidamente enmarcadas no obligan a cerrar la sesión; `buildException()` incrementa `exceptionsSent` y marca temporalmente `Last error=Modbus exception`. El request válido posterior incrementa `requestsOk` y limpia el error a `OK`.

## Contrato de framing fatal

Un MBAP con `Protocol ID != 0` o una longitud MBAP fatal se trata como error de framing/protocolo. El Server incrementa `protocolErrors`, marca el error correspondiente y fuerza `dropClient()` para evitar desincronización.

El siguiente gate debe validar físicamente esa diferencia:

```text
INVALID_MBAP -> CLIENT_CONNECTION_DROPPED
SERVER_REMAINS_LISTENING
NEW_TCP_CONNECTION -> VALID_FC03_PASS
HOLDING_0_PRESERVED=12345
NO_RESET
```

## Hallazgo para A14.2

El backend actual `EthernetClient::connect()` espera síncronamente a conexión/timeout. A14.2 no reutilizará ese bloqueo como si fuera cooperativo.

Se requiere extensión mínima por estados:

```text
start connect
poll socket state
connected / failed / timeout
cancel
```

El ownership SPI debe mantenerse sólo durante operaciones W5500 cortas.

## Lo que todavía NO se afirma

```text
MODBUS_TCP_SERVER=PASS            -> NO, falta framing fatal/reconnect y lifecycle
MODBUS_TCP_CLIENT=PASS            -> NO
MODBUS_TCP_RECONNECT=PASS         -> NO
MODBUS_RTU_TCP_SIMULTANEOUS=PASS -> NO
ROBOT_INTEROPERABILITY=PASS       -> NO
```

## Próximo gate

Validar MBAP inválido y recuperación por nueva conexión TCP sin reset del JWPLC.
