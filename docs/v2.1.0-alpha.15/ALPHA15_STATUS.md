# v2.1.0-alpha.15 — Estado

Actualizado: `2026-10-17`

## Identidad

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner
PARENT_BRANCH=v2.0.1-alpha.15-(HmiDesigner) (congelada; último commit fusionado af797d79)
TEAM_BRANCH=v2.1.0-alpha.15-HmiDesigner (André trabaja aquí desde 2026-10-17)
BASE_SHA=cc66d69aa1eca461b7111c856293e5f1f6e103b4
OWNER_ALPHA15=André (HMI Designer)
ALPHA15_STATUS=IN_PROGRESS
ALPHA15_RELEASE_PR=NOT_OPENED
```

## Organización de alphas

```text
ALPHA13=robustez del package (jefe)
ALPHA14=OpenPLC (Kenyi)
ALPHA15=HMI Designer (André)
ROADMAP_V2_1_NUMERACION=SUSTITUIDA_POR_ESTA_ORGANIZACION
```

## Gates

```text
A15_0_AUDIT=PASS
A15_1_CODEGEN_COMPILE=PASS (host g++/nm; arduino-cli en A15-4)
A15_2_PERSISTENCE_PAGES=PASS
A15_3_STATIC_OUT_OF_TICK=PASS (tiempos de bus en A15-9)
A15_4_DESIGNER_CI=PASS_ESP32_COMPILE (smoke host + arduino-cli ESP32 en la nube; falta automatizar en CI)
A15_5_PURE_CODEGEN_TESTS=PASS_PARTIAL (golden 5/5 + núcleo en node; estática/NAV/PixelMap aún por capas)
A15_6_JWHMI_V2_MODES_HOST=PENDING
A15_7A_BAR_FINISH=PASS (A15_7_INSPECTOR_CLOSE_20261017; paridad lienzo/firmware 16/16)
A15_7B_VALUE=PASS
A15_7C_TEXT=PASS
A15_7D_BOOL=PASS
A15_7E_RAW_SHAPES_NAV_PIXEL=PENDING
A15_8_MENU=PASS_HOST_AND_ESP32_COMPILE (A15_8_MENU_20261017; físico pendiente)
A15_9_PHYSICAL_PARITY=PENDING
A15_10_PACKAGING_DOCS=PENDING
A15_OPENPLC_INTEGRATION_DECISION=PASS (adopta plan rev.2 del fork + correcciones C1-C5)
A15_ALPHA12_MERGE=PASS (VPP 2.1.0-alpha.29 + overlay IDLE de Kenyi; merge 7cbd427c)
A15_F0_OPENPLC_BOOL_RELE=PASS_COMPILE (HAL VPP + bloque F0; OpenPLC y placa pendientes, Kenyi)
```

## Bloqueantes abiertos (de A15-0)

```text
B1_GETTFT_NOT_IN_API=CLOSED (A15-1)
B2_TFT_MISSING_PRIMITIVES=CLOSED (JWPLC_TFT 0.2.0 nativo + respaldo para 0.1.0)
B3_TFT_A_NOT_REGENERATED_ABI=MITIGATED (fuentes JWPLC_TFT revertidas a main; fuentes -> Alpha13 desde bf0fba1)
B4_BAR_CIRCULAR_GRADIENT_HELPERS_MISSING=CLOSED_IN_DESIGNER (deshabilitado + fallback lineal; firmware -> Alpha13)
B5_DELETE_PAGE_BREAKS_REOPEN=CLOSED (A15-2)
B6_STATIC_DRAW_EVERY_TICK=CLOSED (A15-3)
B7_NO_PHYSICAL_GATES=OPEN (gate A15_1_3_Codegen_Static listo, pendiente de taller)
```

## Inspector (adopción del patrón Propuesta 2)

```text
BAR=ADOPTED (BAR-1..BAR-7 cerrados; variantes sin firmware deshabilitadas)
SHAPES=PARTIAL (reutiliza bar-inspector-wrap)
VALUE=ADOPTED (designer-field-inspector.js)
TEXT=ADOPTED (designer-field-inspector.js)
BOOL=ADOPTED (designer-field-inspector.js)
RAW_TEXT=LEGACY
NAV_BAR=ADOPTED_UI (André, af797d79: Diseño/Páginas/Técnico, 6 presets, 4 posiciones); codegen replica el lienzo con el pintor compartido (NAV-1/NAV-2, 207f1594)
PIXELMAP=LEGACY
MENU=ADOPTED (designer-menu.js; firmware JWPLC_Display 1.1.0)
```

## Versiones

```text
DESIGNER_UI=Alpha11
ELECTRON_PACKAGE=0.11.0-alpha.11
JWPLC_TFT=0.3.0 (elipses/triángulos + FreeFonts con setFont/drawString; .a regenerado, ABI compatible)
JWPLC_DISPLAY=1.1.0 (MENU + texto/fondo por estado de BOOL + anclaje fijo de etiqueta/valor) · SOURCE_FIRST hasta el cierre (.a 0317c3ac… histórico, regenerar al cierre)
DESIGNER_FONTS=GLCD + 24 FreeFonts (Font2/Font4 deshabilitadas)
PROJECT_FORMAT=JWPLC_HMI_PROJECT v1
VPP_OPENPLC=2.1.0-alpha.29 (rama v2.1.0-alpha.12/feature/openplc-engineering-closure)
```

## Insumos externos pendientes

```text
BOTONERA=6_TECLAS (confirmado; matriz 2x3)
OPENPLC_DEBUG_SET_FORCING0_SEMANTICS=PENDIENTE_F0 (Alpha14)
MENU_MAX_DEPTH=3 (MVP)
MENU_MVP_NODES=SUBMENU, VIEW, EDIT_NUM, EDIT_BOOL
MENU_NODE_COUNT_LIMIT=64 por menú, 4 menús, 1 por página (9 nodos = +952 B RAM, +6.8 KB flash)
```

## Registro

- 2026-10-16 — Rama creada desde `cc66d69`. Auditoría A15-0, `CLAUDE.md`
  (raíz y `tools/jwplc-hmi-designer/`) y plan inicial. Sin cambios de código.
- 2026-10-16 — Revisión del fork `openplc-editor` (`develop/alpha12-backplane-closure`)
  y del VPP. Se adopta el plan HMI rev. 2 de Kenyi con correcciones
  (`A15_OPENPLC_INTEGRATION_DECISION_20261016.md`). MENU definido como objeto
  de página para configurar variables. Organización de alphas 13/14/15 fijada.
- 2026-10-16 — Confirmado: botonera de 6 teclas (matriz 2x3). MENU MVP con
  profundidad máxima 3 y nodos SUBMENU/VIEW/EDIT_NUM/EDIT_BOOL.
- 2026-10-16 — Especificación A15-7 (cerrar BAR, rehacer TEXT/VALUE/BOOL) con
  hallazgos de los inspectores actuales; prioridad alta tras A15-1..A15-3.
- 2026-10-17 — A15-1, A15-2 y A15-3 cerrados (`A15_1_3_CODEGEN_PAGES_STATIC_20261017.md`).
  Smoke test `tools/jwplc-hmi-designer/tests/codegen-smoke.cjs` 20/20. Fuentes de
  `JWPLC_TFT` revertidas a `main`. Siguiente: A15-7A..7D.
- 2026-10-17 — Corrección de orden de capas en el codegen (descendente, igual que el lienzo).
  Gate físico listo para taller: `tools/jwplc-hmi-designer/gates/A15_1_3_Codegen_Static/`.
  Regenerar `libJWPLC_TFT.a` desde la nube bloqueado por red (github.com releases y
  downloads.arduino.cc denegados) — resuelto el mismo día al habilitar los dominios.
- 2026-10-17 — Toolchain ESP32 en la nube (`tools/alpha15/regenerate_libJWPLC_TFT.sh`).
  Gate A15 y headers del smoke test compilan/enlazan para ESP32. `JWPLC_TFT` 0.2.0 con
  primitivas de elipse/triángulo, `.a` regenerado y ABI compatible
  (`A15_4_TFT_PRIMITIVES_ESP32_COMPILE_20261017.md`).
- 2026-10-17 — Fusionado `af797d79` de André (inspector NAV_BAR estilo BAR, presets, posición
  vertical, reglas perimetrales, cuentagotas). Único conflicto: cache-busters de `index.html`.
  Smoke test PASS, gate A15 compila para ESP32 (444649 B / 34620 B). Hallazgo NAV-1: el preview
  aplica `fontSize` micro/mediana, `textAlign` y `labelFormat`, el codegen no (GLCD 5x7 centrado,
  nombre corto) -> A15-7E. André continúa en esta rama.
- 2026-10-17 — A15-7A..7D cerrados (`A15_7_INSPECTOR_CLOSE_20261017.md`). Inspector Propuesta 2
  para TEXT/VALUE/BOOL (`designer-field-inspector.js`) y cierre de BAR-1..BAR-7. Hallazgo: el lienzo
  de BAR no coincidía con la TFT (la matriz 3×3 no cambiaba `layout`; INLINE centraba la etiqueta;
  alto/fondo/AUTO). `applyFirmwareCaps()` normaliza el modelo y `inspector-smoke.cjs` verifica la
  paridad contra una traducción de `computeFieldGeometry()`. Fondo TRANSPARENT de campos ya no
  genera `0xTRANSPARENT`.
- 2026-10-17 — A15-8 MENU MVP (`A15_8_MENU_20261017.md`). Firmware: `JWPLC_UI_Menu.h/.cpp` en
  `JWPLC_Display` 1.1.0 (hooks weak/strong, sin cambio de layout de clases), `.a` regenerado con
  `tools/alpha15/regenerate_libJWPLC_Display.sh` (reproduce el `.a` anterior con 0 diferencias antes
  del cambio). Simulación en host del firmware real 39/39 y paridad exacta lienzo/firmware. Designer:
  herramienta MENU, inspector con árbol, codegen con `#error` de versión. Gates `A15_8_Menu` (a mano)
  y `A15_8_Menu_Designer` compilan para ESP32. Corregido: duplicado de `jwplcUIUpdate()` en una
  segunda pasada del codegen sin gráfica estática.
