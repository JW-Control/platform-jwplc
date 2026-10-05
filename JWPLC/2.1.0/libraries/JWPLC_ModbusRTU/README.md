# JWPLC_ModbusRTU

`JWPLC_ModbusRTU` permite comunicar el **JWPLC Basic** con otros equipos
industriales usando **Modbus RTU** sobre el puerto RS-485 integrado.

Puedes usar el JWPLC como **Slave** o como **Master**. Para código nuevo, el
Master trabaja por defecto en modo `ASYNC`, de forma que una comunicación no
detenga el resto de tu `loop()`.

## ¿Para qué sirve?

Modbus RTU es un protocolo industrial muy común para comunicar:

- PLC;
- variadores;
- medidores;
- módulos de E/S;
- sensores;
- controladores de temperatura;
- otros JWPLC.

La librería soporta:

```text
FC01  Read Coils
FC02  Read Discrete Inputs
FC03  Read Holding Registers
FC04  Read Input Registers
FC05  Write Single Coil
FC06  Write Single Register
FC15  Write Multiple Coils
FC16  Write Multiple Registers
```

## Qué hace automáticamente el JWPLC

No necesitas:

- manejar DE/RE del transceptor RS-485;
- calcular CRC manualmente;
- construir tramas Modbus byte por byte;
- controlar los pines internos del puerto;
- gestionar el sentido TX/RX.

La librería usa `JWPLC_RS485` como transporte.

## Inicio rápido

Este ejemplo crea un Slave ID 2 con ocho Holding Registers.

```cpp
#include <JWPLC_ModbusRTU.h>

uint16_t holding[8] = {};

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusRTU.setHoldingRegisters(
        holding,
        8);

    if (!JWPLC_ModbusRTU.begin(
            2,
            115200,
            SERIAL_8N1))
    {
        Serial.println(
            JWPLC_ModbusRTU.lastErrorString());
    }
}

void loop()
{
    JWPLC_ModbusRTU.task();

    holding[0] =
        millis() / 1000;
}
```

Desde un Master externo puedes leer HR0..HR7 del Slave ID 2.

## Conceptos básicos

### Master y Slave

Un **Master** inicia una solicitud.

Un **Slave** responde cuando recibe una solicitud dirigida a su ID.

Ejemplo:

```text
JWPLC Master
    |
    |  leer HR0 del Slave 2
    v
Equipo Slave ID 2
```

### Slave ID

En Modbus RTU cada Slave tiene una dirección entre 1 y 247.

```cpp
JWPLC_ModbusRTU.begin(
    2,
    115200,
    SERIAL_8N1);
```

Aquí el ID local es `2`.

### Tipos de datos Modbus

- **Coil**: un bit que el Master puede leer y escribir.
- **Discrete Input**: un bit de sólo lectura desde el Master.
- **Holding Register**: registro de 16 bits que el Master puede leer y escribir.
- **Input Register**: registro de 16 bits de sólo lectura desde el Master.

### Dirección Modbus

La API usa direcciones internas empezando en `0`.

Por ejemplo:

```cpp
readHoldingRegisters(
    2,
    0,
    4,
    values,
    1000);
```

significa:

```text
Slave       = 2
Inicio      = 0
Cantidad    = 4 registros
Timeout     = 1000 ms
```

### ASYNC

El modo recomendado del Master es:

```cpp
JWPLC_ModbusRTU.motor(ASYNC);
```

Es también el modo por defecto.

Una llamada como:

```cpp
JWPLC_ModbusRTU.readHoldingRegisters(...);
```

**inicia** la operación y regresa enseguida.

El resultado llega después mientras `task()` sigue ejecutándose.

## Ejemplo 1 — Básico: Slave con Holding Registers

