# JWPLC_Ethernet

Librería interna del package **JWPLC ESP32** para manejar el W5500 integrado del **JWPLC Basic**.

En `v2.1.0-alpha.6` el backend W5500 queda consolidado dentro de `JWPLC_Ethernet` y el autoload usa un runtime **cooperativo/no bloqueante** para sondeo físico, link, adquisición DHCP, recuperación y mantenimiento T1/T2.

> Esta librería pertenece al ecosistema JWPLC. No está planteada como wrapper genérico para cualquier ESP32 + W5500.

## Uso normal

En JWPLC Basic Ethernet forma parte del autoload. Para uso normal el sketch no necesita llamar `begin()` ni `maintain()`.

```cpp
void setup()
{
    Serial.begin(115200);
}

void loop()
{
    Serial.print("ETH: ");
    Serial.print(JWPLC_Ethernet.statusString());
    Serial.print(" | IP: ");
    Serial.println(JWPLC_Ethernet.localIP());

    delay(1000);
}
```

Los objetos globales del package quedan expuestos por el entorno JWPLC. También puede incluirse explícitamente:

```cpp
#include <JWPLC_Ethernet.h>
```

## Arquitectura Alpha6

El autoload llama periódicamente:

```cpp
JWPLC_Ethernet.service();
```

Cada llamada realiza un paso corto. El runtime no intenta completar toda la secuencia de red dentro de un único tick.

Estados públicos:

```text
NOT_STARTED
PROBING
PHY_READY
LINK_OFF
DHCP_PENDING
READY
ERROR
```

Flujo típico DHCP:

```text
NOT_STARTED
→ PROBING
→ PHY_READY
→ LINK_OFF o DHCP_PENDING
→ READY
```

Si el cable se desconecta después de estar listo, el runtime pasa a `LINK_OFF` y reintenta cuando vuelve el link. No requiere reset del ESP32.

## DHCP cooperativo

Por defecto el JWPLC Basic usa DHCP.

Alpha6 separa dos problemas:

1. adquisición inicial del lease;
2. mantenimiento posterior del lease.

Durante la adquisición inicial el runtime sigue avanzando por llamadas de `service()` y conserva un límite de espera/reintento.

Durante mantenimiento se atienden de forma cooperativa:

- T1 `renew`;
- T2 `rebind`.

Un lease ya válido no se borra sólo porque haya comenzado una renovación. Mientras la configuración vigente siga utilizable, `isReady()` e IP pueden permanecer válidos durante el mantenimiento.

Las pruebas aceleradas de Alpha6 confirmaron T1 y T2 reales con:

- estado DHCP pendiente observado;
- link conservado;
- IP conservada;
- `READY` conservado cuando correspondía;
- llamadas `service()` muy por debajo del límite de 100 ms utilizado por el gate.

Los hooks empleados para acelerar T1/T2 existen únicamente bajo una macro de prueba y quedaron verificados como ausentes en un build normal de producción.

## IP estática

Puede configurarse antes del primer intento automático:

```cpp
JWPLC_Ethernet.setStaticIP(
    IPAddress(192, 168, 1, 50),
    IPAddress(8, 8, 8, 8),
    IPAddress(192, 168, 1, 1),
    IPAddress(255, 255, 255, 0));
```

También puede volver a DHCP:

```cpp
JWPLC_Ethernet.useDHCP();
```

## API principal

### Estado

```cpp
JWPLC_Ethernet.isEnabled();
JWPLC_Ethernet.isBeginAttempted();
JWPLC_Ethernet.isReady();
JWPLC_Ethernet.isBusy();
JWPLC_Ethernet.hardwarePresent();
JWPLC_Ethernet.linkUp();

JWPLC_Ethernet.mode();
JWPLC_Ethernet.runtimeState();
JWPLC_Ethernet.lastError();
JWPLC_Ethernet.lastErrorString();
JWPLC_Ethernet.statusString();
JWPLC_Ethernet.diagnosticCode();
```

### Red

```cpp
JWPLC_Ethernet.localIP();
JWPLC_Ethernet.gatewayIP();
JWPLC_Ethernet.subnetMask();
JWPLC_Ethernet.dnsServerIP();
JWPLC_Ethernet.mac();
```

### Diagnóstico completo