- 2026-10-17 — A15-5 parcial (`A15_5_PURE_CODEGEN_TESTS_20261017.md`): golden tests de 5 proyectos
  generados antes de refactorizar, núcleo `designer-codegen-core.js` (bloques de campos) usado por
  app/bool/bar, MENU ejecutable en node, `tests/codegen-core.test.cjs` y `tests/run-all.cjs`
  (omite lo que requiere g++). El golden detectó y evitó una regresión en BAR durante la extracción.
- 2026-10-17 — Revisión del jefe (`A15_7_INSPECTOR_FOLLOWUP_20261017.md`): `JWPLC_Display` pasa a modo
  fuente hasta el cierre (convención del equipo; el `.a` se regenera antes de documentar y publicar).
  Colores en vivo en los inspectores, color por estado de BOOL (firmware + Designer, con respaldo
  `#ifdef`), regla/cursor alineados con la grilla, Apariencia del MENU con dos mini-modelos. El
  desplazamiento de 1 px de TEXT en 4× es la regla de padding del firmware (decisión pendiente).
- 2026-10-17 — 2.ª revisión del jefe (`A15_7_INSPECTOR_FOLLOWUP_2_20261017.md`):
  - corregido en firmware el corrimiento de 1 px en 4× (margen superior/izquierdo fijo de 3 px);
  - BOOL con texto y fondo por estado (constructor de 6 argumentos, `#ifdef` en el codegen);
  - Apariencia de TEXT/VALUE/BOOL con mini-modelos como el MENU;
  - selector de color limitado a 25 eventos/s;
  - objetos nuevos al frente;
  - grilla de 1/2 px.

  Pendiente: extender los mini-modelos a BAR, figuras, RAW_TEXT y NAV_BAR (A15-7E).
- 2026-10-18 — Merge de `v2.1.0-alpha.12/feature/openplc-engineering-closure` (Kenyi, VPP 2.1.0-alpha.29)
  en la rama del Designer. Archivos de alpha.12 sin cambios; sólo chocó `libJWPLC_Display.a`, que no se
  enlaza en modo fuente (se conserva el de alpha.15, regenerar al cierre). Gate
  `gates/A15_F0_OpenPLC_Bool_Rele` (BOOL ↔ relé Q0_0) y ruta para Kenyi en
  `A15_F0_OPENPLC_RUTA_KENYI_20261018.md`.
