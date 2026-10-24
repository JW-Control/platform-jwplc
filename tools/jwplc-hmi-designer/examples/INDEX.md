# Índice: `examples/`

Proyectos en forma corta para agentes. Cada uno valida sin errores ni avisos y su header compila
(`tests/agent-cli.test.cjs`). Probar: `node tools/jwplc-hmi-designer/cli/jwhmi.cjs preview <archivo>`.

| Archivo | Muestra |
|---|---|
| `01_campos.json` | TEXT, VALUE (número, miles, duración, fecha/hora), BOOL (texto con colores y piloto), BAR. |
| `02_figuras_texto.json` | RECT, ELLIPSE, TRIANGLE, POLYGON, LINE, RAW_TEXT con fuentes y una capa PIXEL. |
| `03_menu.json` | MENU con SUBMENU anidados (`children`), EDIT_NUM float/int32, EDIT_BOOL, GO_IDLE e IDLE con timeout. |
| `04_paginas_navegacion.json` | Tres páginas con nombre corto y color, y NAV_BAR. |
| `05_estacion_bombeo.json` | HMI completo: proceso, alarmas y ajustes con MENU, barra de título, reloj y NAV_BAR. |
| `06_iconos.json` | ICON del catálogo: tanque y bomba de 64 px, válvula y motor de 48, pilotos de 24 con etiqueta INLINE, `uint8_t` con 3-4 estados y `bool` con 2. |
