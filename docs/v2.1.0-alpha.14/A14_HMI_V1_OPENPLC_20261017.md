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
VPP_TIMING_VERSION=2.1.0-alpha.31 (pantalla "Pantalla HMI": medir tiempos de scan)
F0D_TIMING_COMPILE=PASS (468173 B con medición; sin medición = alpha.30)
F0D_1_BASELINE=PASS (sin HMI: scan ≈ 60 µs, periodo 20 ms, periodo máx. 26–28 ms)
F0D_2_HMI_IDLE_ALPHA31=FAIL (HMI ≈ 167 µs/scan > 50 µs: jwplcUIUpdate() en el scan)
VPP_TIMING_FIX=2.1.0-alpha.32 (el scan sólo copia enlaces; jwplcUIUpdate queda en la tarea del Display)
F0D_2B_HMI_IDLE_ALPHA32=PASS (HMI 7–9 µs prom., 21 µs máx. total; scan 46–53 µs)
F0D_3_VALUES_ALPHA32=PASS_CON_OBSERVACION (HMI 14–15 µs prom., 19–25 µs máx./s; un pico de 90 µs por desalojo de la tarea del Display)
F0D_4_MENU_ALPHA32=PASS_CON_OBSERVACION (HMI 5–14 µs prom., un pico de ≈ 85 µs; periodo máx. hasta 31,3 ms)
F0D_5_PAGES_ALPHA32=FAIL_DETERMINISMO (enlace HMI 7–13 µs OK; periodo máx. 53–65 ms al cambiar de página)
F0D_HMI_COST=PASS (alpha.32: 5–15 µs prom., ≤ 27 µs máx./s; picos 73–90 µs por desalojo)
F0D_DISPLAY_JITTER=ABIERTO (redibujo de página en jwplcSystemTask, mismo core y mayor prioridad que loopTask)
F3_DESIGNER_EN_OPENPLC=IMPLEMENTADO (abrir, editar, Actualizar HMI y compilar probados por Kenyi; selector Variable PLC probado en Chromium con anfitrión simulado)
F3_CADENA_COMPLETA_EN_PLACA=PENDING [no verificado]
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

## 10. F0d: costo de la HMI en el scan (VPP 2.1.0-alpha.31)

El VPP alpha.31 agrega al maestro la pantalla de configuración "Pantalla HMI"
con la casilla "Medir tiempos de scan" (`VPP_HMI_DIAGNOSTICS_TIMING` en
`vpp_config.h`). Con ella el HAL mide `jwplcHMIUpdate()`, el scan
(`updateInputBuffers` → fin de `updateOutputBuffers`) y el periodo entre
scans, y publica cada segundo 8 palabras `%MW` (base configurable, por defecto
`%MW10`). Se usan `%MW` y no Serial para no chocar con el debugger Modbus RTU
por USB. Sin la casilla el firmware es idéntico al de alpha.30.

Procedimiento, escenarios y criterios: gate
`tools/jwplc-hmi-designer/gates/A14_F0d_OpenPLC_Tiempos/`.

### F0d-1 · línea base sin HMI `[ejecutado 2026-10-17, Kenyi]`

Con la casilla de medición activa y sin pantalla en el proyecto:

- HMI 0 µs;
- scan ≈ 60–65 µs de promedio (máximo habitual 80–170 µs, un pico de ≈ 640 µs);
- periodo promedio 20 000 ± 80 µs;
- 50–51 scans/s;
- **periodo máximo 26–28 ms**.

El retraso de 6–8 ms en algunos scans ya está sin HMI, así que es la
referencia contra la que se comparan F0d-2..F0d-5. Probablemente viene del
refresco de la IDLE o de la atención del debugger por USB. Detalle y tabla en
el README del gate `A14_F0d_OpenPLC_Tiempos`.

### F0d-2 · HMI en IDLE con alpha.31 `[ejecutado 2026-10-17, Kenyi]`: no cumple

Resultados:

- HMI ≈ 167 µs de promedio, 340 µs de máximo y 837 µs desde el arranque;
- scan ≈ 222 µs (+160 µs sobre la base);
- periodo y scans/s iguales a la base.

Causa: el bloque F0 del HAL llamaba `jwplcUIUpdate()` en cada scan.
`setValue()` formatea el número en cada llamada, y esa función ya la llama la
tarea de sistema del Display en cada refresco de USER (no en la IDLE). Además
se entraba a `JWPLC_UI` desde dos tareas.

VPP 2.1.0-alpha.32: el scan sólo ejecuta `jwplcHMIBindingsSnapshot()`.
Detalle en el gate `A14_F0d_OpenPLC_Tiempos`. Queda pendiente F0d-2b (repetir
con alpha.32) y F0d-3..5.

### F0d-2b · HMI en IDLE con alpha.32 `[ejecutado 2026-10-17, Kenyi]`: PASS

- HMI: 7–9 µs de promedio, 10–16 µs de máximo y 21 µs desde el arranque
  (objetivo < 50 µs);
- scan: 46–53 µs de promedio, en el orden de la línea base;
- periodo y scans/s: iguales a la línea base.

La corrección de alpha.32 bajó la HMI unas 20 veces.

