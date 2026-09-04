# JWPLC_ModbusRTU

Librería del package **JWPLC ESP32** para Modbus RTU sobre `JWPLC_RS485`.

El Master recomendado usa un motor **cooperativo/no bloqueante**. Las variantes `...Sync()` se conservan para commissioning o sketches donde una espera bloqueante sea aceptable.

## Configuración

Ejemplo Slave:

```cpp
JWPLC_ModbusRTU.begin(2, 115200, SERIAL_8N1);
```

Ejemplo Master:

```cpp
JWPLC_ModbusRTU.begin(247, 115200, SERIAL_8N1);
```

El ID `247` se usa en los ejemplos como ID local interno del Master; el ID destino se especifica en cada `request...()`.

## Funciones soportadas

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

| Código | API |
|---:|---|
| `0x01` | `requestReadCoils()` |
| `0x02` | `requestReadDiscreteInputs()` |
| `0x03` | `requestReadHoldingRegisters()` |
| `0x04` | `requestReadInputRegisters()` |
| `0x05` | `requestWriteSingleCoil()` |
| `0x06` | `requestWriteSingleRegister()` |
| `0x0F` | `requestWriteMultipleCoils()` |

## Mapas Slave

```cpp
uint8_t coils = 0;
uint8_t discreteInputs = 0;
uint16_t holding[8] = {};
uint16_t inputRegisters[8] = {};

JWPLC_ModbusRTU.setCoils(&coils, 8);
JWPLC_ModbusRTU.setDiscreteInputs(&discreteInputs, 8);
JWPLC_ModbusRTU.setHoldingRegisters(holding, 8);
JWPLC_ModbusRTU.setInputRegisters(inputRegisters, 8);
```

Coils/Discrete Inputs usan bits empaquetados LSB-first. Bit 0 corresponde a la dirección 0.

Helpers de mapa:

```cpp
JWPLC_ModbusRTU.getCoil(...);
JWPLC_ModbusRTU.setCoil(...);
JWPLC_ModbusRTU.getDiscreteInput(...);
JWPLC_ModbusRTU.getHoldingRegister(...);
JWPLC_ModbusRTU.setHoldingRegister(...);
JWPLC_ModbusRTU.getInputRegister(...);
```

## Slave: atender requests

```cpp
void loop()
{
    JWPLC_ModbusRTU.task();
}
```

`task()` debe ejecutarse con frecuencia.

Actividad válida disponible para Remote I/O/fail-safe:

```cpp
JWPLC_ModbusRTU.hasValidRequest();
JWPLC_ModbusRTU.lastValidRequestMs();
JWPLC_ModbusRTU.hasCoilWrite();
JWPLC_ModbusRTU.lastCoilWriteMs();
```

## Master cooperativo

Una solicitud cooperativa **inicia** la transacción y retorna. `true` significa que fue aceptada, no que la respuesta ya llegó.

```cpp
uint16_t values[4];

JWPLC_ModbusRTU.requestReadHoldingRegisters(
    2, 0, 4, values, 1000);
```

Contrato:

1. llamar `JWPLC_ModbusRTU.task()` frecuentemente;
2. mantener vivos los buffers hasta terminar;
3. no iniciar otra transacción mientras `masterBusy()` sea `true`;
4. esperar `masterDone()`;
5. revisar `masterSucceeded()` / `masterResult()`;
6. llamar `clearMasterResult()` antes del siguiente ciclo.

Estado:

```cpp
JWPLC_ModbusRTU.masterBusy();
JWPLC_ModbusRTU.masterDone();
JWPLC_ModbusRTU.masterSucceeded();
JWPLC_ModbusRTU.masterState();
JWPLC_ModbusRTU.masterResult();
JWPLC_ModbusRTU.clearMasterResult();
```

## API síncrona explícita

```text
readCoilsSync()
readDiscreteInputsSync()
readHoldingRegistersSync()
readInputRegistersSync()
writeSingleCoilSync()
writeSingleRegisterSync()
writeMultipleCoilsSync()
```

Los wrappers históricos `readHoldingRegisters()` y `writeSingleRegister()` continúan disponibles por compatibilidad, pero código nuevo debe preferir `request...()` o una variante `...Sync()` explícita.

## Estado y estadísticas

```cpp
JWPLC_ModbusRTU.lastError();
JWPLC_ModbusRTU.lastErrorString();
JWPLC_ModbusRTU.configString();
JWPLC_ModbusRTU.printStatus(Serial);

const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();
```

Estadísticas:

```text
rxFrames
txFrames
requestsOk
crcErrors
exceptionsSent
masterTimeouts
```

## Multidrop / Remote I/O

Alpha7 corrigió el parser multidrop para delimitar ADUs según la estructura Modbus antes de validar CRC. Se validaron FC01, FC02, FC05 y FC15 en Remote I/O, pérdida de bus, fail-safe y recuperación sin reiniciar el Master.

Los ejemplos avanzados Remote I/O existentes se conservan para esa validación.

## Indicador BUS

`JWPLC_Display.setBusLedAuto(true)` combina el estado del transporte RS-485 con el resultado Modbus. Entre los códigos visibles están:

```text
TMO CRC EXC RSP OVF FUN
```

## Precompilación

Al cierre de Alpha7 se restauró `precompiled=full` y se regeneró el archive final:

```text
Archivo : src/esp32/libJWPLC_ModbusRTU.a
Bytes   : 231062
SHA256  : 444BE3A04079A579252B2737FE6070E00ADCA949FD176880588FE69561B2A79F
```

Alpha8 no modifica la fuente de `JWPLC_ModbusRTU`, por lo que ese archive continúa siendo la base validada.

## Ejemplos numerados para taller

Configuración común:

```text
Slave ID = 2
115200 8N1
```

```text
01.ModbusRTU_Slave_Holding
02.ModbusRTU_Master_Read
03.ModbusRTU_Master_Write
```

`02` y `03` pueden probarse directamente contra `01` cargado en otro JWPLC Basic.

## Estado Alpha8

```text
JWPLC ESP32 2.1.0-alpha.8
JWPLC_ModbusRTU 1.0.0
Master cooperativo: recomendado
Master Sync: explícito
Remote I/O digital: validado en Alpha7
```
