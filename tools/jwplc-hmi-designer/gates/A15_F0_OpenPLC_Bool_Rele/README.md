# Gate F0 · OpenPLC: campo BOOL del HMI vinculado al relé Q0_0

Primera prueba de integración HMI Designer ↔ OpenPLC (fase F0 del plan).
Un programa OpenPLC conmuta el relé `Q0_0`, y la TFT muestra su estado en un campo BOOL
diseñado en el HMI Designer.

Ruta completa y contexto: `docs/v2.1.0-alpha.15/A15_F0_OPENPLC_RUTA_KENYI_20261018.md`.

| Archivo | Qué es |
|---|---|
| `A15_F0_OpenPLC_Bool_Rele.jwhmi` | Proyecto del Designer: una página y un BOOL "Rele Q0.0". En OFF va blanco sobre rojo; en ON, negro sobre verde. |
| `JWPLC_HMI_Generated.h` | Header que genera el Designer para ese proyecto (variable `bool rele_q0_0`). |
| `hal_f0_bloque.cpp` | Bloque que se pega en `hal/jwplcbasic.cpp` del VPP: el header y el enlace `%QX0.0 → rele_q0_0`. |
| `bindings.json` | Enlace `rele_q0_0 <- RELE` para la v1 de OpenPLC (sin pegar en el HAL), ver abajo. |
| `check_f0_compile.sh` | Compila, sin OpenPLC, el HAL real del VPP más el bloque F0 contra el package de esta rama. |

## Cambios en el HAL (`openplc-editor-installers/v4.2.7/vpp/hal/jwplcbasic.cpp`)

1. Después de `#include <JWPLC_ModbusRTU.h>`, pegar `hal_f0_bloque.cpp`. En la línea
   `// @@JWPLC_HMI_GENERATED@@` va el contenido de `JWPLC_HMI_Generated.h`, sin `#pragma once`.
2. Primera línea de `hardwareInit()`:

   ```cpp
   jwplcHMIF0Begin();
   ```

3. En `updateOutputBuffers()`, después del bucle de `digitalWrite` y antes de
   `jwplcPublishForcedInputs();`:

   ```cpp
   jwplcHMIF0Update();
   ```

`check_f0_compile.sh` aplica exactamente estos tres cambios sobre una copia del HAL.

## Programa OpenPLC

```text
VAR
  PULSADOR AT %IX0.0 : BOOL;   (* I0_0 *)
  RELE     AT %QX0.0 : BOOL;   (* Q0_0, relé 0 *)
END_VAR
Ladder: ──[ PULSADOR ]──( RELE )──
```

Sin la variable en `%QX0.0`, `bool_output[0][0]` es `NULL` y el campo queda en OFF.

## Resultado esperado `[no verificado]`

| # | Acción | Esperado |
|---|---|---|
| F0-1 | Arranque | La IDLE de siempre, con el overlay de entradas forzadas de alpha.12 |
| F0-2 | Cualquier tecla | Página del Designer: "Rele Q0.0" con `OFF` blanco sobre rojo |
| F0-3 | Activar I0_0 | El relé Q0_0 conmuta y el campo pasa a `ON` negro sobre verde, sin parpadeo del resto |
| F0-4 | Soltar I0_0 | Vuelve a `OFF` rojo |
| F0-5 | ESC | Vuelve a la IDLE |
| F0-6 | Forzar `RELE` desde el debugger | El campo sigue al valor forzado `[inferido]`: se lee `%QX0.0` tal como sale al relé |
| F0-7 | Debugger Modbus RTU/TCP | Sigue funcionando igual que sin HMI |

## Verificación sin hardware `[ejecutado 2026-10-18]`

```bash
tools/alpha15/regenerate_libJWPLC_TFT.sh setup     # una vez
tools/jwplc-hmi-designer/gates/A15_F0_OpenPLC_Bool_Rele/check_f0_compile.sh
# Sketch uses 422401 bytes · Global variables use 36612 bytes
```

## Con OpenPLC v1 (VPP 2.1.0-alpha.30)

Desde el VPP alpha.30 no hace falta pegar nada en el HAL: se copian
`JWPLC_HMI_Generated.h` y `bindings.json` a la carpeta `hmi/` del proyecto
OpenPLC y el editor (rama `develop/alpha14-openplc-hmi-v1`) los incluye al
compilar. Detalle: `docs/v2.1.0-alpha.14/A14_HMI_V1_OPENPLC_20261017.md`.
