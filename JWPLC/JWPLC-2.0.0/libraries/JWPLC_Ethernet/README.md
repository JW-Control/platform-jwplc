# JWPLC_Ethernet

Librería interna del package **JWPLC ESP32** para manejar Ethernet W5500 en **JWPLC Basic**.

> Estado: integración inicial para `2.0.0-alpha.26`.

---

## 1. Objetivo

`JWPLC_Ethernet` ofrece una API simple para inicializar y monitorear Ethernet sin que el usuario tenga que configurar manualmente:

- Pines SPI.
- CS del W5500.
- MAC local.
- Detección de hardware.
- Estado de link.
- DHCP.
- IP estática.
- Timeouts básicos.
- Protección SPI básica dentro del ecosistema JWPLC.

API principal:

```cpp
JWPLC_Ethernet.begin();
JWPLC_Ethernet.isReady();
JWPLC_Ethernet.hardwarePresent();
JWPLC_Ethernet.linkUp();
JWPLC_Ethernet.localIP();
JWPLC_Ethernet.statusString();
JWPLC_Ethernet.maintain();
```

---

## 2. Alcance de esta versión

Esta versión usa la librería Arduino `Ethernet` como base.

El objetivo de `alpha26` no es todavía exprimir el máximo rendimiento del W5500, sino dejar una integración interna:

- Estable.
- Fácil de usar.
- Compatible con JWPLC Basic.
- Segura para convivir con TFT, SD y FRAM en el mismo bus SPI.
- No bloqueante cuando no hay cable RJ45 conectado.

La optimización avanzada de throughput del W5500 queda para una etapa posterior.

---

## 3. Uso básico con DHCP

```cpp
#include <JWPLC_Ethernet.h>

void setup()
{
    Serial.begin(115200);
    delay(1200);

    if (JWPLC_Ethernet.begin())
    {
        Serial.print("IP: ");
        Serial.println(JWPLC_Ethernet.localIP());
    }
    else
    {
        Serial.print("Ethernet error: ");
        Serial.println(JWPLC_Ethernet.statusString());
    }
}

void loop()
{
    JWPLC_Ethernet.maintain();
}
```

Resultado esperado con cable conectado y servidor DHCP disponible:

```text
Ethernet begin OK
Status: OK
IP: 192.168.x.x
```

Si no hay cable RJ45:

```text
Ethernet begin failed
Status: Link OFF
IP: 0.0.0.0
```

---

## 4. Uso con IP estática

```cpp
#include <JWPLC_Ethernet.h>

void setup()
{
    Serial.begin(115200);
    delay(1200);

    bool ok = JWPLC_Ethernet.begin(
        IPAddress(192, 168, 0, 180),
        IPAddress(8, 8, 8, 8),
        IPAddress(192, 168, 0, 1),
        IPAddress(255, 255, 255, 0));

    Serial.println(ok ? "Ethernet static begin OK" : "Ethernet static begin failed");
    JWPLC_Ethernet.printStatus(Serial);
}

void loop()
{
}
```

Orden de parámetros:

```cpp
JWPLC_Ethernet.begin(localIP, dnsIP, gatewayIP, subnetMask);
```

---

## 5. API principal

### `begin()`

Inicializa Ethernet usando DHCP.

```cpp
bool ok = JWPLC_Ethernet.begin();
```

En `alpha26`, si no hay link físico, sale rápido y evita esperar DHCP durante varios segundos.

### `begin(mac)`

Inicializa usando una MAC personalizada.

```cpp
uint8_t mac[6] = {0x02, 0x4A, 0x57, 0x00, 0x00, 0x01};
JWPLC_Ethernet.begin(mac);
```

### `begin(localIP, dnsIP, gatewayIP, subnetMask)`

Inicializa usando IP estática.

```cpp
JWPLC_Ethernet.begin(
    IPAddress(192, 168, 0, 180),
    IPAddress(8, 8, 8, 8),
    IPAddress(192, 168, 0, 1),
    IPAddress(255, 255, 255, 0));
```

### `maintain()`

Mantiene DHCP.

```cpp
JWPLC_Ethernet.maintain();
```

Debe llamarse periódicamente en `loop()` cuando se usa DHCP.

### `isEnabled()`

Indica si Ethernet está habilitado para la placa seleccionada.

```cpp
if (JWPLC_Ethernet.isEnabled())
{
    Serial.println("Ethernet habilitado");
}
```

