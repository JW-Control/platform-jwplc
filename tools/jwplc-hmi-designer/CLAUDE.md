# CLAUDE.md — tools/jwplc-hmi-designer

Contexto local del HMI Designer. Lee primero `/CLAUDE.md` (reglas globales) y
`docs/v2.1.0-alpha.15/ALPHA15_STATUS.md` (estado vivo).

## Qué es

Editor visual (HTML/JS sin framework ni bundler, empaquetado con Electron) que
diseña páginas para la TFT ST7789 320x170 del JWPLC Basic y genera
`JWPLC_HMI_Generated.h` contra `JWPLC_Display` / `JWPLC_UI` / `JWPLC_TFT`.

Entrada real de la app: `poc/desktop.html` (Electron carga
`desktop.html?app=...`, ver `electron/main.js`). `desktop.html` hace `fetch`
de `index.html` e inyecta los scripts en orden.

## Orden de carga de módulos (importa)

```text
index.html:      gfx-classic-font -> designer-fonts-data -> designer-fonts -> app -> ux-foundation -> designer-codegen
ux-foundation:   designer-bool -> designer-bar -> designer-field-visibility -> designer-pages -> designer-live
field-visibility: designer-inspector-state, designer-pixelmap-compat -> designer-pixelmap -> designer-pixelmap-stability
desktop.html:    designer-project, designer-native-project, designer-project-integration-fix,
                 designer-pixelmap-workbench/-optimizer/-codegen-guard, desktop-responsive
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
```

## Patrón de inspector objetivo (Propuesta 2 de BAR)

Referencia: `JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/` y `poc/designer-bar.js`.

- Pestañas principales: **Diseño / Datos / Técnico**.
- Diseño en sub-pestañas: **Forma / Contenido / Apariencia / Ubicación**.
- Opciones como **tarjetas visuales clicables** (`bar-visual-btn`, `bar-card`),
  no como selects ni celdas numéricas cuando la opción es enumerable.
- Mostrar sólo los controles de la variante elegida.
- **No ofrecer en la UI una opción que el firmware no pueda dibujar.**

Estado de adopción por tipo: ver `ALPHA15_STATUS.md` (§Inspector).

## Reglas del codegen (A15-1..A15-3)

- Gráfica estática (figuras, RAW_TEXT, NAV_BAR) -> `jwplcUIPageEnter()`; `jwplcUIUpdate()`
  sólo setters. El runtime llama `jwplcUIPageEnter` con el bus tomado, tras `fillScreen` y
  antes de dibujar los campos.
- Acceso TFT: `JWPLC_TFTClass &tft = JWPLC_Display.tft();` (no existe `getTFT()`).
- Elipse/triángulo: helpers `jwplcHMI*` (STATIC_HELPERS) que usan los métodos nativos de
  `JWPLC_TFT` >= 0.2.0 y un respaldo para 0.1.0 (`JWPLC_HMI_TFT_NATIVE_SHAPES`).
- Fuentes: `FIRMWARE_SUPPORTS_FONTS=false` en `designer-fonts.js` hasta que Alpha13
  regenere `libJWPLC_TFT.a` con `setFont/drawString`.
- BAR circular/degradado: deshabilitados (`.insp-needs-fw`); el codegen cae a `JWPLC_UIBarField`.
- Ids de página siempre contiguos `0..N-1` (`deletePage` renumera; abrir normaliza).
- NAV_BAR existe sólo si hay un objeto NAV_BAR visible.
- Capas: `layerOrder` menor = más al frente; la estática se emite en orden descendente.
- Tras tocar el codegen: `node tools/jwplc-hmi-designer/tests/codegen-smoke.cjs`.
- Tras tocar JS/CSS: subir `?v=` (hoy 1792090000) en `index.html`/`desktop.html`/`ux-foundation.js`/
  `designer-field-visibility.js` y `CACHE_NAME` en `service-worker.js`.

## Probar sin hardware

```bash
for f in poc/*.js; do node --check "$f" || echo "FAIL $f"; done
node tests/codegen-smoke.cjs            # genera headers y los compila contra el package
cd poc && python3 -m http.server 8765 --bind 127.0.0.1   # abrir /desktop.html
```

En Playwright: `window.JWPLCHMIProject.serialize()/apply()` para cargar un
proyecto; para leer el header, clic en `#contractTab` y leer `#codeOutput`
sin selección activa.
