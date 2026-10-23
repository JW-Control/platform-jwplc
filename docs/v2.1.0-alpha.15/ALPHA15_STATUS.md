# v2.1.0-alpha.15 — Estado

Actualizado: `2026-10-19`

## Identidad

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner
PARENT_BRANCH=v2.0.1-alpha.15-(HmiDesigner) (congelada; último commit fusionado af797d79)
TEAM_BRANCH=v2.1.0-alpha.15-HmiDesigner (André trabaja aquí desde 2026-10-17)
BASE_SHA=cc66d69aa1eca461b7111c856293e5f1f6e103b4
OWNER_ALPHA15=André (HMI Designer)
ALPHA15_STATUS=DEV_CLOSED (2026-10-19; cierre y transferencia: A15_CIERRE_Y_TRANSFERENCIA_20261019.md)
ALPHA15_RELEASE=2.1.0-alpha.14 (por orden de publicación; ver «Organización de alphas»)
ALPHA15_RELEASE_PR=#141 (borrador; rama v2.1.0-alpha.14/feature/hmi-designer desde release/v2.1.x)
ALPHA14_INTEGRATION=FASE_1_PASS (CI d7afe353: designer-tests 10/10, package smoke PASS; TFT .a regenerado ABI_COMPAT=PASS)
ALPHA14_NEXT=FASE_2_FISICA (ver JWPLC_NEW_CHAT_START_ALPHA14_20261023.md)
```

## Organización de alphas

```text
ALPHA13=robustez del package (jefe)                  -> publica 2.1.0-alpha.13
ALPHA15=HMI Designer (André, esta rama)              -> publica 2.1.0-alpha.14 (final del repositorio)
ALPHA14=OpenPLC + integración HMI (Kenyi)            -> publica 2.1.0-alpha.15
RAMAS_Y_CARPETAS=conservan su etiqueta de trabajo (decisión del jefe 2026-10-19: numeración por orden de publicación)
```

## Gates

```text
A15_0_AUDIT=PASS
A15_1_CODEGEN_COMPILE=PASS (host g++/nm; arduino-cli en A15-4)
A15_2_PERSISTENCE_PAGES=PASS
A15_3_STATIC_OUT_OF_TICK=PASS (tiempos de bus en A15-9)
A15_4_DESIGNER_CI=PASS_ESP32_COMPILE (smoke host + arduino-cli ESP32 en la nube; falta automatizar en CI)
A15_5_PURE_CODEGEN_TESTS=PASS_PARTIAL_REST_DEFERRED (golden 5/5 + núcleo en node; estática/NAV/PixelMap aún por capas)
A15_6_JWHMI_V2_MODES_HOST=TRANSFERRED (al trabajo OpenPLC de Kenyi, publicación 2.1.0-alpha.15)
A15_7A_BAR_FINISH=PASS (A15_7_INSPECTOR_CLOSE_20261017; paridad lienzo/firmware 16/16)
A15_7B_VALUE=PASS
A15_7C_TEXT=PASS
A15_7D_BOOL=PASS
A15_7E_RAW_SHAPES_NAV_PIXEL=DEFERRED (NAV: borrar páginas, estilo Contador e indicador NN/TT resueltos en A15_7_INSPECTOR_FOLLOWUP_3)
A15_7F_IDLE_CONFIG=PASS (Proyecto · Pantalla y arranque; ESC/Por inactividad/Nunca + MENU «Ir al inicio»)
A15_7G_APPEARANCE_A1=PASS (designer-zone-indicator.js: contorno doble + ▶ + regla; campos y MENU; A15_7_APARIENCIA_A1_ANALISIS_20261018.md §6)
A15_LOPAKA_REVIEW=DONE (A15_LOPAKA_REVISION_20261018.md; P1 aprobado, P2 ajustado)
A15_INDUSTRIAL_PLAN=APPROVED (A15_INDUSTRIAL_ARQUITECTURA_PLAN_20261018.md §G; Alpha15 cierra con I1-I3 + A1 + buzzer; resto Alpha16)
A15_BUZZER=PASS_PHYSICAL (JWPLC_Buzzer en JWPLC_GlobalPeripherals; pitido, no bloqueante y volumen OK 2026-10-19)
A15_I1_FORMATS=PASS_PHYSICAL (A15_I1_FORMATOS_VALUE_BOOL_20261018.md; piloto, miles y duración OK en la placa 2026-10-19)
A15_I2_RTC=PASS_PHYSICAL (A15_I2_FECHA_HORA_RTC_20261018.md; reloj OK en la placa 2026-10-19)
A15_I3_CP437=PASS_PHYSICAL (A15_I3_TILDES_CP437_20261018.md; «°» OK en la placa 2026-10-19)
A15_SKETCH_FLOW=PASS_DESKTOP (Escribir en sketch + menú del sketch; el jefe abrió el Arduino IDE desde la app de escritorio el 2026-10-18)
A15_SKETCH_GUIDE=PASS_DESKTOP (JWPLC_HMI_LEEME.md escrito por la app; el jefe compiló y subió su sketch base con jwplc_local)
A15_WAKE_KEY_FIX=PASS_PHYSICAL (la tecla que despierta la IDLE ya no entra al contenido ni al MENU; confirmado 2026-10-19)
A15_NAV_TAB_STATES=PASS_PHYSICAL (pestaña posicionada/seleccionada con doble marco; aprobado por el jefe 2026-10-19)
A15_NAV_PAGE_COLOR=PASS_PHYSICAL (color de cada pestaña con el editor fijo en NAV · Páginas; OK en la placa 2026-10-19)
A15_COLOR_EDITOR=PASS (editor de color fijo en Apariencia de campos, MENU y BAR; filas clicables y alineadas)
A15_FRAM_MAP=DEFERRED (plan §G.6: DataLog hoy sin FRAM; reparto del último KB; se fija en I9/I10 del plan industrial)
A15_8_MENU=PASS_PHYSICAL (A15_8_MENU_20261017; MENU OK en la placa 2026-10-19)
A15_9_PHYSICAL_PARITY=PASS_PHYSICAL (MENU, RTC, °, piloto, formatos, buzzer, despertar, pestañas y color de pestaña; 2026-10-19)
A15_LIVE_BRIDGE_FLASHER=PASS_HOST (flasheo en un clic; 10/10 tests; físico [no verificado])
A15_10_PACKAGING_DOCS=PENDING_INTEGRATION (freeze de libJWPLC_Display.a, versiones y publicación 2.1.0-alpha.14; ver cierre §4)
A15_OPENPLC_INTEGRATION_DECISION=PASS (adopta plan rev.2 del fork + correcciones C1-C5)
A15_ALPHA12_MERGE=PASS (VPP 2.1.0-alpha.29 + overlay IDLE de Kenyi; merge 7cbd427c)
A15_F0_OPENPLC_BOOL_RELE=PASS_COMPILE (HAL VPP + bloque F0; OpenPLC y placa pendientes, Kenyi)
```

## Pendiente para la confirmación de cierre (2026-10-21)

```text
P1_LIVE_BRIDGE_FISICO=PENDING (André: regenerar app; sin Live Bridge -> aviso -> instalar -> LIVE conectado y diseño en la TFT)
P2_ABRIR_DESDE_ARDUINO_IDE=PENDING (André: con .jwhmi, con y sin cambios sin guardar)
P3_APP_ESCRITORIO_SIN_MODO_DESARROLLADOR=PENDING (André: Generar-e-Instalar-App-Escritorio.cmd termina y abre la app)
P4_GATES_BUILD_EN_REPO=DONE (2026-10-21: tools/jwplc-hmi-designer/gates/**/build/ fuera del repo y en .gitignore)
P5_VERSION_VISIBLE_DEL_DESIGNER=DONE (2026-10-21: título, distintivo, barra de estado, panel Estado y app Electron dicen 2.1.0-alpha.14)
P6_KNOWLEDGE_PR=OPEN (JW-Control/jwplc-dev-mcp#28, sin conflictos con main; falta fusionarlo)
```

Fuera del cierre de esta alpha (integración y publicación 2.1.0-alpha.14): ver
[A15_CIERRE_Y_TRANSFERENCIA_20261019.md](A15_CIERRE_Y_TRANSFERENCIA_20261019.md) §4.

## Bloqueantes abiertos (de A15-0)

```text
B1_GETTFT_NOT_IN_API=CLOSED (A15-1)
B2_TFT_MISSING_PRIMITIVES=CLOSED (JWPLC_TFT 0.2.0 nativo + respaldo para 0.1.0)
B3_TFT_A_NOT_REGENERATED_ABI=CLOSED (libJWPLC_TFT.a 0.3.0 regenerado con ABI_COMPAT=PASS; fuentes y primitivas en uso)
B4_BAR_CIRCULAR_GRADIENT_HELPERS_MISSING=CLOSED_IN_DESIGNER (deshabilitado + fallback lineal; firmware -> Alpha13)
B5_DELETE_PAGE_BREAKS_REOPEN=CLOSED (A15-2)
B6_STATIC_DRAW_EVERY_TICK=CLOSED (A15-3)
B7_NO_PHYSICAL_GATES=CLOSED (gates físicos en PASS el 2026-10-19; ver A15_9_PHYSICAL_PARITY)
```

## Inspector (adopción del patrón Propuesta 2)

```text
BAR=ADOPTED (BAR-1..BAR-7 cerrados; variantes sin firmware deshabilitadas)
SHAPES=ADOPTED_UI (André 2026-10-20: Apariencia con editor de color fijo y matriz de alineación 3×3)
VALUE=ADOPTED (designer-field-inspector.js)
TEXT=ADOPTED (designer-field-inspector.js)
BOOL=ADOPTED (designer-field-inspector.js)
RAW_TEXT=ADOPTED_UI (André 2026-10-20: #rawTextControlsSection con editor de color fijo)
NAV_BAR=ADOPTED_UI (André, af797d79: Diseño/Páginas/Técnico, 6 presets, 4 posiciones); codegen replica el lienzo con el pintor compartido (NAV-1/NAV-2, 207f1594)
PIXELMAP=PARTIAL (André 2026-10-20: selección múltiple, color del pincel con editor fijo)
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
- 2026-10-19 — El jefe confirma la app de escritorio (abre el Arduino IDE). Sugerencias aplicadas:
  - **Apariencia:** el selector de color nativo (flotante, tapaba «Fondo y pista» de BAR y había que
    cerrarlo para cambiar de fila) se reemplaza por un editor fijo bajo la leyenda
    (`poc/designer-color-editor.js`: cuadro S/V, tono, HEX, RGB565 y paleta rápida), igual en
    TEXT/VALUE/BOOL, MENU y BAR. Toda la fila es clicable (texto = cuadro); texto a la izquierda y
    cuadros a la derecha. [ejecutado] inspector-smoke/menu-smoke + capturas.
  - **Guía del sketch:** «Escribir en sketch» deja además `JWPLC_HMI_LEEME.md` con el sketch base
    (`#include`, `jwplcHMISetup()`, una línea por variable de pantalla y el `switch` de
    `menuChanged()` por MENU) y los pasos para un sketch existente. Se arma leyendo el header generado
    (`poc/designer-sketch-guide.js`). El `.ino` no se toca nunca: insertar código en el `.ino` del usuario
    queda descartado salvo un futuro «agregar `#include`» aditivo y con confirmación. IPC nuevo
    `write-guide` (sólo ese nombre; sólo reemplaza una guía con la marca del Designer): **hay que
    regenerar la app** con `Generar-e-Instalar-App-Escritorio.cmd`. [ejecutado] `tests/sketch-guide.test.cjs`
    compila el sketch base contra los 9 golden (g++ host, headers reales); `sketch-menu-smoke.cjs`
    cubre la escritura, el `.ino` intacto y una guía ajena que no se reemplaza.
  - `run-all.cjs`: 9/9 PASS.
- 2026-10-19 — Prueba del jefe en la placa con el sketch base del LEEME:
  - Con la placa publicada `jwplc:esp32:jwplcbasic` (2.1.0-alpha.12) salta el `#error` del MENU: ese
    `JWPLC_Display` no tiene MENU. Alpha15 se compila con `jwplc_local` (enlace de `JWPLC/2.1.0` en
    `<sketchbook>/hardware/jwplc_local/esp32`). Con `jwplc_local` compila y sube [ejecutado por el jefe].
  - **Bug: «al entrar con OK no me deja moverme entre páginas».** Causa [visto + ejecutado en host]:
    `enterUserUI()` deja intactos los latches de la botonera (Alpha8) y, en el mismo callback,
    `serviceNavigation()` lee el PRESS de la tecla que despertó la IDLE. Un OK entraba directo al
    contenido de la página 1 (LEFT/RIGHT ya eran del contenido o del MENU) y un LEFT/RIGHT cambiaba de
    página. Aparece en Alpha15 porque el Designer usa por defecto `IDLE_WAKE_ANY_BUTTON` (A15-7F).
  - Corrección en firmware: `JWPLCUIPages::prepareEnter()` descarta las pulsaciones pendientes al entrar
    al selector (igual que `enterSelectionMode()`), y `jwplcUIMenuPrepareEnter()` también lo hace si hay
    un MENU en la página de entrada. Sin páginas ni MENU, el sketch sigue recibiendo la tecla (regla 5).
  - Test nuevo `tests/menu-host/wake_sim.cpp` (dentro de `menu-host-sim.cjs`): compila los
    `JWPLC_UI_Pages.cpp` y `JWPLC_UI_Menu.cpp` reales. Falla 7/10 con el código anterior y pasa 10/10
    con la corrección. Compilan para ESP32 los gates `A15_8_Menu`, `A15_8_Menu_Designer` y el sketch
    base del LEEME con MENU. Físico pendiente: despertar con OK y ver el NN/TT negro.
  - **Barra de pestañas: posicionado y seleccionado se veían iguales** (la barra se dibuja una vez por
    página y OK/ESC no cambian de página). Decisión del jefe: Propuesta 4 con doble marco hacia
    adentro, en todos los estilos salvo «Contador»:
    - no posicionado = sin fondo (estilo inactivo de cada estilo);
    - posicionado = fondo de color (una página blanca se posiciona en gris);
    - seleccionado = fondo blanco y doble marco hacia adentro de 1 + 1 + 1 px.
    Un acento claro se oscurece sobre blanco para que se lea. En «Puntos», la cápsula mide 5 px de alto
    y lleva un solo marco.
  - Firmware: hook débil `jwplcUIPageModeChanged(page, selecting)`, que se llama desde
    `jwplcUIRuntimeDrawDirty` (bus tomado) sólo cuando OK/ESC cambia el modo sin cambiar de página.
    Codegen: `jwplcDrawPageNavTab()` dibuja la pestaña activa en sus dos estados;
    `jwplcDrawPageNavBar()` dibuja el resto y la pestaña según `isUserPageSelection()`. Designer:
    tarjeta «Pestaña de la página actual» con los tres estados (pintor real); Posicionado/Seleccionado
    cambian lo que muestra el lienzo.
  - [ejecutado]:
    - `wake_sim.cpp` cubre el hook (una llamada por OK/ESC, ninguna al despertar ni al cambiar de página);
    - los 36 headers de barra compilan en host;
    - golden `full.h` actualizado (diff revisado);
    - compila ESP32 un sketch con la barra de 3 páginas, y en el ELF queda el hook del sketch
      (`T jwplcUIPageModeChanged`);
    - capturas de los 6 estilos en los dos modos.
  - [no verificado] en la placa.
- 2026-10-19 — Prueba del jefe en la placa:
  - aprobados el selector de páginas (pestaña posicionada/seleccionada), el MENU, la hora del RTC, el «°»
    y el BOOL piloto;
  - pedido: color de cada pestaña editable a gusto. «NAV · Páginas» usa ahora el editor de color fijo
    bajo la lista (fila o cuadro elige la pestaña, HEX/RGB565/S-V/paleta, deshacer en un paso). Antes el
    cuadro sólo rotaba 8 colores. `setPageMetadata(page, meta, {commit})` admite la vista previa en vivo
    sin historial. [ejecutado] inspector-smoke (HEX, paleta y deshacer) + captura;
  - aviso `-Wformat-truncation` del NN/TT corregido en `JWPLC_UI_Pages.cpp` (`char text[8]`).
- 2026-10-19 — **Cierre de desarrollo.** El jefe aprueba en la placa el buzzer (pitido, no bloqueante, volumen),
  los formatos, el reloj y el color de pestaña. Alpha15 se publica como **2.1.0-alpha.14** (orden de publicación);
  la Alpha14 de Kenyi pasa a 2.1.0-alpha.15.

  Verificado en modo lectura: esta rama **no contiene Alpha13**. Con Alpha12 publicado como ancestro, la fusión
  deja sólo 2 conflictos (`.gitignore` y `libJWPLC_TFT.a`). La `.a` de la fuente fusionada se regenera con
  `ABI_COMPAT=PASS`. Procedimiento, pendientes y diferidos:
  [A15_CIERRE_Y_TRANSFERENCIA_20261019.md](A15_CIERRE_Y_TRANSFERENCIA_20261019.md).
- 2026-10-20 — Revisión final de André (commit `d06e28c6`):
  - selección múltiple unificada (campos y PixelMap) con arrastre y marquesina;
  - color del pincel PixelMap y Esc para soltar la herramienta;
  - inspectores de RAW_TEXT y de figuras (Apariencia con el editor de color y matriz de alineación de 9 posiciones);
  - pista vacía de BAR negra por defecto (`0x0000`; golden `full.h` actualizado, el cambio es intencional);
  - fase 1 del flasheo del Live Bridge (`PLAN_LIVE_BRIDGE_FLASHER.md`: el sketch puente ya no usa `drawRGBBitmap`;
    binarios en `poc/firmware/` y `esptool-bundle.js`).

  [ejecutado] `run-all.cjs`: 8/9 PASS sobre su commit; sólo fallaba el golden por el color de pista, ahora 9/9.
- 2026-10-20 — En la PC de André, `Generar-e-Instalar-App-Escritorio.cmd` fallaba al extraer `winCodeSign`.
  Causa: los enlaces simbólicos de macOS del `.7z`; sin modo desarrollador, Windows no deja crearlos.
  - Nuevo `JWPLC-ElectronBuilderCache.ps1`, usado por los builds portable y Setup: prepara
    `%LOCALAPPDATA%\electron-builder\Cache\winCodeSign\winCodeSign-2.6.0` sin `darwin\`, con el 7-Zip de electron-builder.
  - Los `.ps1` con tildes llevan ahora BOM UTF-8; sin BOM, Windows PowerShell 5.1 los leía como ANSI («fallÃ³»).
  - [ejecutado] con PowerShell 7 en Linux: sintaxis de los 10 `.ps1`, y la función crea la caché con
    `rcedit-x64.exe` y la reutiliza la segunda vez. [no verificado] build completo en Windows: lo confirma André.
- 2026-10-21 — André (commits `c3fbef42` y `e80145a9`):
  - **flasheo del Live Bridge en un clic**: `designer-flasher.js`, `esptool-bundle.js` (esptool-js 0.7.0) y
    `poc/firmware/`. Ver `INFORME_CAMBIOS_FLASHER.md`;
  - **apertura desde el Arduino IDE** con el sketch activo ya vinculado (argumento o `jwplc-hmi://`), y
    «Desvincular sketch».

  [ejecutado] `run-all.cjs` 10/10 PASS.

  Revisión y continuación:
  - el manifiesto no escribía `boot_app0.bin @0xE000`, que la subida de Arduino sí escribe (`flash_args`); agregado
    (es idéntico a `JWPLC/2.1.0/tools/partitions/boot_app0.bin`), con su test;
  - el modal ahora avisa que el flasheo **reemplaza el programa del PLC** (máquina sin lógica y salidas
    apagadas hasta volver a subir el sketch);
  - abrir desde el Arduino IDE con cambios sin guardar pregunta antes de reemplazar el diseño
    (`JWPLCHMIProject.isDirty()`);
  - quitadas 4 copias idénticas `JWPLC_HMI_Live_Bridge.ino.*` de `poc/firmware/` (4,6 MB que no usaba nadie);
  - nuevo `tools/alpha15/build_live_bridge_firmware.sh`: compila el Live Bridge desde la fuente.

  [ejecutado] Con el package actual (`jwplc_local`), bootloader, particiones y `boot_app0` salen idénticos; la
  app difiere (429.264 frente a 427.392 B; André usó el package publicado alpha.12). Se conserva su binario.

  [no verificado] en la placa:
  - flasheo, autoconexión y FPS del Live Bridge;
  - apertura desde el Arduino IDE.

  Ambas cosas requieren regenerar la app de escritorio, porque cambiaron `electron/main.js` y `preload.cjs`.

  Pendiente de limpieza (no se tocó sin acuerdo):
  - `scratch/` en la raíz (26 scripts temporales);
  - artefactos de compilación en `gates/A11_LivePreview_WebSerial/JWPLC_HMI_Live_Bridge/build/` (~44 MB). Los
    genera el hook «Export compiled binary» del package.
- 2026-10-21 — **Live Bridge en el formato de exportación del package** (observación del jefe):
  - [ejecutado] la partición grabada es `jwplc_max_app_4mb` (`jwplcbasic.build.partitions`): decodificada
    entrada por entrada y comparada con el `.csv` del package, con MD5 válido. El flasher no borra la flash
    entera, así que la NVS del PLC queda intacta;
  - `poc/firmware/jwplc.esp32.jwplcbasic/` guarda los nombres que deja «Exportar binario compilado» y su
    `flash_args`. `designer-flasher.js` lo lee como manifiesto y graba con `flash-mode/freq/size keep`, como
    la subida del package. Se retiran `manifest.json`, escrito a mano (el que había omitido `boot_app0`), y
    `firmware.merged.bin` (4 MB sin uso);
  - [ejecutado] `build_live_bridge_firmware.sh` produce la misma carpeta: el `flash_args`, el bootloader, las
    particiones y `boot_app0` salen idénticos al repo;
  - [ejecutado] en Chromium, `JWPLCHMIFlasher.loadBinaries()` lee `flash_args` y descarga las 4 partes con sus
    direcciones y encabezados. `live-bridge-flasher.test.cjs` pasa 10/10, con tests nuevos de `flash_args`,
    de la partición y de `keep`.
- 2026-10-21 — `scratch/` sale del repo y queda en `.gitignore` (`1b8e4d77`); se recupera con
  `git checkout d06e28c6 -- scratch`.
