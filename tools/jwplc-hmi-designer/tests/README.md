# Tests del HMI Designer

Todo se corre desde la raíz del repo. Para ejecutar todos y ver un resumen:

```bash
node tools/jwplc-hmi-designer/tests/run-all.cjs
```

Sin `g++` (por ejemplo Windows sin MSYS2) se omiten los pasos que compilan C++; el resto corre igual.

| Test | Qué verifica | Requiere |
|---|---|---|
| `codegen-core.test.cjs` | Núcleo puro (`poc/designer-codegen-core.js`) y codegen/validación del MENU (`poc/designer-menu.js`) en node, sin navegador. Con g++: compila el resultado y compara el pintor del MENU con el firmware simulado. | node (+ g++ opcional) |
| `codegen-golden.cjs` | El header generado para cada proyecto de `fixtures.cjs` es idéntico a `golden/<n>.h`. Si el cambio es intencional, `--update` y revisar el diff en git. | Playwright |
| `inspector-smoke.cjs` | Inspector TEXT/VALUE/BOOL operado como usuario (tarjetas, chips, paleta, páginas, undo). BAR: opciones sin firmware deshabilitadas y geometría del lienzo == `computeFieldGeometry()` del firmware (16 casos). | Playwright |
| `menu-smoke.cjs` | MENU: herramienta, inspector del árbol, undo, guardar/abrir, profundidad 3, header. Con g++: lienzo == framebuffer del firmware píxel a píxel y el header compila. | Playwright (+ g++, nm) |
| `icon-smoke.cjs` | ICON: panel Iconos (categorías, búsqueda, tamaño, clic y arrastre), inspector (estados, etiqueta, tamaño, tipo de variable, undo), header, guardar/abrir con `iconAssets` (incluye un icono fuera del catálogo) y límite de 32 campos. Con g++: el header compila y lienzo == framebuffer del firmware (`icon-host/header_sim.cpp`). | Playwright (+ g++) |
| `menu-host-sim.cjs` | Compila el `JWPLC_UI_Menu.cpp` real con g++ contra stubs (`menu-host/stub`) y recorre un guion de teclas (39 verificaciones). Guarda capturas PNG del framebuffer. | g++ |
| `icon-catalog.test.cjs` | Catálogo de iconos al día con `icons/library.cjs` (`build-catalog.cjs --check`) y cada entrada dentro de los límites del firmware (tamaños, estados, 16 colores, largo de las imágenes). | node |
| `icon-host-sim.cjs` | Compila el `JWPLC_UI.cpp` real con g++ contra stubs (`icon-host/stub`) y verifica el ICON: decodificación 4 bpp, transparente, estados, etiqueta, 64 px y que un cambio de estado sólo repinta el icono. Guarda una captura PNG. | g++ |
| `agent-cli.test.cjs` | CLI para agentes (`cli/jwhmi.cjs`): valores por defecto de `cli/lib/spec.cjs` == los que crea el Designer, esquema y referencia al día, normalización y errores típicos, header de la CLI == golden, ejemplos de `examples/` sin avisos y compilando, `build` en un sketch. | Playwright (+ g++ opcional) |
| `codegen-smoke.cjs` | Headers de varios proyectos compilan con `g++ -fsyntax-only` contra los headers reales y cada llamada existe en los `.a` (`nm`). | Playwright, g++, nm, python3 |

`host-stub/` y `menu-host/stub/` sólo reemplazan `Arduino.h`, la botonera, la TFT y los
periféricos que no intervienen. No sustituyen la compilación real para ESP32
(`tools/alpha15/regenerate_libJWPLC_TFT.sh compile <sketch>`) ni la prueba en la placa.

## Windows (PowerShell)

Una sola vez:

```powershell
npm install -g playwright
npx playwright install chromium
```

Cada vez:

```powershell
git switch v2.1.0-alpha.15-HmiDesigner
git pull origin v2.1.0-alpha.15-HmiDesigner
node tools\jwplc-hmi-designer\tests\run-all.cjs
```

Si `npm root -g` no encuentra Playwright, instálalo en el repo (`npm install playwright`)
y vuelve a correr el comando.

## Linux / nube

Playwright global y Chromium en `/opt/pw-browsers` (ya configurado en la nube). Para la
compilación ESP32 real ver `CLAUDE.md` §6.
