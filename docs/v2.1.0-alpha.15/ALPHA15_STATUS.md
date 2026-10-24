# v2.1.0-alpha.15 — Estado

Actualizado: `2026-10-24`

## Identidad

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner
PARENT_BRANCH=v2.0.1-alpha.15-(HmiDesigner) (congelada; último commit fusionado af797d79)
TEAM_BRANCH=v2.1.0-alpha.15-HmiDesigner (André trabaja aquí desde 2026-10-17)
BASE_SHA=cc66d69aa1eca461b7111c856293e5f1f6e103b4
OWNER_ALPHA15=André (HMI Designer)
ALPHA15_STATUS=DEV_CLOSED (2026-10-19; cierre y transferencia: A15_CIERRE_Y_TRANSFERENCIA_20261019.md)
ALPHA15_RELEASE=2.1.0-alpha.14 (por orden de publicación; ver «Organización de alphas»)
ALPHA15_RELEASE_PR=#157 (borrador; rama v2.1.0-alpha.14/HmiDesigner/main desde release/v2.1.x; sustituye a #141)
ALPHA14_INTEGRATION=FASE_1_CLOSED + FASE_3_FREEZE_DONE (Display .a ced1f93 y TFT .a 161e406: ABI_COMPAT=PASS, precompiled=full)
ALPHA14_CLOSURE_CHECK=PASS (24/10: 83/83 sketches en Basic y Basic Core, Designer 16/16, .a reconstruidos sin cambios de ABI)
ALPHA14_FEATURES_MERGED=#164 828eaf3, #158 d782cca, #159 8ddc06d, #160 a00c268, #161 69478cf, #162 a0d3ba7; #163 (cierre) el último
ALPHA14_FASE2_FISICA=PASS_PHYSICAL (24/10, gates/A14_F2_Combinado: Master COM6 + Slave COM3, TCP+RTU 10 min, LIVE con Live Bridge 440 752 B)
ALPHA14_NEXT=benchmark en la PC del jefe -> #157 fuera de borrador y merge con autorización
ALPHA14_NOTES=docs/v2.1.0-alpha.14/PRE_RELEASE.md (final salvo prueba física y benchmark de PC; marcador del README en 2.1.0-alpha.14)
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
JWPLC_TFT=0.3.0 (elipses/triángulos + FreeFonts con setFont/drawString; .a regenerado, ABI compatible)
JWPLC_DISPLAY=1.1.0 (MENU, formatos VALUE, BOOL piloto/por estado, cp437, pestañas) · PRECOMPILED (.a regenerado el 24/10, ABI_COMPAT=PASS, sha256 20d23abe…)
JWPLC_GLOBALPERIPHERALS=1.1.0 (JWPLC_Buzzer)
JWPLC_RS485/JWPLC_MODBUSRTU=1.1.0 (envoltorios de JW_RS485/JW_ModbusRTU 0.1.1; ModbusRTU en modo fuente)
PACKAGE=2.1.0-alpha.14 (marcador del README pendiente de autorización)
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

## Registro (resumen)

Registro detallado hasta el 23/10/2026: [history/ALPHA15_REGISTRO_20261016_20261023.md](history/ALPHA15_REGISTRO_20261016_20261023.md).

- 2026-10-16/17 — Auditoría A15-0; A15-1..3 (codegen, páginas, estática fuera del tick); `JWPLC_TFT` 0.2.0;
  inspector Propuesta 2 (A15-7A..7D); MENU MVP (A15-8); tests golden/núcleo (A15-5).
- 2026-10-18 — VPP alpha.12 de Kenyi fusionado; gate F0; revisiones 3 y 4 del jefe; plan industrial; I1 formatos,
  I2 reloj RTC, I3 tildes cp437, Apariencia A1, `JWPLC_Buzzer`; flujo con el sketch.
- 2026-10-19 — Gates físicos en PASS (A15_9); wake key y pestañas; **cierre de desarrollo**; se publica como 2.1.0-alpha.14.
- 2026-10-20/21 — Revisión final de André; flasher del Live Bridge en un clic con `flash_args` del package; apertura desde
  el Arduino IDE; `scratch/` fuera del repo.
- 2026-10-22/23 — Integración sobre Alpha13 (PR #141, fase 1); LIVE con `pushImage` y protocolo v2; `libJWPLC_TFT.a`
  regenerado con DISPON diferido (`161e406`); herramientas PIXEL, guardado, recientes y CLI `jwhmi` (#143);
  borradores de publicación (README, `PRE_RELEASE.md`, README de librerías).
- 2026-10-24 — Cierre en la nube (83/83 sketches por placa, Designer 16/16, `.a` sin cambios de ABI); ramas al esquema
  `v2.1.0-alpha.14/HmiDesigner/main` + `…/feature/<tema>`; #164, #158–#163 y #165 fusionadas; prueba física PASS.
