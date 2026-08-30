# JWPLC_ModbusRTU

Librería del package **JWPLC ESP32** para Modbus RTU sobre `JWPLC_RS485`.

Desde Alpha7, el criterio de diseño del Master es **cooperativo/no bloqueante por defecto**. La API síncrona se mantiene de forma explícita para commissioning, pruebas o sketches simples, pero no es la ruta recomendada para lógica PLC, Backplane ni Remote I/O.

## Inicio

### Slave

```cpp
#include <JWPLC_ModbusRTU.h>

uint16_t holding[16] = {};

void setup()
{
    JWPLC_ModbusRTU.setHoldingRegisters(holding, 16);
    JWPLC_ModbusRTU.begin(1, 115200, SERIAL_8N1);
}

void loop()
{
    JWPLC_ModbusRTU.task();
}
```

### Master cooperativo — recomendado

```cpp
#include <JWPLC_ModbusRTU.h>

uint16_t values[4];
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
            // values[] ya contiene la respuesta FC03.
        }
        else
        {
            // Revisar masterResult() / lastErrorString().
        }

        JWPLC_ModbusRTU.clearMasterResult();
        nextRequestMs = millis() + 100;
    }

    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone() &&
        (int32_t)(millis() - nextRequestMs) >= 0)
    {
        JWPLC_ModbusRTU.requestReadHoldingRegisters(
            1, 0, 4, values, 1000);
    }
}
```

`requestReadHoldingRegisters()` y `requestWriteSingleRegister()` **solo inician** la transacción. Un retorno `true` significa que la solicitud fue aceptada; no significa que la respuesta ya haya llegado.

Mientras exista una transacción pendiente, `task()` debe ejecutarse con frecuencia. El timeout también se resuelve desde este motor cooperativo, por lo que el `loop()` puede seguir atendiendo lógica, E/S, display, Ethernet y watchdogs.

## Contrato del Master cooperativo

- una sola transacción Master activa a la vez;
- llamar `task()` frecuentemente;
- no iniciar otra solicitud mientras `masterBusy()` sea `true`;
- el buffer `destination` de una lectura debe seguir existiendo hasta que `masterDone()` sea `true`;
- al terminar, revisar `masterSucceeded()` o `masterResult()`;
- llamar `clearMasterResult()` antes de iniciar el siguiente ciclo.

API principal:

```cpp
requestReadHoldingRegisters(...)
requestWriteSingleRegister(...)
masterBusy()
masterDone()
masterSucceeded()
masterState()
masterResult()
clearMasterResult()
task()
```

## API síncrona explícita

Para pruebas, commissioning o sketches donde bloquear sea aceptable:

```cpp
bool ok = JWPLC_ModbusRTU.readHoldingRegistersSync(
    1, 0, 4, values, 1000);

bool ok2 = JWPLC_ModbusRTU.writeSingleRegisterSync(
    1, 2, 1234, 1000);
```

Estas llamadas esperan hasta recibir la respuesta o vencer el timeout. Durante esa espera el flujo de aplicación que realizó la llamada no continúa.

Los nombres históricos `readHoldingRegisters()` y `writeSingleRegister()` se conservan temporalmente durante Alpha7 como wrappers de compatibilidad hacia las variantes `...Sync()`. Código nuevo debe usar la API cooperativa o el sufijo `Sync` de forma explícita.

## Funciones implementadas

### Slave

| Código | Función |
|---:|---|
| `0x03` | Read Holding Registers |
| `0x06` | Write Single Register |
| `0x10` | Write Multiple Registers |

### Master cooperativo

| Código | API |
|---:|---|
| `0x03` | `requestReadHoldingRegisters()` |
| `0x06` | `requestWriteSingleRegister()` |

La API Sync expone las mismas dos operaciones Master en esta etapa.

## Multidrop — Alpha7

Durante la validación M2 + S1 + S2 se detectó que un Slave podía leer en una sola ejecución de `poll()` la request dirigida a otro nodo y la respuesta correspondiente ya acumuladas en el FIFO UART. El parser anterior trataba ambos ADU como una única trama y registraba un falso error CRC.

Alpha7 separa el contenido recibido usando la estructura de la función Modbus antes de validar CRC:

- FC01/02/03/04 request: 8 bytes;
- FC01/02/03/04 response: `5 + byteCount`;
- FC05/06 request/response: 8 bytes;
- FC0F/10 request: `9 + byteCount`;
- FC0F/10 response: 8 bytes;
- exception response: 5 bytes.

El CRC valida una longitud derivada de la estructura del protocolo; ya no se usa para buscar arbitrariamente el primer prefijo coincidente.

Validación física Alpha7 con dos Slaves en el mismo bus:

```text
S1 CRC=0, EXCEPTIONS=0
S2 CRC=0, EXCEPTIONS=0
M2 W/R/V S1=0/0/0
M2 W/R/V S2=0/0/0
```

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

Mientras se valida el nuevo motor Master cooperativo, `JWPLC_ModbusRTU` se compila temporalmente desde source. El archive precompilado anterior no debe reutilizarse porque no contiene la API ni el parser Alpha7.

Antes de cerrar Alpha7 se debe regenerar `src/esp32/libJWPLC_ModbusRTU.a`, auditar sus símbolos, restaurar `precompiled=full` y repetir la validación multidrop con el archive nuevo.

## OpenPLC / Backplane / Remote I/O

La política para integración PLC es usar únicamente el Master cooperativo. OpenPLC sigue siendo una integración externa/opcional y no una dependencia del runtime Arduino normal.

La capa de Backplane/Remote I/O debe ejecutar `JWPLC_ModbusRTU.task()` con frecuencia y programar las solicitudes sin bloquear el ciclo de aplicación.

## Estado

```text
JWPLC ESP32 2.1.0-alpha.7 — en desarrollo
JWPLC_ModbusRTU 1.0.0
Master: cooperativo por defecto
Sync: explícito para pruebas/commissioning
```
