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
GATE6F_UPLOAD_AND_PHYSICAL=PASS       [ejecutado + verificado por el usuario] conexión, IP,
                                      envío, recepción y filtro de canal con la PC en la misma LAN
GATE6G_UPLOAD_DIALOG_WITHOUT_FALSE_FAIL=PASS [ejecutado] sin «Fallo al subir» durante la subida
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

## Gate 6F - Prueba en la placa

[ejecutado] El propietario escribió su red y clave en el bloque «conectar a
WiFi» (no se registran en el repo) y el programa se subió desde mBlock a COM14
(1 049 168 B, hash verificado). Logs: `results/20261024_gate6/GATE6F_UPLOADS.log`.

[verificado por el usuario] La placa se conecta y muestra «IP 192.168.1.16» en
la línea 1 y «Enviado N» subiendo cada 2 s en la línea 2.

Primer intento con la red de un repetidor: la PC está en 192.168.0.4/24
(Ethernet) y la placa quedó en 192.168.1.x. [ejecutado] La PC no recibió los broadcast de la placa (escucha
UDP 47821 durante 10 s) y los enviados por la PC (broadcast 192.168.0.255 y
unicast 192.168.1.16) no llegaron; ping a 192.168.1.16 sin respuesta: no hay
ruta entre las dos subredes. Para cerrar el gate hace falta la PC en la misma
WiFi, una segunda JWPLC o un móvil con una app UDP en 192.168.1.x.

Segundo intento: el propietario indicó que su red principal (192.168.0.x) es
otra; se cambió sólo el nombre de red del bloque (la clave no se tocó ni se
registra) y se volvió a subir desde mBlock (37 s, un único aviso «El código se
ha subido»). La placa tomó la IP 192.168.0.3.

~~~text
PC escucha UDP 47821 (10 s)          5 paquetes "JWPLC|1|contador|10..14" desde 192.168.0.3    PASS [ejecutado]
PC envía "JWPLC|1|contador|42"       línea 3 "Recibido 42"                                     PASS [usuario]
PC envía "JWPLC|1|boton|"            un pitido y "Llego boton" en la línea 4                   PASS [usuario]
PC envía "JWPLC|2|boton|" (canal 2)  ignorado: no suena                                        PASS [usuario]
~~~

Herramientas usadas (scratchpad, no versionadas): un `dgram` de Node que escucha
el puerto 47821 y otro que envía por broadcast a 192.168.0.255.

Barra de progreso: esptool v5 actualiza su progreso con `\r` y sólo pone `\n`
al terminar cada archivo, así que el bridge recibía todo el progreso de la app
al final (la barra quedaba en 50 % y saltaba a 100 %). El bridge corta ahora las
líneas también en `\r`. [no verificado] en una subida real: se verá en la próxima.

### Avisos falsos de «Fallo al subir» (corregido)

El propietario vio dos avisos «Fallo al subir el código» aunque la subida
terminaba bien. Causas [visto] en `renderer/dist/mblock/js/index.*.js`:

1. El cuadro de subida arma un temporizador de 5 s con cada progreso; si no
   llega otro, muestra «Fallo al subir» y pasa a «Intentar otra vez». El driver
   sólo informaba progreso con cada línea del compilador, y la compilación
   pasa decenas de segundos en silencio.
2. `ConnectService.disconnect()` emite «disconnect», que mBlock también muestra
   como fallo.

Correcciones:

- Latido en el driver: reenvía el progreso cada 2 s y avanza despacio mientras
  compila (hasta 48 %).
- El COM se cierra con `setIgnoreError('close', true)` sobre el puerto de
  `ConnectService`, como el `close(cb, true)` del driver `esptoolByExec` de
  mBlock, sin emitir «disconnect».
- El bridge sólo informa el progreso de la app (0x10000): antes bootloader,
  particiones y boot_app0 llegaban cada uno a 100 % y la barra saltaba al final
  antes de tiempo.

[ejecutado] Subida instrumentada (hora de cada progreso y de cada aviso):
progreso 2 -> 10 % durante la compilación, 50 % al empezar el upload, éxito a
los 37 s y un único aviso: «El código se ha subido.» Tras la subida la interfaz
sigue diciendo «conectado» aunque el COM está cerrado (igual que el driver
oficial): para volver a usarlo, Desconectar y Conectar.

Nota de pruebas: en los scripts de prueba los textos fijos se metían como bloque
«texto» suelto encima del hueco y mBlock los pinta en blanco (el código sí los
lleva). Corregido en `test/cdp-program-phase2.js` y `phase3.js`: el texto va en
el propio hueco, como al usar la paleta.

### Checklist para otra prueba con dos placas

Requiere dos JWPLC Basic (o una JWPLC y un emisor UDP) en la misma red WiFi:

1. PASS: la línea 1 muestra «IP x.x.x.x» tras conectar.
2. PASS: cada 2 s la línea 2 muestra «Enviado N»; en la otra placa, la línea 3 «Recibido N» (con la PC: PASS).
3. OK en una placa: la otra hace un pitido y muestra «Llego boton».
4. Con canales distintos (`poner canal de red`) las placas no se oyen.
5. Revisar convivencia con Ethernet activa y la carga del bus mientras llegan mensajes.

Prueba con una sola placa: desde la PC, en la misma red, enviar por UDP
`JWPLC|1|contador|42` a la dirección de broadcast, puerto 47821.

## Siguiente

- Fase 4 (avanzado): RS485, Ethernet y Modbus.
- Integración con el HMI Designer (pantalla diseñada, campos por nombre).
