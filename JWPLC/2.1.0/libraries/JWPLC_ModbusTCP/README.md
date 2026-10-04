# JWPLC_ModbusTCP

Librería Modbus TCP nativa para JWPLC Basic sobre `JWPLC_Ethernet` y el W5500
integrado.

Estado para el próximo release:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_ModbusTCP 0.1.0
SERVER=PASS_PHYSICAL
CLIENT=PASS_PHYSICAL
FC01_02_03_04_05_06_15_16=PASS
RTU_TCP_COEXISTENCE=PASS_PHYSICAL
```

La librería fue desarrollada históricamente bajo una rama etiquetada Alpha14,
pero su release real es Alpha12. La evidencia histórica se conserva en
`docs/v2.1.0-alpha.14/`.

## Uso y autoload

La librería es **opt-in**:

```cpp
#include <JWPLC_ModbusTCP.h>
```

No se incluye automáticamente en todos los sketches del JWPLC Basic.

El Ethernet integrado sí continúa siendo inicializado/mantenido por el runtime
normal del package. Un sketch normal no debe repetir `Ethernet.begin()` ni
`JWPLC_Ethernet.begin()` salvo que esté ejecutando una prueba manual
deliberada.

### Server autoservido cuando la librería está enlazada

Al enlazar `JWPLC_ModbusTCP`, la librería proporciona un callback cooperativo
que ejecuta `JWPLC_ModbusTCP.task()` alrededor del ciclo de usuario.

Por tanto, en el uso normal del Server no es obligatorio añadir un
`task()` manual en cada `loop()`.

Las APIs:

```cpp
JWPLC_ModbusTCP.task();
JWPLC_ModbusTCP.poll();
```

permanecen disponibles para servicio explícito, diagnóstico o arquitecturas
especiales.

### Client

El Client usa su propio objeto:

```cpp
JWPLC_ModbusTCPClient
```

y su state machine se avanza explícitamente con:

```cpp
JWPLC_ModbusTCPClient.task();
// o
JWPLC_ModbusTCPClient.poll();
```

## Server

Ejemplo mínimo:

```cpp
#include <JWPLC_ModbusTCP.h>

uint8_t coils[1] = {0};
uint8_t discreteInputs[1] = {0};
uint16_t holding[16] = {0};
uint16_t inputRegisters[16] = {0};

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
    // Lógica de aplicación.
    // El Server queda atendido por el hook cooperativo cuando la librería
    // está enlazada.
}
```

`beginServer()` acepta la configuración aunque DHCP todavía esté en progreso.
El Server espera cooperativamente a que `JWPLC_Ethernet` alcance `READY`.

Estado:

```cpp
JWPLC_ModbusTCP.serverEnabled();
JWPLC_ModbusTCP.serverReady();
JWPLC_ModbusTCP.clientConnected();
JWPLC_ModbusTCP.serverState();
JWPLC_ModbusTCP.unitId();
JWPLC_ModbusTCP.port();
```

Timeout de frame:

```cpp
JWPLC_ModbusTCP.setFrameTimeoutMs(1000);
uint32_t timeoutMs = JWPLC_ModbusTCP.frameTimeoutMs();
```

Defaults:

```text
PORT=502
UNIT_ID=1
MAX_ADU=260
FRAME_TIMEOUT_MS=1000
RX_BUDGET=64 bytes por servicio
```

## Client cooperativo

Configuración:

```cpp
#include <JWPLC_ModbusTCP.h>

uint16_t regs[4];

void setup()
{
    JWPLC_ModbusTCPClient.begin(
        IPAddress(192, 168, 1, 50),
        502,
        1);
}

void loop()
{
    JWPLC_ModbusTCPClient.task();

    if (!JWPLC_ModbusTCPClient.busy() &&
        !JWPLC_ModbusTCPClient.done())
    {
        JWPLC_ModbusTCPClient.requestReadHoldingRegisters(
            0,
            4,
            regs,
            1000);
    }

    if (JWPLC_ModbusTCPClient.done())
    {
        if (JWPLC_ModbusTCPClient.succeeded())
        {
            // regs[] actualizado
        }

        JWPLC_ModbusTCPClient.clearResult();
    }
}
```

Una llamada `request...` aceptada inicia la transacción y retorna. No implica
que la respuesta ya haya llegado.

Contrato recomendado:

1. mantener `task()/poll()` ejecutándose;
2. no iniciar otro request mientras `busy()` sea true;
3. esperar `done()`;
4. comprobar `succeeded()` / `result()`;
5. leer `exceptionCode()` cuando corresponda;
6. llamar `clearResult()` antes del siguiente ciclo.

Estado Client:

```cpp
configured();
sessionConnected();
busy();
done();
succeeded();
state();
result();
exceptionCode();
transactionId();
serverIP();
serverPort();
unitId();
clearResult();
```

Alpha12 no añade wrappers Sync bloqueantes de alto nivel para Modbus TCP.

## Funciones soportadas

| FC | Función | Server | Client |
|---:|---|:---:|:---:|
| 01 | Read Coils | PASS | PASS |
| 02 | Read Discrete Inputs | PASS | PASS |
| 03 | Read Holding Registers | PASS | PASS |
| 04 | Read Input Registers | PASS | PASS |
| 05 | Write Single Coil | PASS | PASS |
| 06 | Write Single Register | PASS | PASS |
| 15 | Write Multiple Coils | PASS | PASS |
| 16 | Write Multiple Registers | PASS | PASS |

## Mapas Server

Coils y Discrete Inputs usan bits empaquetados LSB-first:

```text
bit 0 byte 0 = dirección 0
bit 1 byte 0 = dirección 1
...
bit 7 byte 0 = dirección 7
bit 0 byte 1 = dirección 8
```

APIs:

```cpp
JWPLC_ModbusTCP.setCoils(...);
JWPLC_ModbusTCP.getCoil(...);
JWPLC_ModbusTCP.setCoil(...);

