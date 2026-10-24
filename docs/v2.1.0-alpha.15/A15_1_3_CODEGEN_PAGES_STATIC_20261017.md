# Alpha15 · A15-1 / A15-2 / A15-3 — Codegen compilable, páginas y estática fuera del tick

Fecha: `2026-10-17`

```text
A15_1_CODEGEN_COMPILE=PASS (host g++ + nm; arduino-cli ESP32 pendiente en A15-4)
A15_2_PERSISTENCE_PAGES=PASS
A15_3_STATIC_OUT_OF_TICK=PASS (diseño); tiempos de bus [no verificado] -> A15-9
VERIFICACION=tools/jwplc-hmi-designer/tests/codegen-smoke.cjs (20/20 PASS) [ejecutado]
```

## A15-1 — El header compila contra la API real

| Bloqueante | Cambio | Archivo |
|---|---|---|
| B1 `getTFT()` | Las figuras y RAW_TEXT usan `JWPLC_TFTClass &tft = JWPLC_Display.tft();` | `poc/designer-codegen.js` |
| B2 elipse/triángulo | Helpers `static` en el header (`jwplcHMIFillEllipse`, `jwplcHMIDrawEllipse`, `jwplcHMIFillTriangle`) construidos con `drawFastHLine`/`drawPixel`/`drawLine`, que sí existen en `libJWPLC_TFT.a`. Se emiten sólo si algún objeto los usa | `poc/designer-codegen.js` |
| B3 ABI de `JWPLC_TFT` | Se restauran `JWPLC_TFT.h/.cpp`, `tft_setup.h` y `README.md` a `main`: el header vuelve a coincidir con el `.a` publicado. El trabajo de fuentes de André sigue en `bf0fba1` (rama `v2.0.1-alpha.15-(HmiDesigner)`) para que Alpha13 lo recompile | `JWPLC/2.1.0/libraries/JWPLC_TFT/` |
| Fuentes no GLCD | Opciones visibles pero deshabilitadas ("requiere firmware"); preview y codegen usan GLCD (`FIRMWARE_SUPPORTS_FONTS=false`). Proyectos con otra fuente generan GLCD con un comentario | `poc/designer-fonts.js`, `poc/app.js`, `poc/designer-codegen.js` |
| B4 BAR circular/degradado | Codegen genera `JWPLC_UIBarField` con un comentario `AVISO`; tarjeta Circular y opción Degradado deshabilitadas | `poc/designer-bar.js`, `poc/styles.css` |
| LINE rotada | El codegen usa los vértices transformados (antes ignoraba la rotación) | `poc/designer-codegen.js` |
| Snippet del inspector | `buildShapeCppSnippet` y el contrato de RAW_TEXT delegan en `JWPLCHMICodegen.staticDrawSnippet()`: misma salida que el header | `poc/app.js` |
| Proyecto sólo con figuras | Antes no generaba header; ahora genera uno sin `HMI_FIELDS` | `poc/app.js` |
| `HMIFieldId` | Sólo campos del runtime, numerados de forma consecutiva (RAW_TEXT ya no aparece) | `poc/designer-codegen.js` |

## A15-2 — Persistencia y páginas

- `deletePage` renumera páginas, campos y PixelMaps a `0..N-1` y borra los PixelMaps de la
  página eliminada (antes quedaban huérfanos). `poc/app.js`
- Al abrir, un proyecto con ids no contiguos se **normaliza** en lugar de rechazarse; los
  PixelMaps de páginas inexistentes se descartan con aviso en consola. `poc/designer-project.js`
- La restauración de PixelMaps lee `data.pixelMaps` después de la normalización.
  `poc/designer-project-integration-fix.js`
- NAV_BAR con una sola fuente de verdad: `JWPLCHMIEditor.getNavBarConfig().enabled` existe
  sólo si hay un objeto NAV_BAR visible. `poc/app.js`
- `jwplcDrawPageNavBar` usa el id de página (antes, el índice del arreglo).

Limitación conocida: deshacer (Ctrl+Z) un borrado de página restaura páginas y campos pero no
los PixelMaps de esa página.

## A15-3 — Gráfica estática fuera del tick

Orden del runtime `[visto]` (`JWPLC_Display.cpp:619-633` al entrar a USER, `:1060-1099` al
cambiar de página), todo dentro de `beginBatch` (bus tomado):

