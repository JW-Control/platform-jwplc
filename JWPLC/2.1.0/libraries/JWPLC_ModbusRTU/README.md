# JWPLC_ModbusRTU

Librería Modbus RTU del package **JWPLC ESP32** para el puerto RS-485 integrado
del **JWPLC Basic**.

La API permite usar el equipo como:

- **Slave** Modbus RTU;
- **Master cooperativo** recomendado para aplicaciones PLC;
- **Master síncrono** cuando una espera bloqueante es aceptable.

Alpha12 mantiene las APIs conocidas y añade un motor cooperativo más eficiente
sin obligar al usuario a configurar detalles internos del transporte RS-485.

---

## Inicio rápido

### Slave

```cpp
#include <JWPLC_ModbusRTU.h>

uint16_t holding[8] = {};

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusRTU.setHoldingRegisters(holding, 8);
    JWPLC_ModbusRTU.begin(2, 115200, SERIAL_8N1);
}

void loop()
{
    JWPLC_ModbusRTU.task();

    // HR0 cambia cada segundo.
    holding[0] = millis() / 1000;
}
```

Configuración del ejemplo:

```text
Slave ID = 2
Baud      = 115200
Formato   = 8N1
```

### Master ASYNC

El motor **ASYNC** es el recomendado y el default del package.

```cpp
#include <JWPLC_ModbusRTU.h>

uint16_t values[4] = {};
uint32_t nextRead = 0;

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusRTU.begin(247, 115200, SERIAL_8N1);
    JWPLC_ModbusRTU.motor(ASYNC);
}

void loop()
{
    JWPLC_ModbusRTU.task();

    if (JWPLC_ModbusRTU.masterDone())
    {
        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            Serial.println(values[0]);
        }
        else
        {
            Serial.println(JWPLC_ModbusRTU.lastErrorString());
        }

        JWPLC_ModbusRTU.clearMasterResult();
        nextRead = millis() + 1000;
    }

    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone() &&
        (int32_t)(millis() - nextRead) >= 0)
    {
        JWPLC_ModbusRTU.readHoldingRegisters(
            2,      // Slave destino
            0,      // Dirección inicial
            4,      // Cantidad
            values, // Buffer destino
            1000);  // Timeout
    }
}
```

En modo ASYNC, una operación devuelve `true` cuando la solicitud fue aceptada
e iniciada. El resultado final se consulta después.

---

## Configuración básica

Formas disponibles:

```cpp
JWPLC_ModbusRTU.begin();
JWPLC_ModbusRTU.begin(slaveId);
JWPLC_ModbusRTU.begin(slaveId, baud, config);
```

Defaults de la librería:

```text
Slave ID = 1
Baud     = 19200
Formato  = SERIAL_8E1
```

Para instalaciones reales se recomienda indicar los valores explícitamente:

```cpp
JWPLC_ModbusRTU.begin(2, 115200, SERIAL_8N1);
```

Estado básico:

```cpp
JWPLC_ModbusRTU.isReady();
JWPLC_ModbusRTU.slaveId();
JWPLC_ModbusRTU.baudRate();
JWPLC_ModbusRTU.config();
JWPLC_ModbusRTU.end();
```

---

## Motor Master: ASYNC y SYNC

Selección:

```cpp
JWPLC_ModbusRTU.motor(ASYNC); // recomendado, default
JWPLC_ModbusRTU.motor(SYNC);  // bloqueante
```

Consulta:

```cpp
JWPLC_ModbusRTU.motor();
JWPLC_ModbusRTU.asyncMotor();
```

### ASYNC

La llamada inicia la transacción y retorna. El `loop()` continúa trabajando.

Debe mantenerse:

```cpp
JWPLC_ModbusRTU.task();
```

y consultar:

```cpp
JWPLC_ModbusRTU.masterBusy();
JWPLC_ModbusRTU.masterDone();
JWPLC_ModbusRTU.masterSucceeded();
JWPLC_ModbusRTU.masterResult();
JWPLC_ModbusRTU.clearMasterResult();
```

### SYNC

La misma operación espera hasta completar la transacción:

```cpp
JWPLC_ModbusRTU.motor(SYNC);

bool ok = JWPLC_ModbusRTU.readHoldingRegisters(
    2, 0, 4, values, 1000);
```

En SYNC, `true` significa que la transacción terminó correctamente.

---

## Funciones Modbus soportadas

