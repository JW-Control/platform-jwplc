# JWPLC Basic - mBlock - Gate 8 (core publicado 2.1.0-alpha.14 e integración HMI Designer)

Fecha: 2026-10-24
Rama: `experiment/mblock-jwplc`

## Estado

~~~text
GATE8A_PUBLISHED_A14_CORE=PASS        [ejecutado] jwplc:esp32 2.1.0-alpha.14 instalada por el propietario
GATE8B_BRIDGE_ON_PUBLISHED_CORE=PASS  [ejecutado] fase 1 y programa A de la fase 4 compilan
GATE8C_HMI_DESIGNER_INTEGRATION=PASS  [ejecutado] pantalla del Designer usada desde bloques, compila
GATE8D_HMI_UPLOAD_AND_PHYSICAL=PASS   [verificado por el usuario] pantalla «Demo» en la JWPLC de COM4
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

## Pendiente

- Bloque de evento «cuando el operador cambie [variable] en el MENU»
  (`JWPLC_Display.menuChanged`).
- Botón o atajo para abrir el HMI Designer en la carpeta `JWPLC-HMI` desde mBlock.