En **JWPLC Basic Core** debe reportar `false`.

### `isReady()`

Indica si Ethernet fue inicializado correctamente.

```cpp
if (JWPLC_Ethernet.isReady())
{
    Serial.println("Ethernet listo");
}
```

### `hardwarePresent()`

Indica si se detectó el W5500.

```cpp
Serial.println(JWPLC_Ethernet.hardwarePresent() ? "W5500 presente" : "W5500 no detectado");
```

### `linkUp()`

Indica si el cable RJ45 tiene link físico.

```cpp
Serial.println(JWPLC_Ethernet.linkUp() ? "Link UP" : "Link DOWN");
```

### `localIP()`

Devuelve la IP local.

```cpp
IPAddress ip = JWPLC_Ethernet.localIP();
Serial.println(ip);
```

### `statusString()`

Devuelve un texto corto del estado.

```cpp
Serial.println(JWPLC_Ethernet.statusString());
```

Posibles mensajes:

```text
OK
Ethernet disabled
SPI not ready
No Ethernet hardware
Link OFF
DHCP failed
Invalid IP
SPI lock timeout
Unknown Ethernet error
```

### `printStatus(Stream &out)`

Imprime un resumen completo del estado.

```cpp
JWPLC_Ethernet.printStatus(Serial);
```

Ejemplo:

```text
Ethernet enabled: yes
Begin attempted: yes
Ready: yes
Hardware: present
Link: up
Status: OK
IP: 192.168.0.159
Gateway: 192.168.0.1
Subnet: 255.255.255.0
DNS: 190.113.220.18
MAC: 02:4A:57:D5:D8:C4
```

---

## 6. Arranque sin cable RJ45

Antes, al llamar DHCP sin cable, el programa podía quedarse varios segundos esperando.

Desde `alpha26`, `JWPLC_Ethernet.begin()` hace una verificación rápida:

1. Inicializa el W5500 sin DHCP.
2. Verifica hardware.
3. Verifica link físico.
4. Si no hay link, sale con `Link OFF`.
5. Solo si hay link intenta DHCP.

Esto evita que la pantalla quede negra durante varios segundos al encender el equipo sin Ethernet conectado.

---

## 7. Reintento automático

Si el equipo arranca sin RJ45, se recomienda reintentar cada cierto tiempo:

```cpp
bool ethernetStarted = false;
unsigned long lastRetryMs = 0;

const unsigned long RETRY_PERIOD_MS = 5000;

void setup()
{
    ethernetStarted = JWPLC_Ethernet.begin();
}

void loop()
{
    unsigned long now = millis();

    if (ethernetStarted)
    {
        JWPLC_Ethernet.maintain();
    }

    if (JWPLC_Ethernet.isEnabled() &&
        !ethernetStarted &&
        (now - lastRetryMs >= RETRY_PERIOD_MS))
    {
        lastRetryMs = now;
        ethernetStarted = JWPLC_Ethernet.begin();
    }
}
```

Comportamiento esperado:

```text
Arranca sin RJ45 -> Link OFF
Conectas RJ45   -> retry
Resultado       -> OK
IP              -> 192.168.x.x
```

---

## 8. Integración con JWPLC_Display

Ejemplo:

```cpp
#include <JWPLC_Ethernet.h>
#include <JWPLC_Display.h>

bool ethernetStarted = false;
bool displayConfigured = false;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    ethernetStarted = JWPLC_Ethernet.begin();
}

void loop()
{
    if (ethernetStarted)
    {
        JWPLC_Ethernet.maintain();
    }

    if (!displayConfigured && JWPLC_Display.isReady())
    {
        displayConfigured = true;

        JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
        JWPLC_Display.setIdleTimeoutMs(8000);
    }

    bool ethOk = ethernetStarted && JWPLC_Ethernet.linkUp();

    if (displayConfigured)
    {
        JWPLC_Display.setEthLed(ethOk);
        JWPLC_Display.setErrLed(!ethOk);
    }
}
```

---

## 9. SPI compartido: regla importante

En JWPLC Basic, Ethernet W5500 comparte SPI con:

- TFT ST7789.
- FRAM.
- microSD.

`JWPLC_Ethernet` protege sus operaciones internas de estado e inicialización con el mutex SPI global del ecosistema JWPLC.

Aun así, para sketches de usuario se recomienda:

> No consultar Ethernet, SD o FRAM dentro de callbacks gráficos del display.

