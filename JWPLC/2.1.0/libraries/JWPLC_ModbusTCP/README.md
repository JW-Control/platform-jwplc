# JWPLC_ModbusTCP

`JWPLC_ModbusTCP` permite comunicar el **JWPLC Basic** con equipos Modbus TCP
a través del puerto Ethernet integrado.

Puedes usar el JWPLC como **Server** para publicar datos a un SCADA, HMI o PLC,
o como **Client** para consultar/escribir datos de otro equipo. La librería usa
el Ethernet que la plataforma ya mantiene automáticamente.

## ¿Para qué sirve?

Modbus TCP utiliza los mismos conceptos de Coils y Registers que Modbus RTU,
pero transporta las solicitudes por Ethernet.

Es útil para conectar el JWPLC con:

- SCADA y HMI;
- PLC con puerto Ethernet;
- gateways;
- software de supervisión;
- otro JWPLC;
- equipos que actúan como Modbus TCP Server.

En la rama actual:

- el **Server** soporta FC01, FC02, FC03, FC04, FC05, FC06, FC15 y FC16;
- el **Client de usuario validado actualmente** ofrece FC03 y FC06.

## Qué hace automáticamente el JWPLC

No necesitas:

- inicializar manualmente el W5500 con `Ethernet.begin()`;
- configurar pines Ethernet;
- gestionar el bus SPI;
- abrir manualmente el socket del Server;
- reconfigurar Ethernet después de una pérdida temporal de enlace.

El Server recibe servicio cooperativo desde el runtime cuando la librería está
enlazada.

El Client sí necesita que tu `loop()` llame frecuentemente:

```cpp
JWPLC_ModbusTCPClient.task();
```

## Inicio rápido

Este ejemplo publica ocho Holding Registers en el puerto estándar 502.

```cpp
#include <JWPLC_ModbusTCP.h>

uint16_t holding[8] = {};

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusTCP.setHoldingRegisters(
        holding,
        8);

    if (!JWPLC_ModbusTCP.beginServer(
            1,
            502))
    {
        Serial.println("No se pudo configurar el Server");
    }
}

void loop()
{
    holding[0] =
        millis() / 1000;
}
```

Cuando Ethernet esté listo, un Client Modbus TCP podrá leer HR0..HR7.

## Conceptos básicos

### Server y Client

Un **Server** publica datos y espera solicitudes.

Un **Client** inicia esas solicitudes.

```text
Client
  |
  |  FC03: leer Holding Registers
  v
Server
```

### IP y puerto

Un Server se identifica por:

```text
IP      192.168.1.50
Puerto  502
```

El puerto estándar Modbus TCP es `502`.

### Unit ID

Modbus TCP conserva un campo Unit ID.

En una conexión directa a un JWPLC puede usarse, por ejemplo:

```text
Unit ID = 1
```

El Client y Server deben usar el valor esperado por la instalación.

### Tipos de datos

- **Coil**: bit de lectura/escritura.
- **Discrete Input**: bit de sólo lectura desde el Client.
- **Holding Register**: registro de 16 bits de lectura/escritura.
- **Input Register**: registro de 16 bits de sólo lectura desde el Client.

### Direcciones

La API usa offsets empezando en `0`.

Por ejemplo:

```cpp
requestReadHoldingRegisters(
    0,
    2,
    values,
    1000);
```

lee dos Holding Registers empezando en la dirección interna 0.

## Ejemplo 1 — Básico: Server de Holding Registers

```cpp
#include <JWPLC_ModbusTCP.h>

uint16_t holding[4] =
{
    100,
    200,
    300,
    400
};

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusTCP.setHoldingRegisters(
        holding,
        4);

    JWPLC_ModbusTCP.beginServer(
        1,
        502);
}

void loop()
{
    static uint32_t ultimoReporte = 0;

    if (millis() - ultimoReporte >= 1000)
    {
        ultimoReporte = millis();

        Serial.print("Server listo=");
        Serial.print(
            JWPLC_ModbusTCP.serverReady());

        Serial.print(" HR0=");
        Serial.println(holding[0]);
    }
}
```

Un Client externo puede leer o escribir el arreglo `holding[]`.

## Ejemplo 2 — Intermedio: Client FC03

Este ejemplo lee dos Holding Registers de un Server cada segundo.

