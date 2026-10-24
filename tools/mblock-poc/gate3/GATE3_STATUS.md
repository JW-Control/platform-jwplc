# JWPLC Basic - mBlock - Gate 2 runtime y Gate 3 (flujo extremo a extremo)

Fecha: 2026-10-24
Rama: `experiment/mblock-jwplc` (antes `experiment/mblock-jwplc-poc`)
Entorno: Windows 10, mBlock 5.4.3 (Electron 21.4.4), Arduino CLI 1.5.1,
core `jwplc:esp32` 2.1.0-alpha.13 instalado, JWPLC Basic por CH340 en COM14.

## Estado

~~~text
GATE2_RUNTIME_CHILD_PROCESS=FAIL                       [ejecutado]
GATE2_RUNTIME_LOCAL_HTTP_FROM_RENDERER=PASS            [ejecutado]  (alternativa probada)

GATE3A_BRIDGE_SERVICE=PASS                             [ejecutado]
GATE3B_EXTENSION_BUILD_WITHOUT_EXTENSION_BUILDER=PASS  [ejecutado]
GATE3C_DEVICE_DISCOVERY_AND_ADD=PASS                   [ejecutado]
GATE3D_CODEGEN_JWPLC_TEMPLATE=PASS                     [ejecutado]
GATE3E_COMPILE_FROM_MBLOCK_RENDERER=PASS               [ejecutado]
GATE3F_BRIDGE_SECURITY=PASS                            [ejecutado]
GATE3G_UPLOAD_FROM_MBLOCK=PENDING                      [no verificado] requiere confirmación para flashear
GATE3H_PHYSICAL_IO=PENDING                             [no verificado] requiere verificación física (LEDs Q0)
~~~

## Gate 2 runtime: `child_process` dentro de mBlock

Resultado: **FAIL**. Un middleware/driver de extensión no puede lanzar procesos.

Evidencia [ejecutado], vía Chrome DevTools Protocol sobre el renderer
(`mBlock.exe --remote-debugging-port=9229`):

~~~text
typeof require             = undefined
typeof process             = undefined
typeof window.MbApi.require = function   (cargador de scripts por URL, NO es require de Node)
window.MeApi               = { fe, sys, ch340, mlink, getMBlockFile, ... }   (contextBridge)
~~~

Causa [visto] en `resources/app.asar`:

- `packages/main/dist/index.cjs`: la ventana principal usa
  `webPreferences:{webgl:!0, webSecurity:!1, nodeIntegration:!0, preload:...}` sin
  `contextIsolation`, que en Electron 21 vale `true` por defecto;
- `packages/preload/dist/index.cjs` publica `MeApi` con
  `contextBridge.exposeInMainWorld`: sólo archivos (`fe`), rutas/sistema (`sys`),
  instalador CH340 y mLink. Ninguna función ejecuta procesos arbitrarios;
- los middlewares se ejecutan en el renderer (`runMiddlewares` en
  `renderer/dist/mblock/js/index.*.js`), firma
  `(app, device, code, params, log, undefined, portAddress, channel)`;
  si lanzan una excepción, mBlock la registra y sigue con el código original.

El probe Gate 2P que dejó la PoC (`jwplc_basic_poc_008` v0.0.11,
`require('child_process').execFileSync(...)`) no podía funcionar por la misma causa.

## Alternativa probada: servicio local por HTTP

[ejecutado] Desde el renderer, `fetch('http://127.0.0.1:47999/...')` con
`Content-Type: application/json` llega a un servidor Node local. Con
`webSecurity=false` no hay CORS y el renderer **no** envía cabecera `Origin`.

Arquitectura adoptada:

~~~text
Bloques mBlock (Device jwplc_basic)
    -> generador Arduino C con plantilla JWPLC
    -> botón Subir de mBlock
        -> middleware jwplc_capture_port  (guarda COMx y el canal mLink)
        -> driver jwplcUploadDriver       (renderer, sin Node)
            -> cierra el COM que mLink tiene abierto
            -> POST http://127.0.0.1:47820/upload  (token)
                -> JWPLC mBlock Bridge (Node, proceso aparte)
                    -> arduino-cli compile --fqbn jwplc:esp32:jwplcbasic
                    -> arduino-cli upload  --port COMx   (esptool del core)
            <- NDJSON: log / etapa / progreso / resultado
        -> barra de progreso y log de mBlock
~~~

No se usa el `esptoolByExec` de mLink: escribe un único binario en un único
offset, sin `--chip` ni parámetros de flash, y rechaza ante cualquier salida por
stderr. `arduino-cli upload` usa la receta validada del package.

## Gate 3A - Bridge

`tools/mblock-poc/bridge/jwplc-mblock-bridge.cjs` (Node >= 18, sin dependencias).

- `GET /health`, `POST /compile`, `POST /upload` (respuesta NDJSON en streaming).
- Un trabajo a la vez; build path persistente en `%USERPROFILE%\.jwplc\mblock-bridge\build`.
- Arduino CLI: `JWPLC_ARDUINO_CLI` o `C:\Program Files\Arduino CLI\arduino-cli.exe`
  o el CLI incluido en Arduino IDE 2. Ya no se usa el CLI 1.0.2 de Arduino PLC IDE Tools.

