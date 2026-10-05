# JWPLC_Ethernet

`JWPLC_Ethernet` permite usar el puerto Ethernet W5500 integrado del
**JWPLC Basic**.

La plataforma se encarga de inicializar y mantener el hardware de red. Tu
sketch normalmente sólo decide si usará DHCP o una IP fija y luego trabaja con
`EthernetClient`, `EthernetServer` o `EthernetUDP`.

## ¿Para qué sirve?

Con Ethernet puedes:

- conectar el JWPLC a una red local;
- obtener una IP automáticamente por DHCP;
- usar una IP estática;
- crear clientes TCP;
- crear servidores TCP;
- enviar y recibir UDP;
- usar protocolos construidos sobre Ethernet, como Modbus TCP.

Si tu objetivo es Modbus TCP, usa `JWPLC_ModbusTCP`. Esta librería es la base
de red.

## Qué hace automáticamente el JWPLC

En un sketch normal no tienes que:

- llamar `Ethernet.begin()`;
- configurar el pin CS del W5500;
- reiniciar manualmente el chip;
- administrar el bus SPI compartido;
- llamar continuamente a `JWPLC_Ethernet.service()`.

El runtime mantiene Ethernet en segundo plano.

## Inicio rápido

Este sketch solicita una dirección por DHCP y la imprime cuando esté lista.

```cpp
#include <JWPLC_Ethernet.h>

bool ipMostrada = false;

void setup()
{
    Serial.begin(115200);

    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (JWPLC_Ethernet.isReady() &&
        !ipMostrada)
    {
        ipMostrada = true;

        Serial.print("IP: ");
        Serial.println(
            JWPLC_Ethernet.localIP());
    }
}
```

Conecta el cable Ethernet a una red con DHCP y abre el Monitor Serie.

## Conceptos básicos

### Dirección IP

Una IP identifica un equipo dentro de una red.

Ejemplo:

```text
192.168.1.50
```

### DHCP

DHCP permite que el router o servidor de red asigne automáticamente:

- IP;
- gateway;
- máscara;
- DNS.

En JWPLC:

```cpp
JWPLC_Ethernet.useDHCP();
```

### IP estática

Puedes fijar una dirección manualmente:

```cpp
JWPLC_Ethernet.setStaticIP(
    IPAddress(192, 168, 1, 50),
    IPAddress(192, 168, 1, 1),
    IPAddress(192, 168, 1, 1),
    IPAddress(255, 255, 255, 0));
```

### TCP

TCP crea una conexión entre dos equipos.

Normalmente tendrás:

- un **Server**, que escucha en un puerto;
- un **Client**, que se conecta al Server.

### UDP

UDP envía datagramas sin crear una conexión permanente.

Es simple y rápido, pero no garantiza que cada datagrama llegue.

### Puerto

Un puerto identifica un servicio dentro de una IP.

Ejemplo:

```text
IP    = 192.168.1.50
Port  = 5000
```

## Ejemplo 1 — Básico: DHCP y estado de red

```cpp
#include <JWPLC_Ethernet.h>

uint32_t ultimoReporte = 0;

void setup()
{
    Serial.begin(115200);

    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (millis() - ultimoReporte < 1000)
    {
        return;
    }

    ultimoReporte = millis();

    Serial.print("Estado: ");
    Serial.print(
        JWPLC_Ethernet.statusString());

    if (JWPLC_Ethernet.isReady())
    {
        Serial.print(" | IP: ");
        Serial.print(
            JWPLC_Ethernet.localIP());
    }

    Serial.println();
}
```

Estados como “esperando link” o “DHCP en progreso” pueden aparecer durante el
arranque.

## Ejemplo 2 — Intermedio: IP estática

Cambia estos valores según tu red.

```cpp
#include <JWPLC_Ethernet.h>

const IPAddress IP_LOCAL(
    192, 168, 1, 50);

const IPAddress DNS(
    192, 168, 1, 1);

const IPAddress GATEWAY(
    192, 168, 1, 1);

const IPAddress MASCARA(
    255, 255, 255, 0);

void setup()
{
    Serial.begin(115200);

    JWPLC_Ethernet.setStaticIP(
        IP_LOCAL,
        DNS,
        GATEWAY,
        MASCARA);
}

void loop()
{
    static bool mostrado = false;

    if (JWPLC_Ethernet.isReady() &&
        !mostrado)
    {
        mostrado = true;

        Serial.print("IP: ");
        Serial.println(
            JWPLC_Ethernet.localIP());

        Serial.print("Gateway: ");
        Serial.println(
            JWPLC_Ethernet.gatewayIP());
    }
}
```

