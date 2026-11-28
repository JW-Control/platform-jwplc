# JWPLC_WiFi

WiFi del **JWPLC Basic** con la misma forma de uso que `JWPLC_Ethernet`. Versión **0.1.0** (línea
`v2.1.0-alpha.19/JWPLC-Sim`).

```cpp
#include <JWPLC_WiFi.h>

void setup()
{
    JWPLC_WiFi.connect("MiRed", "MiClave");
}

void loop()
{
    if (JWPLC_WiFi.isReady())
    {
        // ya hay IP
    }
}
```

- `connect()` **no bloquea**: la conexión, la IP y las reconexiones corren en una tarea propia (núcleo 0).
  El `loop()` del sketch nunca espera a la red.
- Si la red se cae o el router se reinicia, vuelve a conectar solo (cada 15 s; cada 30 s si la clave es
  incorrecta o la red no aparece).
- `statusString()` dice por qué no conecta.

| Llamada | Para qué |
|---|---|
| `connect(red, clave)` | conectar (DHCP por defecto); `false` si el nombre está vacío o pasa de 32 caracteres, o la clave pasa de 63 |
| `disconnect()` | apagar la conexión y dejar de reintentar |
| `useDHCP()` / `setStaticIP(ip, puerta, máscara, dns)` | dirección automática o fija; llamar antes de `connect()` |
| `setHostname(nombre)` | nombre en la red; por defecto `jwplc-` + 4 hex de la MAC |
| `isReady()` | conectado y con IP |
| `state()` | `WIFI_STATE_OFF`, `_CONNECTING`, `_READY`, `_WRONG_PASSWORD`, `_NO_NETWORK`, `_LOST` |
| `statusString()` | `APAGADO`, `CONECTANDO`, `LISTO`, `CLAVE INCORRECTA`, `RED NO ENCONTRADA`, `RECONECTANDO` |
| `localIP()`, `gatewayIP()`, `subnetMask()`, `dnsServerIP()`, `macAddress()` | datos de la red |
| `rssi()` / `signalPercent()` | señal en dBm y en % aproximado (-90 dBm = 0 %, -30 dBm = 100 %) |
| `reconnects()` | veces que volvió a conectar desde el primer `connect()` |
| `printStatus(Serial)` | resumen en una línea |
| `startAccessPoint(red, clave)` / `stopAccessPoint()` | red propia del PLC (IP 192.168.4.1); puede estar junto con `connect()`. Clave vacía = abierta; si no, 8 a 63 caracteres |
| `accessPointReady()`, `accessPointNetwork()`, `accessPointIP()`, `accessPointClients()` | estado del punto de acceso y celulares conectados |
| `qrText()` | texto del QR «unirse a la red» del punto de acceso, igual al preset WIFI del HMI Designer |

Para que el mismo sketch salga por cable o por WiFi, usar junto con `JWPLC_Red`.

**HMI Designer.** El widget `QR` con preset WIFI (y ejemplos como `pruebas-ia/06_sensor_cod_subvistas`,
«portal WiFi» en 192.168.4.1) suponen que el PLC abre su propia red. `startAccessPoint()` la abre y
`qrText()` da el texto con el mismo formato y escape (`\` `;` `,` `:` `"`) que `formatWifiText()` del
Designer. Para un QR `VARIABLE`: `JWPLC_WiFi.qrText().toCharArray(qrTexto, sizeof(qrTexto));`.

**JWPLC Sim:** todavía no simula la radio WiFi (fase «WiFi virtual» del plan de la línea); un sketch con
esta librería compila solo para la placa.

Ejemplos: `01.WiFi_Conectar` y `02.WiFi_Punto_Acceso_QR`. Se distribuye en código fuente (sin `.a`).
