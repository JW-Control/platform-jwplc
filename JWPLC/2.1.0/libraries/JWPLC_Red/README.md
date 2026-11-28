# JWPLC_Red

Cable o WiFi **con el mismo código**. Versión **0.1.0** (línea `v2.1.0-alpha.19/JWPLC-Sim`).

`JWPLC_Red` elige por qué red sale el JWPLC Basic y da `JWPLC_RedClient`, un cliente TCP que funciona igual
por Ethernet (W5500) o por WiFi. Sobre él se construyen HTTP, HTTPS, MQTT y JW Cloud.

```cpp
#include <JWPLC_Red.h>
#include <JWPLC_WiFi.h> // solo si se usará WiFi

JWPLC_RedClient cliente;

void setup()
{
    JWPLC_WiFi.connect("MiRed", "MiClave");
    JWPLC_Red.useAuto(); // cable si está conectado; si no, WiFi
}

void loop()
{
    if (JWPLC_Red.isReady() && cliente.connect("ejemplo.com", 80))
    {
        // cliente.print(...), cliente.read(), cliente.stop()
    }
}
```

## Capas

```text
Capa física     JWPLC_Ethernet (W5500)        JWPLC_WiFi (radio del ESP32)
Capa común      JWPLC_Red: modo + JWPLC_RedClient
Protocolos      HTTP, HTTPS (JWPLC_TLS), MQTT, JW Cloud (JWPLC_Cloud)   ← próximas
```

## API

| Llamada | Para qué |
|---|---|
| `useEthernet()` | solo cable (modo por defecto, como hasta ahora) |
| `useWiFi()` | solo WiFi |
| `useAuto()` | cable si está listo; si no, WiFi |
| `mode()` | `RED_MODE_ETHERNET`, `RED_MODE_WIFI` o `RED_MODE_AUTO` |
| `active()` / `activeName()` | por dónde saldría una conexión nueva: `RED_ETHERNET`, `RED_WIFI` o `RED_NONE` |
| `isReady()` | hay una red lista según el modo |
| `localIP()` | IP de la red activa |
| `wifiAvailable()` | el sketch incluye `JWPLC_WiFi.h` |
| `ethernetReady()` / `wifiReady()` | estado de cada red |
| `printStatus(Serial)` | resumen en una línea |

`JWPLC_RedClient` es un `Client` de Arduino (`connect`, `print`, `write`, `available`, `read`, `stop`,
`connected`) y además `network()`: por qué red salió la conexión actual. La red se elige en cada `connect()`:
en modo automático, si se desenchufa el cable, la siguiente conexión sale por WiFi sin tocar el sketch.

## Servidor: `JWPLC_RedServer`

```cpp
JWPLC_RedServer servidor(80);
void setup() { servidor.begin(); }
void loop()
{
    JWPLC_RedClient c = servidor.available(); // nunca espera
    if (c) { /* leer el pedido, responder y */ c.stop(); }
}
```

| Llamada | Para qué |
|---|---|
| `begin()` / `begin(puerto)` / `end()` | empezar o dejar de escuchar |
| `available()` | la próxima conexión nueva (cada una se entrega una sola vez) o un cliente vacío |
| `listeningEthernet()` / `listeningWiFi()` | en qué redes está escuchando ahora |
| `port()` | puerto |

- Modo `useEthernet()`: escucha solo por cable. `useWiFi()`: solo WiFi (red del router y punto de acceso
  propio de `JWPLC_WiFi`). `useAuto()`: **las dos a la vez**, un socket en cada red; `available()` alterna
  entre ellas para atender las dos por igual.
- Si el cable se desenchufa o la WiFi se cae, vuelve a escuchar sola cuando la red regresa.
- La conexión aceptada es un `JWPLC_RedClient`: `network()` dice por qué red llegó. Cada operación del
  W5500 toma el bus SPI compartido por dentro (como `EthernetClient`).
- Como `EthernetClient` y `WiFiClient`, un `JWPLC_RedClient` se puede copiar y no cierra la conexión al
  salir de su ámbito: siempre `stop()`.
- Base común de `JWPLC_Web` y de Modbus TCP esclavo por WiFi.

## WiFi opcional

`JWPLC_Red` no incluye la pila WiFi: la engancha solo si el sketch incluye `JWPLC_WiFi.h` (símbolos débiles).
Medido con los ejemplos: `02.Red_Solo_Cable` ocupa 492 KB de flash, igual que un sketch solo con Ethernet;
`01.Red_Cable_o_WiFi`, 1,11 MB. Sin WiFi, el sketch corre en **JWPLC Sim**, que simula el W5500 y sale a
Internet por la red de la PC.

Ejemplos: `01.Red_Cable_o_WiFi` (placa), `02.Red_Solo_Cable` y `03.Red_Servidor` (placa y JWPLC Sim). Código fuente (sin `.a`).
