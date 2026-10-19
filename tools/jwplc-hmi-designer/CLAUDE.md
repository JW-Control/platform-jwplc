# CLAUDE.md — tools/jwplc-hmi-designer

Contexto local del HMI Designer. Lee primero `/CLAUDE.md` (reglas globales) y
`docs/v2.1.0-alpha.15/ALPHA15_STATUS.md` (estado vivo).

## Qué es

Editor visual (HTML/JS sin framework ni bundler, empaquetado con Electron) que
diseña páginas para la TFT ST7789 320x170 del JWPLC Basic y genera
`JWPLC_HMI_Generated.h` contra `JWPLC_Display` / `JWPLC_UI` / `JWPLC_TFT`.

App de escritorio en Windows: `Generar-e-Instalar-App-Escritorio.cmd` (compila Electron + instala con
`-RequireElectron`). `electron/main.js` y `preload.cjs` van dentro del .exe: un cambio ahí exige volver a
generarlo. `poc/` se sincroniza al abrir (`electron/bootstrap.js`).

Entrada real de la app: `poc/desktop.html` (Electron carga
`desktop.html?app=...`, ver `electron/main.js`). `desktop.html` hace `fetch`
de `index.html` e inyecta los scripts en orden.

## Orden de carga de módulos (importa)

```text
index.html:      designer-cp437 -> gfx-classic-font -> designer-fonts-data -> designer-fonts -> designer-value-format -> designer-codegen-core -> app -> ux-foundation -> designer-codegen
ux-foundation:   designer-color-editor -> designer-bool -> designer-bar -> designer-zone-indicator -> designer-field-inspector -> (designer-menu, designer-idle) -> designer-field-visibility -> designer-pages -> designer-live
field-visibility: designer-inspector-state, designer-pixelmap-compat -> designer-pixelmap -> designer-pixelmap-stability
desktop.html:    designer-project, designer-native-project, designer-project-integration-fix,
                 designer-pixelmap-workbench/-optimizer/-codegen-guard, desktop-responsive,
                 designer-sketch-guide -> designer-sketch-menu
```

Varios módulos se "enganchan" a otros con `setInterval` hasta que existe la
API global y luego **envuelven** funciones (`original = ...`). El header C++
final se construye por capas de `.replace()`/`indexOf()` sobre texto y se lee
del DOM (`#codeOutput`) en `designer-project.js:generatedHeaderText`.
Cualquier cambio de un marcador de texto puede romper otra capa.

## APIs globales

```text
window.JWPLCHMIEditor     app.js            modelo (fields, pages, selección, historial, render)
window.JWPLCHMICodegen    designer-codegen  validateIdentifiers, getPageSymbols, refresh
window.JWPLCHMIBar/Bool   designer-bar/bool inspectores y codegen por tipo
window.JWPLCHMIPages      designer-pages    pestañas, NAV_BAR, layout de navegación
window.JWPLCHMIPixelMaps  designer-pixelmap capas Pixel
window.JWPLCHMIProject    designer-project  serialize/apply/open/save/linkSketch/updateGeneratedHeader
window.JWPLCHMILive       designer-live     Live Preview WebSerial (JWH1/JWH2, 921600)
window.JWPLCHMIIdle       designer-idle     Proyecto · Pantalla y arranque (IDLE) -> patchHeader en jwplcHMISetup
window.JWPLCHMISketchMenu designer-sketch-menu    «Escribir en sketch» (header + .jwhmi) y menú «Sketch: nombre ▾»
window.JWPLCZoneIndicator designer-zone-indicator indicador A1 de Apariencia (contorno + ▶ + regla; campos y MENU)
window.JWPLCColorEditor   designer-color-editor   editor de color fijo de Apariencia (campos, MENU, BAR); sin <input type=color>
window.JWPLCSketchGuide   designer-sketch-guide   JWPLC_HMI_LEEME.md: sketch base leído del header (puro, también en node)
window.JWPLCCp437         designer-cp437          Unicode <-> cp437 de la fuente GLCD (lienzo y literales C++); gemelo de utf8ToGlcd
window.JWPLCValueFormat   designer-value-format   formato de VALUE (número, miles, duración, fecha/hora); gemelo de JWPLC_UI_Format.cpp
```

## Flujo con el sketch (Alpha15)

- «Generar C++» está oculto (`#generateButton` sigue en el DOM: varios módulos cuelgan de su clic, y
  todos también de `#contractTab`). El panel «Código generado» muestra el header.