| FC | Función |
|---:|---|
| 01 | Read Coils |
| 02 | Read Discrete Inputs |
| 03 | Read Holding Registers |
| 04 | Read Input Registers |
| 05 | Write Single Coil |
| 06 | Write Single Register |
| 15 | Write Multiple Coils |
| 16 | Write Multiple Registers |

---

## API Master principal

La API recomendada mantiene los mismos nombres en ASYNC y SYNC:

| Operación | API |
|---|---|
| Leer Coils | `readCoils()` |
| Leer Discrete Inputs | `readDiscreteInputs()` |
| Leer Holding Registers | `readHoldingRegisters()` |
| Leer Input Registers | `readInputRegisters()` |
| Escribir un Coil | `writeSingleCoil()` |
| Escribir un Holding Register | `writeSingleRegister()` |
| Escribir varios Coils | `writeMultipleCoils()` |
| Escribir varios Holding Registers | `writeMultipleRegisters()` |

Ejemplos:

```cpp
uint16_t regs[4];

JWPLC_ModbusRTU.readHoldingRegisters(
    2, 0, 4, regs, 1000);

JWPLC_ModbusRTU.writeSingleRegister(
    2, 10, 1234, 1000);

uint16_t data[3] = {100, 200, 300};

JWPLC_ModbusRTU.writeMultipleRegisters(
    2, 20, 3, data, 1000);
```

Para FC01/FC02/FC15, los bits se almacenan empaquetados LSB-first.

---

## Variantes explícitas

También se conservan las APIs explícitas.

### Cooperativas

```text
requestReadCoils()
requestReadDiscreteInputs()
requestReadHoldingRegisters()
requestReadInputRegisters()
requestWriteSingleCoil()
requestWriteSingleRegister()
requestWriteMultipleCoils()
requestWriteMultipleRegisters()
```

Son equivalentes a elegir el motor ASYNC de forma explícita.

### Síncronas

```text
readCoilsSync()
readDiscreteInputsSync()
readHoldingRegistersSync()
readInputRegistersSync()
writeSingleCoilSync()
writeSingleRegisterSync()
writeMultipleCoilsSync()
writeMultipleRegistersSync()
```

Son útiles para compatibilidad o commissioning.

Para código nuevo se recomienda la API principal + `motor(ASYNC/SYNC)`.

---

## Crear un mapa Slave

### Coils

```cpp
uint8_t coils[2] = {};

JWPLC_ModbusRTU.setCoils(coils, 16);
```

Helpers:

```cpp
bool value;

JWPLC_ModbusRTU.getCoil(0, value);
JWPLC_ModbusRTU.setCoil(0, true);
JWPLC_ModbusRTU.coilCount();
```

### Discrete Inputs

```cpp
uint8_t inputs[2] = {};

JWPLC_ModbusRTU.setDiscreteInputs(inputs, 16);
```

Consulta:

```cpp
bool value;
JWPLC_ModbusRTU.getDiscreteInput(0, value);
JWPLC_ModbusRTU.discreteInputCount();
```

### Holding Registers

```cpp
uint16_t holding[16] = {};

JWPLC_ModbusRTU.setHoldingRegisters(holding, 16);
```

Helpers:

```cpp
uint16_t value;

JWPLC_ModbusRTU.getHoldingRegister(0, value);
JWPLC_ModbusRTU.setHoldingRegister(0, 1234);
JWPLC_ModbusRTU.holdingRegisterCount();
```

### Input Registers

```cpp
uint16_t inputRegisters[16] = {};

JWPLC_ModbusRTU.setInputRegisters(inputRegisters, 16);
```

Consulta:

```cpp
uint16_t value;

JWPLC_ModbusRTU.getInputRegister(0, value);
JWPLC_ModbusRTU.inputRegisterCount();
```

Los Input Registers y Discrete Inputs son de sólo lectura desde el punto de
vista del Master remoto. El sketch puede actualizar sus buffers localmente.

---

## Servicio del Slave

Mientras se use Modbus RTU debe llamarse frecuentemente:

```cpp
void loop()
{
    JWPLC_ModbusRTU.task();
}
```

`poll()` es un alias disponible para el mismo servicio.

Para Remote I/O o fail-safe también existen:

```cpp
JWPLC_ModbusRTU.hasValidRequest();
JWPLC_ModbusRTU.lastValidRequestMs();

JWPLC_ModbusRTU.hasCoilWrite();
JWPLC_ModbusRTU.lastCoilWriteMs();
```

