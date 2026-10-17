# Tests del HMI Designer

## `codegen-smoke.cjs`

Verifica sin hardware que el header generado compila contra el package:

```bash
node tools/jwplc-hmi-designer/tests/codegen-smoke.cjs [carpeta-salida]
```

Qué hace:

1. sirve `poc/` en `127.0.0.1` y abre `desktop.html` en Chromium headless (Playwright);
2. carga proyectos de prueba (3 páginas + NAV_BAR + todas las figuras + BAR circular/degradado
   + fuente no GLCD; borrado de página intermedia; proyecto legado con ids `[0, 2]`;
   proyecto sólo con figuras) y genera `JWPLC_HMI_Generated.h`;
3. compila cada header con `g++ -std=gnu++17 -fsyntax-only -Wall` contra los headers
   **reales** de `JWPLC_Display`, `JWPLC_UI` y `JWPLC_TFT`;
4. comprueba con `nm -C` que cada llamada `tft.*` / `JWPLC_Display.*` emitida exista en
   `libJWPLC_Display.a` / `libJWPLC_TFT.a`.

Requisitos: Node 18+, Playwright (local o global), Chromium, `g++`, `nm`, `python3`.

`host-stub/` sólo reemplaza `Arduino.h` (clase `Print` mínima) y los periféricos que
`JWPLC_Display.h` incluye pero que el header generado no usa. No sustituye la compilación
real con `arduino-cli` para ESP32 (gate A15-4) ni la prueba en hardware (A15-9).
