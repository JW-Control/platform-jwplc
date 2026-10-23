# Alpha15 · A15-0 — Auditoría de partida del HMI Designer

Fecha: `2026-10-16`

## Base auditada

```text
BRANCH_AUDITADA=v2.0.1-alpha.15-(HmiDesigner)
HEAD=cc66d69aa1eca461b7111c856293e5f1f6e103b4  (2026-10-13, "patch")
MERGE_BASE_MAIN=aa0bfed170c397cc4135c72c273223558ab3bf72
COMMITS_DESDE_MAIN=38
DIFF=42 archivos, +15790 / -1187
A15_0_AUDIT=PASS
```

Método: lectura de código `[visto]`, ejecución del designer en Chromium
headless con proyectos sintéticos `[ejecutado]`, `nm -C` sobre `.a`.
No hubo compilación ESP32 ni prueba en hardware `[no verificado]`.

## Historial resumido

| Fase | Commits | Contenido |
|---|---|---|
| 15‑sep | `e9b10d1`…`3832d6e` | arranque `desktop.html`, cache-busters, paneo, marquesina |
| 18‑sep | `643841c`…`49f2b8f` | lienzo infinito, reglas, grilla/snap, selección CAD, multiselección, bucket, cuentagotas, RAW_TEXT, inspector de figuras, rotación/flip, copy/paste |
| 09‑oct | `bf0fba1` | fuentes TFT (`JWPLC_TFT.h/.cpp`, `tft_setup.h`, `designer-fonts*.js`) — `.a` sin regenerar |
| 09‑oct | `59c5bbe` "Rev bar" | inspector BAR Propuesta 2 |
| 13‑oct | `cc66d69` "patch" | NAV_BAR como objeto, codegen de figuras/RAW en `jwplcUIUpdate()`, INDEX.md |

## Estado por módulo

| Módulo | % | Nota |
|---|---|---|
| Lienzo/edición | 85 | funcional, sin tests |
| TEXT/VALUE/BOOL | 90 | codegen coherente con `JWPLC_UI.h` |
| BAR lineal | 70 | orientación/estilo/radio sólo como comentario C++ |
| BAR circular/degradado | 25 | helpers C++ inexistentes |
| Figuras | 40 | codegen no compila |
| RAW_TEXT + fuentes | 35 | `getTFT()` + `.a` sin regenerar |
| JWPLC_TFT fuentes | 30 | header y `.a` desalineados |
| PixelMap | 85 | codegen por capas de parches |
| Páginas + NAV_BAR | 50 | bug de borrado, doble fuente de verdad |
| Proyecto `.jwhmi` | 70 | bug de reapertura |
| Live Preview | 80 | no verificado |
| Electron | 60 | sigue en `0.11.0-alpha.11` |

Criterio: 1/3 UI carga y opera, 1/3 codegen compatible con API real,
1/3 validación física/gate.

## Hallazgos bloqueantes

| ID | Hallazgo | Evidencia |
|---|---|---|
| B1 | `JWPLC_Display.getTFT()->` no existe; la API es `tft()`/`display()` (referencia) | `poc/designer-codegen.js:236-337`, `JWPLC_Display_API.h:126-129`, header generado `[ejecutado]` |
| B2 | `drawEllipse/fillEllipse/drawTriangle/fillTriangle` no existen en `JWPLC_TFTClass` | `JWPLC_TFT.h`, `nm -C libJWPLC_TFT.a` |
| B3 | `libJWPLC_TFT.a` sin `setFont/drawString`; header añadió miembros privados → riesgo ABI | `nm -C`, `JWPLC_TFT.h` (bf0fba1), `docs/v2.1.0-alpha.14/TFT_FONTS_RECOMPILE_PENDING.md` |
| B4 | `JWPLC_UICircularField` / `JWPLC_UIGradientBarField` no existen | `poc/designer-bar.js:3320-3345` |
| B5 | Borrar página intermedia → proyecto no reabrible ("páginas contiguas") | `app.js:3618`, `designer-project.js:283`, reproducido `[ejecutado]` |
| B6 | Figuras/RAW_TEXT redibujados en cada tick dentro de `jwplcUIUpdate()` con bus tomado | `designer-codegen.js:476-515`, `JWPLC_Display.cpp:1120` |
| B7 | Sin gates físicos para figuras, fuentes, NAV_BAR, BAR v2 | `gates/` sólo A11 |

