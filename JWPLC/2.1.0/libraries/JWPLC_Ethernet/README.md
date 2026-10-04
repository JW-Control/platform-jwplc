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


---

# Referencia completa de API pública

Esta sección enumera la API soportada que un usuario puede encontrar en los
headers Ethernet del package.

Se divide en:

- `JWPLC_Ethernet`: configuración/runtime JWPLC;
- `EthernetClient`: TCP Client compatible con Arduino;
- `EthernetServer`: TCP Server compatible con Arduino;
- `EthernetUDP`: UDP compatible con Arduino;
- `Ethernet`: objeto de compatibilidad de bajo nivel.

Las extensiones cooperativas y fast-path también se documentan, pero se marcan
como **AVANZADAS** porque una aplicación normal no las necesita.

## JWPLC_Ethernet — hardware y configuración

| Función | Ejemplo |
|---|---|
| `configure(csPin, resetPin)` | `JWPLC_Ethernet.configure(JWPLC_ETH_CS, JWPLC_ETH_RESET_PIN);` |
| `setResetPin(pin)` | `JWPLC_Ethernet.setResetPin(5);` |
| `setMac(mac)` | `JWPLC_Ethernet.setMac(mac);` |
| `useDefaultMac()` | `JWPLC_Ethernet.useDefaultMac();` |
| `useDHCP()` | `JWPLC_Ethernet.useDHCP();` |
| `setStaticIP(local,dns,gateway,subnet)` | `JWPLC_Ethernet.setStaticIP(ip, dns, gw, mask);` |
| `setTimeouts(dhcp,response)` | `JWPLC_Ethernet.setTimeouts(5000, 1000);` |
| `setRetransmissionCount(count)` | `JWPLC_Ethernet.setRetransmissionCount(3);` |
| `probeHardware()` | `bool ok = JWPLC_Ethernet.probeHardware();` |
| `service()` | `JWPLC_Ethernet.service();` |

`configure()`, `setResetPin()` y `probeHardware()` son principalmente para
diagnóstico/bring-up; el board JWPLC ya define su hardware.

## JWPLC_Ethernet — inicialización síncrona de compatibilidad

| Función | Ejemplo |
|---|---|
| `begin()` | `bool ok = JWPLC_Ethernet.begin();` |
| `begin(mac)` | `bool ok = JWPLC_Ethernet.begin(mac);` |
| `begin(local,dns,gateway,subnet)` | `bool ok = JWPLC_Ethernet.begin(ip, dns, gw, mask);` |
| `maintain()` | `int r = JWPLC_Ethernet.maintain();` |

Estas rutas son soportadas por compatibilidad. Para el autoload normal se
prefieren `useDHCP()` / `setStaticIP()`.

## JWPLC_Ethernet — estado

| Función | Ejemplo |
|---|---|
| `isEnabled()` | `bool x = JWPLC_Ethernet.isEnabled();` |
| `isBeginAttempted()` | `bool x = JWPLC_Ethernet.isBeginAttempted();` |
| `isReady()` | `bool x = JWPLC_Ethernet.isReady();` |
| `isBusy()` | `bool x = JWPLC_Ethernet.isBusy();` |
| `hardwarePresent()` | `bool x = JWPLC_Ethernet.hardwarePresent();` |
| `linkUp()` | `bool x = JWPLC_Ethernet.linkUp();` |
| `hardwareStatus()` | `auto s = JWPLC_Ethernet.hardwareStatus();` |
| `linkStatus()` | `auto s = JWPLC_Ethernet.linkStatus();` |
| `mode()` | `auto m = JWPLC_Ethernet.mode();` |
| `runtimeState()` | `auto s = JWPLC_Ethernet.runtimeState();` |
| `lastError()` | `auto e = JWPLC_Ethernet.lastError();` |
| `lastErrorString()` | `Serial.println(JWPLC_Ethernet.lastErrorString());` |
| `statusString()` | `Serial.println(JWPLC_Ethernet.statusString());` |
| `diagnosticCode()` | `Serial.println(JWPLC_Ethernet.diagnosticCode());` |

## JWPLC_Ethernet — datos de red/hardware

| Función | Ejemplo |
|---|---|
| `localIP()` | `IPAddress ip = JWPLC_Ethernet.localIP();` |
| `subnetMask()` | `IPAddress mask = JWPLC_Ethernet.subnetMask();` |
| `gatewayIP()` | `IPAddress gw = JWPLC_Ethernet.gatewayIP();` |
| `dnsServerIP()` | `IPAddress dns = JWPLC_Ethernet.dnsServerIP();` |
| `mac()` | `const uint8_t *mac = JWPLC_Ethernet.mac();` |
| `csPin()` | `uint8_t pin = JWPLC_Ethernet.csPin();` |
| `resetPin()` | `uint8_t pin = JWPLC_Ethernet.resetPin();` |
| `printStatus(out)` | `JWPLC_Ethernet.printStatus(Serial);` |