La configuración debe hacerse en `setup()`, antes de que el runtime termine
de levantar Ethernet.

## Ejemplo 3 — Aplicación real: servidor TCP de comandos

Este Server escucha en el puerto 5000.

Comandos:

- `1` -> enciende `Q0_0`;
- `0` -> apaga `Q0_0`;
- `?` -> responde el estado.

```cpp
#include <JWPLC_Ethernet.h>

EthernetServer server(5000);

bool serverIniciado = false;
bool salida = false;

void setup()
{
    pinMode(Q0_0, OUTPUT);
    digitalWrite(Q0_0, LOW);

    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (!JWPLC_Ethernet.isReady())
    {
        return;
    }

    if (!serverIniciado)
    {
        server.begin();
        serverIniciado = true;
    }

    EthernetClient client =
        server.available();

    if (!client)
    {
        return;
    }

    while (client.available() > 0)
    {
        const int comando =
            client.read();

        if (comando == '1')
        {
            salida = true;
            digitalWrite(Q0_0, HIGH);
            client.println("ON");
        }
        else if (comando == '0')
        {
            salida = false;
            digitalWrite(Q0_0, LOW);
            client.println("OFF");
        }
        else if (comando == '?')
        {
            client.println(
                salida ? "ON" : "OFF");
        }
    }
}
```

## Ejemplo 4 — Avanzado de usuario: telemetría UDP

Este ejemplo envía el estado de las entradas una vez por segundo.

```cpp
#include <JWPLC_Ethernet.h>

EthernetUDP udp;

const IPAddress DESTINO(
    192, 168, 1, 100);

const uint16_t PUERTO_LOCAL = 5001;
const uint16_t PUERTO_DESTINO = 5000;

bool udpIniciado = false;
uint32_t ultimoEnvio = 0;

void setup()
{
    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (!JWPLC_Ethernet.isReady())
    {
        return;
    }

    if (!udpIniciado)
    {
        udpIniciado =
            udp.begin(PUERTO_LOCAL);

        if (!udpIniciado)
        {
            return;
        }
    }

    if (millis() - ultimoEnvio >= 1000)
    {
        ultimoEnvio = millis();

        udp.beginPacket(
            DESTINO,
            PUERTO_DESTINO);

        udp.print("IN=");
        udp.println(
            JWPLC_readInputs(),
            HEX);

        udp.endPacket();
    }
}
```

## API de usuario

### Elegir configuración de red — Básico

| Función | Qué hace | Nivel |
|---|---|---|
| `useDHCP()` | Selecciona configuración automática | Básico |
| `setStaticIP(local,dns,gateway,subnet)` | Define una IP fija | Intermedio |

Recomendación: configura DHCP o IP estática en `setup()`.

### Consultar el estado — Básico

| Función | Retorno / uso |
|---|---|
| `isReady()` | `true` cuando la red está utilizable |
| `isBusy()` | El runtime está trabajando en la configuración |
| `hardwarePresent()` | W5500 detectado |
| `linkUp()` | Cable/link Ethernet activo |
| `statusString()` | Estado legible |
| `diagnosticCode()` | Código corto de diagnóstico |

### Consultar parámetros de red — Básico / Intermedio

```cpp
JWPLC_Ethernet.localIP();
JWPLC_Ethernet.subnetMask();
JWPLC_Ethernet.gatewayIP();
JWPLC_Ethernet.dnsServerIP();
```

### Diagnóstico — Intermedio

```cpp
JWPLC_Ethernet.lastError();
JWPLC_Ethernet.lastErrorString();
JWPLC_Ethernet.runtimeState();
JWPLC_Ethernet.printStatus(Serial);
```

`printStatus()` es la forma más sencilla de obtener un resumen durante
commissioning.

### EthernetClient — Básico / Intermedio

Crear:

```cpp
EthernetClient client;
```

Conectar:

```cpp
client.connect(
    IPAddress(192, 168, 1, 100),
    5000);
```

Funciones principales:

| Función | Uso |
|---|---|
| `connect(ip,port)` | Conectar a un Server |
| `connect(host,port)` | Conectar usando nombre |
| `connected()` | Consultar si sigue conectado |
| `available()` | Bytes recibidos disponibles |
| `read()` / `read(buffer,size)` | Leer |
| `write()` | Enviar bytes |
| `print()` / `println()` | Enviar texto |
| `flush()` | Esperar TX pendiente |
| `stop()` | Cerrar la conexión |
| `remoteIP()` | IP remota |
| `remotePort()` | Puerto remoto |
| `localPort()` | Puerto local |

Ejemplo típico:

```cpp
if (client.connect(serverIP, 5000))
{
    client.println("Hola");
}
```

### EthernetServer — Intermedio

Crear:

```cpp
EthernetServer server(5000);
```

Funciones principales:

| Función | Uso |
|---|---|
| `begin()` | Empieza a escuchar |
| `available()` | Obtiene un Client con datos disponibles |
| `accept()` | Acepta un Client |
| `write()` / `print()` / `println()` | Envía a Clients conectados |

### EthernetUDP — Intermedio

Crear:

```cpp
EthernetUDP udp;
```

Recibir:

```cpp
udp.begin(5000);

int tamano = udp.parsePacket();

if (tamano > 0)
{
    uint8_t buffer[64];

    int n = udp.read(
        buffer,
        sizeof(buffer));
}
```

Enviar:

```cpp
udp.beginPacket(destino, 5000);
udp.print("Hola");
udp.endPacket();
```

Funciones principales:

| Función | Uso |
|---|---|
| `begin(port)` | Abre un puerto local |
| `beginMulticast(ip,port)` | Escucha multicast |
| `stop()` | Cierra el socket UDP |
| `beginPacket(ip,port)` | Inicia un datagrama |
| `write()` / `print()` | Agrega datos |
| `endPacket()` | Envía el datagrama |
| `parsePacket()` | Detecta un datagrama recibido |
| `available()` | Bytes aún disponibles |
| `read()` | Lee los datos |
| `peek()` | Mira el siguiente byte |
| `flush()` | Descarta/termina lectura pendiente según la API Ethernet |
| `remoteIP()` | IP del emisor |
| `remotePort()` | Puerto del emisor |
| `localPort()` | Puerto local |

## Errores comunes

### Llamar `Ethernet.begin()` en un sketch JWPLC normal

No es necesario. Usa:

```cpp
JWPLC_Ethernet.useDHCP();
```

o:

```cpp
JWPLC_Ethernet.setStaticIP(...);
```

### Esperar la IP con un `while` bloqueante

Evita congelar `setup()` hasta que haya red.

Mejor consulta:

```cpp
if (JWPLC_Ethernet.isReady())
{
    // Red lista
}
```

desde `loop()`.

### Crear un Server antes de saber que la red está lista

Para un Server TCP genérico es más claro esperar:

```cpp
JWPLC_Ethernet.isReady()
```

antes de llamar `server.begin()`.

### Confundir TCP con UDP

TCP crea una conexión. UDP envía datagramas independientes.

### Usar `delay()` para mantener conexiones

No es necesario. Procesa red periódicamente dentro del flujo normal de
`loop()`.

## API avanzada

### Timeouts y retransmisiones del runtime

```cpp
JWPLC_Ethernet.setTimeouts(
    5000,
    1000);

JWPLC_Ethernet.setRetransmissionCount(3);
```

Úsalos sólo si tu red requiere ajustes específicos.

### Estado detallado

```cpp
JWPLCEthernetRuntimeState estado =
    JWPLC_Ethernet.runtimeState();

JWPLCEthernetError error =
    JWPLC_Ethernet.lastError();
```

Los enums exactos están definidos en `JWPLC_Ethernet.h`.

## Compatibilidad

Se conservan rutas Arduino Ethernet históricas como:

```cpp
Ethernet.begin(...);
Ethernet.maintain();
JWPLC_Ethernet.begin();
JWPLC_Ethernet.maintain();
```

Para código nuevo en JWPLC Basic se recomienda el runtime administrado:

```cpp
JWPLC_Ethernet.useDHCP();
```

o:

```cpp
JWPLC_Ethernet.setStaticIP(...);
```

Métodos como `configure()`, `setResetPin()`, `probeHardware()`,
`csPin()`, `resetPin()` y los hooks de profiling/test son de
bring-up/diagnóstico del package y no deben utilizarse para reconfigurar el
hardware del producto desde un sketch normal.

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_Ethernet 1.0.0
```