- «Escribir en sketch» (`#updateHmiButton`) escribe `JWPLC_HMI_Generated.h` y `<sketch>.jwhmi` con una
  sola confirmación, y además `JWPLC_HMI_LEEME.md` (guía con el sketch base; sólo reemplaza una guía que
  empiece con `GUIDE_MARK`). **El `.ino` del usuario nunca se escribe.** «Sketch: nombre ▾» abre el menú: Abrir HMI del sketch · Abrir en Arduino IDE ·
  Abrir carpeta · Cambiar sketch…; al vincular un sketch con `.jwhmi` pregunta si abrirlo.
- `designer-sketch-menu.js` toma esos clics en fase de captura en `document` (otros módulos clonan los
  botones). Escritorio: IPC `read-project`, `open-sketch-ino`, `open-sketch-folder`, `write-guide` en
  `electron/main.js` + `preload.cjs`. Navegador: `linkedFileExists/readLinkedFile/writeLinkedFile` de
  `designer-project.js`. Tests: `tests/sketch-menu-smoke.cjs` (simula `JWPLCHMINative`) y
  `tests/sketch-guide.test.cjs` (compila el sketch base de la guía contra cada golden). Una app generada
  antes de `write-guide` escribe header y `.jwhmi` y avisa que regenere la app para la guía.

## Patrón de inspector objetivo (Propuesta 2 de BAR)

Reglas de UX/UI completas (Apariencia, botonera, capturas obligatorias): skill del repo
`.claude/skills/hmi-designer-ux/SKILL.md` (cualquier agente puede leerlo como guía).

Referencia: `JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/` y `poc/designer-bar.js`.

- Pestañas principales: **Diseño / Datos / Técnico**.
- Diseño en sub-pestañas: **Forma / Contenido / Apariencia / Ubicación**.
- Opciones como **tarjetas visuales clicables** (`bar-visual-btn`, `bar-card`),
  no como selects ni celdas numéricas cuando la opción es enumerable.
- Mostrar sólo los controles de la variante elegida.
- **No ofrecer en la UI una opción que el firmware no pueda dibujar.**

Estado de adopción por tipo: ver `ALPHA15_STATUS.md` (§Inspector).

- TEXT/VALUE/BOOL: `designer-field-inspector.js`. Escribe en los inputs viejos (`#fieldX`,
  `#fieldLayout`…) y dispara `input`/`change`; no tocar el modelo directo (perdería historial).
- BAR: Apariencia con mini-modelo e indicador A1 (`#barAppMini`, `syncBarMini`); los controles de color
  anteriores quedan ocultos en `#barAppLegacyCard`. `applyFirmwareCaps()` en `designer-bar.js` normaliza el modelo a lo que dibuja
  `JWPLC_UIBarField`; al habilitar una variante en firmware, quitarla de ahí y de `closeBarInspector()`.
- Paridad BAR lienzo/TFT y comportamiento del inspector: `tests/inspector-smoke.cjs`.
- MENU: `designer-menu.js` (modelo, pintor, codegen puro `generate()`, inspector propio
  `#menuInspectorSection`). El pintor replica `JWPLC_UI_Menu.cpp` línea por línea: si cambias uno,
  cambia el otro y corre `tests/menu-smoke.cjs` (compara píxel a píxel con el firmware en host).
  `nodes`/`colors` se reemplazan (copy-on-write): el historial copia los campos de forma superficial.

## Reglas del codegen (A15-1..A15-3)

- Gráfica estática (figuras, RAW_TEXT, NAV_BAR) -> `jwplcUIPageEnter()`; `jwplcUIUpdate()`
  sólo setters. El runtime llama `jwplcUIPageEnter` con el bus tomado, tras `fillScreen` y
  antes de dibujar los campos.
- Acceso TFT: `JWPLC_TFTClass &tft = JWPLC_Display.tft();` (no existe `getTFT()`).
- Elipse/triángulo: helpers `jwplcHMI*` (STATIC_HELPERS) que usan los métodos nativos de
  `JWPLC_TFT` >= 0.2.0 y un respaldo para 0.1.0 (`JWPLC_HMI_TFT_NATIVE_SHAPES`).
- Fuentes: GLCD + FreeFonts vía `JWPLC_TFT` 0.3.0 (`setFont/drawString`), siempre dentro de
  `#if JWPLC_HMI_TFT_FONTS` con respaldo GLCD.
