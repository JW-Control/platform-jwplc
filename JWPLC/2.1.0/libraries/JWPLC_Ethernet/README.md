# JWPLC_Ethernet

Librería Ethernet del package **JWPLC ESP32** para el W5500 integrado del
**JWPLC Basic**.

El runtime del JWPLC inicializa y mantiene Ethernet de forma cooperativa. En un
sketch normal no es necesario gestionar manualmente el W5500 ni llamar
`Ethernet.begin()`.

Esta librería cubre:

- DHCP;
- IP estática;
- estado de red y diagnóstico;
- TCP Client;
- TCP Server;
- UDP.

---

## Uso normal

### DHCP

```cpp
#include <JWPLC_Ethernet.h>

void setup()
{
    Serial.begin(115200);

    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (JWPLC_Ethernet.isReady())
    {
        Serial.print("IP: ");
        Serial.println(JWPLC_Ethernet.localIP());
    }

    delay(1000);
}
```

El runtime del package llama internamente a:

```cpp
JWPLC_Ethernet.service();
```

por lo que el usuario no necesita hacerlo en el caso normal.

---

## IP estática

Configurar en `setup()`:

```cpp
JWPLC_Ethernet.setStaticIP(
    IPAddress(192, 168, 1, 50),  // IP local
    IPAddress(192, 168, 1, 1),   // DNS
    IPAddress(192, 168, 1, 1),   // Gateway
    IPAddress(255, 255, 255, 0)); // Subnet
```

Consultar datos de red:

```cpp
JWPLC_Ethernet.localIP();
JWPLC_Ethernet.gatewayIP();
JWPLC_Ethernet.subnetMask();
JWPLC_Ethernet.dnsServerIP();
JWPLC_Ethernet.mac();
```

---

## Estado de Ethernet

Las consultas más útiles para un sketch son:

```cpp
JWPLC_Ethernet.isReady();
JWPLC_Ethernet.isBusy();

JWPLC_Ethernet.hardwarePresent();
JWPLC_Ethernet.linkUp();

JWPLC_Ethernet.runtimeState();

JWPLC_Ethernet.lastError();
JWPLC_Ethernet.lastErrorString();
JWPLC_Ethernet.statusString();
JWPLC_Ethernet.diagnosticCode();
```

Ejemplo:

```cpp
if (!JWPLC_Ethernet.linkUp())
{
    Serial.println("Cable Ethernet desconectado");
}

if (JWPLC_Ethernet.isReady())
{
    Serial.println("Ethernet listo");
}
```

Diagnóstico agrupado:

```cpp
JWPLC_Ethernet.printStatus(Serial);
```

---

# TCP Client

La API es compatible con el estilo Arduino Ethernet.

Objeto:

```cpp
EthernetClient client;
```

## Ejemplo

```cpp
#include <JWPLC_Ethernet.h>

EthernetClient client;
bool connected = false;

void setup()
{
    Serial.begin(115200);
    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (!JWPLC_Ethernet.isReady())
    {
        return;
    }

    if (!connected)
    {
        connected = client.connect(
            IPAddress(192, 168, 1, 100),
            5000);

        if (!connected)
        {
            delay(500);
            return;
        }

        client.println("Hola desde JWPLC");
    }

    while (client.available())
    {
        char c = client.read();
        Serial.write(c);
    }

    if (!client.connected())
    {
        client.stop();
        connected = false;
    }
}
```

Funciones habituales:

```cpp
client.connect(ip, port);
client.connected();

client.available();
client.read();

client.write(data);
client.print(...);
client.println(...);

client.flush();
client.stop();

client.remoteIP();
client.remotePort();
client.localPort();
```

---

# TCP Server

Objeto:

```cpp
EthernetServer server(5000);
```

## Ejemplo

```cpp
#include <JWPLC_Ethernet.h>

EthernetServer server(5000);
bool serverStarted = false;

void setup()
{
    Serial.begin(115200);
    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (!JWPLC_Ethernet.isReady())
    {
        return;
    }

    if (!serverStarted)
    {
        server.begin();
        serverStarted = true;

        Serial.print("Server en ");
        Serial.println(JWPLC_Ethernet.localIP());
    }

    EthernetClient client = server.available();

    if (client)
    {
        while (client.available())
        {
            char c = client.read();
            Serial.write(c);
        }

        client.println("JWPLC TCP Server");
    }
}
```

Funciones principales:

```cpp
server.begin();
server.available();
server.accept();
server.write(...);
server.print(...);
server.println(...);
```

Para protocolos industriales sobre TCP se recomienda usar la librería
`JWPLC_ModbusTCP` en lugar de implementar Modbus manualmente.

---

# UDP

La clase pública es:

```cpp
EthernetUDP udp;
```

La API recomendada es la API UDP clásica de Arduino.

---

## Escuchar UDP