Patrón recomendado:

1. Leer Ethernet/SD/FRAM en `loop()`.
2. Guardar estados en variables simples.
3. Dibujar esas variables en callbacks USER.

Ejemplo recomendado:

```cpp
char ethStatusText[32] = "Unknown";
char ethIpText[20] = "0.0.0.0";

void updateEthernetCache()
{
    strncpy(ethStatusText, JWPLC_Ethernet.statusString(), sizeof(ethStatusText) - 1);
    ethStatusText[sizeof(ethStatusText) - 1] = '\0';

    IPAddress ip = JWPLC_Ethernet.localIP();
    snprintf(ethIpText, sizeof(ethIpText), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void loop()
{
    updateEthernetCache();
}
```

Luego en el callback gráfico:

```cpp
extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;

    auto &tft = JWPLC_Display.tft();

    tft.fillRect(70, 50, 180, 16, ST77XX_BLACK);
    tft.setCursor(70, 50);
    tft.print(ethStatusText);
}
```

No recomendado:

```cpp
extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
    auto &tft = JWPLC_Display.tft();

    // Evitar: Ethernet usa SPI.
    tft.print(JWPLC_Ethernet.statusString());
}
```

---

## 10. JWPLC Basic vs JWPLC Basic Core

### JWPLC Basic

Ethernet está habilitado:

```text
Ethernet enabled: yes
```

### JWPLC Basic Core

Ethernet está deshabilitado:

```text
Ethernet enabled: no
Status: Ethernet disabled
IP: 0.0.0.0
```

Los ejemplos deben evitar reintentos cuando `JWPLC_Ethernet.isEnabled()` sea `false`.

---

## 11. Ejemplos incluidos

### `Ethernet_DHCP_Status`

Prueba básica con DHCP.

Valida:

- `begin()`.
- Detección de hardware.
- Link.
- IP asignada por DHCP.
- `statusString()`.
- `printStatus()`.

### `Ethernet_StaticIP_Status`

Prueba con IP estática.

Valida:

- `begin(localIP, dnsIP, gatewayIP, subnetMask)`.
- IP manual.
- Link.
- Estado general.

### `Ethernet_Display_Status`

Prueba Ethernet + Display.

Valida:

- Indicador `ETH` en pantalla IDLE.
- Indicador `ERR` cuando Ethernet no está OK.
- Arranque sin cable RJ45 sin bloqueo largo.
- Reintento automático cuando se conecta RJ45 después del arranque.
- Comportamiento correcto en JWPLC Basic Core.

### `Ethernet_SPI_Coexistence`

Prueba Ethernet + Display + FRAM + microSD.

Valida:

- Convivencia SPI.
- Logs periódicos en microSD.
- Contador de arranque en FRAM.
- Estado Ethernet en pantalla USER.
- Indicadores ETH/ERR/RUN en IDLE.
- Patrón seguro de cachear estados fuera de callbacks gráficos.

---

## 12. Buenas prácticas

### Llamar `maintain()` periódicamente

```cpp
if (ethernetStarted)
{
    JWPLC_Ethernet.maintain();
}
```

### Evitar DHCP si sabes que usarás IP fija

En redes industriales suele ser común usar IP estática:

```cpp
JWPLC_Ethernet.begin(
    IPAddress(192, 168, 0, 180),
    IPAddress(8, 8, 8, 8),
    IPAddress(192, 168, 0, 1),
    IPAddress(255, 255, 255, 0));
```

### No reintentar demasiado rápido

Si no hay cable, reintentar cada 5 segundos es razonable:

```cpp
const unsigned long RETRY_PERIOD_MS = 5000;
```

### Cachear estado para pantalla

Evita llamar Ethernet directamente desde callbacks de TFT.

---

## 13. Limitaciones actuales

Esta versión todavía no busca el máximo throughput del W5500.

Pendiente para futuras versiones:

- Wrappers seguros para `EthernetClient`.
- Wrappers seguros para `EthernetServer`.
- Wrappers seguros para UDP.
- Pruebas de rendimiento TCP/UDP.
- Ajustes finos de chunk size.
- Optimización de transferencia por bloque.
- Posible librería portable `JW_Ethernet` en `JW-Libraries`.

---

## 14. Estado

Documentación correspondiente a:

```text
JWPLC ESP32 2.0.0-alpha.26
JWPLC_Ethernet 1.0.0
```