```text
fillScreen(BLACK) -> jwplcUIPageEnter(page) -> jwplcUIRuntimeDrawStatic -> jwplcUIRuntimeDrawDirty
```

Header generado:

```cpp
void jwplcUIPageEnter(uint8_t page)     // figuras + RAW_TEXT (por layerOrder) + NAV_BAR
{                                        // + hook opcional jwplcHMIUserPageEnter(page)
    JWPLC_TFTClass &tft = JWPLC_Display.tft();
    switch (page) { case PAGE_PRINCIPAL: ... }
    jwplcDrawPageNavBar(page);
    if (jwplcHMIUserPageEnter) jwplcHMIUserPageEnter(page);
}

void jwplcUIUpdate()                     // sólo setters (dirty refresh)
{
    switch (JWPLC_Display.userPage()) { case PAGE_PRINCIPAL: JWPLC_Display.setValue(...); ... }
}
```

Consecuencias:

- La gráfica estática se dibuja **una vez por página**, no en cada tick.
- Orden de capas: en el lienzo `layerOrder` menor = más al frente (`composeFramebuffer`
  recorre el arreglo al revés). El codegen dibuja en `layerOrder` descendente (de atrás hacia
  adelante) para coincidir `[ejecutado]`.
- Queda **debajo** de los campos y PixelMaps (el runtime los dibuja después).
- La NAV_BAR se repinta al volver de IDLE `[inferido]`: antes el guard `s_lastNavBarPage`
  impedía redibujarla si la página no había cambiado, aunque `fillScreen` la hubiera borrado.
- El sketch Arduino que necesite lógica al cambiar de página define
  `extern "C" void jwplcHMIUserPageEnter(uint8_t page)` (declaración débil en el header).

`[no verificado]`: tiempo de dibujo por página y su efecto en W5500/SD → A15-9 en banco.

## Verificación `[ejecutado]`

`node tools/jwplc-hmi-designer/tests/codegen-smoke.cjs` → 20/20 PASS:

- proyecto completo, borrado de página intermedia + reapertura, proyecto legado `[0, 2]`,
  proyecto sólo con figuras;
- sin errores JS;
- el header no contiene `getTFT`, helpers BAR inexistentes ni API de fuentes;
- `jwplcUIUpdate()` sólo contiene setters;
- `g++ -fsyntax-only -Wall` contra los headers reales de `JWPLC_Display/JWPLC_UI/JWPLC_TFT`: sin errores ni warnings;
- todas las llamadas existen en `libJWPLC_Display.a` / `libJWPLC_TFT.a` (`nm -C`).

Contraprueba: el header generado por `cc66d69` falla con el mismo método
(`'class JWPLC_DisplayClass' has no member named 'getTFT'`), el mismo error que reportó la
revisión del fork con `arduino-cli`.

## Pendiente

```text
A15_4=compilar con arduino-cli para jwplc:esp32:jwplcbasic (taller / CI)
A15_9=paridad lienzo vs TFT (elipse por punto medio, triángulo por scanline), tiempos de bus
POLYGON_CONCAVO=relleno por abanico, correcto sólo para polígonos convexos
ELLIPSE_ROTADA=la rotación de elipses no se genera (O5 del plan del fork)
```

## Gate físico para el equipo

`tools/jwplc-hmi-designer/gates/A15_1_3_Codegen_Static/` (sketch + header generado + `.jwhmi`
+ capturas del Designer + checklist D1-D5 / T1-T8). El sketch compila en host contra los headers
reales `[ejecutado]`; compilación ESP32 y TFT `[no verificado]`.

## Observaciones para A15-7E

- Un objeto nuevo recibe el `layerOrder` más alto y por eso queda **detrás** de los existentes
  `[ejecutado]`. Es contraintuitivo (en Lopaka/Figma lo nuevo queda al frente).
- Los campos del runtime (TEXT/VALUE/BOOL/BAR) siempre quedan sobre la gráfica estática en la
  TFT, aunque en el lienzo una figura esté por delante: el Designer debería avisarlo.

## Higiene

Eliminados `/desktop.html` (copia UTF-16 con mojibake, `e72fb2c`) y `poc/fix.py`. Cache-busters
`?v=1792060000` y `CACHE_NAME=jwplc-hmi-designer-a15-1-codegen-v2`.
