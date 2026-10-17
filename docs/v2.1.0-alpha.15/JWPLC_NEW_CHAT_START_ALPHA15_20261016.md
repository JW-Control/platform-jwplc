# Inicio de chat — JWPLC Alpha15 — HMI Designer

Sirve para Claude, GPT/Codex o cualquier agente que retome el trabajo.

Leer en orden:

```text
CLAUDE.md
tools/jwplc-hmi-designer/CLAUDE.md
docs/v2.1.0-alpha.15/ALPHA15_STATUS.md
docs/v2.1.0-alpha.15/ALPHA15_PLAN.md
docs/v2.1.0-alpha.15/A15_0_HMI_DESIGNER_AUDIT_20261016.md
docs/v2.1.0-alpha.15/A15_OPENPLC_INTEGRATION_DECISION_20261016.md
```

Estado:

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner
PARENT_BRANCH=v2.0.1-alpha.15-(HmiDesigner)   (congelada; fusionada hasta af797d79)
TEAM_BRANCH=v2.1.0-alpha.15-HmiDesigner     (André y agentes; hacer git pull antes de empezar)
OWNER=André (Alpha15) · OpenPLC=Kenyi (Alpha14) · package=jefe (Alpha13)
BASE_SHA=cc66d69aa1eca461b7111c856293e5f1f6e103b4
CURRENT_GATE=A15-7A_BAR_FINISH
CODE_CHANGED_IN_ALPHA15=YES (A15-1..A15-3 cerrados 2026-10-17)
```

Primer trabajo del nuevo chat:

0. `git pull origin v2.1.0-alpha.15-HmiDesigner`.
1. Correr `node tools/jwplc-hmi-designer/tests/codegen-smoke.cjs` (debe dar 20/20).
2. A15-7A..7D: cerrar BAR y rehacer TEXT/VALUE/BOOL según
   `A15_7_INSPECTOR_REDESIGN_SPEC_20261016.md`.
3. Volver a correr el smoke test tras cada cambio de codegen.
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
