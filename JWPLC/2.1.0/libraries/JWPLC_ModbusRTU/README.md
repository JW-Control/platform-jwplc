# JWPLC_ModbusRTU

Librería del package **JWPLC ESP32** para Modbus RTU sobre `JWPLC_RS485`.

Desde Alpha7, el Master usa un modelo **cooperativo/no bloqueante por defecto**. La API síncrona existe de forma explícita para commissioning, pruebas o sketches simples, pero no es la ruta recomendada para lógica PLC, Backplane ni Remote I/O.

## Funciones Modbus soportadas en Alpha7

### Slave / Server

| Código | Función |
|---:|---|
| `0x01` | Read Coils |
| `0x02` | Read Discrete Inputs |
| `0x03` | Read Holding Registers |
| `0x04` | Read Input Registers |
| `0x05` | Write Single Coil |
| `0x06` | Write Single Register |
| `0x0F` | Write Multiple Coils |
| `0x10` | Write Multiple Registers |

### Master cooperativo

| Código | API principal |
|---:|---|
| `0x01` | `requestReadCoils()` |
| `0x02` | `requestReadDiscreteInputs()` |
| `0x03` | `requestReadHoldingRegisters()` |
| `0x04` | `requestReadInputRegisters()` |
| `0x05` | `requestWriteSingleCoil()` |
| `0x06` | `requestWriteSingleRegister()` |
| `0x0F` | `requestWriteMultipleCoils()` |

Las variantes `...Sync()` exponen las mismas operaciones Master cuando bloquear el flujo de aplicación es aceptable.

## Mapas Slave

Coils y Discrete Inputs se almacenan como bits empaquetados LSB-first. Para 8 señales basta un byte:

```cpp
uint8_t coils = 0x00;
uint8_t inputs = 0x00;
uint16_t holding[16] = {};
uint16_t inputRegs[16] = {};

void setup()
{
    JWPLC_ModbusRTU.setCoils(&coils, 8);
    JWPLC_ModbusRTU.setDiscreteInputs(&inputs, 8);
    JWPLC_ModbusRTU.setHoldingRegisters(holding, 16);
    JWPLC_ModbusRTU.setInputRegisters(inputRegs, 16);

    JWPLC_ModbusRTU.begin(1, 115200, SERIAL_8N1);
}

void loop()
{
    // Refrescar inputs antes de atender requests FC02.
    inputs = JWPLC_readInputs();

    JWPLC_ModbusRTU.task();

    // Aplicar la imagen de coils recibida por FC05/FC15.
    JWPLC_writeOutputs(coils);
}
```

Para más de 8 bits, el mapa ocupa `ceil(count / 8)` bytes.

## Actividad válida y fail-safe Remote I/O

La librería registra actividad válida dirigida al Slave local:

```cpp
JWPLC_ModbusRTU.hasValidRequest();
JWPLC_ModbusRTU.lastValidRequestMs();
```

Para salidas remotas, FC05 y FC15 actualizan además:

```cpp
JWPLC_ModbusRTU.hasCoilWrite();
JWPLC_ModbusRTU.lastCoilWriteMs();
```

Esto permite implementar un fail-safe de DO sin volver a parsear Modbus en el sketch:

```cpp
if (JWPLC_ModbusRTU.hasCoilWrite() &&
    millis() - JWPLC_ModbusRTU.lastCoilWriteMs() > 100)
{
    coils = 0x00;
    JWPLC_writeOutputs(0x00);
}
```

La política exacta de timeout/reintentos/offline pertenece a la capa Backplane/Remote Devices, no al driver RTU básico.

## Master cooperativo — recomendado

```cpp
#include <JWPLC_ModbusRTU.h>

uint8_t remoteInputs = 0;
uint32_t nextRequestMs = 0;

void setup()
{
    JWPLC_ModbusRTU.begin(247, 115200, SERIAL_8N1);
}

void loop()
{
    JWPLC_ModbusRTU.task();

    if (JWPLC_ModbusRTU.masterDone())
    {
        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            // remoteInputs ya contiene FC02 I0_0..I0_7.
        }
        else
        {
            // Revisar masterResult() / lastErrorString().
        }

        JWPLC_ModbusRTU.clearMasterResult();
        nextRequestMs = millis() + 50;
    }

    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone() &&
        (int32_t)(millis() - nextRequestMs) >= 0)
    {
        JWPLC_ModbusRTU.requestReadDiscreteInputs(
            2, 0, 8, &remoteInputs, 250);
    }
}
```

Las funciones `request...()` **solo inician** la transacción. Un retorno `true` significa solicitud aceptada; no significa que la respuesta ya haya llegado.

Mientras exista una transacción pendiente, `task()` debe ejecutarse con frecuencia. El timeout también se resuelve desde este motor cooperativo, por lo que el `loop()` puede seguir atendiendo lógica, E/S, display, Ethernet y watchdogs.

## Contrato del Master cooperativo