```cpp
#include <JWPLC_ModbusRTU.h>

uint16_t holding[8] =
{
    0, 100, 200, 300,
    400, 500, 600, 700
};

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusRTU.setHoldingRegisters(
        holding,
        8);

    JWPLC_ModbusRTU.begin(
        2,
        115200,
        SERIAL_8N1);
}

void loop()
{
    JWPLC_ModbusRTU.task();

    holding[0] =
        millis() / 1000;

    static uint32_t ultimoReporte = 0;

    if (millis() - ultimoReporte >= 1000)
    {
        ultimoReporte = millis();

        Serial.print("HR0=");
        Serial.print(holding[0]);

        Serial.print(" HR1=");
        Serial.println(holding[1]);
    }
}
```

Un Master puede modificar `holding[1]` mediante FC06 o FC16.

## Ejemplo 2 — Intermedio: Master que lee registros

Este ejemplo lee cuatro Holding Registers del Slave 2 cada segundo.

```cpp
#include <JWPLC_ModbusRTU.h>

uint16_t values[4] = {};
uint32_t proximaLectura = 0;

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusRTU.begin(
        247,
        115200,
        SERIAL_8N1);

    JWPLC_ModbusRTU.motor(ASYNC);
}

void loop()
{
    JWPLC_ModbusRTU.task();

    const uint32_t ahora =
        millis();

    if (JWPLC_ModbusRTU.masterDone())
    {
        if (JWPLC_ModbusRTU.masterSucceeded())
        {
            Serial.print("HR0=");
            Serial.println(values[0]);
        }
        else
        {
            Serial.println(
                JWPLC_ModbusRTU.lastErrorString());
        }

        JWPLC_ModbusRTU.clearMasterResult();

        proximaLectura =
            ahora + 1000;
    }

    if (!JWPLC_ModbusRTU.masterBusy() &&
        !JWPLC_ModbusRTU.masterDone() &&
        (int32_t)(ahora - proximaLectura) >= 0)
    {
        const bool aceptada =
            JWPLC_ModbusRTU.readHoldingRegisters(
                2,
                0,
                4,
                values,
                1000);

        if (!aceptada)
        {
            proximaLectura =
                ahora + 1000;
        }
    }
}
```

En modo ASYNC, el `bool` que devuelve `readHoldingRegisters()` indica si la
solicitud fue aceptada para empezar, no si ya terminó correctamente.

## Ejemplo 3 — Aplicación real: leer una entrada remota y escribir una salida

Supongamos un Slave que publica:

- HR0: valor de proceso;
- HR1: comando.

El Master lee HR0 y después escribe HR1.

```cpp
#include <JWPLC_ModbusRTU.h>

enum class Paso
{
    Leer,
    Escribir
};

Paso paso = Paso::Leer;

uint16_t proceso = 0;
uint16_t comando = 0;

uint32_t proximaOperacion = 0;

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusRTU.begin(
        247,
        115200,
        SERIAL_8N1);

    JWPLC_ModbusRTU.motor(ASYNC);
}

void loop()
{
    JWPLC_ModbusRTU.task();

    const uint32_t ahora =
        millis();

    if (JWPLC_ModbusRTU.masterDone())
    {
        const bool ok =
            JWPLC_ModbusRTU.masterSucceeded();

        JWPLC_ModbusRTU.clearMasterResult();

        if (!ok)
        {
            Serial.println(
                JWPLC_ModbusRTU.lastErrorString());

            proximaOperacion =
                ahora + 1000;

            return;
        }

        if (paso == Paso::Leer)
        {
            comando =
                proceso > 500 ? 1 : 0;

            paso = Paso::Escribir;
        }
        else
        {
            paso = Paso::Leer;

            proximaOperacion =
                ahora + 500;
        }
    }

    if (JWPLC_ModbusRTU.masterBusy() ||
        JWPLC_ModbusRTU.masterDone() ||
        (int32_t)(ahora - proximaOperacion) < 0)
    {
        return;
    }

    if (paso == Paso::Leer)
    {
        JWPLC_ModbusRTU.readHoldingRegisters(
            2,
            0,
            1,
            &proceso,
            1000);
    }
    else
    {
        JWPLC_ModbusRTU.writeSingleRegister(
            2,
            1,
            comando,
            1000);
    }
}
```