## Importantes

- I1 NAV_BAR: `editor.getNavBarConfig().enabled=true` se serializa, pero
  `JWPLCHMIPages` exige objeto NAV_BAR → no se genera `jwplcDrawPageNavBar` `[ejecutado]`.
- I2 NAV_BAR codegen usa índice de array (`case ${idx}`) vs `HMIPageId` por id.
- I3 BAR: lo que se ve ≠ lo que dibuja la TFT (orientación/estilo/radio).
- I4 Figuras consumen `MAX_FIELDS=32` y emiten `FIELD_RAW_n` sin uso.
- I5 Codegen por capas de `.replace`/`indexOf` en ~6 módulos + lectura del DOM.
- I6 Variables definidas en `.h` sin `inline` (ODR si >1 TU) `[inferido]`.
- I7 Versionado inconsistente (ver abajo).

## Pulido

- Basura: `/desktop.html` (UTF‑16, mojibake, `e72fb2c`), `poc/fix.py`.
- Cache-busters mezclados; `?v=` doble en `ux-foundation.js:loadScript`;
  `CACHE_NAME` alpha11; SW sin `designer-native-project.js`.
- Panel "Estado / Problemas" = checklist hardcodeada (`app.js:2026`);
  "Gate: A11-7D" y "Alpha11" visibles en UI; tipografía de pestañas inferiores.
- `INDEX.md` con enlaces `file:///c:/Users/Andre/...`.
- `catch {}` vacíos; `loadScript` sigue en silencio si falla un módulo.

## UI/UX

- Flujo superior: Nuevo/Abrir/Guardar/Undo/Redo/Generar C++/Actualizar HMI/
  Vincular sketch/Conectar JWPLC. Carga sin errores JS `[ejecutado]`.
- Inspector Propuesta 2 sólo en BAR y parcialmente en figuras
  (`#shapeInspectorSection` reutiliza `bar-inspector-wrap`). TEXT/VALUE/BOOL/
  RAW/NAV_BAR/PixelMap siguen con selects y campos numéricos.
- Legibilidad TFT `[no verificado]`: etiquetas GLCD 5x7 tamaño 1 por defecto;
  pista BAR `0x18C3` sobre negro; NAV_BAR 22 px (13 % del alto).
- SD desde dibujo: no hay referencias en `JWPLC_Display/src` ni en el código
  generado `[visto]`; `.a` no inspeccionados.

## Versionado

```text
RAMA_PRACTICANTE=v2.0.1-alpha.15-(HmiDesigner)
RAMA_TRABAJO=v2.1.0-alpha.15-HmiDesigner
ROADMAP: Alpha13=TFT/Display, Alpha15=Retain/FRAM  (docs/JWPLC_V2_1_CLOSURE_ROADMAP.md:63-75)
DESIGNER/ELECTRON=alpha11 / 0.11.0-alpha.11
JWPLC_TFT=0.1.0
platform.txt version=2.0.0
DOC_FUENTES_EN=docs/v2.1.0-alpha.14/ (debería vivir en alpha.15)
```

Decisión del jefe (2026-10-16): Alpha13 = robustez del package, Alpha14 =
OpenPLC (Kenyi), Alpha15 = HMI Designer (André), en la rama
`v2.1.0-alpha.15-HmiDesigner`. La numeración del roadmap no aplica.

## Acoplamientos a JWPLC (para modo OpenPLC)

1. Target hardcodeado ST7789/320/170/rot3 (`designer-project.js:254`, `designer-live.js:4-5`).
2. `field.variable` = identificador C++; tipo deducido del widget (`designer-codegen.js:219-226`).
3. Salida = header Arduino contra `JWPLC_Display` + hook `jwplcUIUpdate`.
4. Live Preview = framebuffer crudo; ya desacoplado de tags.
