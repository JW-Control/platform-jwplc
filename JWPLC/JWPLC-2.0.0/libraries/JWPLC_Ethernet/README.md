# JWPLC_Ethernet

Librería interna del package **JWPLC ESP32** para manejar el Ethernet W5500 integrado del **JWPLC Basic**.

> Esta librería está pensada exclusivamente para el ecosistema JWPLC Basic / JWPLC Basic Core dentro del package JWPLC 2.0.0. No está planteada como librería genérica portable para cualquier ESP32.

---

## 1. Idea principal

Desde `2.0.0-alpha.26`, Ethernet queda integrado al runtime del JWPLC.

En uso normal, el usuario **no necesita llamar**:

```cpp
JWPLC_Ethernet.begin();
JWPLC_Ethernet.maintain();
```

El sistema JWPLC se encarga automáticamente de:

- Detectar si Ethernet está habilitado para la placa.
- Inicializar el W5500.
- Evitar bloqueos largos si no hay cable RJ45.
- Reintentar si el equipo arrancó sin cable y luego se conecta.
- Mantener DHCP con `maintain()`.
- Proteger el bus SPI compartido.
- Reportar estado mediante `JWPLC_Ethernet`.

---

## 2. Uso recomendado

En sketches para **JWPLC Basic**, puedes consultar el estado directamente:

```cpp
void setup()
{
    Serial.begin(115200);
    delay(1200);
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

No hace falta agregar:

```cpp
#include <JWPLC_Ethernet.h>
```

ni llamar:

```cpp
JWPLC_Ethernet.begin();
```

porque el package JWPLC expone los periféricos globales automáticamente para las placas JWPLC.

---

## 3. Comportamiento por placa

### JWPLC Basic

Ethernet está habilitado.

Resultado esperado con RJ45 conectado:

```text
Ethernet enabled: yes
Begin attempted: yes
Ready: yes
Hardware: present
Link: up
Status: OK
IP: 192.168.x.x
```

Resultado esperado sin RJ45:

```text
Ethernet enabled: yes
Begin attempted: yes
Ready: no
Hardware: present
Link: down
Status: Link OFF
IP: 0.0.0.0
```

Si conectas el RJ45 después del arranque, el runtime reintenta automáticamente.

### JWPLC Basic Core

Ethernet está deshabilitado.

Resultado esperado:

```text
Ethernet enabled: no
Status: Ethernet disabled
IP: 0.0.0.0
```

---

## 4. DHCP automático

Por defecto, `JWPLC_Ethernet` usa DHCP.

Sketch mínimo:

```cpp
unsigned long lastPrintMs = 0;

void setup()
{
    Serial.begin(115200);
    delay(1200);

    Serial.println("Ethernet DHCP auto test");
}

void loop()
{
    unsigned long now = millis();

    if (now - lastPrintMs >= 1000)
    {
        lastPrintMs = now;

        Serial.print("ETH: ");
        Serial.print(JWPLC_Ethernet.statusString());

        Serial.print(" | Ready: ");
        Serial.print(JWPLC_Ethernet.isReady() ? "yes" : "no");

        Serial.print(" | Link: ");
        Serial.print(JWPLC_Ethernet.linkUp() ? "UP" : "DOWN");

        Serial.print(" | IP: ");
        Serial.println(JWPLC_Ethernet.localIP());
    }
}
```

---

## 5. IP estática automática

Para IP estática, configura la IP en `setup()` y deja que el runtime haga el inicio automático.

```cpp
void setup()
{
    Serial.begin(115200);
    delay(1200);

    JWPLC_Ethernet.setStaticIP(
        IPAddress(192, 168, 1, 50),
        IPAddress(8, 8, 8, 8),
        IPAddress(192, 168, 1, 1),
        IPAddress(255, 255, 255, 0));
}
```

El runtime del JWPLC empieza después de `setup()`, por eso esta configuración queda aplicada antes del primer intento automático de Ethernet.

---

## 6. API principal

- `isEnabled()`: indica si la placa tiene Ethernet habilitado.
- `isBeginAttempted()`: indica si el runtime ya intentó iniciar Ethernet.
- `isReady()`: indica si Ethernet está inicializado correctamente.
- `hardwarePresent()`: indica si se detecta el W5500.
- `linkUp()`: indica si hay link físico RJ45.
- `localIP()`: devuelve la IP local.
- `gatewayIP()`: devuelve gateway.
- `subnetMask()`: devuelve máscara de red.
- `dnsServerIP()`: devuelve DNS.
- `statusString()`: devuelve un texto corto del estado.
- `printStatus(Stream &out)`: imprime diagnóstico completo.

Estados esperados:

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
Not started
```

