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

En Alpha13:

- el **Server** implementa FC01, FC02, FC03, FC04, FC05, FC06, FC15 y FC16;
- el **Client** también declara e implementa esos ocho códigos de función;
- **implementado no significa validado en hardware**: FC03 cuenta con
  evidencia física explícita de Alpha13. FC06 se documentó como API de usuario;
  las demás operaciones del Client tienen implementación visible en header/CPP,
  pero no se acredita para ellas una campaña física independiente.

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


### Client: operaciones disponibles — Básico a avanzado

Una función **FC** es el número de una acción Modbus: leer o escribir un
tipo de dato. Por ejemplo, **FC03** significa leer Holding Registers.

**Es importante distinguir tres conceptos:**

- **Disponible en el código:** existe en el archivo `.h` y su implementación
  `.cpp`, y se puede utilizar desde un sketch.
- **Compila/enlaza:** la función forma parte del package y su biblioteca.
  No demuestra que un equipo remoto responda correctamente.
- **Validada físicamente:** hay una prueba real específica con un resultado
  registrado. En Alpha13, la campaña física documentada de Client/Server
  destaca **FC03**; no se debe extender ese resultado a los otros siete FC.

| FC | Qué permite | Método del Client | Nivel sugerido |
|---|---|---|---|
| 01 | Leer Coils (bits) | `requestReadCoils()` | Intermedio; sin H específica |
| 02 | Leer Discrete Inputs (bits) | `requestReadDiscreteInputs()` | Intermedio; sin H específica |
| 03 | Leer Holding Registers (16 bits) | `requestReadHoldingRegisters()` | **Básico; H física acotada** |
| 04 | Leer Input Registers (16 bits) | `requestReadInputRegisters()` | Intermedio; sin H específica |
| 05 | Escribir un Coil | `requestWriteSingleCoil()` | Avanzado; sin H específica |
| 06 | Escribir un Holding Register | `requestWriteSingleRegister()` | Intermedio; sin H específica equivalente a FC03 |
| 15 | Escribir varios Coils | `requestWriteMultipleCoils()` | Avanzado; sin H específica |
| 16 | Escribir varios Holding Registers | `requestWriteMultipleRegisters()` | Avanzado; sin H específica |

Las funciones respetan las direcciones **offset desde 0** y retornan
`bool` al **aceptar o rechazar el inicio**. Un `true` no confirma que
el dispositivo remoto haya completado la operación. La respuesta final se
consulta con `done()` y `succeeded()`, seguida de `clearResult()`.

### Lección 1 — Registros de 16 bits (FC03 y FC04)

Un registro almacena un valor entre 0 y 65535 (`uint16_t`).
Para leer tres Holding Registers a partir de HR0:

```cpp
uint16_t datos[3] = {};

bool iniciada =
    JWPLC_ModbusTCPClient.requestReadHoldingRegisters(
        0,       // Dirección inicial.
        3,       // Cantidad de registros.
        datos,   // Arreglo de destino.
        1000);   // Timeout en milisegundos.
```

Para leer tres Input Registers con **FC04**, cambia solo el método:

```cpp
bool iniciada =
    JWPLC_ModbusTCPClient.requestReadInputRegisters(
        0,
        3,
        datos,
        1000);
```

**No examines `datos` inmediatamente después de iniciar.** Primero
espera a `done()`; comprueba `succeeded()`; luego procesa el resultado
y llama a `clearResult()`. El ejemplo completo de FC03, más arriba,
enseña este ciclo con un `loop()` cooperativo.

### Lección 2 — ¿Cómo se guarda un Coil? (FC01 y FC02)

Un Coil es un **bit**: solo puede valer 0 o 1. Para ahorrar memoria, esta
API almacena hasta **ocho bits por byte**. El bit 0 corresponde al
primer Coil solicitado (orden **LSB-first**).

Si quieres leer 10 Coils necesitas 2 bytes, no 10:

```cpp
uint8_t bits[2] = {0, 0};

JWPLC_ModbusTCPClient.requestReadCoils(
    0,       // Coil inicial.
    10,      // Leer 10 bits.
    bits,    // 2 bytes para 10 bits.
    1000);
```

**Después de `done()` y `succeeded()`**, puedes obtener sus valores:

```cpp
bool coil0 = (bits[0] & 0x01) != 0;
bool coil7 = (bits[0] & 0x80) != 0;
bool coil8 = (bits[1] & 0x01) != 0;
bool coil9 = (bits[1] & 0x02) != 0;
```

Para **FC02**, el método equivalente es:

```cpp
JWPLC_ModbusTCPClient.requestReadDiscreteInputs(
    0, 10, bits, 1000);
```

**Fórmula:** para `cantidad` de bits, reserva al menos
`(cantidad + 7) / 8` bytes. No pases un arreglo demasiado pequeño.
Los bits sobrantes del último byte de respuesta se limpian según el
contrato del código.

