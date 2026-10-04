# JWPLC_ModbusTCP

Librería Modbus TCP nativa para **JWPLC Basic** sobre el W5500 integrado y
`JWPLC_Ethernet`.

La librería ofrece:

- **Server Modbus TCP** cooperativo;
- **Client Modbus TCP** cooperativo;
- FC01, FC02, FC03, FC04, FC05, FC06, FC15 y FC16;
- reutilización automática de la conexión del Client;
- recuperación frente a timeout o pérdida de sesión;
- coexistencia con Modbus RTU y el resto del runtime JWPLC.

---

## Uso y autoload

Modbus TCP es **opt-in**:

```cpp
#include <JWPLC_ModbusTCP.h>
```

El Ethernet del JWPLC Basic ya se inicializa y mantiene desde el runtime normal
del package.

No es necesario llamar:

```cpp
Ethernet.begin();
JWPLC_Ethernet.begin();
```

en un sketch Modbus TCP normal.

---

# Server Modbus TCP

## Ejemplo mínimo

```cpp
#include <JWPLC_ModbusTCP.h>

uint8_t coils[1] = {};
uint8_t discreteInputs[1] = {};
uint16_t holding[16] = {};
uint16_t inputRegisters[16] = {};

void setup()
{
    JWPLC_ModbusTCP.setCoils(coils, 8);
    JWPLC_ModbusTCP.setDiscreteInputs(discreteInputs, 8);
    JWPLC_ModbusTCP.setHoldingRegisters(holding, 16);
    JWPLC_ModbusTCP.setInputRegisters(inputRegisters, 16);

    JWPLC_ModbusTCP.beginServer(1, 502);
}

void loop()
{
    // El Server se atiende automáticamente cuando la librería está enlazada.

    inputRegisters[0] = millis() / 1000;
}
```

Configuración del ejemplo:

```text
Unit ID = 1
Puerto  = 502
```

`beginServer()` puede llamarse aunque DHCP todavía esté en progreso. El
Server esperará a que Ethernet esté listo.

---

## Estado del Server

```cpp
JWPLC_ModbusTCP.serverEnabled();
JWPLC_ModbusTCP.serverReady();
JWPLC_ModbusTCP.clientConnected();

JWPLC_ModbusTCP.serverState();
JWPLC_ModbusTCP.unitId();
JWPLC_ModbusTCP.port();
```

Ejemplo:

```cpp
if (JWPLC_ModbusTCP.serverReady())
{
    Serial.print("IP: ");
    Serial.println(JWPLC_Ethernet.localIP());
}
```

---

## Servicio manual opcional

Normalmente el Server recibe autoservicio cooperativo desde el package.

También están disponibles:

```cpp
JWPLC_ModbusTCP.task();
JWPLC_ModbusTCP.poll();
```

para arquitecturas donde se quiera servicio explícito.

---

# Mapas del Server

## Coils

```cpp
uint8_t coils[2] = {};

JWPLC_ModbusTCP.setCoils(coils, 16);
```

Helpers:

```cpp
bool value;

JWPLC_ModbusTCP.getCoil(0, value);
JWPLC_ModbusTCP.setCoil(0, true);
JWPLC_ModbusTCP.coilCount();
```

## Discrete Inputs

```cpp
uint8_t inputs[2] = {};

JWPLC_ModbusTCP.setDiscreteInputs(inputs, 16);
```

Consulta:

```cpp
bool value;

JWPLC_ModbusTCP.getDiscreteInput(0, value);
JWPLC_ModbusTCP.discreteInputCount();
```

## Holding Registers

```cpp
uint16_t holding[16] = {};

JWPLC_ModbusTCP.setHoldingRegisters(holding, 16);
```

Helpers:

```cpp
uint16_t value;

JWPLC_ModbusTCP.getHoldingRegister(0, value);
JWPLC_ModbusTCP.setHoldingRegister(0, 1234);
JWPLC_ModbusTCP.holdingRegisterCount();
```

## Input Registers

