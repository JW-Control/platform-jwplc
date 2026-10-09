# Fuentes tipograficas en JWPLC_TFT / HMI Designer - PENDIENTE RECOMPILAR

> [!IMPORTANT]
> Los cambios de `JWPLC_TFT` descritos aqui estan **solo en codigo fuente**.
> `libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a` **NO fue regenerado**.
> Hasta recompilarlo, el codigo generado con fuentes distintas de `GLCD` **no compilara/funcionara en hardware**.

Estado: fase de pruebas solo software (sin hardware). Recompilar al final.

## 1. Cambios hechos en la libreria (`JWPLC/2.1.0/libraries/JWPLC_TFT`)

| Archivo | Cambio |
|---|---|
| `src/tft_setup.h` | Se agregan `LOAD_FONT2`, `LOAD_FONT4`, `LOAD_GFXFF` (antes solo `LOAD_GLCD`). |
| `src/JWPLC_TFT.h` | Nuevos `enum class JWPLC_TFTFont`, `JWPLC_TFTDatum`; metodos `setFont/font`, `setTextDatum/textDatum`, `setTextPadding/textPadding`, `drawString`. |
| `src/JWPLC_TFT.cpp` | Tabla ID -> FreeFont de TFT_eSPI, `applyFont()`, `syncTextState()` reaplica fuente/datum/padding, guarda `#error` si faltan los `LOAD_*`. |
| `README.md` | Seccion "Fuentes - Intermedio". |

Fuentes: `GLCD`, `FONT2`, `FONT4`, `SANS|SERIF|MONO` x `9|12` x normal/bold (12 FreeFonts).

## 2. Cambios en el disenador (`tools/jwplc-hmi-designer/poc`)

| Archivo | Cambio |
|---|---|
| `designer-fonts.js` (nuevo) | Catalogo de 15 fuentes (ids = `JWPLC_TFTFont`), metricas y rasterizado aproximado. |
| `index.html` | `<select id="rawTextFont">` en el inspector RAW + script `designer-fonts.js`. |
| `app.js` | Campo `font` en `RAW_TEXT` (default `GLCD`), `drawRawTextAt`, `rawTextBounds`, sync/bind del selector. |
| `designer-codegen.js` | Fuente != GLCD => `setFont` + `setTextDatum(TOP_LEFT)` + `drawString` (con `fillRect` previo si fondo opaco) y `setFont(GLCD)` al final. GLCD conserva el codigo anterior (`setCursor/print`). |
| `designer-fonts-data.js` (generado) | Glifos reales de las 12 FreeFonts + Font 2 + Font 4 (~56 KB). Regenerar con `node tools/jwplc-hmi-designer/scripts/generate-gfx-fonts.js`. |
| `service-worker.js` | Se agrega `designer-fonts.js` al cache y se cambia el nombre del cache. |

Persistencia: el proyecto serializa el campo completo, `font` se guarda solo; proyectos viejos sin `font` => `GLCD`.

> [!NOTE]
> **Preview:** GLCD, FONT2, FONT4 y las 12 FreeFonts (Sans/Serif/Mono 9 y 12 pt) usan glifos REALES de TFT_eSPI
> (designer-fonts-data.js, generado con scripts/generate-gfx-fonts.js desde Fonts/GFXFF, Font16.c y Font32rle.c;
> confirma ademas que esos nombres existen en TFT_eSPI master). Defaults de TFT_eSPI: $ en Font 2 y `  ` = grado.
> Fondo opaco con fuentes no-GLCD: TFT_eSPI no rellena la celda del glifo, por eso el codegen emite
> `fillRect(x, y, ancho, alto, bg)` antes de `drawString` (el preview hace lo mismo). Ancho/alto salen de
> las metricas reales (suma de `xAdvance`, ascenso+descenso de `setFreeFont`).
> [!NOTE]
> El codigo generado asume `JWPLC_Display.getTFT()` devolviendo `JWPLC_TFTClass*`. En el package 2.1.0 actual no se
> encontro `getTFT` en `JWPLC_Display`; verificar al integrar.

Pendientes del disenador:

- [x] Preview con glifos reales para FreeFonts.
- [x] Preview real de FONT2 / FONT4 (Font16.c fila a fila, Font32rle.c RLE de 8 bits).
- [ ] Alineacion (datum) configurable desde la UI (hoy siempre TOP_LEFT).
- [ ] Probar en navegador: seleccion, arrastre, bounds, guardado/carga y exportacion con cada fuente.
## 3. CHECKLIST AL FINAL (recompilacion)

- [ ] Verificar que TFT_eSPI usada para regenerar (2.5.43) define `FreeSans9pt7b`, `FreeSansBold12pt7b`, `FreeSerif*`, `FreeMono*` (9 y 12 pt, normal y Bold).
- [ ] Regenerar `src/esp32/libJWPLC_TFT.a` con el nuevo `tft_setup.h` (ver scripts `Run-JWPLCP6B2GFXPrecompiledPilot.ps1` / gates `a14_h3e*` en `tools/`).
- [ ] Medir tamano del `.a` y flash usada por las 12 FreeFonts; si es excesivo, reducir la tabla en `JWPLC_TFT.cpp` (`freeFontFor`).
- [ ] Subir `version` en `library.properties` y `JWPLC_TFT_VERSION_*` en `JWPLC_TFT.h` (siguen en 0.1.0).
- [ ] Compilar un sketch de prueba con cada fuente (`setFont` + `drawString`) para varias placas del package.
- [ ] Validar en hardware: posicion Y (preview vs pantalla), fondo solido (`fillRect` previo) con las fuentes no-GLCD, restauracion a `GLCD` tras cada campo.
- [ ] Recalcular SHA256/size del package si el `.a` forma parte del release (ver `docs/Antiguos/Comandos_SHA256_Size.txt`).
- [ ] Actualizar notas de release.
