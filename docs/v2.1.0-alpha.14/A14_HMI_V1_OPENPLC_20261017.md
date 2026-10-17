# Alpha14 · HMI v1 en OpenPLC: header del Designer + enlaces BOOL, sin pegar en el HAL

Fecha: `2026-10-17` · responsable: Kenyi

```text
BASE_PLATFORM=v2.1.0-alpha.15-HmiDesigner @ 5f7029c5
BRANCH_PLATFORM=v2.1.0-alpha.14-OpenPLC-HMI
BRANCH_EDITOR=develop/alpha14-openplc-hmi-v1 (desde develop/alpha12-backplane-closure @ 1f12dc549)
VPP_VERSION=2.1.0-alpha.30
VPP_FILE=jwplc-basic-openplc-2.1.0-alpha.30.jwcontrol-signed.vpp
VPP_SIGNATURE=PASS (verifyPackageSignature del editor, keyId jwcontrol-2026)

A14_HMI_V1_EDITOR_TESTS=PASS (30 tests, 100 % stmts/branches/funcs/lines en jwplc-hmi.ts)
A14_HMI_V1_COMPILE_OPENPLC_RUNTIME=PASS (build real de OpenPLC + HAL alpha.30 + jwplc_local)
A14_HMI_V1_COMPILE_SIN_HMI=PASS (JWPLC_HMI_ENABLED 0 y sin jwplc_hmi.h: mismo binario sin símbolos HMI)
A14_HMI_V1_EDITOR_GUI=PASS (Kenyi, proyecto HMIProbe, VPP alpha.30, Local Development)
F0_BOOL_RELE_PHYSICAL=PASS (2026-10-17, Kenyi)
F0B_VALUES_COMPILE=PASS (VALUE/BAR <- %MW0, runtime real, 461349 B flash)
F0B_VALUES_PHYSICAL=PASS (2026-10-17, Kenyi: contador y barra siguen a CONTADOR %MW0)
F0C_WRITE_COMPILE=PASS (MENU EDIT_NUM <-> %MW1 INT y %MD0 REAL, 467749 B flash)
F0C_WRITE_PHYSICAL=PASS (2026-10-17, Kenyi: edición del MENU escribe en el PLC)
```

## 1. Qué cambia respecto de la ruta F0

La ruta F0 (`docs/v2.1.0-alpha.15/A15_F0_OPENPLC_RUTA_KENYI_20261018.md`) pegaba el
header del Designer dentro de `hal/jwplcbasic.cpp`. Eso obligaba a firmar un VPP
nuevo por cada cambio de pantalla. En la v1 la pantalla viaja **con el proyecto
OpenPLC** y el HAL queda fijo:

```text
Proyecto OpenPLC/
  hmi/JWPLC_HMI_Generated.h   exportado por JWPLC HMI Designer (no editar)
  hmi/bindings.json           enlaces variable HMI <- variable PLC
```

Al compilar, el editor escribe junto a `src/arduino.cpp`:

| Archivo | Contenido |
|---|---|
| `src/JWPLC_HMI_Generated.h` | copia del header del proyecto |
| `src/jwplc_hmi.h` | `JWPLC_HMI_ENABLED 0/1`, `#include` del header y `jwplcHMIBindingsSnapshot()` |

`jwplc_hmi.h` se escribe **siempre** para el maestro (con `0` si el proyecto no
tiene pantalla). Así, un header que haya quedado en `build/` de una compilación
anterior nunca vuelve a activar la HMI.

## 2. VPP 2.1.0-alpha.30

