# Alpha15 → Alpha14 · Ruta para Kenyi: HMI Designer dentro de OpenPLC (F0)

Fecha: `2026-10-18` · rama de partida `v2.1.0-alpha.15-HmiDesigner`

```text
ALPHA12_MERGED_INTO_ALPHA15=PASS (4e869985, VPP 2.1.0-alpha.29 -> merge 7cbd427c)
JWPLC_DISPLAY_UNIFIED=PASS_SOURCE (overlay IDLE de alpha.12 + MENU/campos de alpha.15)
F0_BOOL_RELE_COMPILE=PASS (HAL real del VPP + bloque F0, ESP32, sin OpenPLC)
F0_BOOL_RELE_OPENPLC=PENDING (Kenyi: compilar desde OpenPLC y probar en placa)
F0_BOOL_RELE_PHYSICAL=[no verificado]
```

Objetivo de la primera prueba (definido por el jefe): **un campo BOOL del HMI vinculado al
estado de un relé**. El relé `Q0_0` (`%QX0.0`) conmuta desde un programa OpenPLC, y la TFT lo
muestra en un campo diseñado en el HMI Designer. Con eso se da por cerrada la prueba inicial.

## 1. Qué trae esta rama

Merge de `v2.1.0-alpha.12/feature/openplc-engineering-closure` (Kenyi) en la rama del Designer.
No se perdió nada de alpha.12 `[ejecutado]`: los 15 archivos que tocó alpha.12 quedaron
byte a byte iguales a su rama.

- VPP 2.1.0-alpha.29 completo: HAL, plantilla del esclavo, screens, manifest, `signature.json` y `.gitattributes` de EOL.
- `JWPLC_Display` con todo junto:
  - el overlay de entradas forzadas en la IDLE (`jwplcDisplaySetInputForceOverlay`);
  - el MENU, el BOOL con texto/fondo por estado y el anclaje fijo de campos de alpha.15.
- Ejemplo `JWPLC_RemoteIO_Slave_RTU` (failsafe de 1000 ms) y `docs/v2.1.0-alpha.12/BACKPLANE_RTU_CONFIGURATION.md`.

El único conflicto fue `JWPLC_Display/src/esp32/libJWPLC_Display.a`. Durante el alpha,
`JWPLC_Display` se compila **desde fuente** (no tiene `precompiled=full`), así que ese `.a` no
se enlaza. Se conservó el de alpha.15 (`0317c3ac…`); el de alpha.12 (`0d8eca2f…`) sigue en su
rama. Al cierre se regenera uno solo desde el fuente unido:
`tools/alpha15/regenerate_libJWPLC_Display.sh build --install`.

Compilación ESP32 del package unido `[ejecutado]`:

| Sketch | Flash | RAM |
|---|---|---|
| `A15_1_3_Codegen_Static` | 446133 | 35500 |
| `A15_8_Menu` | 441965 | 36092 |
| `A15_8_Menu_Designer` | 440529 | 35980 |
| `JWPLC_RemoteIO_Slave_RTU` | 435005 | 29372 |
| F0: HAL VPP alpha.29 + bloque F0 | 422153 | 36484 |

## 2. Ruta F0 paso a paso

### 2.1 Preparar la PC

```powershell
cd C:\ruta\a\platform-jwplc
git fetch origin
git switch v2.1.0-alpha.15-HmiDesigner
git pull origin v2.1.0-alpha.15-HmiDesigner

# El package local de OpenPLC debe apuntar a este checkout
Get-Item "$env:LOCALAPPDATA\Arduino15\packages\jwplc_local\hardware\esp32\2.1.0-dev" |
  Select-Object LinkType, Target
# Target debe ser ...\platform-jwplc\JWPLC\2.1.0 (si no, recrear el junction)
```

En OpenPLC, dispositivo **JWPLC BASIC [2.0.0]** → *Package Source* = **Local Development**
(`jwplc_local:esp32:jwplcbasic`). El package publicado no tiene nada de alpha.15.

Sugerencia: trabajar en una rama propia creada desde aquí, por ejemplo
`v2.1.0-alpha.14-OpenPLC-HMI`. Así el VPP firmado no choca con el trabajo diario del Designer;
los cambios se traen con `git merge`. La decisión es del equipo.

### 2.2 Programa OpenPLC

```text
PULSADOR AT %IX0.0 : BOOL   (I0_0)
RELE     AT %QX0.0 : BOOL   (Q0_0, relé 0)
Ladder: ──[ PULSADOR ]──( RELE )──
```

También sirve el ejemplo del VPP `examples/JWPLC_Basic_Blink_Q0_0` (parpadeo sobre `Q0_0_OUT`).

### 2.3 HMI

Ya está hecho en `tools/jwplc-hmi-designer/gates/A15_F0_OpenPLC_Bool_Rele/`:

- `A15_F0_OpenPLC_Bool_Rele.jwhmi`: se abre en el Designer para verlo o cambiarlo.
- `JWPLC_HMI_Generated.h`: variable `bool rele_q0_0`; en OFF va blanco sobre rojo y en ON negro sobre verde.

Si se cambia el diseño, regenerar el header desde el Designer (pestaña Contrato → Generar) y no
editarlo a mano.

### 2.4 HAL del VPP

En `openplc-editor-installers/v4.2.7/vpp/hal/jwplcbasic.cpp`, tres cambios (detalle en el
README del gate):

1. Pegar `hal_f0_bloque.cpp` después de `#include <JWPLC_ModbusRTU.h>`, con el contenido del
   header en `// @@JWPLC_HMI_GENERATED@@`.
