# v2.1.0-alpha.15 — Estado

Actualizado: `2026-10-17`

## Identidad

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner
PARENT_BRANCH=v2.0.1-alpha.15-(HmiDesigner)
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
A15_4_DESIGNER_CI=PARTIAL (tests/codegen-smoke.cjs; falta arduino-cli)
A15_5_PURE_CODEGEN_TESTS=PENDING
A15_6_JWHMI_V2_MODES_HOST=PENDING
A15_7A_BAR_FINISH=PENDING (prioridad alta)
A15_7B_VALUE=PENDING (prioridad alta)
A15_7C_TEXT=PENDING (prioridad alta)
A15_7D_BOOL=PENDING (prioridad alta)
A15_7E_RAW_SHAPES_NAV_PIXEL=PENDING
A15_8_MENU=PENDING
A15_9_PHYSICAL_PARITY=PENDING
A15_10_PACKAGING_DOCS=PENDING
A15_OPENPLC_INTEGRATION_DECISION=PASS (adopta plan rev.2 del fork + correcciones C1-C5)
```

## Bloqueantes abiertos (de A15-0)

```text
B1_GETTFT_NOT_IN_API=CLOSED (A15-1)
B2_TFT_MISSING_PRIMITIVES=CLOSED_IN_DESIGNER (helpers en el header; nativo en Alpha13 opcional)
B3_TFT_A_NOT_REGENERATED_ABI=MITIGATED (fuentes JWPLC_TFT revertidas a main; fuentes -> Alpha13 desde bf0fba1)
B4_BAR_CIRCULAR_GRADIENT_HELPERS_MISSING=CLOSED_IN_DESIGNER (deshabilitado + fallback lineal; firmware -> Alpha13)
B5_DELETE_PAGE_BREAKS_REOPEN=CLOSED (A15-2)
B6_STATIC_DRAW_EVERY_TICK=CLOSED (A15-3)
B7_NO_PHYSICAL_GATES=OPEN (gate A15_1_3_Codegen_Static listo, pendiente de taller)
```

## Inspector (adopción del patrón Propuesta 2)

```text
BAR=PARTIAL (pestañas OK; quedan BAR-1..BAR-7 de A15_7_INSPECTOR_REDESIGN_SPEC)
SHAPES=PARTIAL (reutiliza bar-inspector-wrap)
VALUE=LEGACY
TEXT=LEGACY
BOOL=LEGACY
RAW_TEXT=LEGACY
NAV_BAR=LEGACY
PIXELMAP=LEGACY
MENU=NOT_EXISTS
```

## Versiones

```text
DESIGNER_UI=Alpha11
ELECTRON_PACKAGE=0.11.0-alpha.11
JWPLC_TFT=0.1.0 (source = main, alineado con el .a publicado)
DESIGNER_FONTS=sólo GLCD (FIRMWARE_SUPPORTS_FONTS=false)
PROJECT_FORMAT=JWPLC_HMI_PROJECT v1
VPP_OPENPLC=2.1.0-alpha.29 (rama v2.1.0-alpha.12/feature/openplc-engineering-closure)
```

## Insumos externos pendientes

```text
BOTONERA=6_TECLAS (confirmado; matriz 2x3)
OPENPLC_DEBUG_SET_FORCING0_SEMANTICS=PENDIENTE_F0 (Alpha14)
MENU_MAX_DEPTH=3 (MVP)
MENU_MVP_NODES=SUBMENU, VIEW, EDIT_NUM, EDIT_BOOL
MENU_NODE_COUNT_LIMIT=A_MEDIR_EN_A15_8
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
  downloads.arduino.cc denegados).
