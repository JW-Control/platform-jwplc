# Inicio de chat — JWPLC Alpha15 — HMI Designer

Sirve para Claude, GPT/Codex o cualquier agente que retome el trabajo.

Leer en orden:

```text
CLAUDE.md
tools/jwplc-hmi-designer/CLAUDE.md
docs/v2.1.0-alpha.15/ALPHA15_STATUS.md
docs/v2.1.0-alpha.15/ALPHA15_PLAN.md
docs/v2.1.0-alpha.15/A15_0_HMI_DESIGNER_AUDIT_20261016.md
```

Estado:

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner
PARENT_BRANCH=v2.0.1-alpha.15-(HmiDesigner)   (no modificar)
BASE_SHA=cc66d69aa1eca461b7111c856293e5f1f6e103b4
CURRENT_GATE=A15-1_CODEGEN_COMPILE
CODE_CHANGED_IN_ALPHA15=NO
```

Primer trabajo del nuevo chat:

1. A15-1: sustituir `JWPLC_Display.getTFT()->` por `JWPLC_Display.tft().` en
   `poc/designer-codegen.js`.
2. A15-1: decidir para ELLIPSE/TRIANGLE/POLYGON y BAR circular/degradado:
   implementar en firmware o deshabilitar en la UI hasta tenerlo.
3. A15-2: renumerar ids de página al borrar (`app.js:deletePage`) y unificar
   la configuración de NAV_BAR.
4. Documentar cada gate en `docs/v2.1.0-alpha.15/A15_<n>_*.md` y actualizar STATUS.

Reglas críticas:

```text
SPI_BUS=COMPARTIDO -> nada estático en cada tick
TFT_ESPI_PUBLICO=PROHIBIDO
PRECOMPILED_FULL -> header y .a siempre alineados
DISPLAY_CONSUME_LATCHES_APP=PROHIBIDO
OPENPLC_EN_AUTOLOAD=PROHIBIDO
CODEGEN -> sólo llamadas que existen en la API real
COMMITS -> mensajes descriptivos
```