- `hal/jwplcbasic.cpp`, enganche equivalente al bloque F0 del jefe:
  - `#if __has_include("jwplc_hmi.h")` → incluye el glue del editor; si no existe
    (editor sin soporte HMI), `JWPLC_HMI_ENABLED 0` y la placa queda como antes.
  - `jwplcHMIBegin()` como primera línea de `hardwareInit()`: `jwplcHMISetup()` +
    `IDLE_WAKE_ANY_BUTTON`.
  - `jwplcHMIUpdate()` en `updateOutputBuffers()`, después de los `digitalWrite`
    y antes de `jwplcPublishForcedInputs()`: `jwplcHMIBindingsSnapshot()` +
    `jwplcUIUpdate()` (sólo setters, dirty refresh).
- `manifest.json` del maestro:
  - `capabilities.hmiDisplay` = `{ controller: ST7789, width: 320, height: 170,
    pixelFormat: RGB565, keys: [LEFT, UP, RIGHT, ESC, OK, DOWN] }`. El editor
    sólo agrega los archivos HMI a placas con esta capacidad. El esclavo Remote
    I/O no la tiene.
  - `specs.Keyboard`: "7 buttons" → "6 buttons" (corrección C3).
- Sin cambios en `JWPLC_Display` ni en el HAL del esclavo.

## 3. Alcance de la v1

Sólo lo validado en el gate F0:

- variables HMI `bool` enlazadas a variables PLC **BOOL ubicadas** en `%IX` o
  `%QX` (globales o de un programa instanciado; también se acepta la dirección
  directa, p. ej. `"plc": "%QX0.0"`);
- lectura por los punteros `bool_input` / `bool_output` del runtime;
- cualquier otro caso es **error de compilación** con mensaje claro (variable
  inexistente, tipo distinto de BOOL, sin dirección, `%MW`/`%MX`, HMI no `bool`,
  enlace repetido, header que no es del Designer, `bindings.json` mal formado).

Queda para después (ruta F0b/F0c/F3): VALUE ← `%MW`/`%MD`, variables sin
dirección vía `openplc_debug_read()`, escrituras HMI → PLC, `plcBinding` dentro
del `.jwhmi` (A15-6) y abrir el Designer desde OpenPLC.

## 4. Evidencia `[ejecutado 2026-10-17]`

Compilación con `arduino-cli` (el del editor) sobre una copia del build real de
OpenPLC del maestro (`PruebasBlackplane`, runtime STruC++ y `libOpenPLCUserLib.a`
del programa), cambiando sólo `src/arduino.cpp` por el HAL alpha.30 y agregando
los archivos que genera `generateJwplcHmi()` del editor
(`hmi/JWPLC_HMI_Generated.h` del gate F0 + enlace `rele_q0_0 <- Salida1 (%QX0.1)`).
FQBN `jwplc_local:esp32:jwplcbasic` → este checkout.

| Variante | Flash | RAM | Símbolos HMI en el ELF |
|---|---|---|---|
| HMI activa | 459773 | 37836 | `jwplcHMISetup`, `jwplcUIUpdate`, `HMI_FIELDS`, `rele_q0_0` |
| `JWPLC_HMI_ENABLED 0` | 450989 | — | ninguno |
| sin `jwplc_hmi.h` (editor viejo) | 450989 | — | ninguno |

La HMI del gate F0 cuesta unos 8,8 KB de flash.

## 5. Resultado en placa F0 `[ejecutado 2026-10-17, Kenyi]`

Proyecto `HMIProbe` (`PULSADOR`/`RELE` por alias), `hmi/bindings.json` con
`rele_q0_0 <- RELE`:

- el campo "Rele Q0.0" sigue al relé: OFF y ON con sus colores de fondo;
- también sigue al valor forzado desde el debugger (F0-6);
- ESC vuelve a la IDLE y cualquier otra tecla entra a la página (F0-2, F0-5);
- el debugger sigue funcionando (F0-7).

Observación: en la foto el fondo de OFF (`0xF800`, rojo) se ve anaranjado; parece
de la cámara, confirmarlo a simple vista.

## 6. F0b: valores numéricos

