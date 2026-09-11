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
CURRENT_GATE=A14.1_EMPTY_SKETCH_REGRESSION
A14_1_IMPLEMENTATION=PASS_SOURCE_COMPLETE
A14_1_COMPILE=PASS
A14_1_SERVER_RUNTIME=NOT_EXECUTED
A14_2_CLIENT=NOT_STARTED
RTU_TCP_SIMULTANEOUS=NOT_EXECUTED
```

## Evidencia de compilación A14.1

Validación ejecutada por el usuario sobre checkout local de la rama Alpha14:

```text
FQBN=jwplc_local:esp32:jwplcbasic
PLATFORM=jwplc_local:esp32 2.1.0-dev
JWPLC_ModbusTCP=0.1.0
COMPILE_EXIT_CODE=0
PROGRAM_BYTES=417893
PROGRAM_PERCENT=10
GLOBAL_VARIABLE_BYTES=28452
GLOBAL_VARIABLE_PERCENT=8
LOCAL_VARIABLE_BYTES_AVAILABLE=299228
A14_1_COMPILE=PASS
```

La resolución de librerías confirmó que `JWPLC_ModbusTCP`, `JWPLC_Ethernet`, Display, RTC, FRAM, SD, botonera, RS-485 y Modbus RTU provinieron del árbol local `JWPLC/2.1.0/libraries`.

```text
LOCAL_PACKAGE_RESOLUTION=PASS
UNEXPECTED_EXTERNAL_JWPLC_LIBRARY=NO
```

## Implementado en A14.1

- nueva librería opt-in `JWPLC_ModbusTCP`;
- dependencia explícita de `JWPLC_Ethernet`;
- Server configurado por `beginServer(unitId, port)`;
- espera cooperativa a que el autoload Ethernet llegue a `READY`;
- parser MBAP incremental;
- presupuesto RX por llamada a `task()`;
- mutex SPI compartido alrededor de accesos W5500;
- procesamiento de PDU fuera del mutex;
- mapas de Coils, Discrete Inputs, Holding Registers e Input Registers;
- FC01, FC02, FC03, FC04, FC05, FC06, FC15 y FC16;
- excepciones 01/02/03/04;
- estadísticas y estado;
- ejemplo `01.ModbusTCP_Server`;
- README específico.

## Decisiones pendientes de validación

No se añade todavía `JWPLC_ModbusTCP` a `JWPLC_GlobalPeripherals` ni al autoload/discovery global.

Motivo:

```text
NEW_PROTOCOL_FEATURE=OPT_IN_DURING_ALPHA14
ALPHA10_BUILD_SPEED_GATE=PRESERVE
EMPTY_SKETCH_DISCOVERY_COST=DO_NOT_INCREASE_WITHOUT_EVIDENCE
```

La librería todavía no usa `precompiled=full`. Esa decisión se toma después de compilar, medir y estabilizar la API.

## Hallazgo para A14.2

El backend actual `EthernetClient::connect()` espera síncronamente a conexión/timeout. No se usará tal cual para declarar un Client/Master cooperativo.

A14.2 debe implementar una extensión mínima de conexión TCP por estados:

```text
start connect
poll socket state
connected / failed / timeout
cancel
```

Cada paso debe mantener ownership SPI sólo durante operaciones W5500 cortas.

## Lo que todavía NO se afirma

```text
MODBUS_TCP_SERVER=PASS            -> NO
MODBUS_TCP_CLIENT=PASS            -> NO
FC01_02_03_04_05_06_15_16=PASS  -> NO
MODBUS_TCP_RECONNECT=PASS         -> NO
MODBUS_RTU_TCP_SIMULTANEOUS=PASS -> NO
ROBOT_INTEROPERABILITY=PASS       -> NO
```

## Próximo gate

Confirmar que un sketch vacío sigue sin resolver ni compilar `JWPLC_ModbusTCP` y conserva la estructura de compilación cerrada en Alpha11/Alpha10.

```text
EXPECTED_EMPTY_SKETCH_MODBUSTCP_DISCOVERY=NO
EXPECTED_EMPTY_SKETCH_WARM_COMPILERS=1
```