---

# EthernetClient — referencia completa

## Construcción

```cpp
EthernetClient client;
```

La variante con número de socket existe para uso interno/compatibilidad:

```cpp
EthernetClient clientFromSocket(socketIndex);
```

No se recomienda crear Clients desde un socket manual en código normal.

## Conexión

| Función | Ejemplo |
|---|---|
| `status()` | `uint8_t s = client.status();` |
| `connect(ip, port)` | `client.connect(IPAddress(192,168,1,10), 5000);` |
| `connect(host, port)` | `client.connect("example.local", 5000);` |
| `connected()` | `if (client.connected()) { ... }` |
| `stop()` | `client.stop();` |
| `setConnectionTimeout(ms)` | `client.setConnectionTimeout(1000);` |

## Escritura

| Función | Ejemplo |
|---|---|
| `availableForWrite()` | `int n = client.availableForWrite();` |
| `write(byte)` | `client.write((uint8_t)0x55);` |
| `write(buffer, size)` | `client.write(data, sizeof(data));` |
| `print(...)` | `client.print("TEMP=");` |
| `println(...)` | `client.println(25.0);` |
| `flush()` | `client.flush();` |

`print()` y `println()` vienen de `Print` y están disponibles porque
`EthernetClient` hereda de esa interfaz.

## Lectura

| Función | Ejemplo |
|---|---|
| `available()` | `int n = client.available();` |
| `read()` | `int b = client.read();` |
| `read(buffer, size)` | `int n = client.read(buf, sizeof(buf));` |
| `peek()` | `int b = client.peek();` |

## Endpoint/socket

| Función | Ejemplo |
|---|---|
| `localPort()` | `uint16_t p = client.localPort();` |
| `remoteIP()` | `IPAddress ip = client.remoteIP();` |
| `remotePort()` | `uint16_t p = client.remotePort();` |
| `getSocketNumber()` | `uint8_t s = client.getSocketNumber();` |
| `operator bool()` | `if (client) { ... }` |
| `operator==(bool)` | `bool valid = (client == true);` |
| `operator!=(bool)` | `bool invalid = (client != true);` |
| `operator==(EthernetClient)` | `bool same = (clientA == clientB);` |
| `operator!=(EthernetClient)` | `bool different = (clientA != clientB);` |

## EthernetClient — extensiones cooperativas AVANZADAS

Estas funciones son soportadas, pero están pensadas para librerías/runtime que
necesitan state machines no bloqueantes.

### Connect

| Función | Ejemplo |
|---|---|
| `beginConnectAsync(ip, port)` | `int r = client.beginConnectAsync(ip, 5000);` |
| `pollConnectAsync()` | `int r = client.pollConnectAsync();` |
| `connectAsyncInProgress()` | `bool x = client.connectAsyncInProgress();` |
| `cancelConnectAsync()` | `client.cancelConnectAsync();` |

Patrón:

```cpp
int r = client.beginConnectAsync(ip, 5000);

while (r == 0)
{
    r = client.pollConnectAsync();
}
```

### Stop

| Función | Ejemplo |
|---|---|
| `beginStopAsync()` | `int r = client.beginStopAsync();` |
| `pollStopAsync()` | `int r = client.pollStopAsync();` |
| `stopAsyncInProgress()` | `bool x = client.stopAsyncInProgress();` |
| `cancelStopAsync()` | `client.cancelStopAsync();` |

### Flush

| Función | Ejemplo |
|---|---|
| `beginFlushAsync()` | `int r = client.beginFlushAsync();` |
| `pollFlushAsync()` | `int r = client.pollFlushAsync();` |
| `flushAsyncInProgress()` | `bool x = client.flushAsyncInProgress();` |
| `cancelFlushAsync()` | `client.cancelFlushAsync();` |

### Write

| Función | Ejemplo |
|---|---|
| `beginWriteAsync(buf, size)` | `int r = client.beginWriteAsync(data, len);` |
| `pollWriteAsync()` | `int r = client.pollWriteAsync();` |
| `writeAsyncInProgress()` | `bool x = client.writeAsyncInProgress();` |
| `cancelWriteAsync()` | `client.cancelWriteAsync();` |