```cpp
#include <JWPLC_Ethernet.h>

EthernetUDP udp;

bool udpStarted = false;
const uint16_t LOCAL_PORT = 5000;

void setup()
{
    Serial.begin(115200);
    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (!JWPLC_Ethernet.isReady())
    {
        return;
    }

    if (!udpStarted)
    {
        udpStarted = udp.begin(LOCAL_PORT);

        if (!udpStarted)
        {
            Serial.println("No se pudo abrir UDP");
            return;
        }
    }

    int packetSize = udp.parsePacket();

    if (packetSize > 0)
    {
        char buffer[64];

        int n = udp.read(
            buffer,
            sizeof(buffer) - 1);

        if (n > 0)
        {
            buffer[n] = '\0';

            Serial.print("RX de ");
            Serial.print(udp.remoteIP());
            Serial.print(':');
            Serial.print(udp.remotePort());
            Serial.print(" -> ");
            Serial.println(buffer);
        }
    }
}
```

Funciones de recepción:

```cpp
udp.begin(localPort);
udp.beginMulticast(groupIP, port);

udp.parsePacket();
udp.available();
udp.read(...);
udp.peek();
udp.flush();

udp.remoteIP();
udp.remotePort();
udp.localPort();

udp.stop();
```

---

## Enviar UDP

```cpp
IPAddress destination(192, 168, 1, 100);

udp.beginPacket(destination, 5000);
udp.print("JWPLC UDP");
udp.endPacket();
```

También puede enviarse un buffer:

```cpp
uint8_t data[] = {1, 2, 3, 4};

udp.beginPacket(destination, 5000);
udp.write(data, sizeof(data));
udp.endPacket();
```

Funciones de envío:

```cpp
udp.beginPacket(ip, port);
udp.beginPacket(host, port);

udp.write(...);
udp.print(...);
udp.println(...);

udp.endPacket();
```

UDP no garantiza entrega, orden ni retransmisión. Si el protocolo necesita
confirmación, debe implementarse a nivel de aplicación o usarse TCP.

---

## TCP/UDP cooperativo y fast-path

Alpha12 incorpora internamente rutas cooperativas y fast-path para librerías
como `JWPLC_ModbusTCP` y para perfiles de alto rendimiento.

Existen extensiones como:

```text
beginConnectAsync()
beginWriteAsync()
beginEndPacketAsync()
jwplcReadTcpFastDeferred()
jwplcReadPacketFastDeferred()
```

pero **no son necesarias para un sketch normal**.

Para aplicaciones de usuario se recomienda mantener las APIs estándar
`EthernetClient`, `EthernetServer` y `EthernetUDP` mostradas arriba.

---

## Recuperación de red

El runtime JWPLC gestiona de forma cooperativa:

- detección del W5500;
- link RJ45;
- DHCP;
- mantenimiento del lease;
- recuperación después de desconexión/reconexión.

El usuario puede comprobar:

```cpp
JWPLC_Ethernet.isReady();
JWPLC_Ethernet.linkUp();
JWPLC_Ethernet.statusString();
```

sin reiniciar el ESP32 para recuperar el enlace.

---

## Códigos ETH en la TFT

El indicador Ethernet del Display puede gestionarse automáticamente:

```cpp
JWPLC_Display.setEthLedAuto(true);
```

Códigos habituales:

| Código | Significado |
|---|---|
| `DIS` | Ethernet deshabilitado |
| `INI` | Inicializando |
| `PHY` | Preparando W5500 |
| `LNK` | Sin link RJ45 |
| `DHC` | DHCP en progreso |
| `HW` | W5500 no detectado |
| `IP` | Configuración IP inválida |
| `SPI` | Problema temporal de acceso al bus |
| `---` | Operativo |

---

## Configuración adicional

Disponible cuando se necesita:

```cpp
JWPLC_Ethernet.setMac(mac);
JWPLC_Ethernet.useDefaultMac();

JWPLC_Ethernet.setTimeouts(
    dhcpTimeoutMs,
    responseTimeoutMs);

JWPLC_Ethernet.setRetransmissionCount(count);
```

La configuración física del CS/reset del W5500 pertenece al board JWPLC y no
debería modificarse en un sketch normal.

---

## Compatibilidad

Se conservan las rutas síncronas históricas:

```cpp
JWPLC_Ethernet.begin();
JWPLC_Ethernet.maintain();
```

Son útiles para compatibilidad o pruebas manuales. El autoload normal usa el
runtime cooperativo.

---

## Ejemplos incluidos

```text
01.Ethernet_DHCP_Basic
02.Ethernet_StaticIP_Basic
03.Ethernet_Diagnostics
```

Además, este README incluye ejemplos mínimos de TCP Client, TCP Server y UDP.

---

## Qué no necesita configurar el usuario

Alpha12 optimiza internamente:

- acceso al W5500;
- recepción TCP/UDP;
- envío cooperativo;
- arbitraje del SPI compartido;
- mantenimiento de red.

No es necesario configurar políticas RX, FIFO, INT, caches SPI ni otros knobs
internos para usar Ethernet normalmente.

---

## Estado Alpha12

```text
JWPLC_Ethernet 1.0.0
AUTOLOAD_COOPERATIVE=YES
DHCP_RECOVERY=VALIDATED
STATIC_IP=SUPPORTED
TCP_CLIENT_SERVER=SUPPORTED
UDP=SUPPORTED
LEGACY_API_PRESERVED=YES
```

Alpha12 conserva Ethernet dentro del autoload normal y mantiene compatibilidad
con las APIs Arduino Ethernet de uso habitual.