Ejemplo de watchdog simple:

```cpp
if (JWPLC_ModbusRTU.hasValidRequest() &&
    millis() - JWPLC_ModbusRTU.lastValidRequestMs() > 2000)
{
    // Aplicar estado seguro.
}
```

---

## Timing de trama

La mayoría de sketches no necesita modificarlo.

API disponible:

```cpp
JWPLC_ModbusRTU.setFrameGapMs(5);
JWPLC_ModbusRTU.frameGapMs();

JWPLC_ModbusRTU.setFrameGapUs(5000);
JWPLC_ModbusRTU.frameGapUs();
```

El default público continúa siendo 5 ms.

---

## Errores y diagnóstico

Último error:

```cpp
JWPLC_ModbusRTU.lastError();
JWPLC_ModbusRTU.lastErrorString();
```

Configuración activa:

```cpp
JWPLC_ModbusRTU.configString();
```

Estado completo:

```cpp
JWPLC_ModbusRTU.printStatus(Serial);
```

Estadísticas principales:

```cpp
const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();

Serial.println(s.rxFrames);
Serial.println(s.txFrames);
Serial.println(s.requestsOk);
Serial.println(s.crcErrors);
Serial.println(s.masterTimeouts);
```

Reset:

```cpp
JWPLC_ModbusRTU.resetStats();
```

---

## Indicador BUS en la TFT

El Display puede mostrar automáticamente el estado RS-485/Modbus:

```cpp
JWPLC_Display.setBusLedAuto(true);
```

Entre los códigos de diagnóstico visibles pueden aparecer:

```text
TMO
CRC
EXC
RSP
OVF
FUN
```

---

## Ejemplos incluidos

```text
01.ModbusRTU_Slave_Holding
02.ModbusRTU_Master_Read
03.ModbusRTU_Master_Write
```

Uso recomendado para una prueba con dos JWPLC Basic:

1. cargar `01.ModbusRTU_Slave_Holding` en el Slave;
2. cargar `02.ModbusRTU_Master_Read` en el Master;
3. probar `03.ModbusRTU_Master_Write` para escritura FC06.

Configuración común de esos ejemplos:

```text
Slave ID = 2
115200 8N1
```

---

## Qué no necesita configurar el usuario

Alpha12 optimiza internamente:

- manejo TX sobre RS-485 AutoDirection;
- recepción y framing;
- CRC;
- coexistencia con el resto del runtime.

Esos detalles no son necesarios para usar Modbus RTU desde un sketch normal.

---

## Estado Alpha12

```text
JWPLC_ModbusRTU 1.0.0
MOTOR_DEFAULT=ASYNC
SYNC_COMPATIBILITY=PRESERVED
FC01_02_03_04_05_06_15_16=SUPPORTED
REMOTE_IO_DIGITAL=VALIDATED
PRECOMPILED_RELEASE_LIKE=ACTIVE
```

Archive cualificado para el package Alpha12:

```text
Bytes  : 292390
SHA256 : 424ed3f462bb57cce0019d690486e612d5f243c40ae62d44d3ad973b0a521085
```

El perfil de coexistencia de Alpha12 validó RTU simultáneo con Modbus TCP,
UDP, Display y demás periféricos. Las cifras de benchmark son evidencia de
capacidad, no una garantía hard-real-time.


---

# Referencia completa de API pública

Esta sección enumera **todos los métodos públicos soportados para el usuario**
de `JWPLC_ModbusRTU`. Los métodos avanzados se documentan también, pero están
marcados para evitar que se usen por accidente.

El objeto global es:

```cpp
JWPLC_ModbusRTU
```

## Inicialización y cierre

| Función | Qué hace | Ejemplo |
|---|---|---|
| `begin()` | Inicia con defaults públicos. | `JWPLC_ModbusRTU.begin();` |
| `begin(slaveId)` | Inicia con ID indicado y baud/config por defecto. | `JWPLC_ModbusRTU.begin(2);` |
| `begin(slaveId, baud, config)` | Inicia con configuración completa. | `JWPLC_ModbusRTU.begin(2, 115200, SERIAL_8N1);` |
| `end()` | Detiene Modbus RTU. | `JWPLC_ModbusRTU.end();` |

## Consulta de configuración