El editor amplía los enlaces a `float` (VALUE/BAR) desde `%IW`/`%QW`/`%MW`
(INT, UINT, WORD), `%MD` (DINT, UDINT, DWORD, REAL) e `%ID`/`%QD` (REAL), con la
conversión de signo correcta y `memcpy` para REAL en `%MD`. El HAL y el VPP
alpha.30 no cambian. Gate: `tools/jwplc-hmi-designer/gates/A14_F0b_OpenPLC_Valores/`.

## 7. Procedimiento de la prueba F0

1. Instalar `jwplc-basic-openplc-2.1.0-alpha.30.jwcontrol-signed.vpp` en el editor
   de la rama `develop/alpha14-openplc-hmi-v1`.
2. Dispositivo **JWPLC BASIC [2.0.0]**, *Package Source* = **Local Development**
   (`jwplc_local` → `platform-jwplc/JWPLC/2.1.0` en esta rama).
3. Programa: `PULSADOR AT %IX0.0 : BOOL`, `RELE AT %QX0.0 : BOOL`,
   `──[ PULSADOR ]──( RELE )──`.
4. Copiar a la carpeta del proyecto `hmi/JWPLC_HMI_Generated.h` y
   `hmi/bindings.json` de `tools/jwplc-hmi-designer/gates/A15_F0_OpenPLC_Bool_Rele/`.
5. Compilar: la consola debe mostrar `HMI: rele_q0_0 <- RELE (%QX0.0)`.
6. Pasos F0-1..F0-7 del README del gate. Criterio PASS: el campo sigue al relé
   sin parpadeo, ESC vuelve a la IDLE y el debugger sigue funcionando.

Anotar aquí el resultado y la foto con `F0_BOOL_RELE_PHYSICAL`.

## 8. F0b en placa y F0c: escritura HMI → PLC

F0b aprobado en placa (2026-10-17). Observación para Alpha15: en la foto, el
marco del VALUE "Contador" (`valueSize` 3, `labelSize` 2, INLINE) parece
cerrarse sólo bajo la etiqueta; la línea inferior no llega hasta el valor.
Revisar la geometría del marco en `JWPLC_Display`.

F0c: los enlaces aceptan `"access": "RW"` sobre `%MW` / `%MD`. La escritura
se hace en `jwplcHMIBindingsSnapshot()`, al final del scan: primero HMI → PLC
si la pantalla cambió el valor respecto de la sombra, después PLC → HMI. El
programa lo ve en el scan siguiente, la tarea Modbus lo conserva (copia memoria
→ buffer antes de volcar el buffer en memoria) y un forzado del debugger gana.
Al arrancar manda el PLC. No cambia el HAL ni el VPP. Gate:
`tools/jwplc-hmi-designer/gates/A14_F0c_OpenPLC_Escritura/`.

## 9. Resultado en placa F0c `[ejecutado 2026-10-17, Kenyi]`

Proyecto `HMIProbe` con el gate `A14_F0c_OpenPLC_Escritura`:

- La HMI tiene 2 páginas con indicador `01/02` / `02/02` arriba a la derecha.
- Página "Valores": Rele, Contador y Nivel como en F0b.
- Página "Ajustes": el MENU muestra Contador, Limite, Consigna (`50.0 %`) y
  Lleno (`NO`).
- La edición de **Limite** funciona y llega al PLC (en la foto quedó en 9):
  UP/DOWN cambian de a una unidad y LEFT/RIGHT de a diez.
- Pasos de la tabla del gate no reportados uno por uno: ESC sin confirmar
  (F0c-4), Consigna REAL (F0c-5), forzado (F0c-6/7) y reinicio (F0c-8).

Detalle de uso reportado: al entrar a la HMI primero se selecciona la página
(queda resaltado el indicador `01/02`) y hace falta OK para entrar al contenido
del MENU. Es el comportamiento `PAGE_SELECT` → `PAGE_CONTENT` de
`JWPLC_Display`; si se quiere entrar directo al MENU cuando la página sólo
tiene un MENU, es una decisión de Alpha15 (André).
