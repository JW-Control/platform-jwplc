# JWPLC Basic - mBlock - Gate 6 (fase 3: WiFi y mensajes entre placas)

Fecha: 2026-10-24
Rama: `experiment/mblock-jwplc`
Package: 2.1.0-alpha.14 (`origin/v2.1.0-alpha.14/HmiDesigner/main` @ `eddee62e`) vía `jwplc_a14:esp32:jwplcbasic`.
Extensión: `jwplc_basic` 0.5.0. Bridge: 0.3.0.

## Estado

~~~text
GATE6A_WIFI_LIBRARY=PASS              [ejecutado] JWPLC_mBlock_WiFi.h (aparte del header común)
GATE6B_PALETTE_WIFI=PASS              [ejecutado] categoría WiFi visible
GATE6C_CODEGEN_PHASE3=PASS            [ejecutado]
GATE6D_COMPILE_A14_FROM_MBLOCK=PASS   [ejecutado]
GATE6E_NO_WIFI_COST_WITHOUT_BLOCKS=PASS [ejecutado] programa sin WiFi sigue en 425 608 B
GATE6F_UPLOAD_AND_PHYSICAL=PENDING    [no verificado] sólo compilación por indicación del propietario;
                                      la prueba entre placas necesita dos JWPLC y una red WiFi
~~~

## Diseño

Modelo: el «LAN broadcast» de CyberPi. Para niños: conectarse a una red y
mandarse mensajes con nombre (y un valor opcional) entre placas del aula.

| Bloque | Código |
|---|---|
| conectar a WiFi [red] contraseña [clave] | `JWPLC_Net.connect(ssid, pass)` (espera hasta 10 s con `delay`) |
| ¿conectado a WiFi? / dirección IP / desconectar WiFi | `connected()` / `ip()` / `disconnect()` |
| enviar mensaje [m] por la red | `JWPLC_Net.send(m)` |
| enviar mensaje [m] con valor [v] por la red | `JWPLC_Net.send(m, v)` |
| cuando reciba el mensaje [m] (evento) | `if (JWPLC_Net.received(m)) {...}` en `loop()` |
| valor recibido del mensaje [m] | `JWPLC_Net.value(m)` (0 si nunca llegó) |
| poner canal de red [n] | `JWPLC_Net.channel(n)` |

Protocolo: UDP broadcast a la dirección de broadcast de la red WiFi
(`WiFi.broadcastIP()`), puerto 47821, texto `JWPLC|<canal>|<mensaje>|<valor>`
(`|` dentro de mensaje o valor se reemplaza por `/`). Sólo se aceptan paquetes
del mismo canal. Se guarda el último valor de hasta 8 nombres de mensaje; si
no hay sitio se reemplaza el más antiguo. `received()` devuelve true una vez por
mensaje recibido (lo consume): dos eventos con el mismo nombre no se disparan
ambos con un único mensaje.

Interfaces: `WiFiUDP` usa lwIP, es decir, la interfaz WiFi. La Ethernet del
JWPLC (`JWPLC_Ethernet`) usa su propia pila W5x00 por SPI, así que no se cruzan.

Header aparte: `JWPLC_mBlock_WiFi.h` sólo lo incluyen los bloques WiFi.
Con el objeto `JWPLC_Net` en el header común, todos los programas habrían
compilado y enlazado la librería WiFi.

## Código generado (programa de prueba `test/cdp-program-phase3.js`)

~~~cpp
#include <Arduino.h>
#include "JWPLC_mBlock_WiFi.h"
#include "JWPLC_mBlock.h"

float cuenta = 0;

void setup() {
  JWPLC_Screen.begin();
  JWPLC_Buttons.clearPendingInput();
  JWPLC_Net.channel(1);
  JWPLC_Screen.print(1, "Conectando...");
  JWPLC_Net.connect("MiRed", "clave");
  JWPLC_Screen.print(1, JWPLC_Join("IP ", JWPLC_Net.ip()));
}

void loop() {
  if (JWPLC_Net.received("contador")) {
    JWPLC_Screen.print(3, JWPLC_Join("Recibido ", JWPLC_Net.value("contador")));
  }
  ...
}
~~~

Completo en `results/20261024_gate6/GATE6_PHASE3_GENERATED.ino`.

## Compilación

[ejecutado] Desde el renderer de mBlock (headers enviados por la ruta del driver):

~~~text
programa WiFi            1 049 012 B flash (25 %), 62 864 B RAM (19 %)
                         163 s la primera vez (librería WiFi), 16 s en caliente
programa fase 2 sin WiFi 425 608 B (sin cambios): la librería WiFi no entra
~~~

Log: `results/20261024_gate6/GATE6_COMPILE_A14.log`.

## Prueba física pendiente (Gate 6F)

Requiere dos JWPLC Basic (o una JWPLC y un emisor UDP en la PC) en la misma red
WiFi, con el SSID/clave reales en el bloque «conectar a WiFi»:

1. La línea 1 muestra «IP x.x.x.x» tras conectar (o 0.0.0.0 si falla).
2. Cada 2 s la línea 2 muestra «Enviado N»; en la otra placa, la línea 3 «Recibido N».
3. OK en una placa: la otra hace un pitido y muestra «Llego boton».
4. Con canales distintos (`poner canal de red`) las placas no se oyen.
5. Revisar convivencia con Ethernet activa y la carga del bus mientras llegan mensajes.

Prueba con una sola placa: desde la PC, en la misma red, enviar por UDP
`JWPLC|1|contador|42` a la dirección de broadcast, puerto 47821.

## Siguiente

- Fase 4 (avanzado): RS485, Ethernet y Modbus.
- Integración con el HMI Designer (pantalla diseñada, campos por nombre).