```cpp
#include <JWPLC_ModbusTCP.h>

const IPAddress SERVER_IP(
    192, 168, 1, 50);

uint16_t values[2] = {};

uint32_t proximaLectura = 0;

void setup()
{
    Serial.begin(115200);

    if (!JWPLC_ModbusTCPClient.begin(
            SERVER_IP,
            1,
            502))
    {
        Serial.println(
            JWPLC_ModbusTCPClient.resultString());
    }
}

void loop()
{
    JWPLC_ModbusTCPClient.task();

    const uint32_t ahora =
        millis();

    if (JWPLC_ModbusTCPClient.done())
    {
        if (JWPLC_ModbusTCPClient.succeeded())
        {
            Serial.print("HR0=");
            Serial.print(values[0]);

            Serial.print(" HR1=");
            Serial.println(values[1]);
        }
        else
        {
            Serial.println(
                JWPLC_ModbusTCPClient.resultString());
        }

        JWPLC_ModbusTCPClient.clearResult();

        proximaLectura =
            ahora + 1000;
    }

    if (!JWPLC_ModbusTCPClient.busy() &&
        !JWPLC_ModbusTCPClient.done() &&
        (int32_t)(ahora - proximaLectura) >= 0)
    {
        const bool aceptada =
            JWPLC_ModbusTCPClient.requestReadHoldingRegisters(
                0,
                2,
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

La función `requestReadHoldingRegisters()` devuelve `true` cuando la
solicitud pudo empezar. El resultado final se obtiene después mediante
`done()` y `succeeded()`.

## Ejemplo 3 — Aplicación real: Server ligado a las E/S del JWPLC

Este ejemplo publica:

- HR0: bitmap de entradas I0_0..I0_7;
- HR1: bitmap de salidas Q0_0..Q0_7;
- HR2: segundos desde arranque.

Además, el Client remoto puede escribir HR1 para controlar las ocho salidas.

```cpp
#include <JWPLC_ModbusTCP.h>

uint16_t holding[3] = {};

void setup()
{
    JWPLC_ModbusTCP.setHoldingRegisters(
        holding,
        3);

    JWPLC_ModbusTCP.beginServer(
        1,
        502);
}

void loop()
{
    holding[0] =
        JWPLC_readInputs();

    const uint8_t salidasRemotas =
        (uint8_t)(holding[1] & 0x00FF);

    JWPLC_writeOutputs(
        salidasRemotas);

    holding[1] =
        JWPLC_readOutputs();

    holding[2] =
        (uint16_t)(millis() / 1000);
}
```

En una máquina real conviene añadir reglas de seguridad antes de aplicar una
orden remota directamente a las salidas.

## Ejemplo 4 — Avanzado de usuario: Client FC06

Este Client cambia el Holding Register 1 de un Server.

```cpp
#include <JWPLC_ModbusTCP.h>

const IPAddress SERVER_IP(
    192, 168, 1, 50);

uint16_t valor = 0;
uint32_t proximaEscritura = 0;

void setup()
{
    Serial.begin(115200);

    JWPLC_ModbusTCPClient.begin(
        SERVER_IP,
        1,
        502);
}

void loop()
{
    JWPLC_ModbusTCPClient.task();

    const uint32_t ahora =
        millis();

    if (JWPLC_ModbusTCPClient.done())
    {
        if (!JWPLC_ModbusTCPClient.succeeded())
        {
            Serial.println(
                JWPLC_ModbusTCPClient.resultString());
        }

        JWPLC_ModbusTCPClient.clearResult();

        proximaEscritura =
            ahora + 2000;
    }

    if (!JWPLC_ModbusTCPClient.busy() &&
        !JWPLC_ModbusTCPClient.done() &&
        (int32_t)(ahora - proximaEscritura) >= 0)
    {
        valor++;

        JWPLC_ModbusTCPClient.requestWriteSingleRegister(
            1,
            valor,
            1000);
    }
}
```

## API de usuario

### Server: iniciar — Básico

```cpp
bool aceptado =
    JWPLC_ModbusTCP.beginServer(
        unitId,
        port);
```

Parámetros:

- `unitId`: identificador Modbus.
- `port`: normalmente 502.

`true` significa que la configuración fue aceptada. Ethernet puede seguir
inicializándose; consulta `serverReady()` para saber cuándo está operativo.

### Server: estado — Básico / Intermedio

| Función | Qué indica |
|---|---|
| `serverEnabled()` | El Server fue configurado |
| `serverReady()` | Está listo para recibir solicitudes |
| `clientConnected()` | Hay un Client conectado |
| `unitId()` | Unit ID configurado |
| `port()` | Puerto configurado |
| `serverState()` | Estado detallado |

### Server: mapas — Básico / Intermedio

Coils:

```cpp
JWPLC_ModbusTCP.setCoils(
    coils,
    count);
```

Discrete Inputs:

```cpp
JWPLC_ModbusTCP.setDiscreteInputs(
    discreteInputs,
    count);
```

Holding Registers:

```cpp
JWPLC_ModbusTCP.setHoldingRegisters(
    holding,
    count);
```

Input Registers:

```cpp
JWPLC_ModbusTCP.setInputRegisters(
    inputRegisters,
    count);
```

Helpers disponibles:

```text
coilCount()
getCoil()
setCoil()

discreteInputCount()
getDiscreteInput()

holdingRegisterCount()
getHoldingRegister()
setHoldingRegister()

