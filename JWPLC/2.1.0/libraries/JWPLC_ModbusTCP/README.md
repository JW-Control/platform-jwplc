# JWPLC_ModbusTCP

Librería Modbus TCP nativa para JWPLC Basic sobre `JWPLC_Ethernet` y el W5500 integrado.

Estado actual en `v2.1.0-alpha.14`:

```text
A14.1 Foundation + Server = IMPLEMENTED_NOT_YET_VALIDATED
A14.2 Client/Master       = PENDING
RTU + TCP simultáneo      = NOT_TESTED
```

## Uso

La librería es opt-in durante Alpha14:

```cpp
#include <JWPLC_ModbusTCP.h>
```

No inicia un servidor automáticamente y no añade un nuevo servicio al autoload normal.

El Ethernet del JWPLC continúa siendo inicializado y mantenido por el runtime del package. Por ello el sketch no debe repetir `Ethernet.begin()` ni `JWPLC_Ethernet.begin()` salvo que esté ejecutando una prueba deliberadamente manual.

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
    JWPLC_ModbusTCP.task();
}
```

`beginServer()` retorna `true` cuando la configuración es aceptada. Si DHCP todavía está en progreso, el servidor espera de forma cooperativa a que `JWPLC_Ethernet` alcance `READY`.

Para saber si el socket ya está disponible:

```cpp
if (JWPLC_ModbusTCP.serverReady())
{
    // Puerto 502 listo.
}
```

## Funciones soportadas por A14.1

| FC | Función | Estado de código |
|---:|---|---|
| 01 | Read Coils | Implementada |
| 02 | Read Discrete Inputs | Implementada |
| 03 | Read Holding Registers | Implementada |
| 04 | Read Input Registers | Implementada |
| 05 | Write Single Coil | Implementada |
| 06 | Write Single Register | Implementada |
| 15 | Write Multiple Coils | Implementada |
| 16 | Write Multiple Registers | Implementada |

No se marcarán como `PASS` hasta ejecutar los gates de compilación y hardware.

## Mapas

Coils y Discrete Inputs usan bits empaquetados LSB-first, igual que `JWPLC_ModbusRTU`:

```text
bit 0 byte 0 = dirección 0
bit 1 byte 0 = dirección 1
...
bit 7 byte 0 = dirección 7
bit 0 byte 1 = dirección 8
```

Holding Registers son `uint16_t*` modificables.

Input Registers son `const uint16_t*` de sólo lectura desde Modbus.

## MBAP

La implementación valida:

```text
Transaction ID = se conserva en la respuesta
Protocol ID    = 0
Length         = 2..254 bytes, incluye Unit ID + PDU
Unit ID        = debe coincidir con el configurado en beginServer()
ADU máxima     = 260 bytes
```

Una trama con `Protocol ID != 0` o longitud MBAP inválida se considera framing inválido y se cierra la conexión para evitar continuar desalineado con bytes residuales.

## Límites

```text
FC01 / FC02 = 1..2000 bits
FC03 / FC04 = 1..125 registers
FC15        = 1..1968 coils
FC16        = 1..123 registers
```

Las direcciones se validan además contra el mapa configurado.

## Excepciones

Se implementan:

```text
0x01 Illegal Function
0x02 Illegal Data Address
0x03 Illegal Data Value
0x04 Server Device Failure
```

## Modelo cooperativo

`task()` debe ejecutarse frecuentemente.

El parser no intenta consumir tráfico indefinidamente en una sola llamada. En A14.1 existe un presupuesto por defecto:

```text
JWPLC_MODBUS_TCP_RX_BUDGET = 64 bytes / task()
```

El objetivo es mantener oportunidades de ejecución para:

```text
TFT
RTC
FRAM
microSD
I/O
RS-485 / Modbus RTU
servicio Ethernet
```

## SPI compartido

Las operaciones contra `EthernetServer`/`EthernetClient` se realizan dentro del mutex SPI global de JWPLC:

```text
jwplcSPI_acquire()
jwplcSPI_deselectAll()
operación W5500
jwplcSPI_release()
```

El procesamiento de PDU y mapas ocurre fuera del mutex.

## Estado y estadísticas

```cpp
JWPLC_ModbusTCP.serverReady();
JWPLC_ModbusTCP.clientConnected();
JWPLC_ModbusTCP.serverState();
JWPLC_ModbusTCP.lastError();
JWPLC_ModbusTCP.lastErrorString();
JWPLC_ModbusTCP.stats();
JWPLC_ModbusTCP.printStatus(Serial);
```

Contadores actuales:

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

## Pendiente A14.2

El Client/Master no se implementa usando directamente `EthernetClient::connect()` porque el backend actual espera de forma síncrona hasta conexión/timeout. Alpha14 requiere un motor cooperativo: se añadirá una extensión mínima al backend W5500 para iniciar/pollear/cancelar la conexión TCP sin sostener el SPI durante un timeout largo.

Sobre esa base se implementarán FC01/02/03/04/05/06/15/16 como Client y, posteriormente, wrappers Sync sobre el mismo state machine.

## Validación

No asumir todavía:

```text
MODBUS_TCP_SERVER=PASS
MODBUS_TCP_CLIENT=PASS
MODBUS_RTU_TCP_SIMULTANEOUS=PASS
```

Esos estados requieren los gates Alpha14 correspondientes.
