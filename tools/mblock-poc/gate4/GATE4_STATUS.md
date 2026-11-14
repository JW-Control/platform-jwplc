# JWPLC Basic - mBlock - Gate 4 (fase 1 de bloques para niños)

Fecha: 2026-10-24
Rama: `experiment/mblock-jwplc`
Package: 2.1.0-alpha.14 (`origin/v2.1.0-alpha.14/HmiDesigner/main` @ `eddee62e`), aún no publicado en el índice.
Extensión: `jwplc_basic` 0.3.0. Bridge: 0.3.0.

## Estado

~~~text
GATE4A_PACKAGE_A14_FOR_BRIDGE=PASS        [ejecutado]
GATE4B_CATEGORIES_IN_PALETTE=PASS         [ejecutado]
GATE4C_CODEGEN_PHASE1=PASS                [ejecutado]
GATE4D_COMPILE_A14_FROM_MBLOCK=PASS       [ejecutado]
GATE4F_CLEAN_PREVIEW_LIBRARY=PASS         [ejecutado] JWPLC_mBlock.h viaja con la extensión
GATE4E_UPLOAD_AND_PHYSICAL=PASS           [verificado por el usuario] pantalla, botonera y sonido en la placa
~~~

## Decisión de diseño

Público: niños. Nada de lógica PLC (TON/TOF/Ladder). El modelo son los
Devices oficiales de mBlock (CyberPi, mCore): bloques en lenguaje cotidiano y
la parte industrial por debajo, en el código generado. Propuesta aprobada por
el propietario (2026-10-24):

~~~text
Eventos | Entradas y salidas | Pantalla | Botonera | Sonido | Tiempo | Memoria | microSD | WiFi | Avanzado (RS485, Ethernet, Modbus)
~~~

Hardware confirmado por el propietario: buzzer interno en GPIO26; sin
entradas analógicas (placa totalmente digital); sin LEDs visibles (los
indicadores están en la pantalla IDLE de la TFT).

Fase 1 (este gate): Eventos, Pantalla, Botonera, Sonido.

## Gate 4A - Package alpha.14 para el bridge

`JWPLC_Buzzer` sólo existe desde `JWPLC_GlobalPeripherals` 1.1.0 (alpha.14); la
alpha.13 instalada no lo tiene. Como la alpha.14 no está en el índice,
`bridge/install-package-from-ref.cjs` copia `JWPLC/2.1.0` de un ref git a
`%USERPROFILE%\.jwplc\mblock-bridge\sketchbook\hardware\jwplc_a14\esp32` y escribe
`bridge.json` (`fqbn=jwplc_a14:esp32:jwplcbasic`). El bridge pasa ese sketchbook a
arduino-cli con `ARDUINO_DIRECTORIES_USER`: no toca el Arduino IDE del usuario
y las toolchains siguen saliendo de Arduino15 (mismo método `hardware/jwplc_a14`
del traspaso alpha.14). Con `--reset` se vuelve al core publicado `jwplc:esp32`.

La extensión ya no envía FQBN: la decide el bridge (`/health` informa
`fqbn`, `packageRef` y `packageSha`).

Verificación [ejecutado]: `07.Buzzer_Basic` del package compila con
`jwplc_a14:esp32:jwplcbasic` (415 640 B).

## Gate 4B - Paleta

[ejecutado] Categorías visibles con mBlock en español:

~~~text
Eventos | Entradas y salidas | Pantalla | Botonera | Sonido | Control | Operadores | Variables | Mis bloques
~~~

Hallazgo: mBlock sólo muestra las categorías que declara el Device. Las
versiones 0.1.x no declaraban `operators`, `data` ni `myBlocks`, así que la
paleta no tenía Operadores, Variables ni Mis bloques. Declararlas vacías (o
sólo con sobrescrituras) conserva todos los bloques estándar (comprobado en el
XML de la paleta: 18 operadores y 25 bloques de variables/listas).

## Gate 4C - Bloques y código generado

Definiciones en `extension/src/blocks.cjs`; el generador las agrupa por categoría.

| Categoría | Bloque | Código |
|---|---|---|
| Eventos | cuando JWPLC Basic arranca | cuerpo en `setup()` |
| Eventos | cuando se pulse el botón [B] | `if (JWPLC_Buttons.pressed(B)) {...}` en `loop()`; `clearPendingInput()` en `setup()` |
| Eventos | cuando la entrada [I] se active | flanco de subida con `static bool` en `loop()` |
| Eventos | cada [N] segundos | `millis()` con `static unsigned long` en `loop()`, sin `delay` |
| Pantalla | escribir [x] en la línea [1-4] | `JWPLC_Screen.print(n, x)` -> `JWPLC_Display.setText()` |
| Pantalla | limpiar pantalla / mostrar mi pantalla / mostrar la pantalla de entradas/salidas | `JWPLC_Screen.clear()` / `JWPLC_Display.enterUserUI()` / `goIdle()` |
| Botonera | ¿botón [B] presionado? | `JWPLC_Buttons.isDown(B)` |
| Sonido | tocar nota [Do4..Do6] durante [s] / sonar a [Hz] durante [s] | `JWPLC_Sound.play(hz, s)`: `JWPLC_Buzzer.tone()` + espera (como mCore) |
| Sonido | pitido / detener sonido / poner volumen al [%] | `JWPLC_Buzzer.beep()` / `stop()` / `JWPLC_Sound.volume(%)` |