JWPLC_ModbusTCP.setDiscreteInputs(...);
JWPLC_ModbusTCP.getDiscreteInput(...);

JWPLC_ModbusTCP.setHoldingRegisters(...);
JWPLC_ModbusTCP.getHoldingRegister(...);
JWPLC_ModbusTCP.setHoldingRegister(...);

JWPLC_ModbusTCP.setInputRegisters(...);
JWPLC_ModbusTCP.getInputRegister(...);
```

Holding Registers son modificables.

Input Registers son de sólo lectura desde Modbus.

## MBAP y límites

La implementación valida:

```text
Transaction ID = conservado en respuesta
Protocol ID    = 0
Length         = Unit ID + PDU válido
Unit ID        = coincide con el configurado
ADU máxima     = 260 bytes
```

Límites Modbus:

```text
FC01 / FC02 = 1..2000 bits
FC03 / FC04 = 1..125 registers
FC15        = 1..1968 coils
FC16        = 1..123 registers
```

Una cabecera MBAP inválida cierra la conexión cuando continuar podría dejar el
stream desalineado.

## Excepciones

Server implementa:

```text
0x01 Illegal Function
0x02 Illegal Data Address
0x03 Illegal Data Value
0x04 Server Device Failure
```

El Client reporta excepciones mediante:

```cpp
JWPLC_ModbusTCPClient.result();
JWPLC_ModbusTCPClient.exceptionCode();
```

## Estado, errores y estadísticas

Server:

```cpp
JWPLC_ModbusTCP.lastError();
JWPLC_ModbusTCP.lastErrorString();
JWPLC_ModbusTCP.stats();
JWPLC_ModbusTCP.resetStats();
JWPLC_ModbusTCP.printStatus(Serial);
```

Client:

```cpp
JWPLC_ModbusTCPClient.result();
JWPLC_ModbusTCPClient.resultString();
JWPLC_ModbusTCPClient.stats();
JWPLC_ModbusTCPClient.resetStats();
JWPLC_ModbusTCPClient.printStatus(Serial);
```

Entre los contadores del Server se incluyen:

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

## Modelo cooperativo y SPI compartido

El W5500 comparte SPI con TFT, FRAM y microSD.

Las operaciones W5500 del runtime JWPLC usan el mutex SPI global. El trabajo de
procesamiento de Modbus se mantiene fuera del ownership SPI siempre que es
posible.

El Client usa primitives cooperativas del backend Ethernet para:

- connect;
- TX;
- cierre/recovery;

sin convertir `EthernetClient::write()` legacy en otra API.

## Política RX de Alpha12

Después de evaluar polling, INT puro y variantes adaptativas bajo full runtime,
JWPLC Basic v2 conserva:

```text
TCP_RX_POLICY=POLLING_C0
INT_DEFAULT=OFF
D2_DEFAULT=OFF
D3_DEFAULT=OFF
E1_RSR_DRAIN_DEFAULT=OFF
```

La infraestructura INT permanece disponible para qualification y futura
reevaluación en hardware posterior, pero no es la política productiva de
JWPLC Basic v2.

## Rendimiento validado

Las cifras dependen del FC, tamaño de trama, carga RTU, runtime y duración.
No deben interpretarse como garantía universal.

Referencias de Alpha12:

```text
TCP-only stress/full-runtime:
~1000 req/s validado en perfiles específicos

Coexistencia confirmada 600 s:
TCP Modbus = 250.001 req/s
RTU        = 796.953 req/s
RTU scan   = 99.619 scans/s
UDP FAST   = 0.998 Mbps
Full runtime = clean
```

El perfil RTU de ~100 Hz es **operacional**, no una garantía hard-real-time
cero-jitter.

## Compatibilidad

Alpha12 conserva:

- APIs Arduino Ethernet legacy;
- autoload Ethernet;
- periféricos normales del JWPLC Basic;
- separación entre Modbus TCP y Modbus RTU;
- puerto 502 estándar configurable;
- API Server y Client cooperativas.

No se asume:

```text
OPENPLC_AUTOLOAD=NO
OTA_DEFINED=NO
HARD_REALTIME_100HZ=NO
```

## Ejemplos

Incluidos:

```text
01.ModbusTCP_Server
02.ModbusTCP_Client
```

Ambos deben formar parte del gate final de compilación Alpha12 antes de
publicación.

## Evidencia

Cierre técnico principal:

```text
docs/v2.1.0-alpha.12/ALPHA12_STATUS.md
docs/v2.1.0-alpha.12/ALPHA12_PACKAGE_INVENTORY_20261003.md
```

Evidencia histórica detallada de implementación/gates:

```text
docs/v2.1.0-alpha.14/
```