```cpp
uint16_t inputRegisters[16] = {};

JWPLC_ModbusTCP.setInputRegisters(inputRegisters, 16);
```

Consulta:

```cpp
uint16_t value;

JWPLC_ModbusTCP.getInputRegister(0, value);
JWPLC_ModbusTCP.inputRegisterCount();
```

Coils y Discrete Inputs usan bits empaquetados LSB-first.

---

# Client Modbus TCP

El Client utiliza el objeto global:

```cpp
JWPLC_ModbusTCPClient
```

## Configuración

```cpp
JWPLC_ModbusTCPClient.begin(
    IPAddress(192, 168, 1, 50),
    1,
    502);
```

Parámetros:

```text
Server IP
Unit ID
Puerto
```

El puerto por defecto es 502.

La conexión TCP se abre cuando se inicia la primera solicitud y puede
reutilizarse para las siguientes.

---

## Ejemplo Client FC03

```cpp
#include <JWPLC_ModbusTCP.h>

IPAddress serverIP(192, 168, 1, 50);

uint16_t regs[4] = {};
uint32_t nextRead = 0;

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusTCPClient.begin(
        serverIP,
        1,
        502);
}

void loop()
{
    JWPLC_ModbusTCPClient.task();

    if (JWPLC_ModbusTCPClient.done())
    {
        if (JWPLC_ModbusTCPClient.succeeded())
        {
            Serial.println(regs[0]);
        }
        else
        {
            Serial.println(JWPLC_ModbusTCPClient.resultString());
        }

        JWPLC_ModbusTCPClient.clearResult();
        nextRead = millis() + 1000;
    }

    if (!JWPLC_ModbusTCPClient.busy() &&
        !JWPLC_ModbusTCPClient.done() &&
        (int32_t)(millis() - nextRead) >= 0)
    {
        JWPLC_ModbusTCPClient.requestReadHoldingRegisters(
            0,
            4,
            regs,
            1000);
    }
}
```

Una función `request...()` que devuelve `true` significa que la transacción
fue aceptada e iniciada. No significa que ya haya terminado.

---

## Funciones soportadas

| FC | Función | Server | Client |
|---:|---|:---:|:---:|
| 01 | Read Coils | Sí | Sí |
| 02 | Read Discrete Inputs | Sí | Sí |
| 03 | Read Holding Registers | Sí | Sí |
| 04 | Read Input Registers | Sí | Sí |
| 05 | Write Single Coil | Sí | Sí |
| 06 | Write Single Register | Sí | Sí |
| 15 | Write Multiple Coils | Sí | Sí |
| 16 | Write Multiple Registers | Sí | Sí |

---

## API del Client

### Lecturas

```text
requestReadCoils()
requestReadDiscreteInputs()
requestReadHoldingRegisters()
requestReadInputRegisters()
```

### Escrituras

```text
requestWriteSingleCoil()
requestWriteSingleRegister()
requestWriteMultipleCoils()
requestWriteMultipleRegisters()
```

Ejemplos:

```cpp
uint16_t regs[4];

JWPLC_ModbusTCPClient.requestReadHoldingRegisters(
    0, 4, regs, 1000);

JWPLC_ModbusTCPClient.requestWriteSingleRegister(
    10, 1234, 1000);

uint16_t data[3] = {100, 200, 300};

JWPLC_ModbusTCPClient.requestWriteMultipleRegisters(
    20, 3, data, 1000);
```

Para FC01/FC02/FC15 los bits se almacenan empaquetados LSB-first.

---

## Estado del Client

La secuencia normal es:

1. llamar `task()` frecuentemente;
2. no iniciar otra operación mientras `busy()` sea `true`;
3. esperar `done()`;
4. consultar `succeeded()`;
5. procesar el resultado;
6. llamar `clearResult()`.

API:

```cpp
JWPLC_ModbusTCPClient.configured();
JWPLC_ModbusTCPClient.sessionConnected();

JWPLC_ModbusTCPClient.busy();
JWPLC_ModbusTCPClient.done();
JWPLC_ModbusTCPClient.succeeded();

JWPLC_ModbusTCPClient.state();
JWPLC_ModbusTCPClient.result();
JWPLC_ModbusTCPClient.resultString();

JWPLC_ModbusTCPClient.exceptionCode();
JWPLC_ModbusTCPClient.transactionId();

JWPLC_ModbusTCPClient.serverIP();
JWPLC_ModbusTCPClient.serverPort();
JWPLC_ModbusTCPClient.unitId();

JWPLC_ModbusTCPClient.clearResult();
```

Para cerrar el Client:

```cpp
JWPLC_ModbusTCPClient.end();
```

---

## Excepciones Modbus

El Server implementa:

```text
0x01 Illegal Function
0x02 Illegal Data Address
0x03 Illegal Data Value
0x04 Server Device Failure
```

En el Client:

```cpp
if (JWPLC_ModbusTCPClient.result() ==
    JWPLC_MODBUS_TCP_CLIENT_EXCEPTION)
{
    uint8_t code =
        JWPLC_ModbusTCPClient.exceptionCode();
}
```

---

## Timeout

### Server

```cpp
JWPLC_ModbusTCP.setFrameTimeoutMs(1000);
JWPLC_ModbusTCP.frameTimeoutMs();
```

### Client

El timeout se indica en cada operación:

```cpp
JWPLC_ModbusTCPClient.requestReadHoldingRegisters(
    0, 4, regs, 1000);
```

---

## Diagnóstico

### Server

```cpp
JWPLC_ModbusTCP.lastError();
JWPLC_ModbusTCP.lastErrorString();

JWPLC_ModbusTCP.printStatus(Serial);

const JWPLCModbusTCPStats &s =
    JWPLC_ModbusTCP.stats();

JWPLC_ModbusTCP.resetStats();
```

Contadores principales:

```text
clientConnections
rxFrames
txFrames
requestsOk
exceptionsSent
protocolErrors
frameTimeouts
busLockTimeouts
```

### Client

```cpp
JWPLC_ModbusTCPClient.result();
JWPLC_ModbusTCPClient.resultString();

JWPLC_ModbusTCPClient.printStatus(Serial);

const JWPLCModbusTCPClientStats &s =
    JWPLC_ModbusTCPClient.stats();

JWPLC_ModbusTCPClient.resetStats();
```

---

## Configuración de Ethernet

DHCP:

```cpp
JWPLC_Ethernet.useDHCP();
```

IP estática:

```cpp
JWPLC_Ethernet.setStaticIP(
    IPAddress(192, 168, 1, 20),
    IPAddress(192, 168, 1, 1),
    IPAddress(192, 168, 1, 1),
    IPAddress(255, 255, 255, 0));
```

Consultar IP:

```cpp
JWPLC_Ethernet.localIP();
JWPLC_Ethernet.isReady();
```

El runtime JWPLC mantiene Ethernet automáticamente.

---

## Funciones internas que el usuario no necesita configurar

Alpha12 incluye optimizaciones internas para:

- recepción TCP;
- envío cooperativo;
- W5500;
- arbitraje SPI;
- coexistencia con RTU/UDP/Display.

No es necesario seleccionar políticas de polling, INT, FIFO o fast-path para
usar Modbus TCP desde un sketch normal.

---

## Ejemplos incluidos

```text
01.ModbusTCP_Server
02.ModbusTCP_Client
```

El Server publica Coils, Discrete Inputs, Holding Registers e Input Registers.

El Client incluido muestra FC03 con reconexión automática de la sesión cuando
sea necesario.

---

## Estado Alpha12

```text
JWPLC_ModbusTCP 0.1.0
SERVER=VALIDATED
CLIENT=VALIDATED
FC01_02_03_04_05_06_15_16=SUPPORTED
RTU_TCP_COEXISTENCE=VALIDATED
ETHERNET_AUTOLOAD=YES
```

Durante la qualification de Alpha12 se validó coexistencia prolongada de
Modbus TCP, Modbus RTU, UDP y el full runtime. Esos benchmarks son evidencia
de capacidad del sistema y no un SLA hard-real-time.