2. `jwplcHMIF0Begin();` como primera línea de `hardwareInit()`.
3. `jwplcHMIF0Update();` en `updateOutputBuffers()`, después del bucle de `digitalWrite` y
   antes de `jwplcPublishForcedInputs();`.

Por qué así `[visto en openplc-editor 1f12dc5]`:

- El editor copia **sólo** el archivo `hal.source` del VPP como `arduino.cpp` del sketch
  (`compiler-module.ts:1267`). Un `#include "JWPLC_HMI_Generated.h"` no resolvería, así que en
  F0 el header se pega. Que el editor escriba el header junto al HAL es F3.
- `bool_output[0][0]` es el valor que el HAL acaba de escribir al relé. Debería incluir un
  forzado del debugger `[inferido]`; F0-6 lo confirma.
- `jwplcUIUpdate()` sólo llama setters. Si `rele_q0_0` no cambió, no se redibuja nada
  (dirty refresh), así que es barato llamarlo en cada scan.
- En el firmware, la IDLE por defecto **no** despierta con teclas (`IDLE_WAKE_DISABLED`). Desde
  2026-10-18 el Designer lo configura en *Proyecto · Pantalla y arranque*, y `jwplcHMISetup()` lo
  aplica. El header del gate trae: arranca en la IDLE (con el overlay de forzadas), cualquier tecla
  entra a la página y ESC vuelve.

Después, subir la versión del VPP (2.1.0-alpha.30) y firmar con el flujo de siempre:
`openplc-editor-installers/v4.2.7/tools/build-dev-signed-vpp.ps1`
(ver `openplc-editor-installers/v4.2.7/docs/SIGNING_AND_TRUST.md`). No editar `signature.json` a mano.

Antes de firmar se puede comprobar la compilación sin OpenPLC (nube/WSL):

```bash
tools/jwplc-hmi-designer/gates/A15_F0_OpenPLC_Bool_Rele/check_f0_compile.sh
```

### 2.5 Prueba en placa

Pasos F0-1..F0-7 del README del gate. Criterio PASS:

- F0-3 y F0-4: el campo sigue al relé, sin parpadeo;
- F0-5: ESC vuelve a la IDLE;
- F0-7: el debugger sigue funcionando.

Anotar resultados y foto en `docs/v2.1.0-alpha.14/` (o en esta carpeta) con el flag
`F0_BOOL_RELE_PHYSICAL`.

## 3. Reglas para la IA de Kenyi

Leer, en orden:

1. `/CLAUDE.md`;
2. `tools/jwplc-hmi-designer/CLAUDE.md`;
3. este documento;
4. `A15_OPENPLC_INTEGRATION_DECISION_20261016.md`.

- No regenerar `libJWPLC_Display.a` ni volver a poner `precompiled=full`: se hace una vez al
  cierre del alpha.
- No editar `JWPLC_HMI_Generated.h` a mano: se regenera desde el Designer.
- No tocar `JWPLC_Display` para la prueba F0. Si hace falta algo del firmware, avisar a Alpha15
  (André) para que el Designer y el firmware sigan iguales; `tests/run-all.cjs` lo vigila.
- El bus SPI es compartido. Nada de dibujar directo en la TFT desde el HAL: sólo setters de
  `JWPLC_Display`.
- `signature.json` sólo con el script de firma; `.gitattributes` fija el EOL de los archivos
  firmados.

## 4. Después de F0

| Paso | Qué | Quién |
|---|---|---|
| F0b | Más tipos con memoria ubicada: VALUE ← `%MW`/`%MD` (`int_memory`/`dint_memory`), TEXT de estado | Kenyi |
| F0c | Escribir del HMI/MENU al PLC (setpoints): `%MW` en `updateInputBuffers()` o `openplc_debug_set(..., forcing=0)`; confirmar la semántica de `forcing=0` | Kenyi |
| F0d | Medir el costo de `jwplcUIUpdate()` por scan (objetivo < 50 µs) | Kenyi |
| F0e | **Bits de memoria `%MX` (IEC 61131-3)** en el runtime Arduino de OpenPLC (aprobado el 2026-10-18, plan industrial §G.11) | Kenyi |

Detalle de F0e:

- Hoy `resources/sources/arduino/openplc.h` y `arduino_runtime_glue.cpp` (openplc-editor 1f12dc5) enlazan `%MW/%MD/%ML` (`int_memory`, `dint_memory`, `lint_memory`), pero **no** `%MX` `[visto]`. `bool_memory` sólo aparece en el esquema de S7Comm.
- Falta:
  - `IEC_BOOL *bool_memory[MAX_MEMORY_BOOL/8][8]` en `openplc.h`;
  - el caso `%MX` al enlazar las variables ubicadas en `arduino_runtime_glue.cpp`;
  - el tamaño en el VPP.
- Lo usarán los bits de aviso y de solicitud de la HMI (ALARM y RECIPE, Alpha16).
| F3 | El editor escribe `JWPLC_HMI_Generated.h` junto a `arduino.cpp`; el HAL lo toma con `#if __has_include("JWPLC_HMI_Generated.h")` y deja de pegarse | Kenyi |
| A15-6 | Modo OpenPLC del Designer: elegir la variable PLC existente en vez de escribir un nombre, y `plcBinding` en el `.jwhmi` | André (Alpha15) |
| — | Manifest: "Keyboard: 7 buttons" → 6 (corrección C3) | Kenyi |