### Lección 3 — Escribir un solo valor (FC05 y FC06)

Con FC06 escribes un registro de 16 bits:

```cpp
JWPLC_ModbusTCPClient.requestWriteSingleRegister(
    1,     // Dirección.
    250,   // Nuevo valor.
    1000);
```

Con FC05 escribes un Coil (`true` = ON, `false` = OFF):

```cpp
JWPLC_ModbusTCPClient.requestWriteSingleCoil(
    0, true, 1000);
```

La ejecución también termina mediante `done()` y `succeeded()`.
El servidor puede rechazar una dirección o una escritura aunque el inicio
de la solicitud haya sido aceptado.

**Seguridad industrial:** practicar FC05/FC06 únicamente con mapas de
prueba que no accionen motores, contactores ni otras cargas reales.
Una orden remota de salida necesita permisos, enclavamientos y estado seguro.

### Lección 4 — Escribir varios valores (FC15 y FC16)

Para **FC16**, pasa un arreglo de registros de 16 bits:

```cpp
const uint16_t consignas[2] = {100, 200};

JWPLC_ModbusTCPClient.requestWriteMultipleRegisters(
    0,          // Registro inicial.
    2,          // Cantidad de registros.
    consignas,  // Valores que se enviarán.
    1000);
```

Para **FC15**, los Coils se empacan de ocho en ocho, como en la lectura
de bits. Este ejemplo escribiría cuatro Coils: ON, OFF, ON, ON.

```cpp
const uint8_t bobinas[1] = {0b00001101};

JWPLC_ModbusTCPClient.requestWriteMultipleCoils(
    0, 4, bobinas, 1000);
```

En FC15 los bits del último byte que excedan la cantidad declarada no
forman parte de la solicitud. La implementación limpia ese relleno antes
de transmitirlo. No excedas los límites del buffer documentados por el
header; **FC15** acepta hasta 1968 bits y **FC16** hasta 123 registros
por petición. En la práctica también rigen los límites del equipo remoto.

### Reglas que debes conservar para todos los FC

1. Ejecuta `JWPLC_ModbusTCPClient.task()` periódicamente en `loop()`.
2. Inicia una nueva operación solo cuando no haya una anterior ocupada ni
   un resultado pendiente: comprueba `!busy() && !done()`.
3. No cambies ni destruyas el arreglo de destino de lectura mientras la
   operación siga pendiente.
4. Revisa `done()`, luego `succeeded()` y `resultString()` en caso de
   error; llama a `clearResult()` antes de pedir otro FC.
5. Empieza probando **FC03** con un servidor de laboratorio. Las operaciones
   adicionales están implementadas, pero requieren ensayos de integración
   específicos antes de afirmar que están validadas para producción.

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

### Confundir «implementado» con «validado físicamente»

El Client declara ocho FC, pero la prueba física documentada de Alpha13
solo demuestra el alcance que se ensayó, principalmente FC03. FC01/02/04/05/15/16
necesitan validación específica antes de presentarlos como probados para
una instalación industrial. FC06 está documentado como método de usuario,
sin atribuirle una H física equivalente a FC03.

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

## Mejoras de estabilidad en Alpha13

El Server y el Client mantienen las mismas APIs de usuario. Internamente,
las conexiones sobre el W5500 administran mejor el cierre y la reutilización
de sockets cuando Ethernet comparte el bus SPI con SD, TFT y FRAM.

- Se mejora la recuperación después de perder o recuperar el cable Ethernet.
- Una transacción TCP no debe apropiarse de un socket que ya se cerró
  o que pertenece a una operación nueva.
- Modbus TCP puede coexistir con **Modbus RTU por RS-485** sin que sea
  necesario detener el servicio RTU para esperar al servidor TCP.

Ejemplo de uso recomendado: configura el Server una vez y consulta su estado.

```cpp
JWPLC_ModbusTCP.beginServer(1, 502);

if (JWPLC_ModbusTCP.serverReady())
{
    // El Server está disponible para los Clients.
}
```

En el banco con dos JWPLC Basic conectados a un router aislado se comprobó
FC03 con cuatro registros en ambos servidores, además de tráfico TCP/UDP
en R1. Esto no significa que todas las funciones Modbus se hayan ensayado
bajo fallos físicos simultáneos. R3 interno (contención/leases/cierre diferido),
R4 prolongado y DHCP/DNS específicos siguen sin prueba integral.

## Compatibilidad

`poll()` permanece como alias de `task()` tanto en Server como en Client.

No se recomienda implementar Modbus TCP directamente sobre
`EthernetClient` si `JWPLC_ModbusTCP` cubre tu necesidad.

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.13 (candidata)
JWPLC_ModbusTCP 0.1.0
```