El buffer pasado a `beginWriteAsync()` debe permanecer válido mientras la
operación esté esperando espacio TX.

## EthernetClient — fast RX AVANZADO

| Función | Ejemplo |
|---|---|
| `jwplcReadTcpFastDeferred(buf, size)` | `int n = client.jwplcReadTcpFastDeferred(buf, len);` |
| `jwplcCommitRxFast()` | `bool ok = client.jwplcCommitRxFast();` |

Contrato mínimo:

```cpp
int n = client.jwplcReadTcpFastDeferred(buf, sizeof(buf));

if (n > 0)
{
    // Procesar buf[0..n-1]
    client.jwplcCommitRxFast();
}
```

No usar esta ruta como sustituto casual de `read()`; está destinada a
consumers cooperativos que entienden el commit diferido.

---

# EthernetServer — referencia completa

Construcción:

```cpp
EthernetServer server(5000);
```

| Función | Ejemplo |
|---|---|
| `begin()` | `server.begin();` |
| `available()` | `EthernetClient c = server.available();` |
| `accept()` | `EthernetClient c = server.accept();` |
| `write(byte)` | `server.write((uint8_t)0x55);` |
| `write(buffer,size)` | `server.write(data, sizeof(data));` |
| `print(...)` | `server.print("RUN");` |
| `println(...)` | `server.println("OK");` |
| `operator bool()` | `if (server) { ... }` |

---

# EthernetUDP — referencia completa

Construcción:

```cpp
EthernetUDP udp;
```

## Apertura/cierre

| Función | Ejemplo |
|---|---|
| `begin(port)` | `udp.begin(5000);` |
| `beginMulticast(ip, port)` | `udp.beginMulticast(groupIP, 5000);` |
| `stop()` | `udp.stop();` |
| `localPort()` | `uint16_t p = udp.localPort();` |

## Envío

| Función | Ejemplo |
|---|---|
| `beginPacket(ip, port)` | `udp.beginPacket(IPAddress(192,168,1,10), 5000);` |
| `beginPacket(host, port)` | `udp.beginPacket("host.local", 5000);` |
| `write(byte)` | `udp.write((uint8_t)0x01);` |
| `write(buffer,size)` | `udp.write(data, sizeof(data));` |
| `print(...)` | `udp.print("JWPLC");` |
| `println(...)` | `udp.println("RUN");` |
| `endPacket()` | `udp.endPacket();` |

## Recepción

| Función | Ejemplo |
|---|---|
| `parsePacket()` | `int size = udp.parsePacket();` |
| `available()` | `int n = udp.available();` |
| `read()` | `int b = udp.read();` |
| `read(uint8_t*,len)` | `int n = udp.read(buf, sizeof(buf));` |
| `read(char*,len)` | `int n = udp.read(text, sizeof(text));` |
| `peek()` | `int b = udp.peek();` |
| `flush()` | `udp.flush();` |
| `remoteIP()` | `IPAddress ip = udp.remoteIP();` |
| `remotePort()` | `uint16_t p = udp.remotePort();` |

## UDP TX cooperativo AVANZADO

| Función | Ejemplo |
|---|---|
| `beginEndPacketAsync()` | `int r = udp.beginEndPacketAsync();` |
| `pollEndPacketAsync()` | `int r = udp.pollEndPacketAsync();` |
| `endPacketAsyncInProgress()` | `bool x = udp.endPacketAsyncInProgress();` |
| `cancelEndPacketAsync()` | `udp.cancelEndPacketAsync();` |

Patrón:

```cpp
udp.beginPacket(ip, 5000);
udp.write(data, len);

int r = udp.beginEndPacketAsync();

while (r == 0)
{
    r = udp.pollEndPacketAsync();
}
```

## UDP fast RX AVANZADO

| Función | Ejemplo |
|---|---|
| `jwplcReadPacketFastDeferred(buf,len)` | `int n = udp.jwplcReadPacketFastDeferred(buf, len);` |
| `jwplcCommitRxFast()` | `bool ok = udp.jwplcCommitRxFast();` |

Contrato mínimo:

```cpp
int n = udp.jwplcReadPacketFastDeferred(buf, sizeof(buf));

if (n > 0)
{
    // Procesar datagrama.
    udp.jwplcCommitRxFast();
}
```

---

# Objeto Ethernet — compatibilidad Arduino

El package conserva el objeto:

```cpp
Ethernet
```

para compatibilidad con código Arduino Ethernet.

