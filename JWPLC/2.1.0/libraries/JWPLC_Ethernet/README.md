# JWPLC_Ethernet

Librería del package **JWPLC ESP32** para el W5500 integrado del **JWPLC Basic**.

El autoload usa un runtime **cooperativo/no bloqueante** para detectar hardware, revisar link, adquirir DHCP, recuperar la red y mantener el lease.

## Uso normal

En JWPLC Basic no es necesario llamar `begin()` ni `maintain()` desde el sketch.

```cpp
#include <JWPLC_Ethernet.h>

void setup()
{
    Serial.begin(115200);
    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    Serial.println(JWPLC_Ethernet.statusString());
    delay(1000);
}
```

El task del sistema llama periódicamente:

```cpp
JWPLC_Ethernet.service();
```

Cada llamada ejecuta un paso corto y retorna.

## Estados del runtime

```text
JWPLC_ETH_STATE_NOT_STARTED
JWPLC_ETH_STATE_PROBING
JWPLC_ETH_STATE_PHY_READY
JWPLC_ETH_STATE_LINK_OFF
JWPLC_ETH_STATE_DHCP_PENDING
JWPLC_ETH_STATE_READY
JWPLC_ETH_STATE_ERROR
```

Flujo típico DHCP:

```text
NOT_STARTED -> PROBING -> PHY_READY -> DHCP_PENDING -> READY
```

La desconexión de RJ45 lleva a `LINK_OFF`; el runtime puede recuperarse al volver el link sin resetear el ESP32.

## DHCP

```cpp
JWPLC_Ethernet.useDHCP();
```

El mantenimiento T1/T2 se ejecuta cooperativamente. Un lease vigente no se invalida sólo por haber iniciado una renovación.

## IP estática

Configurar en `setup()` antes de que finalice y arranque el task automático de sistema:

```cpp
JWPLC_Ethernet.setStaticIP(
    IPAddress(192, 168, 1, 50),
    IPAddress(192, 168, 1, 1),
    IPAddress(192, 168, 1, 1),
    IPAddress(255, 255, 255, 0));
```

## API principal

Estado:

```cpp
JWPLC_Ethernet.isEnabled();
JWPLC_Ethernet.isBeginAttempted();
JWPLC_Ethernet.isReady();
JWPLC_Ethernet.isBusy();
JWPLC_Ethernet.hardwarePresent();
JWPLC_Ethernet.linkUp();
JWPLC_Ethernet.hardwareStatus();
JWPLC_Ethernet.linkStatus();
JWPLC_Ethernet.runtimeState();
JWPLC_Ethernet.lastError();
JWPLC_Ethernet.lastErrorString();
JWPLC_Ethernet.statusString();
JWPLC_Ethernet.diagnosticCode();
```

Red:

```cpp
JWPLC_Ethernet.localIP();
JWPLC_Ethernet.gatewayIP();
JWPLC_Ethernet.subnetMask();
JWPLC_Ethernet.dnsServerIP();
JWPLC_Ethernet.mac();
```

Diagnóstico agrupado:

```cpp
JWPLC_Ethernet.printStatus(Serial);
```

Configuración:

```cpp
JWPLC_Ethernet.setMac(mac);
JWPLC_Ethernet.useDefaultMac();
JWPLC_Ethernet.useDHCP();
JWPLC_Ethernet.setStaticIP(localIP, dnsIP, gatewayIP, subnetMask);
JWPLC_Ethernet.setTimeouts(dhcpTimeoutMs, responseTimeoutMs);
JWPLC_Ethernet.setRetransmissionCount(count);
```

`configure()`/CS/reset pertenecen al hardware del JWPLC Basic y normalmente no deben cambiarse.

## Compatibilidad síncrona

Se conservan:

```cpp
JWPLC_Ethernet.begin();
JWPLC_Ethernet.maintain();
```

Son rutas explícitas/legacy. El autoload normal no depende de ellas.

## Códigos ETH

| Código | Significado |
|---|---|
| `DIS` | Ethernet deshabilitado por la variante. |
| `INI` | Runtime aún no iniciado. |
| `PHY` | Sondeo/preparación del W5500. |
| `LNK` | Sin link RJ45. |
| `DHC` | Adquisición/mantenimiento DHCP. |
| `HW` | W5500 no detectado. |
| `IP` | Configuración/IP inválida. |
| `SPI` | Timeout del mutex SPI. |
| `---` | Operativo. |

El Display puede consumir estos códigos automáticamente con:

```cpp
JWPLC_Display.setEthLedAuto(true);
```

## SPI compartido

W5500 comparte SPI con TFT, FRAM y microSD. `JWPLC_Ethernet` usa el mutex global antes de acceder al W5500.

Alpha7 corrigió el caso donde una contención temporal del mutex podía interpretarse erróneamente como `LINK_OFF`. Un timeout SPI ya no equivale automáticamente a cable desconectado.

## Ejemplos numerados para taller

```text
01.Ethernet_DHCP_Basic
02.Ethernet_StaticIP_Basic
03.Ethernet_Diagnostics
```

Los ejemplos de stress, HTTP/TFT y coexistencia SPI existentes permanecen como material avanzado.

## Estado Alpha8

```text
JWPLC ESP32 2.1.0-alpha.8
JWPLC_Ethernet 1.0.0
Autoload cooperativo: activo
```

Alpha8 no retira Ethernet del autoload normal ni cambia la API pública validada.