| Función | Qué devuelve | Ejemplo |
|---|---|---|
| `isReady()` | Si la librería está lista. | `if (JWPLC_ModbusRTU.isReady()) { ... }` |
| `slaveId()` | ID local configurado. | `uint8_t id = JWPLC_ModbusRTU.slaveId();` |
| `baudRate()` | Baud configurado. | `uint32_t baud = JWPLC_ModbusRTU.baudRate();` |
| `effectiveBaudRate()` | Baud efectivo del transporte. | `uint32_t baud = JWPLC_ModbusRTU.effectiveBaudRate();` |
| `config()` | Configuración serial activa. | `uint32_t cfg = JWPLC_ModbusRTU.config();` |

## Selección de motor

| Función | Uso | Ejemplo |
|---|---|---|
| `motor(mode)` | Selecciona ASYNC o SYNC. | `JWPLC_ModbusRTU.motor(ASYNC);` |
| `motor()` | Consulta el motor activo. | `JWPLCModbusMotor m = JWPLC_ModbusRTU.motor();` |
| `asyncMotor()` | Indica si el motor activo es ASYNC. | `if (JWPLC_ModbusRTU.asyncMotor()) { ... }` |

## Timing de trama

| Función | Uso | Ejemplo |
|---|---|---|
| `setFrameGapMs(ms)` | Define gap en milisegundos. | `JWPLC_ModbusRTU.setFrameGapMs(5);` |
| `frameGapMs()` | Lee el gap en ms. | `uint16_t gap = JWPLC_ModbusRTU.frameGapMs();` |
| `setFrameGapUs(us)` | Define gap fino en microsegundos. | `JWPLC_ModbusRTU.setFrameGapUs(5000);` |
| `frameGapUs()` | Lee el gap en µs. | `uint32_t gap = JWPLC_ModbusRTU.frameGapUs();` |

## Mapas Slave: Coils

| Función | Uso | Ejemplo |
|---|---|---|
| `setCoils(bits, count)` | Publica mapa de Coils. | `JWPLC_ModbusRTU.setCoils(coils, 16);` |
| `coilCount()` | Cantidad publicada. | `uint16_t n = JWPLC_ModbusRTU.coilCount();` |
| `getCoil(address, value)` | Lee un Coil local. | `bool v; JWPLC_ModbusRTU.getCoil(0, v);` |
| `setCoil(address, value)` | Escribe un Coil local. | `JWPLC_ModbusRTU.setCoil(0, true);` |

## Mapas Slave: Discrete Inputs

| Función | Uso | Ejemplo |
|---|---|---|
| `setDiscreteInputs(bits, count)` | Publica mapa de entradas discretas. | `JWPLC_ModbusRTU.setDiscreteInputs(inputs, 16);` |
| `discreteInputCount()` | Cantidad publicada. | `uint16_t n = JWPLC_ModbusRTU.discreteInputCount();` |
| `getDiscreteInput(address, value)` | Lee una entrada local. | `bool v; JWPLC_ModbusRTU.getDiscreteInput(0, v);` |

## Mapas Slave: Holding Registers

| Función | Uso | Ejemplo |
|---|---|---|
| `setHoldingRegisters(registers, count)` | Publica Holding Registers. | `JWPLC_ModbusRTU.setHoldingRegisters(holding, 16);` |
| `holdingRegisterCount()` | Cantidad publicada. | `uint16_t n = JWPLC_ModbusRTU.holdingRegisterCount();` |
| `getHoldingRegister(address, value)` | Lee un Holding local. | `uint16_t v; JWPLC_ModbusRTU.getHoldingRegister(0, v);` |
| `setHoldingRegister(address, value)` | Escribe un Holding local. | `JWPLC_ModbusRTU.setHoldingRegister(0, 1234);` |

## Mapas Slave: Input Registers

| Función | Uso | Ejemplo |
|---|---|---|
| `setInputRegisters(registers, count)` | Publica Input Registers. | `JWPLC_ModbusRTU.setInputRegisters(inputs16, 16);` |
| `inputRegisterCount()` | Cantidad publicada. | `uint16_t n = JWPLC_ModbusRTU.inputRegisterCount();` |
| `getInputRegister(address, value)` | Lee un Input Register local. | `uint16_t v; JWPLC_ModbusRTU.getInputRegister(0, v);` |

## Actividad Slave / fail-safe