La idea importante es no iniciar una segunda operación mientras la anterior
sigue ocupada.

## Ejemplo 4 — Avanzado de usuario: modo SYNC

Si estás haciendo commissioning o una herramienta muy simple puedes elegir:

```cpp
JWPLC_ModbusRTU.motor(SYNC);
```

y luego:

```cpp
uint16_t values[4] = {};

bool ok =
    JWPLC_ModbusRTU.readHoldingRegisters(
        2,
        0,
        4,
        values,
        1000);
```

En modo `SYNC`, la función espera hasta terminar o alcanzar el timeout.

Para la lógica principal de una máquina se recomienda `ASYNC`.

## API de usuario

### Inicialización — Básico

| Función | Uso | Nivel |
|---|---|---|
| `begin()` | Defaults: ID 1, 19200, 8E1 | Básico |
| `begin(slaveId)` | ID explícito, resto por defecto | Básico |
| `begin(slaveId,baud,config)` | Configuración completa | Básico |
| `end()` | Detener Modbus/RS-485 | Intermedio |
| `isReady()` | Saber si quedó listo | Básico |

Para proyectos reales se recomienda indicar los parámetros:

```cpp
JWPLC_ModbusRTU.begin(
    2,
    115200,
    SERIAL_8N1);
```

### Selección de motor Master — Básico / Avanzado

```cpp
JWPLC_ModbusRTU.motor(ASYNC);
JWPLC_ModbusRTU.motor(SYNC);
```

Consultar:

```cpp
JWPLC_ModbusRTU.motor();
JWPLC_ModbusRTU.asyncMotor();
```

- `ASYNC`: recomendado.
- `SYNC`: avanzado, bloqueante.

### Servicio — Básico

```cpp
JWPLC_ModbusRTU.task();
```

Debe ejecutarse frecuentemente cuando usas RTU.

`poll()` es un alias compatible.

### Crear mapas Slave — Básico / Intermedio

Coils:

```cpp
uint8_t coils[2] = {};

JWPLC_ModbusRTU.setCoils(
    coils,
    16);
```

Discrete Inputs:

```cpp
uint8_t inputs[2] = {};

JWPLC_ModbusRTU.setDiscreteInputs(
    inputs,
    16);
```

Holding Registers:

```cpp
uint16_t holding[16] = {};

JWPLC_ModbusRTU.setHoldingRegisters(
    holding,
    16);
```

Input Registers:

```cpp
uint16_t inputRegs[16] = {};

JWPLC_ModbusRTU.setInputRegisters(
    inputRegs,
    16);
```

Consultas y escritura local:

```cpp
JWPLC_ModbusRTU.coilCount();
JWPLC_ModbusRTU.getCoil(...);
JWPLC_ModbusRTU.setCoil(...);

JWPLC_ModbusRTU.discreteInputCount();
JWPLC_ModbusRTU.getDiscreteInput(...);

JWPLC_ModbusRTU.holdingRegisterCount();
JWPLC_ModbusRTU.getHoldingRegister(...);
JWPLC_ModbusRTU.setHoldingRegister(...);

JWPLC_ModbusRTU.inputRegisterCount();
JWPLC_ModbusRTU.getInputRegister(...);
```

### Master — API recomendada

Lecturas:

```cpp
readCoils()
readDiscreteInputs()
readHoldingRegisters()
readInputRegisters()
```

Escrituras:

```cpp
writeSingleCoil()
writeSingleRegister()
writeMultipleCoils()
writeMultipleRegisters()
```

Todos reciben:

- Slave destino;
- dirección inicial;
- cantidad o valor;
- buffer cuando corresponde;
- timeout.

Ejemplo:

```cpp
JWPLC_ModbusRTU.readInputRegisters(
    3,
    10,
    2,
    values,
    1000);
```

