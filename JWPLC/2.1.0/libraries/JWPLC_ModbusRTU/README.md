# JWPLC_ModbusRTU

`JWPLC_ModbusRTU` es la librería nativa del ecosistema JWPLC para usar Modbus RTU sobre el puerto RS-485 del JWPLC Basic.

Internamente usa `JWPLC_RS485`, así que el usuario no configura `Serial2`, RX/TX ni pines del transceptor.

Puedes usarla directamente en JWPLC Basic sin `#include` manual:

```cpp
JWPLC_ModbusRTU.begin();
```

> OpenPLC no está integrado todavía. `JWPLC_ModbusRTU` provee la base Modbus RTU propia del package JWPLC y puede servir para futuras pruebas de integración OpenPLC/Modbus, pero alpha31 no incluye OpenPLC real.

## 1. Qué es Modbus RTU

Modbus RTU es un protocolo industrial maestro/esclavo usado en PLCs, variadores, sensores, medidores, controladores y HMIs.

La lógica básica es:

```txt
Master / Cliente  -> pregunta
Slave / Servidor  -> responde
```

El slave no habla por iniciativa propia. Solo responde si la trama tiene su ID, CRC correcto, función soportada y dirección válida.

Por eso los mensajes como `Read failed` o `Write failed` normalmente los muestra el master.

## 2. RS-485 vs Modbus RTU

RS-485 es la capa física:

```txt
A/B, transceptor, cable, terminación, bias
```

Modbus RTU es el protocolo:

```txt
Slave ID, function code, address, data, CRC16
```

En JWPLC:

```txt
JWPLC_RS485      -> puerto físico RS-485
JWPLC_ModbusRTU  -> protocolo Modbus RTU
```

## 3. Default

```cpp
JWPLC_ModbusRTU.begin();
```

equivale a:

```cpp
JWPLC_ModbusRTU.begin(1, 19200, SERIAL_8E1);
```

Default:

```txt
Slave ID: 1
Baudrate: 19200
Formato:  SERIAL_8E1
```

Este default es conservador y típico para Modbus RTU industrial.

## 4. Por qué no inicia automáticamente

`JWPLC_ModbusRTU` es nativo, pero no auto-iniciado.

Comparte `Serial2` con `JWPLC_RS485`, y cada modo puede necesitar configuración distinta:

```txt
JWPLC_RS485.begin()      -> 115200, SERIAL_8N1
JWPLC_ModbusRTU.begin()  -> 19200, SERIAL_8E1
```

Por eso el usuario debe elegir explícitamente el modo que quiere usar.

## 5. Master y Slave

### Slave / Servidor

Un slave espera consultas y responde.

Ejemplo: un JWPLC configurado como slave puede exponer registros internos para que otro equipo los lea.

### Master / Cliente

Un master inicia las consultas.

Ejemplo: un JWPLC configurado como master puede leer registros de un variador o escribir consignas en otro equipo.

## 6. Por qué en modo master usamos ID local 247

Esta parte suele confundir al inicio.

En Modbus RTU, una solicitud contiene el ID del slave objetivo, pero no contiene un ID del master.

Esta llamada:

```cpp
JWPLC_ModbusRTU.readHoldingRegisters(1, 0, 4, values, 1000);
```

significa:

```txt
Preguntar al slave ID 1
Leer desde address 0
Leer 4 registros
Guardar en values
Timeout de 1000 ms
```

El `1` ahí sí es el equipo objetivo.

Entonces, ¿por qué antes se usó esto?

```cpp
JWPLC_ModbusRTU.begin(247, 19200, SERIAL_8E1);
```

Porque la clase también puede trabajar como slave. El parámetro de `begin()` es el ID local de esta instancia si alguien la consulta como slave.

Cuando el JWPLC trabaja solo como master, ese ID local no se usa como “remitente” en la trama Modbus RTU. Por eso se usa `247`: es un ID alto y poco probable, útil para inicializar el stack sin chocar con slaves comunes.

Resumen:

```txt
JWPLC_ModbusRTU.begin(247, ...)
  -> ID local de esta instancia.

JWPLC_ModbusRTU.readHoldingRegisters(1, ...)
  -> slave objetivo real.
```

Si algún día el mismo JWPLC funciona como master y slave a la vez, entonces usa en `begin()` el ID real con el que otros masters podrían consultarlo.

## 7. Address 0 vs registro 40001

En muchos manuales se habla de:

```txt
40001
40002
40003
```

Pero en la trama Modbus RTU real se usa dirección base 0.

Equivalencia habitual:

```txt
40001 -> address 0
40002 -> address 1
40003 -> address 2
```

En esta librería usamos address base 0.

```cpp
JWPLC_ModbusRTU.readHoldingRegisters(1, 0, 4, values, 1000);
```

lee:

```txt
HR0, HR1, HR2, HR3
```

## 8. Funciones soportadas

Slave:

| Código | Función | Estado |
|---:|---|---|
| `0x03` | Read Holding Registers | Soportado |
| `0x06` | Write Single Register | Soportado |
| `0x10` | Write Multiple Registers | Soportado |

Master:

| API | Función |
|---|---|
| `readHoldingRegisters()` | `0x03` |
| `writeSingleRegister()` | `0x06` |

Futuro:

```txt
0x01 Read Coils
0x02 Read Discrete Inputs
0x04 Read Input Registers
0x05 Write Single Coil
0x0F Write Multiple Coils
```

## 9. Holding Registers

Un holding register es un registro de 16 bits:

```cpp
uint16_t
```

Para crear un mapa:

```cpp
uint16_t holdingRegs[16];
```

Para vincularlo a Modbus:

```cpp
JWPLC_ModbusRTU.setHoldingRegisters(holdingRegs, 16);
```

## 10. Ejemplo: JWPLC como Slave

```cpp
uint16_t holdingRegs[8];

void setup()
{
    Serial.begin(115200);
    delay(1200);

    for (uint8_t i = 0; i < 8; i++)
    {
        holdingRegs[i] = 1000 + i;
    }

    JWPLC_ModbusRTU.setHoldingRegisters(holdingRegs, 8);
    JWPLC_ModbusRTU.begin(); // ID 1, 19200, SERIAL_8E1
    JWPLC_ModbusRTU.printStatus(Serial);
}

void loop()
{
    holdingRegs[0] = millis() / 1000;
    JWPLC_ModbusRTU.task();
}
```

La línea más importante en modo slave es:

```cpp
JWPLC_ModbusRTU.task();
```

Debe ejecutarse constantemente en `loop()`.

## 11. Ejemplo: JWPLC como Master

```cpp
uint16_t values[4];

void setup()
{
    Serial.begin(115200);
    delay(1200);

    JWPLC_ModbusRTU.begin(247, 19200, SERIAL_8E1);
    JWPLC_ModbusRTU.printStatus(Serial);
}

void loop()
{
    bool ok = JWPLC_ModbusRTU.readHoldingRegisters(1, 0, 4, values, 1000);

    if (ok)
    {
        Serial.print("Read OK | HR0: ");
        Serial.print(values[0]);
        Serial.print(" HR1: ");
        Serial.print(values[1]);
        Serial.print(" HR2: ");
        Serial.print(values[2]);
        Serial.print(" HR3: ");
        Serial.println(values[3]);
    }
    else
    {
        Serial.print("Read failed: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
    }

    delay(1000);
}
```

## 12. Ejemplo: escribir un registro

```cpp
uint16_t valueToWrite = 0;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    JWPLC_ModbusRTU.begin(247, 19200, SERIAL_8E1);
}

void loop()
{
    valueToWrite++;

    bool ok = JWPLC_ModbusRTU.writeSingleRegister(
        1,             // slave objetivo
        1,             // address
        valueToWrite,  // valor
        1000           // timeout
    );

    if (ok)
    {
        Serial.print("Write OK | HR1 = ");
        Serial.println(valueToWrite);
    }
    else
    {
        Serial.print("Write failed: ");
        Serial.println(JWPLC_ModbusRTU.lastErrorString());
    }

    delay(2000);
}
```

## 13. API principal

### `begin()`

```cpp
JWPLC_ModbusRTU.begin();
```

Inicializa como slave ID 1 a `19200, SERIAL_8E1`.

### `begin(slaveId)`

```cpp
JWPLC_ModbusRTU.begin(5);
```

Inicializa con ID local personalizado y default `19200, SERIAL_8E1`.

### `begin(slaveId, baud, config)`

```cpp
JWPLC_ModbusRTU.begin(1, 9600, SERIAL_8N1);
```

Inicializa con ID, velocidad y formato personalizados.

### `end()`

```cpp
JWPLC_ModbusRTU.end();
```

Cierra Modbus y libera RS-485.

### `isReady()`

```cpp
if (JWPLC_ModbusRTU.isReady())
{
    Serial.println("Modbus listo");
}
```

### `setHoldingRegisters()`

```cpp
JWPLC_ModbusRTU.setHoldingRegisters(holdingRegs, 16);
```