Pantalla: `JWPLC_Screen.begin()` registra 4 campos `JWPLC_UITextField` (24
caracteres) con `setFields()` y `USER_REFRESH_ON_DEMAND`; `setText()` sólo
redibuja si el texto cambió, así que escribir en cada vuelta de `loop()` no
carga el SPI. No se expone TFT_eSPI. `print()` tiene sobrecargas para texto,
`String`, `double`/`float` (sin decimales si es entero, 2 si no), enteros y
`bool` (SI/NO).

### Vista previa limpia: librería `JWPLC_mBlock.h`

Pedido del propietario: la vista previa Arduino C no debe mostrar funciones
propias. Así lo hacen los Devices oficiales: HaloCode genera MicroPython que
llama a su firmware (`halo.led.show_animation(...)`) y mCore llama a objetos
de `MeMCore.h` (`rgbled_7.setColor(...)`); en la vista previa sólo quedan el
`#include` y los auxiliares mínimos de mBlock (`_delay`).

Opción elegida (1 de 2): `extension/src/arduino/JWPLC_mBlock.h`, sólo header,
viaja con la extensión (embebido en `index.js` y copiado en `arduino/` de la
carpeta instalada). El driver lo envía al bridge en `files` y el bridge lo
escribe junto al `.ino` (sólo `.h/.hpp/.c/.cpp` con nombre simple; el sketch
se limpia en cada trabajo). No toca el package ni añade precompilados.

~~~cpp
#include <Arduino.h>
#include "JWPLC_mBlock.h"

void setup() {
  JWPLC_Screen.begin();
  JWPLC_Sound.volume(30);
  JWPLC_Screen.print(1, "Hola JWPLC");
  JWPLC_Sound.play(523, 0.2);
}
~~~

La opción 2 (librería en el package) quedaría para una alpha: sólo header,
fuera del autoload de `Arduino.h` para que no entre en los sketches que no la
incluyen, y medida con `Compare-JWPLCBuildBenchmark.ps1`.

Quedan con `_jw_` sólo las variables locales de los eventos "cada N
segundos" y "cuando la entrada se active" (dentro de `{ }` para no chocar con
variables del niño).

Sobrescritura de operador: mBlock genera `10 / 3` y C++ hace división entera
(3). `operator_divide` ahora emite `((double)(a) / (double)(b))` -> 3,33.

Código generado completo del programa de prueba (todos los bloques):
`results/20261024_gate4/GATE4_PHASE1_GENERATED.ino`.

Detalle cosmético conocido: si un bloque "cuando JWPLC Basic arranca" sólo
contiene "por siempre", mBlock añade `// DO SOMETHING` en `setup()`.

## Gate 4D - Compilación

[ejecutado] Desde el renderer de mBlock (token + `fetch`), FQBN `jwplc_a14:esp32:jwplcbasic`:

~~~text
programa fase 1           418 480 B flash (10 %), 37 756 B RAM (11 %), 97 s en frío
+ unir/división           23 s en caliente
con JWPLC_mBlock.h        418 832 B flash, 37 764 B RAM; 96 s la primera vez, 13 s en caliente
avisos del compilador     0
~~~

El enlace correcto confirma que todas las llamadas existen en los `.a` del package.
Log: `results/20261024_gate4/GATE4_COMPILE_A14.log`.

## Gate 4E - Subida y prueba física

[ejecutado] Programa de prueba (`test/cdp-program-phase1.js`) subido desde el
botón «Subir código» de mBlock (interfaz en español) a COM14: compilación con
`JWPLC_mBlock.h` y la alpha.14, upload en 54 s con hash verificado.
Log: `results/20261024_gate4/GATE4E_UPLOAD_PHASE1.log`.

[verificado por el usuario] Todo como se describe:

~~~text
ARRANQUE      "Hola JWPLC" en la línea 1, nota Do5 a volumen 30 %          PASS
CADA_1S       la línea 2 cuenta cada segundo, Q0.7 conmuta                PASS
LINEA_4       SI/NO según I0.1                                            PASS
BOTON_OK      pitido + "OK pulsado" en la línea 3                         PASS
BOTON_ESC     limpia las líneas y para el sonido (y vuelve a IDLE)        PASS
BOTON_UP      mantener ▲ enciende Q0.0                                    PASS
I0.0          tono agudo corto una vez por activación                     PASS
~~~

Ajuste tras la prueba: el texto se veía pequeño con el estilo por defecto de
`JWPLC_UITextFieldStyle()` (letra 1, 6x8 px). `JWPLC_Screen` usa ahora letra 2
(12x16 px): 24 caracteres por línea (10 + 24 x 12 = 298 px de 320) y 4 líneas
cada 34 px. Subido de nuevo (61 s): «se ve bien así» (propietario).

Arreglo del bridge: esptool v5 manda las líneas "Writing at 0x..." precedidas
por ; el parser de progreso ahora las reconoce, así la barra de mBlock avanza
durante la escritura.

## Pendiente

- Fase 2: Tiempo (cronómetro, reloj RTC, temperatura), Memoria (FRAM) y microSD.
- Integración con el HMI Designer: usar una pantalla diseñada en el Designer
  (`JWPLC_HMI_Generated.h`) y escribir en sus campos por nombre. Requiere que el
  bridge acepte archivos extra junto al sketch.
- Cuando la alpha.14 esté publicada: instalarla y `install-package-from-ref.cjs --reset`.