| Función | Uso | Ejemplo |
|---|---|---|
| `hasValidRequest()` | Indica si hubo al menos una request válida. | `if (JWPLC_ModbusRTU.hasValidRequest()) { ... }` |
| `lastValidRequestMs()` | Timestamp de última request válida. | `uint32_t t = JWPLC_ModbusRTU.lastValidRequestMs();` |
| `hasCoilWrite()` | Indica si hubo escritura de Coil. | `if (JWPLC_ModbusRTU.hasCoilWrite()) { ... }` |
| `lastCoilWriteMs()` | Timestamp de última escritura de Coil. | `uint32_t t = JWPLC_ModbusRTU.lastCoilWriteMs();` |

## Servicio cooperativo

| Función | Uso | Ejemplo |
|---|---|---|
| `task()` | Avanza Slave/Master. Recomendada. | `JWPLC_ModbusRTU.task();` |
| `poll()` | Alias de servicio. | `JWPLC_ModbusRTU.poll();` |

## Master ASYNC explícito: lecturas

| Función | Ejemplo |
|---|---|
| `requestReadCoils()` | `JWPLC_ModbusRTU.requestReadCoils(2, 0, 8, bits, 1000);` |
| `requestReadDiscreteInputs()` | `JWPLC_ModbusRTU.requestReadDiscreteInputs(2, 0, 8, bits, 1000);` |
| `requestReadHoldingRegisters()` | `JWPLC_ModbusRTU.requestReadHoldingRegisters(2, 0, 4, regs, 1000);` |
| `requestReadInputRegisters()` | `JWPLC_ModbusRTU.requestReadInputRegisters(2, 0, 4, regs, 1000);` |

## Master ASYNC explícito: escrituras

| Función | Ejemplo |
|---|---|
| `requestWriteSingleCoil()` | `JWPLC_ModbusRTU.requestWriteSingleCoil(2, 0, true, 1000);` |
| `requestWriteSingleRegister()` | `JWPLC_ModbusRTU.requestWriteSingleRegister(2, 0, 1234, 1000);` |
| `requestWriteMultipleCoils()` | `JWPLC_ModbusRTU.requestWriteMultipleCoils(2, 0, 8, bits, 1000);` |
| `requestWriteMultipleRegisters()` | `JWPLC_ModbusRTU.requestWriteMultipleRegisters(2, 0, 4, regs, 1000);` |

## Estado Master

| Función | Uso | Ejemplo |
|---|---|---|
| `masterBusy()` | Transacción en curso. | `if (JWPLC_ModbusRTU.masterBusy()) { ... }` |
| `masterDone()` | Resultado pendiente de consumir. | `if (JWPLC_ModbusRTU.masterDone()) { ... }` |
| `masterSucceeded()` | Última transacción terminó OK. | `if (JWPLC_ModbusRTU.masterSucceeded()) { ... }` |
| `masterState()` | Estado detallado del motor. | `auto s = JWPLC_ModbusRTU.masterState();` |
| `masterResult()` | Código final de la transacción. | `auto r = JWPLC_ModbusRTU.masterResult();` |
| `clearMasterResult()` | Libera el resultado para la siguiente operación. | `JWPLC_ModbusRTU.clearMasterResult();` |

## Master SYNC explícito: lecturas

| Función | Ejemplo |
|---|---|
| `readCoilsSync()` | `bool ok = JWPLC_ModbusRTU.readCoilsSync(2, 0, 8, bits, 1000);` |
| `readDiscreteInputsSync()` | `bool ok = JWPLC_ModbusRTU.readDiscreteInputsSync(2, 0, 8, bits, 1000);` |
| `readHoldingRegistersSync()` | `bool ok = JWPLC_ModbusRTU.readHoldingRegistersSync(2, 0, 4, regs, 1000);` |
| `readInputRegistersSync()` | `bool ok = JWPLC_ModbusRTU.readInputRegistersSync(2, 0, 4, regs, 1000);` |

## Master SYNC explícito: escrituras

| Función | Ejemplo |
|---|---|
| `writeSingleCoilSync()` | `bool ok = JWPLC_ModbusRTU.writeSingleCoilSync(2, 0, true, 1000);` |
| `writeSingleRegisterSync()` | `bool ok = JWPLC_ModbusRTU.writeSingleRegisterSync(2, 0, 1234, 1000);` |
| `writeMultipleCoilsSync()` | `bool ok = JWPLC_ModbusRTU.writeMultipleCoilsSync(2, 0, 8, bits, 1000);` |
| `writeMultipleRegistersSync()` | `bool ok = JWPLC_ModbusRTU.writeMultipleRegistersSync(2, 0, 4, regs, 1000);` |

