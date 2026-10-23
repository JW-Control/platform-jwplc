# JW_ModbusRTU

**Modbus RTU** Master y Slave para **ESP32**, sobre un puerto [`JW_RS485`](https://github.com/JW-Control/JW_RS485).

Es el mismo motor de `JWPLC_ModbusRTU` (package JWPLC), con el puerto inyectado: se pueden tener varios buses Modbus en la misma placa y usarlo en cualquier ESP32, no solo en el JWPLC Basic. `JWPLC_ModbusRTU` es una instancia preconfigurada de esta clase.

## Compatibilidad

- ESP32 con Arduino core 3.x (probado con `jwplc:esp32` 2.1.0-alpha.12, core 3.3.8).

## Dependencias

- `JW_RS485`.

## Funciones Modbus

```text
FC01 Read Coils              FC05 Write Single Coil
FC02 Read Discrete Inputs    FC06 Write Single Register
FC03 Read Holding Registers  FC15 Write Multiple Coils
FC04 Read Input Registers    FC16 Write Multiple Registers
```

## Ejemplo mínimo (Slave)

```cpp
#include <JW_ModbusRTU.h>

JW_RS485 rs485(Serial2, JWPLC_RS485_RX_PIN, JWPLC_RS485_TX_PIN);
JW_ModbusRTU modbus(rs485);

uint16_t holding[8] = {};

void setup()
{
    rs485.setAutoDirection();
    modbus.setHoldingRegisters(holding, 8);
    modbus.begin(2, 115200, SERIAL_8N1);  // Slave ID 2
}

void loop()
{
    modbus.task();
    holding[0] = millis() / 1000;
}
```

## Ejemplo mínimo (Master)

```cpp
uint16_t values[4];

// ASYNC (por defecto): no bloquea el loop.
modbus.requestReadHoldingRegisters(2, 0, 4, values, 1000);
// ... en cada loop: modbus.task(); y luego masterDone()/masterSucceeded()

// SYNC explícito: retorna con el resultado.
bool ok = modbus.readHoldingRegistersSync(2, 0, 4, values, 200);
```

## API principal

| Grupo | Métodos |
|---|---|
| Ciclo de vida | `JW_ModbusRTU(JW_RS485&)`, `begin(slaveId, baud, config)`, `end()`, `isReady()`, `port()` |
| Motor | `task()` / `poll()`, `motor(JW_MODBUS_SYNC / JW_MODBUS_ASYNC)`, `setQueuedTxEnabled()`, `setFrameGapUs()` |
| Slave | `setHoldingRegisters`, `setInputRegisters`, `setCoils`, `setDiscreteInputs`, `hasValidRequest()`, `lastValidRequestMs()` |
| Master ASYNC | `request*()`, `masterBusy()`, `masterDone()`, `masterSucceeded()`, `masterResult()`, `clearMasterResult()` |
| Master SYNC | `*Sync()` (bloqueante con timeout) |
| Master unificado | `readHoldingRegisters()`, `writeSingleRegister()`, ... según el motor seleccionado |
| Diagnóstico | `stats()`, `resetStats()`, `lastError()`, `lastErrorString()`, `printStatus()` |

## Errores posibles

| Código | Causa |
|---|---|
| `JW_MODBUS_TIMEOUT` | El esclavo no respondió a tiempo. |
| `JW_MODBUS_CRC_ERROR` | Trama con CRC inválido. |
| `JW_MODBUS_EXCEPTION` | El esclavo respondió con una excepción Modbus. |
| `JW_MODBUS_INVALID_RESPONSE` | Respuesta de otro esclavo o con formato inesperado. |
| `JW_MODBUS_BUSY` | Hay una transacción Master en curso. |
| `JW_MODBUS_DISABLED` / `JW_MODBUS_NOT_STARTED` | El puerto no pudo iniciarse o falta `begin()`. |

## Notas de uso

- El ID de `begin()` es el ID propio. En modo Master usar uno que no exista en el bus (por ejemplo 247).
- En modo ASYNC, `task()` debe llamarse con frecuencia mientras haya una transacción pendiente.
- La API `*Sync()` bloquea hasta la respuesta o el timeout: elegir timeouts cortos si el loop atiende seguridad.
- La cola TX (`setQueuedTxEnabled(true)`) solo se usa si el puerto tiene AutoDirection.

## Limitaciones

- Un Master atiende una transacción a la vez.
- Solo ESP32 en esta versión (por `JW_RS485`).

## Pruebas

Equivalencia byte a byte contra el `JWPLC_ModbusRTU.cpp` original del package:

```bash
bash extras/test/run_equivalence.sh <ruta a platform-jwplc/JWPLC/2.1.0/libraries>
```
