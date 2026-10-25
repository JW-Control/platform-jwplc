# JWPLC Basic - mBlock - Gate 8 (core publicado 2.1.0-alpha.14 e integración HMI Designer)

Fecha: 2026-10-24
Rama: `experiment/mblock-jwplc`

## Estado

~~~text
GATE8A_PUBLISHED_A14_CORE=PASS        [ejecutado] jwplc:esp32 2.1.0-alpha.14 instalada por el propietario
GATE8B_BRIDGE_ON_PUBLISHED_CORE=PASS  [ejecutado] fase 1 y programa A de la fase 4 compilan
GATE8C_HMI_DESIGNER_INTEGRATION=PASS  [ejecutado] pantalla del Designer usada desde bloques, compila
GATE8D_HMI_UPLOAD_AND_PHYSICAL=PASS   [verificado por el usuario] pantalla «Demo» en la JWPLC de COM4
GATE8E_HMI_PICK_BUTTON=PASS           [verificado por el usuario] botón «Elegir pantalla…», bridge 0.5.0
GATE8F_IDLE_USER_TRANSITIONS=PASS     [ejecutado] compila; prueba en placa pendiente
~~~

## Gate 8A/8B - Core publicado

La alpha.14 está publicada: tag `v2.1.0-alpha.14` (`f3f47aee`, PR #157), ZIP del
release y entrada en `JWPLC/package_jwplc_index_dev.json` (no en el índice
principal `package_jwplc_index.json`, que es el que tenía configurado el Arduino
CLI de la PC; Arduino IDE 2 sí usa el índice dev). El propietario la instaló
(`jwplc:esp32 2.1.0-alpha.14`, reemplaza la alpha.13): `JWPLC_Display` 1.1.0,
`JWPLC_GlobalPeripherals` 1.1.0 con `JWPLC_Buzzer`.

El bridge vuelve al core publicado con `install-package-from-ref.cjs --reset`,
que ahora escribe `bridge.json` con `fqbn=jwplc:esp32:jwplcbasic` y un sketchbook
vacío propio (`%USERPROFILE%\.jwplc\mblock-bridge\sketchbook-empty`), para que
ninguna librería del sketchbook del usuario tape las del package.

~~~text
programa                 copia de la rama (jwplc_a14)   core publicado (jwplc)
fase 1                   418 244 B                      450 020 B   (98 s en frío)
fase 4 A                 438 217 B                      468 725 B   (66 s)
~~~

Causa de la diferencia [ejecutado]: los `.a` de Display/TFT y el `core.a`
precompilado son idénticos entre la rama (`eddee62e`) y el tag; el resto del
diff son ejemplos y finales de línea. Lo que cambia es la toolchain: una
plataforma en el sketchbook (`hardware/jwplc_a14`) no tiene herramientas propias
instaladas y arduino-cli resolvió `esp32-libs` con la de `esp32:esp32`
(**3.3.11**), mientras que el core publicado usa la que declara el package,
`jwplc/esp32-libs/`**3.3.8** (`compile_commands.json` del build).

Consecuencia: los Gates 4 a 7 se compilaron contra las librerías IDF 3.3.11. Las
pruebas en placa de las fases 1, 2 y 3 (Gates 4E, 5E, 6F) pasaron con esa base;
desde aquí todo compila con la toolchain oficial del package. Si se vuelve a
usar `install-package-from-ref.cjs` con un ref no publicado, tener en cuenta
este efecto.

## Gate 8C - Pantallas del HMI Designer desde los bloques

Flujo (igual que el modo Arduino del Designer, donde el usuario nombra las variables):

1. El niño diseña la pantalla en el HMI Designer y la escribe con «Escribir en
   sketch» en `%USERPROFILE%\JWPLC-HMI\<Nombre>\` (una carpeta por diseño;
   `bridge.json` puede cambiar la raíz con `hmiRoot`).
2. En mBlock, categoría **Pantalla HMI**:

| Bloque | Código |
|---|---|
| usar la pantalla del HMI Designer [Nombre] | `JWPLC_HMI.begin("Nombre");` -> `jwplcHMISetup()` |
| poner variable de pantalla [variable] a [valor] | `JWPLC_HmiSet(variable, valor);` |
| variable de pantalla [variable] | `JWPLC_HmiGet(variable)` (número) |
| ¿variable de pantalla [variable] activa? | `(JWPLC_HmiGet(variable) != 0)` |

3. Al compilar, el bridge 0.4.0 detecta `#include "JWPLC_mBlock_HMI.h"`, toma el
   nombre de `JWPLC_HMI.begin("…")` y copia `<raíz>/<Nombre>/JWPLC_HMI_Generated.h`
   junto al sketch. Si no existe, el log de mBlock lo dice y lista las pantallas
   disponibles [ejecutado]:

~~~text
ERROR: no existe C:\Users\jeykc\JWPLC-HMI\NoExiste\JWPLC_HMI_Generated.h. Escribe la pantalla
desde el HMI Designer en esa carpeta (pantallas disponibles: Demo)
~~~

Detalles:

- El nombre de la variable se escribe en un hueco de texto; mBlock lo entrega
  entre comillas y la expresión del generador
  `/*{{String(this.VAR).replace(/[^A-Za-z0-9_]/g, '')}}*/` lo deja como
  identificador C++, así la vista previa muestra `JWPLC_HmiSet(temperatura, …)`.
- `JWPLC_HmiSet` (`JWPLC_mBlock_HMI.h`, `if constexpr`) asigna según el tipo que
  declaró el Designer: `char[N]` (texto, con `strncpy`), `bool` o número; el valor
  puede ser número, verdadero/falso o texto (un número escrito llega como "25").
- Las variables las declara `JWPLC_HMI_Generated.h`: en mBlock no hay que crear
  variables con el mismo nombre (sería una redefinición). No mezclar con los
  bloques «escribir en la línea» (`JWPLC_Screen`): ambos registran los campos.
- El MENU del Designer edita sus variables (`EDIT_NUM`, `EDIT_BOOL`); los bloques
  las leen con «variable de pantalla». El aviso `menuChanged()` queda para después.

### Prueba [ejecutado]

Diseño `test/hmi/Demo.json` (forma corta del Designer): página «Planta» (TEXT
estado, VALUE temperatura y piezas, BOOL bomba, BAR nivel) y página «Ajustes»
(MENU con EDIT_NUM consigna y EDIT_BOOL modoAuto), con NAV_BAR. Escrito con la
CLI del Designer de la alpha.14 publicada (tag `v2.1.0-alpha.14`):

~~~text
jwhmi validate Demo.json                       OK: 0 error(es), 0 aviso(s)
jwhmi build Demo.json --sketch %USERPROFILE%\JWPLC-HMI\Demo --create-ino --preview
~~~

La CLI necesita Playwright: se usó el de la caché de npx con `NODE_PATH`, sin
instalar nada global. Vistas previas: `results/20261024_gate8/pagina_0_Planta.png`
y `pagina_1_Ajustes.png`.

Programa de bloques `test/cdp-program-hmi.js`: usa la pantalla «Demo»; cada 1 s
pone temperatura (RTC), piezas (cronómetro), nivel, bomba (I0.0) y, si modoAuto y
temperatura > consigna (ambas del MENU), enciende Q0.0 y pone el estado
«CALIENTE». Código generado: `results/20261024_gate8/GATE8C_HMI_GENERATED.ino`.

~~~text
compilación desde mBlock, core jwplc:esp32 2.1.0-alpha.14   PASS  455 621 B flash, 38 348 B RAM
~~~

Log: `results/20261024_gate8/GATE8_COMPILE.log` (incluye el primer intento, que
falló porque el script de prueba no enviaba `JWPLC_mBlock_HMI.h`; el driver real
envía todos los headers de la extensión).

## Gate 8D - Subida y prueba física

[ejecutado] 2026-10-25: a pedido del propietario el programa se subió a la otra
JWPLC Basic, en **COM4** (CH340), eligiendo COM4 en el desplegable del diálogo de
conexión de mBlock. Compilación 16 s, upload 35 s en total, 455 776 B con hash
verificado. Log: `results/20261024_gate8/GATE8D_UPLOAD_COM4.log`.

[verificado por el usuario] Todo PASS:

~~~text
PAGINA_PLANTA   título, Temp (RTC), Piezas cada segundo, barra Tanque 0..100, Bomba = I0.0   PASS
NAVEGACION      ◀ ▶ entre Planta y Ajustes, barra inferior PLNT / AJST                        PASS
MENU_AJUSTES    Consigna y Modo auto editables con ▲▼ OK                                     PASS
LOGICA_MENU     Modo auto + temperatura > consigna -> Q0.0 y «CALIENTE»; si no, «LISTO»       PASS
~~~

Nota de GUI: tras conectar, el diálogo de conexión de mBlock quedó abierto y no se
cerró por DOM; con una sola conexión activa (COM4) la subida funcionó sin
reintentos.

## Gate 8E - Elegir la pantalla con «Explorar» (bridge 0.5.0, extensión 0.8.0)

Pedido del propietario: no obligar a guardar el diseño en `JWPLC-HMI`; elegirlo
con un diálogo de archivos.

- Paleta **Pantalla HMI**: botón «Elegir pantalla del HMI Designer…»
  (`blockType: 'button'` de mBlock; la función está en `runtime.js`,
  `JWPLC_BUTTONS`). Abre `MeApi.fe.openFileDialog` (filtro `.jwhmi`, `.ino`,
  `.h`) y pone la carpeta del archivo, con `/`, en el bloque «usar la pantalla…»
  seleccionado, o en el primero del programa, o crea uno nuevo a la vista.
- Bridge 0.5.0: `JWPLC_HMI.begin("…")` acepta un nombre (se busca en cada raíz:
  `%USERPROFILE%\JWPLC-HMI` y la carpeta de sketches de Arduino, o
  `hmiRoot`/`hmiRoots` de `bridge.json`) o una ruta absoluta (carpeta, o el
  `.jwhmi`/`.ino`/`.h` del diseño). `/health` informa `hmiRoots`.

[verificado por el usuario] 2026-10-25: elegir el diseño con el botón y subir a
la placa funcionó.

## Gate 8F - Cambio entre la pantalla de entradas/salidas (IDLE) y mi pantalla

Pedido del propietario: revisar las condiciones para ir de la IDLE a la pantalla
de usuario y volver, según la configuración del HMI Designer («Pantalla y
arranque», `display` del `.jwhmi`: `start` IDLE|HMI, `wake` ANY|BUTTON|NONE,
`wakeButton`, `back` ESC|TIMEOUT|NONE, `timeoutS` 1..3600; por defecto IDLE, ANY,
BTN_OK, ESC, 15).

### Hallazgos [ejecutado, lectura del package alpha.14]

1. El package arranca con `wake = IDLE_WAKE_DISABLED` y `back = ESC_ONLY`
   (`JWPLC_Display.cpp`). El Designer siempre escribe su configuración en
   `jwplcHMISetup()`, pero las 4 líneas (`JWPLC_Screen.begin()`) no la fijaban:
   tras pulsar ESC el niño quedaba en la IDLE sin poder volver con la botonera.
2. La pantalla no consume los avisos `pressed()` del sketch al despertar ni al
   volver con ESC (los detecta por flancos físicos), y el selector de páginas y
   el MENU sí los consumen. «cuando se pulse el botón» (`JWPLC_Buttons.pressed`)
   se disparaba con la tecla que abre la pantalla, con ESC, y competía con el
   selector y el MENU por la misma pulsación.
3. «mostrar mi pantalla» añadía `JWPLC_Screen.begin()`: con una pantalla del
   Designer registraba las 4 líneas encima de sus campos.

### Cambios (extensión 0.9.0)

- `JWPLC_Screen.begin()` usa los valores por defecto del Designer: cualquier
  botón abre mi pantalla y ESC vuelve a la de entradas/salidas.
- Bloques nuevos en **Pantalla** (valen para las 4 líneas y para el Designer):

| Bloque | Código | Designer |
|---|---|---|
| abrir mi pantalla con [cualquier botón / OK / ESC / ▲ ▼ ◀ ▶ / ningún botón] | `JWPLC_Screen.openWith(…)` | `wake`, `wakeButton` |
| volver a la pantalla de entradas/salidas con [ESC / ningún botón] | `JWPLC_Screen.closeWith(…)` | `back` ESC / NONE |
| volver a la pantalla de entradas/salidas tras [15] s sin pulsar botones | `JWPLC_Screen.closeAfter(15)` | `back` TIMEOUT, `timeoutS` (1..3600) |
| ¿se ve mi pantalla? | `JWPLC_Screen.isShown()` | |

  `start` sigue con los bloques que ya existían: «mostrar mi pantalla» /
  «mostrar la pantalla de entradas/salidas» en «cuando JWPLC Basic arranca».
  Lo que piden los bloques se recuerda y `JWPLC_HMI.begin()` lo vuelve a aplicar
  después de `jwplcHMISetup()`: manda sobre el diseño aunque el bloque esté
  antes de «usar la pantalla».
- «cuando se pulse el botón» y «¿botón presionado?» usan `JWPLC_Keypad`, que solo
  toma la tecla cuando no es de la pantalla (criterio del ejemplo
  `06.HMI_Botonera_Interactiva`), y no consume las que no le tocan:

~~~text
pantalla de entradas/salidas  abrir con cualquier botón -> ninguna tecla es del programa
                              abrir con un botón       -> todas menos ese botón
                              abrir con ningún botón   -> todas (programas sin pantalla)
mi pantalla                   selector de páginas (NN/TT) o página con MENU -> ninguna
                              varias páginas           -> todas menos ESC (vuelve al selector)
                              una página               -> todas menos ESC si «volver con ESC»
~~~

- La tecla que hace el cambio no llega además a un bloque: `JWPLC_mBlock.h`
  define los hooks weak `jwplcUserDisplayEnterCallback/ExitCallback` de
  `JWPLC_Display` y limpia los avisos pendientes (como hace el selector de
  páginas al entrar). Solo `JWPLC_LogicRuntime_UI` define esos hooks, y los
  programas de mBlock no lo usan.
- Las páginas con MENU se conocen sin tocar el header del Designer:
  `JWPLC_mBlock_HMI.h` lo incluye con una macro `setMenus(lista, n)` que anota la
  lista (`JWPLC_UIMenu::page`) antes de pasarla a `JWPLC_Display.setMenus`.
- «mostrar mi pantalla» ya no registra las 4 líneas.

### Prueba [ejecutado, solo compilación]

Dos programas escritos como los genera mBlock (`test/idle/`), compilados por el
bridge con las librerías de la extensión (`test/idle/compile-via-bridge.cjs`,
core `jwplc:esp32 2.1.0-alpha.14`):

~~~text
idle_lines.ino  4 líneas, abrir con OK, volver tras 20 s, botones ▲ ESC ▼   PASS  440 817 B flash, 37 516 B RAM
idle_hmi.ino    pantalla «Demo» (2 páginas, MENU), abrir con cualquier botón,
                volver con ningún botón (antes de «usar la pantalla»), OK, ESC  PASS  454 909 B flash, 38 356 B RAM
~~~

Logs y programas: `results/20261025_gate8f/`. La extensión 0.9.0 se generó e
instaló en `%USERPROFILE%\mblock\exts\jwplc_basic` (mBlock la carga al reiniciar).

Verificación en placa pendiente (con confirmación del propietario):

~~~text
LINES_WAKE     en la IDLE, ▲ no hace nada; OK abre mi pantalla y no cuenta como ▲/OK del programa
LINES_KEYS     en mi pantalla ▲ suma, ▼ mantiene Q0.0, ESC llega al programa (vuelta por tiempo)
LINES_TIMEOUT  20 s sin tocar botones -> pantalla de entradas/salidas
HMI_SELECTOR   en el selector NN/TT, OK entra a la página y no suma piezas
HMI_MENU       en Ajustes, ▲ ▼ OK editan el MENU y no suman piezas
HMI_CONTENT    en Planta, OK suma piezas; ESC vuelve al selector
~~~


## Pendiente

- Prueba en placa del Gate 8F (lista de arriba).
- Bloque de evento «cuando el operador cambie [variable] en el MENU»
  (`JWPLC_Display.menuChanged`).
- Botón o atajo para abrir el HMI Designer en la carpeta `JWPLC-HMI` desde mBlock.
- Para el package: una consulta pública «¿la página N tiene MENU?» evitaría la
  macro `setMenus` de `JWPLC_mBlock_HMI.h`.
