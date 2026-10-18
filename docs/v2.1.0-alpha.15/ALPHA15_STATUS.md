# v2.1.0-alpha.15 — Estado

Actualizado: `2026-10-18`

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
A15_7E_RAW_SHAPES_NAV_PIXEL=PENDING (NAV: borrar páginas, estilo Contador e indicador NN/TT resueltos en A15_7_INSPECTOR_FOLLOWUP_3)
A15_7F_IDLE_CONFIG=PASS (Proyecto · Pantalla y arranque; ESC/Por inactividad/Nunca + MENU «Ir al inicio»)
A15_7G_APPEARANCE_A1=PASS (designer-zone-indicator.js: contorno doble + ▶ + regla; campos y MENU; A15_7_APARIENCIA_A1_ANALISIS_20261018.md §6)
A15_LOPAKA_REVIEW=DONE (A15_LOPAKA_REVISION_20261018.md; P1 aprobado, P2 ajustado)
A15_INDUSTRIAL_PLAN=APPROVED (A15_INDUSTRIAL_ARQUITECTURA_PLAN_20261018.md §G; Alpha15 cierra con I1-I3 + A1 + buzzer; resto Alpha16)
A15_BUZZER=PASS_ESP32_COMPILE (JWPLC_Buzzer en JWPLC_GlobalPeripherals; físico pendiente)
A15_I1_FORMATS=PASS_HOST_AND_ESP32_COMPILE (A15_I1_FORMATOS_VALUE_BOOL_20261018.md; VALUE miles/duración + BOOL piloto; físico pendiente)
A15_I2_RTC=PASS_HOST_AND_ESP32_COMPILE (A15_I2_FECHA_HORA_RTC_20261018.md; VALUE fecha/hora sin variable; físico pendiente)
A15_I3_CP437=PASS_HOST_AND_ESP32_COMPILE (A15_I3_TILDES_CP437_20261018.md; desfase cp437(false) resuelto; físico pendiente: ver ° en la placa)
A15_SKETCH_FLOW=PASS_WEB_SIMULATED (Escribir en sketch + menú del sketch; app de escritorio real [no verificado])
A15_FRAM_MAP=PROPOSED (plan §G.6: DataLog hoy sin FRAM; reparto del último KB)
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
JWPLC_DISPLAY=1.1.0 (MENU + BOOL por estado con fondo de todo el campo + anclaje fijo + marco sin recorte + setPageIndicator + VALUE miles/duración/fecha-hora + BOOL piloto + setText UTF-8 → cp437) · SOURCE_FIRST hasta el cierre (.a 0317c3ac… histórico, regenerar al cierre)
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
- 2026-10-18 — 3.ª revisión del jefe (`A15_7_INSPECTOR_FOLLOWUP_3_20261018.md`):
  - marco sin recorte en 3×/4× (margen derecho/inferior = escala + 1);
  - BOOL con fondo por estado en todo el campo;
  - flechas con codo de 90° en la Apariencia de campos y MENU;
  - sección "Pantalla y arranque" (IDLE) que escribe en `jwplcHMISetup()`;
  - NAV: borrar página con confirmación, estilo «Contador» (NN/TT de Alpha11 con `setPageIndicator`), indicador nativo en paridad con el lienzo y abreviaturas sin tildes.

  Gate F0 de Kenyi regenerado.
- 2026-10-18 — 4.ª revisión (`A15_7_INSPECTOR_FOLLOWUP_4_20261018.md`):
  - confirmada la cadena de ESC (edición → submenú → contenido → selector NN/TT → IDLE);
  - se retira «volver con otra tecla»;
  - la vuelta por inactividad ahora también cuenta dentro del contenido (firmware);
  - nuevo ítem de MENU «⏎ Ir al inicio» (`JWPLC_UIMenuGoIdle`).

  Análisis de la propuesta A1 de Apariencia, con crítica de diseño, y revisión de Lopaka con propuestas.
- 2026-10-18 — Skill del repo `.claude/skills/hmi-designer-ux` (reglas UX/UI del Designer).

  Plan de arquitectura para la ampliación industrial:
  - avisos ALARM/INTERLOCK y árbitro de teclas;
  - SEQUENCE;
  - duración y RTC en VALUE;
  - plantillas RECIPE, EVENT_LOG e IO_DIAGNOSTIC;
  - tendencia y gauge.

  Hitos I1–I15 y 7 decisiones pendientes. Nada implementado.
- 2026-10-18 — Plan industrial aprobado con ajustes (§G):
  - FRAM: último KB para el sistema;
  - diálogo de confirmación CANCELAR/ACEPTAR;
  - FINALIZADO + ACEPTAR;
  - `%MX` en OpenPLC (F0e para Kenyi).

  Nuevo `JWPLC_Buzzer` en el package (GPIO26, no bloqueante, volumen en `setup()`, ejemplo `07.Buzzer_Basic`).
  Sigue: I1 → I2 → I3 → A1 para cerrar Alpha15.
