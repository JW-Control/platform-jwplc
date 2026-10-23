# Inicio de chat — JWPLC Alpha15 — HMI Designer (2026-10-17)

Reemplaza a `JWPLC_NEW_CHAT_START_ALPHA15_20261016.md`. Sirve para Claude, GPT/Codex o
cualquier agente que retome el trabajo.

Leer en orden:

```text
CLAUDE.md
tools/jwplc-hmi-designer/CLAUDE.md
docs/v2.1.0-alpha.15/ALPHA15_STATUS.md
docs/v2.1.0-alpha.15/ALPHA15_PLAN.md
docs/v2.1.0-alpha.15/A15_7_INSPECTOR_CLOSE_20261017.md
docs/v2.1.0-alpha.15/A15_8_MENU_20261017.md
docs/v2.1.0-alpha.15/A15_5_PURE_CODEGEN_TESTS_20261017.md
```

Estado:

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner   (André y agentes; git pull antes de empezar)
CLOSED=A15-0, A15-1, A15-2, A15-3, A15-4 (compile ESP32), A15-7A..7D, A15-8 (host + compile)
PARTIAL=A15-5 (golden + núcleo de campos/MENU; estática/NAV/PixelMap aún por capas)
PENDING=A15-6 (.jwhmi v2 + modo Arduino/OpenPLC), A15-7E (RAW/figuras/NAV/PixelMap), A15-9 (físico), A15-10
FIRMWARE=JWPLC_TFT 0.3.0 · JWPLC_Display 1.1.0 (MENU) — ambos .a regenerados con ABI_COMPAT=PASS
PHYSICAL_GATES_READY=gates/A15_1_3_Codegen_Static, gates/A15_8_Menu, gates/A15_8_Menu_Designer
```

Primer trabajo del nuevo chat:

0. `git pull origin v2.1.0-alpha.15-HmiDesigner`.
1. `node tools/jwplc-hmi-designer/tests/run-all.cjs` (todo PASS; sin g++ se omiten 2).
2. Según prioridad del jefe:
   - **A15-9**: recoger resultados de taller de los gates físicos y corregir diferencias;
   - **A15-6**: `.jwhmi` v2 con `link.mode` y selector de variable PLC en la pestaña "Datos";
   - **A15-5 resto**: mover estática, NAV_BAR y PixelMap al núcleo puro, siempre con `codegen-golden.cjs` en verde.
3. Documentar cada gate en `docs/v2.1.0-alpha.15/A15_<n>_*.md` y actualizar STATUS.

Para André (pendientes de UI detectados):

```text
NAV-4=la opción "Mediana 7x9" de NAV_BAR dibuja GLCD 5x7 en la TFT (renombrar o quitar)
NAV-5=en la barra vertical, "1:PRNC" desborda una pestaña de 20 px (igual en lienzo y TFT)
NEW_OBJECT_LAYER=un objeto nuevo queda detrás de los existentes (layerOrder)
MENU_UI=probar el inspector del árbol con proyectos grandes (64 nodos)
```

Herramientas:

```text
tools/alpha15/regenerate_libJWPLC_TFT.sh setup|build|compile   toolchain ESP32 en Linux/nube
tools/alpha15/regenerate_libJWPLC_Display.sh build [--install] .a del Display + chequeo ABI
tools/jwplc-hmi-designer/tests/run-all.cjs                     todos los tests
```

Reglas críticas:

```text
SPI_BUS=COMPARTIDO -> nada estático en cada tick; el MENU no toca la TFT fuera del refresh
TFT_ESPI_PUBLICO=PROHIBIDO
PRECOMPILED_FULL -> header y .a siempre alineados (regenerar con los scripts de tools/alpha15)
DISPLAY_CONSUME_LATCHES_APP=PROHIBIDO salvo declaración explícita (MENU en PAGE_CONTENT)
OPENPLC_EN_AUTOLOAD=PROHIBIDO
CODEGEN -> sólo llamadas que existen en la API real; golden en verde
PINTOR_MENU == JWPLC_UI_Menu.cpp (menu-smoke compara píxel a píxel)
COMMITS -> mensajes descriptivos
```