Vincula el mapa de holding registers.

### `getHoldingRegister()`

```cpp
uint16_t value;
JWPLC_ModbusRTU.getHoldingRegister(0, value);
```

Lee un holding register local desde código.

### `setHoldingRegister()`

```cpp
JWPLC_ModbusRTU.setHoldingRegister(1, 1234);
```

Escribe un holding register local desde código.

### `task()` / `poll()`

```cpp
JWPLC_ModbusRTU.task();
```

Procesa solicitudes en modo slave. `poll()` es equivalente.

### `readHoldingRegisters()`

```cpp
JWPLC_ModbusRTU.readHoldingRegisters(1, 0, 4, values, 1000);
```

Función master para leer registros.

### `writeSingleRegister()`

```cpp
JWPLC_ModbusRTU.writeSingleRegister(1, 1, 1234, 1000);
```

Función master para escribir un registro.

### `crc16()`

```cpp
uint16_t crc = JWPLC_ModbusRTU.crc16(data, length);
```

Calcula CRC16 Modbus.

### `stats()`

```cpp
const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();
```

Devuelve estadísticas.

### `lastErrorString()`

```cpp
Serial.println(JWPLC_ModbusRTU.lastErrorString());
```

Devuelve último error como texto.

### `printStatus()`

```cpp
JWPLC_ModbusRTU.printStatus(Serial);
```

Imprime estado general.

## 14. Estadísticas

Campos de `JWPLCModbusRTUStats`:

| Campo | Significado |
|---|---|
| `rxFrames` | Tramas recibidas |
| `txFrames` | Tramas transmitidas |
| `requestsOk` | Solicitudes procesadas correctamente |
| `crcErrors` | Errores CRC |
| `exceptionsSent` | Excepciones enviadas |
| `masterTimeouts` | Timeouts en modo master |

## 15. Errores comunes

### `Timeout`

El master preguntó, pero no llegó respuesta.

Revisar slave encendido, ID, baudrate, paridad, A/B, cable, COM/GND y que el slave llame `task()`.

### `CRC error`

La trama llegó corrupta. Revisar ruido, baudrate, paridad, terminación y cableado.

### `Modbus exception`

El slave respondió con error. Puede ser función no soportada, dirección fuera de rango o cantidad inválida.

### `Invalid response`

El master recibió algo que no coincide con lo esperado. Puede haber ID duplicado, respuesta incompleta o configuración incorrecta.

## 16. Reglas de oro

1. Un master pregunta y un slave responde.
2. Dos slaves no conversan entre sí.
3. Todos deben usar el mismo baudrate y formato.
4. Cada slave debe tener ID único.
5. En modo slave, `task()` debe estar en `loop()`.
6. En modo master puro, puedes iniciar con ID local `247`.

## 17. Ejemplos incluidos

```txt
ModbusRTU_CRC_Test
ModbusRTU_Slave_HoldingRegisters
ModbusRTU_Master_ReadHoldingRegisters
ModbusRTU_Master_WriteSingleRegister
```

## 18. Validado en alpha28

- CRC16 con vector conocido.
- JWPLC Basic v2.0.0 como slave.
- JWPLC Basic v2.0.0 como master.
- JWPLC Basic v1.4.1 como master externo.
- JWPLC Basic v1.4.1 como slave externo.
- Lectura de holding registers.
- Escritura de single holding register.
- API nativa sin includes manuales.
- Comunicación real por RS-485.
- CRC errors en 0 durante pruebas de banco.

## 19. Futuro OpenPLC

OpenPLC no forma parte de alpha28.

La integración futura debería mapear:

```txt
Coils              -> salidas digitales / bits de control
Discrete Inputs    -> entradas digitales
Holding Registers  -> variables internas/configurables
Input Registers    -> estados/mediciones
```

## Validación alpha31

Para alpha31 se recomienda validar:

- `ModbusRTU_CRC_Test`;
- `ModbusRTU_Slave_HoldingRegisters`;
- `ModbusRTU_Master_ReadHoldingRegisters`;
- `ModbusRTU_Master_WriteSingleRegister`;
- CRC16 con vector conocido;
- slave holding registers;
- master read;
- master write;
- prueba física por RS-485 si hay equipo externo disponible;
- `JWPLC_RS485` no inicializado simultáneamente con otra configuración incompatible.

OpenPLC queda fuera de alpha31.

## Estado

Documentación revisada para:

```text
JWPLC ESP32 2.0.0-alpha.31
JWPLC_ModbusRTU 1.0.0
```