- una sola transacción Master activa a la vez;
- llamar `task()` frecuentemente;
- no iniciar otra solicitud mientras `masterBusy()` sea `true`;
- buffers de lectura deben seguir existiendo hasta `masterDone()`;
- buffers de bits son empaquetados LSB-first;
- al terminar, revisar `masterSucceeded()` o `masterResult()`;
- llamar `clearMasterResult()` antes del siguiente ciclo.

API de estado:

```cpp
masterBusy()
masterDone()
masterSucceeded()
masterState()
masterResult()
clearMasterResult()
task()
```

## API síncrona explícita

Las variantes bloqueantes usan internamente el mismo motor cooperativo para evitar mantener dos parsers Master distintos:

```cpp
uint8_t inputs = 0;

bool ok = JWPLC_ModbusRTU.readDiscreteInputsSync(
    2, 0, 8, &inputs, 250);

bool ok2 = JWPLC_ModbusRTU.writeSingleCoilSync(
    2, 0, true, 250);
```

También están disponibles:

```text
readCoilsSync()
readHoldingRegistersSync()
readInputRegistersSync()
writeSingleRegisterSync()
writeMultipleCoilsSync()
```

Los nombres históricos `readHoldingRegisters()` y `writeSingleRegister()` se conservan temporalmente durante Alpha7 como wrappers hacia las variantes `...Sync()`. Código nuevo debe usar `request...()` o el sufijo `Sync` de forma explícita.

## Multidrop — Alpha7

Durante la validación M2 + S1 + S2 se detectó que un Slave podía leer en una sola ejecución de `poll()` la request dirigida a otro nodo y la respuesta correspondiente ya acumuladas en el FIFO UART. El parser anterior trataba ambos ADU como una única trama y registraba falsos CRC.

Alpha7 usa la estructura de la función Modbus para delimitar ADUs antes de validar CRC:

- FC01/02/03/04 request: 8 bytes;
- FC01/02 response: `5 + byteCount`;
- FC03/04 response: `5 + byteCount`;
- FC05/06 request/response: 8 bytes;
- FC0F/10 request: `9 + byteCount`;
- FC0F/10 response: 8 bytes;
- exception response: 5 bytes.

El Master también conoce de antemano la longitud esperada de las funciones soportadas y no cierra una respuesta larga por un `frameGap` intermedio.

El servidor drena todos los bytes UART disponibles en cada `task()` para evitar fragmentar requests FC0F/10 largas por un límite artificial de lectura.

El tráfico dirigido a otros Slaves y los tails ambiguos de respuestas ajenas se descartan sin contaminar `crcErrors`, `rxFrames` ni `lastError` del nodo local.

## Estado y estadísticas

```cpp
const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();
```

Contadores disponibles:

```text
rxFrames
txFrames
requestsOk
crcErrors
exceptionsSent
masterTimeouts
```

Errores relevantes:

```text
JWPLC_MODBUS_OK
JWPLC_MODBUS_DISABLED
JWPLC_MODBUS_NOT_STARTED
JWPLC_MODBUS_INVALID_SLAVE_ID
JWPLC_MODBUS_INVALID_REGISTER_MAP
JWPLC_MODBUS_TIMEOUT
JWPLC_MODBUS_CRC_ERROR
JWPLC_MODBUS_EXCEPTION
JWPLC_MODBUS_INVALID_RESPONSE
JWPLC_MODBUS_BUFFER_OVERFLOW
JWPLC_MODBUS_UNSUPPORTED_FUNCTION
JWPLC_MODBUS_BUSY
JWPLC_MODBUS_TRANSPORT_ERROR
```

## Indicador BUS

`JWPLC_Display.setBusLedAuto(true)` combina el estado RS-485 con el último error Modbus. Los códigos visibles incluyen `TMO`, `CRC`, `EXC`, `RSP`, `OVF` y `FUN`.

## Precompilación durante Alpha7

Mientras se valida el motor cooperativo y el soporte Remote I/O, `JWPLC_ModbusRTU` se compila temporalmente desde source. El archive precompilado anterior no debe reutilizarse porque no contiene la API ni el parser Alpha7.

Antes de cerrar Alpha7 se debe regenerar `src/esp32/libJWPLC_ModbusRTU.a`, auditar sus símbolos, restaurar `precompiled=full` y repetir la validación multidrop/Remote I/O con el archive nuevo.

## OpenPLC / Backplane / Remote I/O

La política para integración PLC es usar el Master cooperativo. OpenPLC sigue siendo una integración externa/opcional y no una dependencia del runtime Arduino normal.

La capa Backplane/Remote Devices debe ejecutar `JWPLC_ModbusRTU.task()` con frecuencia y manejar política de reintentos, estado ONLINE/OFFLINE, datos stale y fail-safe sin bloquear el scan PLC.

## Estado

```text
JWPLC ESP32 2.1.0-alpha.7 — en desarrollo
JWPLC_ModbusRTU 1.0.0
Master: cooperativo por defecto
Remote I/O digital: FC01/02/05/15 integrado
Registers: FC03/04/06/16 integrado en Slave; FC03/04/06 en Master
Sync: explícito para pruebas/commissioning
```
