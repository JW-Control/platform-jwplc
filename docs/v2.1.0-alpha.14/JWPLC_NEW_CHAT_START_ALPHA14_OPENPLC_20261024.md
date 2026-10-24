# Traspaso Alpha14 · HMI Designer dentro de OpenPLC (24/10/2026)

Documento para retomar en otro chat la integración **OpenPLC ↔ JWPLC HMI Designer** (Kenyi).
Leer antes: `CLAUDE.md` de este repo (§5 autoría), este documento, el contrato
`A14_F3_CONTRATO_HMI_OPENPLC_20261017.md` y el informe `A14_HMI_V1_OPENPLC_20261017.md`.

```text
LINEA=v2.1.0-alpha.14-OpenPLC-HMI (platform-jwplc) + develop/alpha14-openplc-hmi-v1 (openplc-editor)
PUBLICA_COMO=2.1.0-alpha.15 (después de 2.1.0-alpha.14 = HMI Designer de André)
VPP_VIGENTE=2.1.0-alpha.32 (OpenPlc/jwplc-basic-openplc-2.1.0-alpha.32.jwcontrol-signed.vpp)
F0_BOOL=PASS · F0b_VALORES=PASS · F0c_ESCRITURA=PASS · F0d_COSTO_HMI=PASS (alpha.32)
F0d_JITTER_TFT=ABIERTO (package) · F3_DESIGNER_EN_OPENPLC=IMPLEMENTADO, cadena completa sin probar
PUSH=al día en los dos repos (24/10)
```

## 1. Ramas y commits

| Repo · rama | Commits de esta línea (más nuevo arriba) |
|---|---|
| openplc-editor · `develop/alpha14-openplc-hmi-v1` (desde `develop/alpha12-backplane-closure` @ `1f12dc549`) | `1ff71450f` docs F3 · `f407f32fe` feat F3 · `729583c91` docs F0d · `1b75bc75c` docs plan/v1 · `e7ac83acf` feat HMI v1 (F0–F0c) · `7f0405eac` fix Radix |
| platform-jwplc · `v2.1.0-alpha.14-OpenPLC-HMI` (desde `v2.1.0-alpha.15-HmiDesigner` @ `5f7029c5`) | `4a6acdb1` (jefe, regla de autoría) · `1fabab8c` docs contrato/F3 · `d04db07b` feat modo OpenPLC del Designer · `87258cad` docs F0d · `1eaa3be2` VPP alpha.32 · `2bf0250f` docs F0b/F0c · `5e8c8d98` VPP alpha.30 |

Otras ramas que importan (platform-jwplc):