---

## 7. Configuración avanzada

Aunque el arranque automático es el modo recomendado, estas funciones siguen disponibles antes del inicio automático:

```cpp
JWPLC_Ethernet.setMac(mac);
JWPLC_Ethernet.useDefaultMac();
JWPLC_Ethernet.useDHCP();
JWPLC_Ethernet.setStaticIP(localIP, dnsIP, gatewayIP, subnetMask);
JWPLC_Ethernet.setTimeouts(dhcpTimeoutMs, responseTimeoutMs);
JWPLC_Ethernet.setRetransmissionCount(count);
```

Para uso normal en JWPLC Basic, no cambies `configure()` ni el CS del W5500.

---

## 8. Uso manual avanzado

`begin()` sigue existiendo, pero queda reservado para casos avanzados, pruebas internas o reinicios controlados.

Uso normal:

```cpp
// No llamar begin()
```

Uso avanzado:

```cpp
bool ok = JWPLC_Ethernet.begin();
```

---

## 9. Integración con JWPLC_Display

La pantalla IDLE puede mostrar el estado Ethernet usando el indicador `ETH`.

```cpp
bool ethOk = JWPLC_Ethernet.isReady() && JWPLC_Ethernet.linkUp();

JWPLC_Display.setEthLed(ethOk);
JWPLC_Display.setErrLed(!ethOk);
```

En **JWPLC Basic Core**, no conviene marcar error por Ethernet deshabilitado:

```cpp
bool ethernetDisabled = !JWPLC_Ethernet.isEnabled();
JWPLC_Display.setErrLed(!ethernetDisabled && !ethOk);
```

---

## 10. SPI compartido

En JWPLC Basic, Ethernet comparte SPI con:

- TFT ST7789.
- FRAM.
- microSD.
- W5500.

La librería protege sus operaciones internas con el mutex SPI del ecosistema JWPLC.

Regla importante:

> En callbacks gráficos del display, no consultes directamente `JWPLC_Ethernet`, `JWPLC_SD` ni `JWPLC_FRAM`.

Patrón recomendado:

1. Leer periféricos SPI en `loop()`.
2. Guardar resultados en variables simples.
3. En callbacks de display, solo dibujar esas variables cacheadas.

---

## 11. Ejemplos incluidos

### `Ethernet_Auto_DHCP_Status`

Valida el arranque automático por DHCP. No llama `begin()`.

### `Ethernet_Auto_StaticIP_Status`

Configura IP estática y deja que el runtime inicie Ethernet. No llama `begin()`.

### `Ethernet_Display_Status`

Muestra estado Ethernet en Serial y actualiza indicadores de la pantalla IDLE. No llama `begin()`.

### `Ethernet_SPI_Coexistence`

Valida coexistencia SPI entre Ethernet, Display, FRAM y microSD. No llama `begin()` ni `maintain()`.

---

## 12. Checklist de validación alpha26

### JWPLC Basic

- Arranque con RJ45 conectado.
- Arranque sin RJ45.
- Conectar RJ45 después del arranque.
- Desconectar/reconectar RJ45.
- DHCP automático.
- IP estática configurada en `setup()`.
- Indicador ETH en IDLE.
- Coexistencia con SD, FRAM y TFT.

### JWPLC Basic Core

- Ethernet aparece como disabled.
- No bloquea.
- Display sigue operativo.

---

## Validación alpha31

Para alpha31, `JWPLC_Ethernet` se valida como parte del package completo instalado desde cero.

Pruebas recomendadas:

- arranque con RJ45 conectado;
- arranque sin RJ45 conectado;
- conexión posterior de RJ45;
- desconexión y reconexión sin falso error permanente;
- DHCP automático;
- IP estática configurada desde `setup()`;
- indicador `ETH` en pantalla IDLE;
- coexistencia SPI con TFT, microSD y FRAM;
- comportamiento `Ethernet disabled` esperado en `JWPLC Basic Core`.

No se agregan cambios funcionales de Ethernet en alpha31.

---

## Estado

Documentación revisada para:

```text
JWPLC ESP32 2.0.0-alpha.31
JWPLC_Ethernet 1.0.0
```

Alpha31 valida la integración Ethernet actual dentro del package completo. No cambia la arquitectura de inicialización automática.
