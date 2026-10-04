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

## Extensiones cooperativas Alpha12

El backend Ethernet incorpora primitives aditivas para que librerías como
`JWPLC_ModbusTCP` puedan trabajar sin bloquear largos periodos.

`EthernetClient`:

```text
beginConnectAsync()
pollConnectAsync()
connectAsyncInProgress()
cancelConnectAsync()

beginStopAsync()
pollStopAsync()
stopAsyncInProgress()
cancelStopAsync()

beginFlushAsync()
pollFlushAsync()
flushAsyncInProgress()
cancelFlushAsync()

beginWriteAsync()
pollWriteAsync()
writeAsyncInProgress()
cancelWriteAsync()
```

UDP:

```text
beginEndPacketAsync()
pollEndPacketAsync()
endPacketAsyncInProgress()
cancelEndPacketAsync()
```

Estas extensiones no sustituyen ni cambian la semántica de las APIs Arduino
legacy.

## Fast RX aditivo/interno

Para consumers cooperativos de alto rendimiento existen extensiones JWPLC:

UDP:

```text
jwplcReadPacketFastDeferred()
jwplcCommitRxFast()
```

TCP:

```text
jwplcReadTcpFastDeferred()
jwplcCommitRxFast()
```

Estas rutas son aditivas. No reemplazan transparentemente
`parsePacket()/read()` ni `EthernetClient::read()`.

## Refresh L2

El runtime incluye un refresh L2 best-effort para mantener fresca la presencia
de la MAC durante periodos largos sin tráfico saliente.

Defaults:

```text
JWPLC_ETH_L2_REFRESH_PERIOD_MS=120000
JWPLC_ETH_L2_REFRESH_UDP_PORT=9
```

Un fallo de este refresh no invalida por sí solo el estado `READY`.

## Perfil W5500 validado en Alpha12

```text
SPI_W5500=26 MHz
RX_FIFO_REUSE=ON
RX_DIRECT_TRANSFER=OFF
```

Las optimizaciones de SPI compartido como DLEN cache y COPY_OUT_64 pertenecen al
backend interno y no requieren cambios en sketches de usuario.

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

## Estado Alpha12

```text
JWPLC ESP32 2.1.0-alpha.12
JWPLC_Ethernet 1.0.0
AUTOLOAD_COOPERATIVE=YES
W5500_SPI_HZ=26000000
LEGACY_API_PRESERVED=YES
ASYNC_BACKEND=QUALIFIED
FAST_RX_PATH=ADDITIVE_INTERNAL
```

Alpha12 conserva Ethernet dentro del autoload normal y mantiene la API Arduino
legacy. Las nuevas primitives cooperativas/fast-path se añaden para librerías y
consumers JWPLC sin obligar a sketches existentes a cambiar.