```cpp
JWPLC_Ethernet.printStatus(Serial);
```

### Configuración previa al inicio

```cpp
JWPLC_Ethernet.setMac(mac);
JWPLC_Ethernet.useDefaultMac();
JWPLC_Ethernet.useDHCP();
JWPLC_Ethernet.setStaticIP(localIP, dnsIP, gatewayIP, subnetMask);
JWPLC_Ethernet.setTimeouts(dhcpTimeoutMs, responseTimeoutMs);
JWPLC_Ethernet.setRetransmissionCount(count);
```

No se recomienda cambiar `configure()` ni el CS del W5500 en un JWPLC Basic normal.

## Compatibilidad síncrona

Las APIs antiguas siguen disponibles:

```cpp
bool ok = JWPLC_Ethernet.begin();
int result = JWPLC_Ethernet.maintain();
```

`begin()` realiza inicialización completa síncrona y `maintain()` conserva el mantenimiento síncrono legacy. Se mantienen para compatibilidad, pruebas o flujos explícitos del usuario.

El autoload Alpha6 **no depende de ellas**: usa `service()` y mantenimiento DHCP cooperativo.

## Errores

```text
JWPLC_ETH_OK
JWPLC_ETH_DISABLED
JWPLC_ETH_SPI_NOT_READY
JWPLC_ETH_NO_HARDWARE
JWPLC_ETH_LINK_OFF
JWPLC_ETH_DHCP_FAILED
JWPLC_ETH_INVALID_IP
JWPLC_ETH_BUS_LOCK_TIMEOUT
JWPLC_ETH_UNKNOWN_ERROR
```

`LINK_OFF` no implica que el sistema quede permanentemente fallado. Es un estado recuperable del runtime.

## Códigos para el indicador ETH

`diagnosticCode()` entrega un código corto usado por `JWPLC_Display`:

| Código | Significado |
|---|---|
| `DIS` | Ethernet deshabilitado por la variante. |
| `INI` | Runtime aún no iniciado. |
| `PHY` | Sondeo/preparación física. |
| `LNK` | Sin link RJ45. |
| `DHC` | Adquisición o mantenimiento DHCP. |
| `HW` | W5500 no detectado. |
| `IP` | IP/configuración inválida. |
| `SPI` | Timeout de acceso al bus SPI. |
| `---` | Operativo. |

La pantalla IDLE usa estos códigos automáticamente cuando:

```cpp
JWPLC_Display.setEthLedAuto(true);
```

No es necesario trasladar fallas de Ethernet al indicador `ERR`: en Alpha6 `ERR` queda reservado para la aplicación.

## Comportamiento por placa

### JWPLC Basic

Ethernet está habilitado y el W5500 forma parte del runtime normal.

### JWPLC Basic Core

Ethernet puede estar deshabilitado por configuración de variante. En ese caso:

```text
isEnabled() = false
diagnosticCode() = DIS
```

La ausencia intencional de Ethernet no debe interpretarse como error de aplicación.

## SPI compartido

W5500 comparte SPI con:

- TFT ST7789;
- FRAM;
- microSD.

`JWPLC_Ethernet` adquiere el mutex del ecosistema JWPLC antes de tocar el W5500 y libera el bus al terminar cada paso.

Regla recomendada para UI:

> No realizar consultas largas a Ethernet, FRAM o SD dentro de un callback gráfico mientras la TFT posee SPI.

Leer/cachar primero y dibujar después mantiene la coexistencia determinista.

## Recuperación validada en Alpha6

Se validaron sobre hardware real:

- arranque con link;
- arranque sin link;
- conectar/desconectar/reconectar RJ45 sin reset;
- router DHCP;
- IP estática;
- servidor HTTP y tráfico repetitivo;
- estrés simultáneo con TFT, FRAM y microSD;
- transición router → red sin DHCP → router;
- T1 renew y T2 rebind cooperativos;
- exclusión de hooks DHCP en builds normales.

## Estado

```text
JWPLC ESP32 2.1.0-alpha.6
JWPLC_Ethernet 1.0.0
```

Alpha6 cambia la arquitectura interna de servicio Ethernet para evitar bloqueos largos y mejorar recuperación, manteniendo las APIs síncronas ya existentes por compatibilidad.
