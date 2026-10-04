# Alpha14 — API Modbus RTU con selector de motor

Fecha: 2026-09-25

## Decision recuperada y aplicada

La API publica usa los mismos nombres de operacion para ambos motores:

```cpp
JWPLC_ModbusRTU.motor(ASYNC);
JWPLC_ModbusRTU.readHoldingRegisters(...);

JWPLC_ModbusRTU.motor(SYNC);
JWPLC_ModbusRTU.readHoldingRegisters(...);
```

## Politica

| Motor | Default | Semantica | Transporte recomendado |
|---|---|---|---|
| ASYNC | si | cooperativa/no bloqueante | TX queued en AutoDirection |
| SYNC | no | bloqueante | TX bloqueante historica |

ASYNC es el default para runtime PLC. SYNC se conserva por compatibilidad y
commissioning.

## Compatibilidad

Se mantienen las APIs explicitas existentes:

- `requestRead...` / `requestWrite...`
- `read...Sync()` / `write...Sync()`

La API unificada redirige internamente a una de esas rutas segun el motor.

## Semantica del bool

- ASYNC: `true` = solicitud aceptada/iniciada.
- SYNC: `true` = transaccion completada correctamente.

La documentacion debe dejar esta diferencia visible para evitar que un sketch
interprete una aceptacion ASYNC como respuesta recibida.

## Selector comun

El enum `JWPLCModbusMotor` vive en
`cores/jwcontrol/jwplc_modbus_motor.h` para poder reutilizar en Modbus TCP sin
crear nombres diferentes de motor entre RTU y TCP.