- BAR circular/degradado: deshabilitados (`.insp-needs-fw`); el codegen cae a `JWPLC_UIBarField`.
- Ids de página siempre contiguos `0..N-1` (`deletePage` renumera; abrir normaliza).
- NAV con pestañas (todos los estilos salvo «Contador»), pestaña de la página actual en tres estados:
  no posicionada (estilo inactivo) · posicionada (selector, fondo de acento; una página blanca va en
  gris) · seleccionada (OK, fondo blanco + doble marco hacia adentro de 1 + 1 + 1 px, si mide ≥ 9 px).
  `paintNavBar(painter, layout, cfg, state)` marca cada pestaña con `painter.mark`; el codegen separa la
  activa en `jwplcDrawPageNavTab(page, selecting)` y emite `jwplcUIPageModeChanged()`, que el firmware
  llama al pasar del selector al contenido y al volver. El lienzo sigue `JWPLCHMIPages` `navigationMode`
  (tarjeta «Pestaña de la página actual» del inspector).
- NAV_BAR existe sólo si hay un objeto NAV_BAR visible. Estilo `counter`: no se dibuja barra; el
  firmware pinta el NN/TT (`setPageIndicator(true, x, y)`). Barra con pestañas: `setPageIndicator(false)`.
- Márgenes de campo: arriba/izquierda 3 fijo; derecha/abajo `max(3, escala + 1)` (celda 6×8 + marco),
  igual en `JWPLC_UI.cpp`, `app.js` y `designer-bar.js`; `tests/inspector-smoke.cjs` "MARCO" lo vigila.
- Formato de VALUE (Alpha15 I1): `poc/designer-value-format.js` y `JWPLC_Display/src/JWPLC_UI_Format.cpp`
  producen el mismo texto (muestra, desborde, valor); si cambias uno, cambia el otro y corre
  `tests/value-format-parity.cjs`. Duración y contadores con miles declaran `uint32_t`/`int32_t`.
- VALUE de fecha/hora (I2, `valueKind: 'DATETIME'` → `JWPLC_UIDateTimeFormat`): sin variable ni setter
  (`JWPLCHMICodegenCore.isClockField`); el firmware lo llena desde el RTC en `pollClockFields()`.
- BOOL «Piloto» (`boolStyle: 'PILOT'` → `JWPLC_UIBoolPilot`): círculo de 7 × escala con `fillCircle`
  de TFT_eSPI (`app.js:fillBufferCircle`, misma rutina).
- Texto en la TFT (Alpha15 I3): todo literal C++ de texto pasa por `JWPLCCp437.cppEscape` (octal, nunca
  `\xNN`); el lienzo dibuja con `glyphFor(Unicode)`. TFT_eSPI usa `cp437(false)`: glifo > 176 se envía como
  byte − 1 (`°` → 0xF7). Variables TEXT: `2 × capacidad + 1` bytes (UTF-8).
- Capas: `layerOrder` menor = más al frente; la estática se emite en orden descendente. El lienzo
  (`composeFramebuffer`, BAR en `render`) dibuja ordenando por `layerOrder`, nunca por el orden del arreglo
  (los objetos nuevos se agregan al final con el `layerOrder` más bajo).
- Definición C++ de cada campo: `poc/designer-codegen-core.js` (puro, también en node). `app.js`
  emite TEXT/VALUE; BOOL/BAR salen como `JWPLC_UITextField` provisional que reemplazan sus capas.
- Tras tocar el codegen: `node tests/codegen-golden.cjs` (salida byte a byte) y
  `node tests/codegen-smoke.cjs`. Un cambio intencional: `--update` y revisar el diff.
- Tras tocar JS/CSS: subir `?v=` (hoy 1793700000) en `index.html`/`desktop.html`/`ux-foundation.js`/
  `designer-field-visibility.js` y `CACHE_NAME` en `service-worker.js`.

## Probar sin hardware

```bash
for f in poc/*.js; do node --check "$f" || echo "FAIL $f"; done
node tests/codegen-smoke.cjs            # genera headers y los compila contra el package
node tests/value-format-parity.cjs      # formato de VALUE: JWPLC_UI_Format.cpp real vs Designer
node tests/inspector-smoke.cjs          # inspector TEXT/VALUE/BOOL/BAR + paridad BAR
node tests/menu-host-sim.cjs            # MENU: JWPLC_UI_Menu.cpp real en host (g++)
node tests/menu-smoke.cjs               # MENU: Designer + paridad lienzo/firmware + header
node tests/sketch-menu-smoke.cjs        # Escribir en sketch + menú del sketch
node tests/sketch-guide.test.cjs        # guía JWPLC_HMI_LEEME.md + su sketch base compila
cd poc && python3 -m http.server 8765 --bind 127.0.0.1   # abrir /desktop.html
```

En Playwright: `window.JWPLCHMIProject.serialize()/apply()` para cargar un
proyecto; para leer el header, clic en `#contractTab` y leer `#codeOutput`
sin selección activa.