Para proyectos JWPLC nuevos se recomienda `JWPLC_Ethernet`, pero estas
funciones continúan disponibles.

## Inicialización DHCP

| Función | Ejemplo |
|---|---|
| `Ethernet.begin(mac, timeout, responseTimeout)` | `int ok = Ethernet.begin(mac, 5000, 1000);` |
| `Ethernet.maintain()` | `int r = Ethernet.maintain();` |

## Inicialización IP estática

| Función | Ejemplo |
|---|---|
| `Ethernet.begin(mac, ip)` | `Ethernet.begin(mac, ip);` |
| `Ethernet.begin(mac, ip, dns)` | `Ethernet.begin(mac, ip, dns);` |
| `Ethernet.begin(mac, ip, dns, gateway)` | `Ethernet.begin(mac, ip, dns, gw);` |
| `Ethernet.begin(mac, ip, dns, gateway, subnet)` | `Ethernet.begin(mac, ip, dns, gw, mask);` |
| `Ethernet.init(csPin)` | `Ethernet.init(5);` |

`Ethernet.init()` no debería usarse para cambiar el CS del JWPLC Basic en un
sketch normal.

## Estado/datos

| Función | Ejemplo |
|---|---|
| `Ethernet.linkStatus()` | `auto s = Ethernet.linkStatus();` |
| `Ethernet.hardwareStatus()` | `auto s = Ethernet.hardwareStatus();` |
| `Ethernet.MACAddress(mac)` | `Ethernet.MACAddress(mac);` |
| `Ethernet.localIP()` | `IPAddress ip = Ethernet.localIP();` |
| `Ethernet.subnetMask()` | `IPAddress m = Ethernet.subnetMask();` |
| `Ethernet.gatewayIP()` | `IPAddress g = Ethernet.gatewayIP();` |
| `Ethernet.dnsServerIP()` | `IPAddress d = Ethernet.dnsServerIP();` |

## Setters de compatibilidad

| Función | Ejemplo |
|---|---|
| `setMACAddress(mac)` | `Ethernet.setMACAddress(mac);` |
| `setLocalIP(ip)` | `Ethernet.setLocalIP(ip);` |
| `setSubnetMask(mask)` | `Ethernet.setSubnetMask(mask);` |
| `setGatewayIP(gateway)` | `Ethernet.setGatewayIP(gw);` |
| `setDnsServerIP(dns)` | `Ethernet.setDnsServerIP(dns);` |
| `setRetransmissionTimeout(ms)` | `Ethernet.setRetransmissionTimeout(1000);` |
| `setRetransmissionCount(num)` | `Ethernet.setRetransmissionCount(3);` |

## DHCP cooperativo AVANZADO

Estas funciones existen para el runtime JWPLC:

| Función | Ejemplo |
|---|---|
| `beginDHCPAsync(mac,...)` | `int r = Ethernet.beginDHCPAsync(mac, 5000, 1000);` |
| `pollDHCP()` | `int r = Ethernet.pollDHCP();` |
| `dhcpInProgress()` | `bool x = Ethernet.dhcpInProgress();` |
| `cancelDHCP()` | `Ethernet.cancelDHCP();` |
| `maintainAsync()` | `int r = Ethernet.maintainAsync();` |
| `dhcpMaintenanceInProgress()` | `bool x = Ethernet.dhcpMaintenanceInProgress();` |

No se recomienda controlar DHCP con estas primitivas cuando se utiliza
`JWPLC_Ethernet`.

## APIs de test/profiling condicionales

Los nombres `testSetDhcpLeaseTimers()`, `testGetDhcpLeaseTimers()`,
`testDhcpLeaseMaintenanceMode()`, `jwplcProfileResetTcpRx()` y
`jwplcProfileGetTcpRx()` sólo existen cuando se activan macros de test/profile.

Ejemplo válido únicamente en un build de qualification:

```cpp
#ifdef JWPLC_ETHERNET_ENABLE_TEST_HOOKS
Ethernet.testSetDhcpLeaseTimers(10, 20);
#endif

#if JWPLC_ETHERNET_ENABLE_PROFILE_HOOKS
Ethernet.jwplcProfileResetTcpRx();
auto p = Ethernet.jwplcProfileGetTcpRx();
#endif
```

No forman parte del contrato normal de usuario.

## DhcpClass

`DhcpClass` aparece en el header por implementación de la librería Ethernet.
**No se considera API de aplicación soportada del JWPLC**.

Una IA o usuario debe preferir:

```cpp
JWPLC_Ethernet.useDHCP();
```

y no instanciar `DhcpClass` directamente.