### F0d-3 · página Valores con cambios, alpha.32 `[ejecutado 2026-10-17, Kenyi]`

- HMI: 14–15 µs de promedio y 19–25 µs de máximo por segundo; un único pico de
  90 µs desde el arranque.
- Scan: 71–74 µs.
- Periodo máximo: 21–28 ms, igual que la base.

La tarea que dibuja la TFT (`jwplcSystemTask`) corre en el mismo core que el
PLC con más prioridad y lo desaloja. La medición de reloj incluye ese tiempo:
es la causa probable del pico y de los ≈ 20 µs más que en IDLE. El periodo no
empeora respecto de la base.

### F0d-4 · MENU editando, alpha.32 `[ejecutado 2026-10-17, Kenyi]`

Edición de Limite con UP/DOWN mantenidos y decenas con LEFT/RIGHT:

- HMI: 5–14 µs de promedio, 10–27 µs de máximo por segundo, con un pico de
  ≈ 85 µs (desalojo);
- scan: 49–67 µs;
- 50–51 scans/s;
- periodo máximo: hasta 31,3 ms, unos 3 ms más que el peor caso de la base,
  por el redibujo del MENU en cada repetición de tecla.

No se pierden scans.

### F0d-5 · cambio de páginas, alpha.32 `[ejecutado 2026-10-17, Kenyi]`

- HMI: 7–13 µs (el enlace sigue sin pesar en el scan).
- **Periodo máximo: 53–65 ms en cada segundo con cambio de página.** Un scan
  arranca hasta ≈ 45 ms tarde y el planificador lo recupera después; no se
  pierden scans.

Causa: el redibujo completo de página en `jwplcSystemTask`, que corre en el
mismo core que `loopTask` y con mayor prioridad.

### Conclusión F0d

El objetivo de la HMI (< 50 µs por scan) se cumple desde alpha.32: 5–15 µs de
promedio y ≤ 27 µs de máximo por segundo. Queda abierto un problema de
determinismo **del runtime de pantalla del package**, no del enlace: el dibujo
de la TFT desaloja al PLC, poco en IDLE y Valores (≤ 28 ms), algo más en el
MENU (≤ 31 ms) y mucho al cambiar de página (≤ 65 ms con tarea de 20 ms).
Opciones para Alpha13/Alpha15 (decisión del jefe):

- tarea del Display en el otro core, o con menor prioridad;
- redibujo de página en tramos.

Detalle en el gate `A14_F0d_OpenPLC_Tiempos`.

## 11. F3: el Designer dentro de OpenPLC

Contrato: `A14_F3_CONTRATO_HMI_OPENPLC_20261017.md`.

**OpenPLC** (editor, rama `develop/alpha14-openplc-hmi-v1`):

- botón **Abrir HMI Designer** en el dispositivo;
- ventana con el Designer de este repo, vinculada a `hmi/` del proyecto;
- `listVariables()` con las reglas de la compilación;
- al compilar, los enlaces salen de `plcBinding` en `hmi/JWPLC_HMI.jwhmi`;
  `bindings.json` queda como respaldo.

**Designer** (este repo):

- `designer-openplc-host.js`, nuevo, sólo se activa con `JWPLCHMIHost` en modo
  `openplc`: en la pestaña Datos de BOOL, VALUE, BAR y nodos del MENU agrega el
  selector **Variable PLC** filtrado por tipo y acceso;
- el nombre C++ se genera solo y se guarda `plcBinding`;
- `designer-menu.js`: getter `selectedNode`;
- `desktop.html`: carga el módulo.

El modo Arduino y el `.exe` no cambian. André decide si integra este módulo en
A15-6 o lo reemplaza.

Prueba de Kenyi en la app (primera versión):

- abrir, editar, "Actualizar HMI", compilar y correr: OK;
- dos fallas, corregidas:
  - no se podía elegir la variable PLC: ahora hay selector;
  - al reabrir salía la pantalla vieja: ahora "Actualizar HMI" y el cierre de
    la ventana guardan `JWPLC_HMI.jwhmi`.

Prueba sin hardware: en Chromium, con anfitrión simulado y el proyecto F0c,
`BOOL ← RELE` (sólo ofrece BOOL) y `MENU Limite <-> LIMITE` (sólo ofrece
`%MW`/`%MD` con RW; tipo `int32`) quedan con `plcBinding` en `serialize()`.

Pendiente: probar la cadena completa en la app y en la placa (elegir
variables, Actualizar HMI, borrar `bindings.json`, compilar, reabrir).

## 12. Estado al 24/10/2026 y traspaso

- Ramas y push al día. El jefe publicó Alpha13 e integró la línea HMI en
  `v2.1.0-alpha.14/feature/hmi-designer` (PR #141).
- Esta línea sale como **2.1.0-alpha.15**.
- Siguiente:
  1. traer la integración, con el OK del jefe; el merge simulado da 2
     conflictos triviales y no toca el VPP;
  2. ampliar el anfitrión de OpenPLC al puente nuevo del Designer (contrato 1.1);
  3. probar la cadena completa de F3 en la placa.
- Todo el detalle está en `JWPLC_NEW_CHAT_START_ALPHA14_OPENPLC_20261024.md`.
