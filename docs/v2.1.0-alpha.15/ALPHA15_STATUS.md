# v2.1.0-alpha.15 — Estado

Actualizado: `2026-10-16`

## Identidad

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner
PARENT_BRANCH=v2.0.1-alpha.15-(HmiDesigner)
BASE_SHA=cc66d69aa1eca461b7111c856293e5f1f6e103b4
OWNER_ALPHA15=André (HMI Designer)
ALPHA15_STATUS=PLANNING
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
A15_1_CODEGEN_COMPILE=PENDING
A15_2_PERSISTENCE_PAGES=PENDING
A15_3_STATIC_OUT_OF_TICK=PENDING
A15_4_DESIGNER_CI=PENDING
A15_5_PURE_CODEGEN_TESTS=PENDING
A15_6_JWHMI_V2_MODES_HOST=PENDING
A15_7_INSPECTOR_VISUAL=PENDING
A15_8_MENU=PENDING
A15_9_PHYSICAL_PARITY=PENDING
A15_10_PACKAGING_DOCS=PENDING
A15_OPENPLC_INTEGRATION_DECISION=PASS (adopta plan rev.2 del fork + correcciones C1-C5)
```

## Bloqueantes abiertos (de A15-0)

```text
B1_GETTFT_NOT_IN_API=OPEN
B2_TFT_MISSING_PRIMITIVES=OPEN
B3_TFT_A_NOT_REGENERATED_ABI=OPEN (depende de Alpha13)
B4_BAR_CIRCULAR_GRADIENT_HELPERS_MISSING=OPEN
B5_DELETE_PAGE_BREAKS_REOPEN=OPEN
B6_STATIC_DRAW_EVERY_TICK=OPEN
B7_NO_PHYSICAL_GATES=OPEN
```

## Inspector (adopción del patrón Propuesta 2)

```text
BAR=ADOPTED (checklist de la spec sin validar)
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
JWPLC_TFT=0.1.0 (source con fuentes, .a sin regenerar)
PROJECT_FORMAT=JWPLC_HMI_PROJECT v1
VPP_OPENPLC=2.1.0-alpha.29 (rama v2.1.0-alpha.12/feature/openplc-engineering-closure)
```

## Insumos externos pendientes

```text
BOTONERA_7A_TECLA_F1=PENDIENTE_CONFIRMAR (package define 6)
OPENPLC_DEBUG_SET_FORCING0_SEMANTICS=PENDIENTE_F0 (Alpha14)
MENU_LIMITES_ARBOL=PENDIENTE
```

## Registro

- 2026-10-16 — Rama creada desde `cc66d69`. Auditoría A15-0, `CLAUDE.md`
  (raíz y `tools/jwplc-hmi-designer/`) y plan inicial. Sin cambios de código.
- 2026-10-16 — Revisión del fork `openplc-editor` (`develop/alpha12-backplane-closure`)
  y del VPP. Se adopta el plan HMI rev. 2 de Kenyi con correcciones
  (`A15_OPENPLC_INTEGRATION_DECISION_20261016.md`). MENU definido como objeto
  de página para configurar variables. Organización de alphas 13/14/15 fijada.
