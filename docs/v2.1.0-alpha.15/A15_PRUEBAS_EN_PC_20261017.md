# Alpha15 — Cómo probar en la PC (Windows)

Fecha: `2026-10-17` · rama `v2.1.0-alpha.15-HmiDesigner`

## 1. Traer la rama

```powershell
cd C:\ruta\a\platform-jwplc
git fetch origin
git switch v2.1.0-alpha.15-HmiDesigner
git pull origin v2.1.0-alpha.15-HmiDesigner
```

## 2. Abrir el Designer

```powershell
powershell -ExecutionPolicy Bypass -File tools\jwplc-hmi-designer\Start-JWPLC-HMI-Designer.ps1
```

Abre Edge/Chrome en modo app sobre `http://127.0.0.1:8765/desktop.html`. Si ves la versión
anterior, recarga con `Ctrl+F5`: los scripts cambiaron de `?v=` y la caché del service worker
pasó a `a15-codegen-core-v10`.

Qué revisar:

| Qué | Dónde | Esperado |
|---|---|---|
| Inspector TEXT / VALUE / BOOL | Crear cada tipo desde la barra | Pestañas Diseño (Forma/Contenido/Apariencia/Ubicación), Datos, Técnico; tarjetas y chips, sin selects; valor de prueba sólo en Datos |
| BAR | Crear una barra | Vertical, estilos, redondeo, borde, degradado, circular y "% de carga" deshabilitados ("Requiere firmware"); grosor fijo 12 px; la matriz sólo permite ↖ (apilado) y ← (en línea) |
| MENU | Desplegable de variables → "Menú de configuración (MENU)" | Árbol de ejemplo en el lienzo; pestaña "Árbol y datos" para agregar/subir/bajar/eliminar ítems; "+ Submenú" se deshabilita en el nivel 3 |
| Gate del MENU | Abrir `tools\jwplc-hmi-designer\gates\A15_8_Menu_Designer\A15_8_Menu_Designer.jwhmi` | Mismo menú que `gates\A15_8_Menu\menu_root.png` |
| Header | Pestaña Contrato / Generar | `JWPLC_Display.setMenus(...)` dentro de `jwplcHMISetup()` |

## 3. Tests automáticos

Una sola vez:

```powershell
npm install -g playwright
npx playwright install chromium
```

Cada vez:

```powershell
node tools\jwplc-hmi-designer\tests\run-all.cjs
```

Sin `g++` se omiten `menu-host-sim` y `codegen-smoke` (compilan C++); el resto debe dar PASS.
Las capturas quedan en la carpeta temporal que imprime cada test.

## 4. En la placa (gates físicos)

El MENU y las fuentes necesitan los `.a` nuevos de esta rama (`JWPLC_Display` 1.1.0,
`JWPLC_TFT` 0.3.0). Con el package local `jwplc_local` apuntando al repo (el que se usó en
Alpha14) basta con el `git pull`. Para comprobarlo:

```powershell
Get-Item "$env:LOCALAPPDATA\Arduino15\packages\jwplc_local\hardware\esp32\2.1.0-dev" |
  Select-Object LinkType, Target
```

Si `Target` no es `...\platform-jwplc\JWPLC\2.1.0`, el package instalado no tiene el MENU:
compila con la placa de `jwplc_local` o crea el junction antes de probar.

En Arduino IDE, con la placa **JWPLC Basic** de `jwplc_local`:

| Gate | Sketch | Guía |
|---|---|---|
| Estática fuera del tick | `gates\A15_1_3_Codegen_Static\A15_1_3_Codegen_Static\` | `gates\A15_1_3_Codegen_Static\README.md` (D1-D5, T1-T8) |
| MENU a mano | `gates\A15_8_Menu\A15_8_Menu.ino` | `gates\A15_8_Menu\README.md` (M1-M13) |
| MENU del Designer | `gates\A15_8_Menu_Designer\A15_8_Menu_Designer.ino` | `gates\A15_8_Menu_Designer\README.md` |

Monitor serie a 115200. Anotar resultados y fotos en `docs/v2.1.0-alpha.15/` (gate A15-9).