inputRegisterCount()
getInputRegister()
```

### Server: servicio manual — Avanzado de usuario

```cpp
JWPLC_ModbusTCP.task();
```

El Server recibe autoservicio desde el runtime cuando está enlazado, por lo que
normalmente no necesitas llamar `task()` manualmente.

`poll()` es un alias compatible.

### Server: timeout — Avanzado de usuario

```cpp
JWPLC_ModbusTCP.setFrameTimeoutMs(
    1000);

uint32_t timeout =
    JWPLC_ModbusTCP.frameTimeoutMs();
```

No lo cambies salvo que tengas una necesidad concreta.

### Server: diagnóstico — Intermedio

```cpp
JWPLC_ModbusTCP.lastError();
JWPLC_ModbusTCP.lastErrorString();
JWPLC_ModbusTCP.printStatus(Serial);

const JWPLCModbusTCPStats &stats =
    JWPLC_ModbusTCP.stats();

JWPLC_ModbusTCP.resetStats();
```

### Client: configurar — Básico

```cpp
JWPLC_ModbusTCPClient.begin(
    serverIP,
    unitId,
    port);
```

Consulta:

```cpp
JWPLC_ModbusTCPClient.configured();
JWPLC_ModbusTCPClient.serverIP();
JWPLC_ModbusTCPClient.serverPort();
JWPLC_ModbusTCPClient.unitId();
```

Cerrar:

```cpp
JWPLC_ModbusTCPClient.end();
```

### Client: servicio — Básico

```cpp
JWPLC_ModbusTCPClient.task();
```

Debe ejecutarse frecuentemente en `loop()`.

`poll()` es un alias compatible.

### Client: operaciones disponibles en esta versión

#### FC03 — leer Holding Registers

```cpp
JWPLC_ModbusTCPClient.requestReadHoldingRegisters(
    startAddress,
    quantity,
    destination,
    timeoutMs);
```

Nivel: **Básico / recomendado**.

#### FC06 — escribir un Holding Register

```cpp
JWPLC_ModbusTCPClient.requestWriteSingleRegister(
    address,
    value,
    timeoutMs);
```

Nivel: **Intermedio**.

El Server soporta más Function Codes que el Client de esta versión. No asumas
que una operación Server implica automáticamente una función Client equivalente.

### Client: estado — Básico / Intermedio

| Función | Uso |
|---|---|
| `busy()` | Hay una operación en curso |
| `done()` | Hay un resultado pendiente |
| `succeeded()` | La última operación terminó correctamente |
| `state()` | Estado detallado |
| `result()` | Código del resultado |
| `resultString()` | Resultado en texto |
| `clearResult()` | Libera el resultado |
| `sessionConnected()` | La sesión TCP sigue conectada |
| `transactionId()` | Identificador de la operación |
| `exceptionCode()` | Código de excepción Modbus si el Server respondió con una |

### Client: diagnóstico — Intermedio

```cpp
const JWPLCModbusTCPClientStats &stats =
    JWPLC_ModbusTCPClient.stats();

JWPLC_ModbusTCPClient.resetStats();

JWPLC_ModbusTCPClient.printStatus(
    Serial);
```

## Errores comunes

### Llamar `Ethernet.begin()`

No es necesario. Modbus TCP utiliza el Ethernet que mantiene el JWPLC.

### Olvidar `JWPLC_ModbusTCPClient.task()`

El Client no puede avanzar correctamente si no recibe servicio frecuente.

### Iniciar otra request cuando `busy()` es true

Espera a que termine la operación actual.

### No llamar `clearResult()`

Después de procesar `done()`, libera el resultado antes de la siguiente
operación.

### Confundir Server y Client

- Server: publica mapas.
- Client: inicia solicitudes.

### Usar direcciones 40001 directamente

La API usa offsets. Si un manual dice `40001`, normalmente corresponde al
offset `0`, pero confirma la convención del fabricante.

### Asumir que el Client actual tiene todos los FC del Server

En esta versión el camino Client destinado al usuario está implementado para:

```text
FC03
FC06
```

## API avanzada

### Estado detallado del Server

```cpp
JWPLCModbusTCPServerState state =
    JWPLC_ModbusTCP.serverState();
```

Los valores válidos se encuentran en `JWPLC_ModbusTCP.h`.

### Estado detallado del Client

```cpp
JWPLCModbusTCPClientState state =
    JWPLC_ModbusTCPClient.state();

JWPLCModbusTCPClientError result =
    JWPLC_ModbusTCPClient.result();
```

### Hooks internos/condicionales de profiling

El header puede exponer condicionalmente:

```text
jwplcSchedulerProfile()
jwplcSchedulerProfileReset()
```

Sólo existen cuando el package se compila con la opción de profiling
correspondiente.

Son herramientas de desarrollo/qualification y **no forman parte de la API de
aplicación que debe usar un sketch normal**.

## Compatibilidad

`poll()` permanece como alias de `task()` tanto en Server como en Client.

No se recomienda implementar Modbus TCP directamente sobre
`EthernetClient` si `JWPLC_ModbusTCP` cubre tu necesidad.

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_ModbusTCP 0.1.0
```