- 2026-10-18 — FRAM: `JWPLCDataLog` hoy no usa FRAM (A14_6 difirió el journal: RAM → SD). Propuesta de
  reparto del último KB en el plan §G.6: cabecera 32 B, avisos 64 B, recetas 416 B, últimos eventos 256 B
  y reserva 256 B para un journal futuro; el usuario conserva los 7 KB.
- 2026-10-18 — I1 (`A15_I1_FORMATOS_VALUE_BOOL_20261018.md`):
  - VALUE con separador de miles y duración (`JWPLC_UI_Format.cpp` + `designer-value-format.js`; paridad 19 200/19 200);
  - BOOL «Piloto» (`JWPLC_UIBoolPilot`);
  - las duraciones y los contadores con miles declaran enteros de 32 bits.

  Corregido el redondeo de empates del Designer (`toFixed` frente a `printf`). Gate `A15_I1_Formatos` compila para ESP32.
- 2026-10-18 — I2 (`A15_I2_FECHA_HORA_RTC_20261018.md`): VALUE «Fecha/hora» leído del RTC
  (`JWPLC_UIDateTimeFormat`, 5 patrones, `--:--` sin RTC válido). Sin variable ni setter; el firmware lo
  actualiza en `pollClockFields()` y sólo repinta cuando cambia el texto. Paridad 55/55; gate
  `A15_I2_Reloj` compila para ESP32.
- 2026-10-18 — I3 (`A15_I3_TILDES_CP437_20261018.md`): tildes, ñ y ° en la TFT.
  - El lienzo usa los 256 glifos de `glcdfont.c`.
  - El codegen emite cp437 en octal: un glifo > 176 se envía como byte − 1, por el `cp437(false)` de TFT_eSPI que usa `JWPLC_TFT`.
  - `setText()` convierte UTF-8 del sketch.
  - Las variables TEXT reservan 2 bytes por carácter.
  - Paridad lienzo == literal == firmware (16 textos).
  - Todos los gates A15 compilan para ESP32.
- 2026-10-18 — Apariencia A1 (`A15_7_APARIENCIA_A1_ANALISIS_20261018.md` §6): `designer-zone-indicator.js`
  reemplaza las flechas largas. Contorno de doble trazo neutro, ▶ en un canal izquierdo y banda en la
  mini-regla. Etiqueta = etiqueta + unidad; lo común del BOOL por estado se marca en ambos casos. Los
  píxeles del mini-modelo no cambian (verificado). Con esto quedan hechos I1, I2, I3, A1 y el buzzer del
  alcance de Alpha15 (§G.1).
- 2026-10-18 — Revisión del jefe tras A1:
  - el lienzo dibujaba los objetos en el orden del arreglo y no por capa: un objeto nuevo (al frente en la lista) quedaba detrás. Corregido en `composeFramebuffer` y en el dibujo de BAR, con test que falla con el código anterior;
  - la Apariencia de BAR pasa al patrón de mini-modelo, leyenda e indicador A1.

  Propuestas pendientes de decisión:
  - flujo de los botones Generar / Actualizar / Sketch;
  - glifos propios para `Á Í Ó Ú` (requiere regenerar `libJWPLC_TFT.a`; decidido: otra alpha).
- 2026-10-18 — Flujo con el sketch (aprobado por el jefe):
  - se oculta «Generar C++», porque el panel «Código generado» ya muestra el header;
  - «Actualizar HMI» pasa a **«Escribir en sketch»** y escribe el header y `<sketch>.jwhmi` con una sola confirmación;
  - «Sketch: nombre ▾» abre un menú: Abrir HMI del sketch · Abrir en Arduino IDE · Abrir carpeta · Cambiar sketch…;
  - al vincular un sketch con `.jwhmi`, pregunta si abrirlo.

  Electron: IPC nuevos `read-project`, `open-sketch-ino` y `open-sketch-folder`. Test
  `sketch-menu-smoke.cjs` con la app de escritorio simulada.

  Corregido además un error existente: el bloque de paneo de `app.js` llamaba funciones internas del
  editor fuera de su alcance (`updateCrosshairs is not defined`), así que la cruceta y la regla no se
  limpiaban al salir del lienzo.

  Los glifos `Á Í Ó Ú` quedan para otra alpha (no se regenera `libJWPLC_TFT.a`).
- 2026-10-18 — El menú del sketch no mostraba «Abrir en Arduino IDE» ni «Abrir carpeta» en la PC del jefe.
  - Causa: el Designer corría en modo navegador. `Install-JWPLC-HMI-Designer.ps1`, si no encuentra la app
    de escritorio compilada, instala sin avisar el lanzador que abre Edge.
  - Nuevo `Generar-e-Instalar-App-Escritorio.cmd` (doble clic): compila Electron y lo instala con
    `-RequireElectron`, que falla en vez de caer al modo navegador.
  - README con «qué hace cada archivo».