Hallazgo [ejecutado]: si el bridge lo lanza una app empaquetada MSIX (como la
app de escritorio desde la que se automatizó esta prueba), Windows virtualiza
las escrituras nuevas en `%APPDATA%`/`%LOCALAPPDATA%` y mBlock no ve el token.
Por eso token y carpeta de trabajo viven en `%USERPROFILE%\.jwplc\mblock-bridge`.

## Gate 3B - Extensión sin Extension Builder

`tools/mblock-poc/extension/build-extension.cjs` genera `dist/jwplc_basic/` y
`dist/jwplc_basic_v0.1.0.mext` (zip con rutas `/`). `install-extension.cjs`
copia la carpeta a `%USERPROFILE%\mblock\exts\jwplc_basic` y registra el Device
en `exts\device.json` (lista local que mBlock lee al arrancar), con backup.

El `index.js` sigue el formato UMD del Extension Builder (`getInfo()` /
`getHandler()`), sin `feature: ["worker"]`: los handlers quedan vacíos y no
hace falta `handlerProxy.js`. Ventaja: la extensión es código versionado y
reproducible; ya no depende de la cuenta ni del editor web.

## Gate 3C - Discovery y alta del Device

[ejecutado] Device Library -> buscar "JWPLC Basic" -> OK. El Device aparece en
la lista, la paleta muestra la categoría "JWPLC I/O" y la pestaña de código es
Arduino C. No apareció el aviso `File error. Install Failed` de la PoC.

## Gate 3D - Plantilla Arduino y bloques

[visto] `cores/jwcontrol/main.cpp`: `jwplcSystemTask` (scan de E/S cada 20 ms,
RTC, Ethernet, display; prioridad 2 > loopTask) espera `initSemaphore`, que se
libera cuando `setup()` retorna. La plantilla estándar de mBlock genera el
`por siempre` como `while(1)` dentro de `setup()`: con el core JWPLC dejaría
el runtime sin arrancar (entradas congeladas, display sin refresco).

Plantilla JWPLC:

- bloque "cuando JWPLC Basic arranca" -> `setup()` (una vez);
- cuerpo de "por siempre" -> `loop()` (ciclo de scan), sin `while(1)` ni `_loop()`;
- "esperar" -> `delay(...)`, que cede la CPU (el runtime tiene más prioridad);
- sin `pinMode()`: el core configura internamente I0_x/Q0_x.

Bloques (categoría JWPLC I/O): poner salida Q0.x encendida/apagada, conmutar
salida, ¿entrada I0.x activa?, ¿salida Q0.x activa?, escribir salidas Q0 con
byte, apagar todas las salidas, entradas I0 (byte), salidas Q0 (byte),
milisegundos desde el arranque, imprimir por serie. Todas las llamadas existen
en el core (`jwplc_peripherals.h`, redirecciones de `Arduino.h`).

Código generado [ejecutado] para el programa de demostración
(`test/cdp-program-demo.js`):

~~~cpp
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  JWPLC_writeOutputs(0);
}

void loop() {
  if((digitalRead(I0_0) == HIGH)){
    digitalWrite(Q0_0, HIGH);
  }else{
    digitalWrite(Q0_0, LOW);
  }
  digitalWrite(Q0_7, !digitalRead(Q0_7));
  for(int count2=0;count2<3;count2++){
    Serial.println(7);
  }
  delay((unsigned long)((0.5) * 1000));
}
~~~

Limitación conocida: un bloque que espere una entrada ("esperar hasta") colgado
directamente de "cuando JWPLC Basic arranca" se ejecuta en `setup()`, antes de
que arranque el scan de E/S; debe ir dentro de "por siempre".

## Gate 3E - Compilación desde mBlock

[ejecutado] Desde el renderer (token leído con `MeApi.fe`, `fetch` al bridge):

~~~text
FQBN=jwplc:esp32:jwplcbasic
RESULT=ok exit=0
COLD_BUILD_MS=71214
WARM_BUILD_MS=13837
SKETCH=429501 bytes (10%)  RAM=29532 bytes (9%)
~~~

## Gate 3F - Seguridad del bridge

[ejecutado]

~~~text
sin token                         -> 403 invalid token
Origin: https://evil.example      -> 403 origin not allowed
port "/dev/x; rm"                 -> 400 invalid port
fqbn arduino:avr:uno              -> 400 fqbn not allowed
~~~

Escucha sólo en 127.0.0.1; `spawn` sin shell; el token se regenera en cada arranque.

## Pendiente

- Gate 3G: conectar la JWPLC Basic en mBlock (COM14) y pulsar Subir. Comprueba
  el cierre del COM por el canal mLink y el upload por `arduino-cli`.
- Gate 3H: verificar en la placa que Q0.0 sigue a I0.0 y que Q0.7 parpadea a 1 Hz.

## Herramientas de prueba

`tools/mblock-poc/test/`: cliente CDP mínimo (`cdp.cjs`, `click.cjs`,
`type.cjs`, `shot.cjs`) y scripts para inspeccionar mBlock 5.4.3 sin tocar su
instalación (`asar-read.cjs` lee `app.asar`). Requieren lanzar mBlock con
`--remote-debugging-port=9229`.