- `v2.1.0-alpha.15-HmiDesigner`: línea HMI de André.
- `v2.1.0-alpha.14/feature/hmi-designer`: integración de la línea HMI sobre
  `release/v2.1.x` (PR #141, borrador). Traspaso del jefe:
  `docs/v2.1.0-alpha.15/JWPLC_NEW_CHAT_START_ALPHA14_20261023.md`, en esa rama.

## 2. Qué funciona hoy

**Firmware (VPP alpha.32, HAL del maestro):**

- toma `jwplc_hmi.h` que escribe el editor (`__has_include`);
- `jwplcHMISetup()` en `hardwareInit()`;
- en cada scan sólo `jwplcHMIBindingsSnapshot()`; los setters y el dibujo
  quedan en la tarea del Display;
- pestaña de configuración "Pantalla HMI" con la casilla "Medir tiempos de
  scan", que publica 8 `%MW` desde `%MW10`.

**Editor:**

- `hmi/` del proyecto: `JWPLC_HMI_Generated.h`, `JWPLC_HMI.jwhmi`
  (`plcBinding`) y `bindings.json` (respaldo).
- Al compilar genera `src/jwplc_hmi.h` con los enlaces:
  - `bool` ← BOOL en `%IX`/`%QX`;
  - `float`/`int32_t` ← `%IW`/`%QW`/`%MW`, `%MD`, `%ID`/`%QD`;
  - `RW` en `%MW`/`%MD`.
- Cualquier problema es error de compilación con el motivo.
- Botón **Abrir HMI Designer** en la configuración del dispositivo: ventana
  propia, servidor local de sólo lectura, puente `JWPLCHMINative` (4
  funciones) y `JWPLCHMIHost`.
- "Actualizar HMI" y el cierre de la ventana guardan `JWPLC_HMI.jwhmi`.

**Designer (esta rama):**

- `designer-openplc-host.js`: en modo `openplc`, selector **Variable PLC** en
  la pestaña Datos (BOOL, VALUE, BAR y nodos del MENU);
- nombre C++ automático y `plcBinding` en el objeto.

**Resultados en placa (proyecto `OpenPlc/HMIProbe`, tarea `T#20ms`):**

| Gate | Resultado |
|---|---|
| F0 BOOL ↔ relé | PASS (también con forzado) |
| F0b VALUE/BAR ← `%MW0` | PASS |
| F0c MENU → `%MW1` (Limite) | PASS: OK escribe, UP/DOWN ±1, LEFT/RIGHT ±10 |
| F0d costo HMI | alpha.31 ≈ 167 µs (FAIL) → alpha.32 5–15 µs prom., ≤ 27 µs máx./s (PASS) |
| F0d periodo | IDLE/Valores ≤ 28 ms (igual sin HMI), MENU ≤ 31 ms, **cambio de página 53–65 ms** |
| F3 (primera versión) | abrir, editar, Actualizar HMI, compilar y correr: OK; las 2 fallas (selector y guardado) corregidas después, sin volver a probar en la app |

Detalle: gates `tools/jwplc-hmi-designer/gates/A14_F0b_*`, `A14_F0c_*`, `A14_F0d_*` e informe A14.

## 3. Lo que falta, en orden

1. **Autorización del jefe para traer `v2.1.0-alpha.14/feature/hmi-designer` a esta línea.**
   El merge simulado (`git merge-tree`) da sólo 2 conflictos:
   - `.gitattributes`: conservar las dos líneas nuevas;
   - `tools/jwplc-hmi-designer/poc/desktop.html`: tomar la lista de scripts de
     André y volver a agregar `designer-openplc-host.js`.

   La integración **no toca el VPP**: alpha.32 se mantiene.

   Luego verificar que `designer-openplc-host.js` y el getter `selectedNode` de
   `designer-menu.js` sigan funcionando con el Designer nuevo. Prueba en
   Chromium: script de la sección 5.
2. **Ampliar el anfitrión de OpenPLC al puente nuevo del Designer** (contrato
   1.1). El Designer de la integración llama unas 20 funciones de
   `JWPLCHMINative` (ver `tools/jwplc-hmi-designer/electron/preload.cjs`), y
   `readProject` se llama sin `?.`. Propuesta de cómo responde cada una en
   OpenPLC:

   | Función | En OpenPLC |
   |---|---|
   | `readProject` | Contenido de `hmi/JWPLC_HMI.jwhmi` (o el único `.jwhmi`) |
   | `chooseSaveTarget` / `applySaveTarget` | Siempre `hmi/` del proyecto; sin cuadro de diálogo |
   | `writeGuide` | Escribir la guía en `hmi/` o devolver ok sin escribir (decidir) |
   | `registerCloseGuard` / `confirmClose` | Unir con el guardado al cerrar que ya existe en el editor |
   | `createSketch`, `unlinkSketch`, `setLinkedSketch` | No aplica: `{ ok: false, error }` claro |
   | `listRecentProjects`, `openRecentProject`, `clearRecentProjects`, `openProjectFile` | Lista vacía / no aplica |
   | `openSketchIno`, `openSketchFolder`, `onSketchOpened` | Abrir la carpeta del proyecto con `shell` / no aplica |
   | `registerSerialPortPicker`, `chooseSerialPort` | Live Bridge por WebSerial: manejar `select-serial-port` en la sesión de la ventana, o desactivar Live en OpenPLC (el COM puede estar ocupado por el debugger) |

   El código está en openplc-editor: `src/main/modules/hmi-designer/index.ts`
   y `src/main/modules/preload/hmi-designer-preload.ts`. Anotar la decisión
   en el contrato.
3. **Probar la cadena completa en la app y en la placa** (`npm run dev`, proyecto HMIProbe):
   1. Abrir HMI Designer y elegir en Datos: Relé → RELE, Contador y Nivel →
      CONTADOR, MENU Limite → LIMITE, Consigna → CONSIGNA, Lleno → LLENO.
   2. Pulsar "Actualizar HMI": debe aparecer `hmi/JWPLC_HMI.jwhmi`.
   3. **Borrar `hmi/bindings.json`** (los nombres C++ cambian:
      `rele_q0_0` → `rele`) y el `.jwhmi` viejo `A14_F0c_OpenPLC_Escritura.jwhmi`.
   4. Compilar y subir: la placa igual que en F0c.
   5. Cerrar y reabrir el Designer: debe mostrar la última pantalla.
4. **Revisión del contrato y de `designer-openplc-host.js` con el jefe y André**
   (A15-6): André decide si lo integra tal cual o lo reemplaza; el contrato se
   mantiene.
5. Después:
   - el Designer dentro del VPP firmado; hoy se toma de
     `JWPLC_HMI_DESIGNER_DIR` o del checkout vecino
     `../platform-jwplc/tools/jwplc-hmi-designer/poc`;
   - pestaña en vez de ventana (F4);
   - variables sin dirección (`openplc_debug_read`);
   - `EDIT_BOOL` en RW (hace falta `%MX` en el runtime);
   - TEXT.
6. **Del package (Alpha13/jefe):** el redibujo de página en `jwplcSystemTask`
   (mismo core y mayor prioridad que `loopTask`) atrasa un scan hasta ≈ 45 ms.
   Opciones: otro core o menor prioridad, o redibujo por tramos.

## 4. Reglas que aplican

- **Autoría (`CLAUDE.md` §5, desde 22/10):**
  - commits como `JW-Control <jw.control.peru@gmail.com>`;
  - sin `Co-Authored-By`, `Claude-Session` ni pie "Generated with Claude Code".
  - Los commits de esta línea anteriores a la regla llevan `Co-Authored-By`.
  - Para openplc-editor falta confirmar si se aplica igual; preguntar a Kenyi.
- Merges a ramas compartidas, Releases, flash y borrado de ramas: sólo con autorización del propietario.
- El VPP se firma con `openplc-editor-installers/v4.2.7/tools/sign-vpp-package.mjs`
  y la llave `jwcontrol-2026` (`OpenPlc/jwcontrol-2026-private.pem`). La
  versión sube en cada cambio del payload. Archivos nuevos del VPP: regla de
  EOL en `.gitattributes`.
- openplc-editor: 100 % de cobertura en `src/backend/shared`,
  `src/frontend/utils`, `src/frontend/store/slices` y
  `src/middleware/adapters/editor`.
- Después de cualquier `npm install` en el editor:
  - `npm run setup:strucpp` (npm borra `node_modules/strucpp`);
  - `npm run build:dll`.
  - Paquetes nuevos: usar `--before=<hace 2 semanas>`.

## 5. Entorno y herramientas (PC de Kenyi)

- Junction `%LOCALAPPDATA%/Arduino15/packages/jwplc_local/hardware/esp32/2.1.0-dev`
  → `OpenPlc/platform-jwplc/JWPLC/2.1.0`; sigue la rama que tenga el checkout.
  En OpenPLC: *Package Source* = **Local Development**.
- Proyecto de prueba `OpenPlc/HMIProbe`:
  - `hmi/` con la pantalla F0c (`A14_F0c_OpenPLC_Escritura.jwhmi`,
    `JWPLC_HMI_Generated.h`, `bindings.json`);
  - respaldos en `hmi/F0_bool` y `hmi/F0b_valores`;
  - variables `PULSADOR`/`RELE`/`RESET` por alias, `LLENO %QX0.1`,
    `CONTADOR %MW0`, `LIMITE %MW1 := 10`, `CONSIGNA %MD0`, `CTU0`, y
    `T_* %MW10..17` para F0d.
- Compilar sin la app: `openplc-editor/resources/bin/win32/x64/arduino-cli.exe`
  sobre una copia de un build de OpenPLC (`PruebasBlackplane/build/JWPLC BASIC
  [2.0.0]`), reemplazando `src/arduino.cpp` por el HAL y agregando los archivos
  de `generateJwplcHmi()`.
- Probar el Designer sin la app: Playwright de `openplc-editor/node_modules`
  (`channel: 'chrome'`), sirviendo `tools/jwplc-hmi-designer/poc`. Para el
  modo OpenPLC, simular el anfitrión con `page.addInitScript` definiendo
  `window.JWPLCHMIHost = { version: 1, mode: 'openplc', listVariables, ... }`.