### Estado del Master — Básico / Intermedio

| Función | Uso |
|---|---|
| `masterBusy()` | Hay una operación en curso |
| `masterDone()` | Hay un resultado pendiente |
| `masterSucceeded()` | El último resultado fue correcto |
| `masterResult()` | Código detallado |
| `masterState()` | Estado de la máquina interna |
| `clearMasterResult()` | Libera el resultado para la siguiente operación |

### Diagnóstico — Intermedio

```cpp
JWPLC_ModbusRTU.lastError();
JWPLC_ModbusRTU.lastErrorString();
JWPLC_ModbusRTU.configString();
JWPLC_ModbusRTU.printStatus(Serial);
```

Estadísticas:

```cpp
const JWPLCModbusRTUStats &stats =
    JWPLC_ModbusRTU.stats();

JWPLC_ModbusRTU.resetStats();
```

### Actividad válida del Slave — Intermedio

```cpp
JWPLC_ModbusRTU.hasValidRequest();
JWPLC_ModbusRTU.lastValidRequestMs();

JWPLC_ModbusRTU.hasCoilWrite();
JWPLC_ModbusRTU.lastCoilWriteMs();
```

Son útiles para implementar watchdogs o fail-safe.

## Errores comunes

### Olvidar `task()`

En RTU, `task()` debe ejecutarse frecuentemente.

### Lanzar otra operación cuando el Master está ocupado

Comprueba:

```cpp
JWPLC_ModbusRTU.masterBusy();
```

### No limpiar el resultado

Después de procesar `masterDone()` llama:

```cpp
JWPLC_ModbusRTU.clearMasterResult();
```

### Confundir dirección Modbus con número mostrado en un manual

Muchos manuales muestran `40001`, `40002`, etc.

La API normalmente trabaja con offset:

```text
40001 -> dirección 0
40002 -> dirección 1
```

Confirma siempre la convención del fabricante.

### Configurar distinto baud/paridad en Master y Slave

Los parámetros deben coincidir.

### Usar `delay()` para “esperar la respuesta”

En modo ASYNC no hagas eso. Deja que `task()` avance la comunicación y
consulta `masterDone()`.

## API avanzada

### Frame gap

Sólo cámbialo si un equipo requiere un timing específico:

```cpp
JWPLC_ModbusRTU.setFrameGapMs(5);
JWPLC_ModbusRTU.frameGapMs();

JWPLC_ModbusRTU.setFrameGapUs(5000);
JWPLC_ModbusRTU.frameGapUs();
```

### API ASYNC explícita

Existen las variantes:

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

Son válidas, pero para código nuevo se recomienda la API unificada
`read...()/write...()` junto con `motor(ASYNC)`.

### API SYNC explícita

También existen:

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

Para código nuevo, normalmente es más claro seleccionar `motor(SYNC)` y usar
la API unificada.

### Ajustes de transporte

El header expone:

```text
setQueuedTxEnabled()
queuedTxEnabled()
queuedTxActive()
setBulkRxEnabled()
bulkRxEnabled()
setEarlyServerDispatchEnabled()
earlyServerDispatchEnabled()
```

Son controles de transporte/qualification. No son necesarios para una
aplicación Modbus RTU normal y no deben cambiarse sin una razón específica.

### Helpers CRC

```text
setCrcLookupEnabled()
crcLookupEnabled()
crc16()
checkCRC()
appendCRC()
```

Son útiles para herramientas o protocolos manuales. Las operaciones Modbus de
alto nivel ya gestionan el CRC.

## Compatibilidad

`poll()` se mantiene como alias de `task()`.

Las familias `request...()` y `...Sync()` se conservan para código
existente y usos explícitos.

Para código nuevo, el camino recomendado es:

```text
begin(...)
motor(ASYNC)
task()
read...()/write...()
masterDone()
masterSucceeded()
clearMasterResult()
```

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_ModbusRTU 1.0.0
```