## API unificada por motor: lecturas

| Función | Ejemplo |
|---|---|
| `readCoils()` | `JWPLC_ModbusRTU.readCoils(2, 0, 8, bits, 1000);` |
| `readDiscreteInputs()` | `JWPLC_ModbusRTU.readDiscreteInputs(2, 0, 8, bits, 1000);` |
| `readHoldingRegisters()` | `JWPLC_ModbusRTU.readHoldingRegisters(2, 0, 4, regs, 1000);` |
| `readInputRegisters()` | `JWPLC_ModbusRTU.readInputRegisters(2, 0, 4, regs, 1000);` |

## API unificada por motor: escrituras

| Función | Ejemplo |
|---|---|
| `writeSingleCoil()` | `JWPLC_ModbusRTU.writeSingleCoil(2, 0, true, 1000);` |
| `writeSingleRegister()` | `JWPLC_ModbusRTU.writeSingleRegister(2, 0, 1234, 1000);` |
| `writeMultipleCoils()` | `JWPLC_ModbusRTU.writeMultipleCoils(2, 0, 8, bits, 1000);` |
| `writeMultipleRegisters()` | `JWPLC_ModbusRTU.writeMultipleRegisters(2, 0, 4, regs, 1000);` |

## Diagnóstico general

| Función | Uso | Ejemplo |
|---|---|---|
| `lastError()` | Código del último error. | `auto e = JWPLC_ModbusRTU.lastError();` |
| `lastErrorString()` | Texto del último error. | `Serial.println(JWPLC_ModbusRTU.lastErrorString());` |
| `configString()` | Resumen de configuración. | `Serial.println(JWPLC_ModbusRTU.configString());` |
| `stats()` | Referencia a estadísticas. | `const auto &s = JWPLC_ModbusRTU.stats();` |
| `resetStats()` | Reinicia contadores. | `JWPLC_ModbusRTU.resetStats();` |
| `printStatus(out)` | Imprime diagnóstico completo. | `JWPLC_ModbusRTU.printStatus(Serial);` |

## API avanzada de transporte

Estas funciones son públicas por compatibilidad/qualification, pero **no son
necesarias para un sketch normal**.

| Función | Ejemplo | Recomendación |
|---|---|---|
| `setQueuedTxEnabled(enabled)` | `JWPLC_ModbusRTU.setQueuedTxEnabled(true);` | Avanzada |
| `queuedTxEnabled()` | `bool x = JWPLC_ModbusRTU.queuedTxEnabled();` | Diagnóstico |
| `queuedTxActive()` | `bool x = JWPLC_ModbusRTU.queuedTxActive();` | Diagnóstico |
| `setBulkRxEnabled(enabled)` | `JWPLC_ModbusRTU.setBulkRxEnabled(true);` | Qualification |
| `bulkRxEnabled()` | `bool x = JWPLC_ModbusRTU.bulkRxEnabled();` | Qualification |
| `setEarlyServerDispatchEnabled(enabled)` | `JWPLC_ModbusRTU.setEarlyServerDispatchEnabled(true);` | Qualification |
| `earlyServerDispatchEnabled()` | `bool x = JWPLC_ModbusRTU.earlyServerDispatchEnabled();` | Qualification |

Para aplicación normal mantenga los defaults del package.

## Helpers CRC

También existen helpers públicos para herramientas o protocolos auxiliares:

| Función | Ejemplo |
|---|---|
| `setCrcLookupEnabled(enabled)` | `JWPLC_ModbusRTU.setCrcLookupEnabled(true);` |
| `crcLookupEnabled()` | `bool x = JWPLC_ModbusRTU.crcLookupEnabled();` |
| `crc16(data, length)` | `uint16_t crc = JWPLC_ModbusRTU.crc16(data, len);` |
| `checkCRC(frame, length)` | `bool ok = JWPLC_ModbusRTU.checkCRC(frame, len);` |
| `appendCRC(frame, payloadLength)` | `JWPLC_ModbusRTU.appendCRC(frame, payloadLen);` |

Estos helpers no son necesarios al usar las operaciones Modbus de alto nivel.
