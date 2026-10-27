# Contrato HMI Designer ↔ OpenPLC (`plcBinding` y `jwplc-hmi-host/1`)

Fecha: `2026-10-17` · propuesta de Kenyi (Alpha14) para validar con el jefe y André (Alpha15)

```text
ESTADO=PROPUESTA (pendiente de visto bueno del jefe y André)
IMPLEMENTADO_OPENPLC=sí (openplc-editor develop/alpha14-openplc-hmi-v1)
VERSION_1_2=IMPLEMENTADA (24/10/2026, §8): variables sin dirección (tabla de depuración de STruC++)
VERSION_1_1=IMPLEMENTADA (24/10/2026, §7): puente para el Designer de 2.1.0-alpha.14 en
  v2.1.0-alpha.15/OpenPLC-HMI/feature/designer-alpha14-en-openplc (los dos repos)
ADELANTO_DESIGNER=tools/jwplc-hmi-designer/poc/designer-openplc-host.js (selector "Variable PLC"; André decide si lo integra en A15-6)
OBJETIVO=en un proyecto OpenPLC, el Designer no crea variables: elige variables del PLC
BASE_PROBADA=bindings.json + header del Designer (F0, F0b, F0c PASS en placa)
REEMPLAZA=hmi/bindings.json (queda como respaldo y para migrar)
ALPHA14=lado OpenPLC (F3: abrir el Designer, entregar variables, guardar, compilar)
ALPHA15=lado Designer (A15-6: modo OpenPLC, selector "Variable PLC", plcBinding)
```

## 1. Archivos del proyecto OpenPLC

| Archivo | Lo escribe | Lo lee |
|---|---|---|
| `hmi/JWPLC_HMI.jwhmi` | Designer (vía OpenPLC) | Designer al abrir; OpenPLC al compilar (`plcBinding`) |
| `hmi/JWPLC_HMI_Generated.h` | Designer (vía OpenPLC) | OpenPLC al compilar |
| `hmi/bindings.json` | a mano (v1) | OpenPLC al compilar, sumado a `plcBinding` |

Los nombres son fijos: OpenPLC guarda el proyecto siempre como `JWPLC_HMI.jwhmi`,
aunque el Designer pida otro nombre.

## 2. `plcBinding` en el `.jwhmi`

Se agrega a cada objeto con dato (`BOOL`, `VALUE`, `BAR`, más adelante `TEXT`) y a
cada nodo de MENU con variable (`VIEW`, `EDIT_NUM`, `EDIT_BOOL`):

```json
{
  "type": "VALUE",
  "id": "FIELD_CONTADOR",
  "variable": "contador",
  "plcBinding": { "var": "CONTADOR", "access": "R" }
}
```

```json
{
  "type": "EDIT_NUM",
  "label": "Limite",
  "variable": "limite",
  "valueType": "int32",
  "plcBinding": { "var": "LIMITE", "access": "RW" }
}
```

| Clave | Valor |
|---|---|
| `var` | Nombre de la variable PLC tal como está en OpenPLC (global o de un programa instanciado; sin distinguir mayúsculas), o una dirección directa (`"%MW1"`). |
| `access` | `"R"` (por defecto) o `"RW"`. |

Equivale uno a uno a una entrada de `bindings.json` (`{ "hmi": <variable>, "plc": <var>, "access": <access> }`), que es lo probado en placa.

Reglas para el Designer en modo OpenPLC:

- `variable` (el nombre C++) **no lo edita el usuario**: el Designer lo deriva del
  nombre PLC (minúsculas, caracteres no válidos → `_`), único en el proyecto
  (`limite`, `limite_2`, ...). El tipo C++ sale del objeto (`bool`, `float`,
  `int32_t` del `EDIT_NUM` entero).
- No hay valor inicial editable: al arrancar manda el PLC.
- `"RW"` sólo en objetos editables (`EDIT_NUM`; `EDIT_BOOL` cuando el runtime lo
  admita, ver §5) y sólo si `listVariables()` informa `access: "RW"`.
- Objeto sin `plcBinding` en modo OpenPLC: el Designer lo marca como incompleto.

## 3. API de anfitrión `jwplc-hmi-host/1`

OpenPLC abre el Designer (los mismos archivos web del `.exe`) en una ventana
propia con un preload que expone dos objetos:

### 3.1 `window.JWPLCHMINative` (ya existe; mismo contrato que el `.exe`)

| Función | En OpenPLC |
|---|---|
| `getLinkedSketch()` | `{ ok: true, name: <proyecto>, path: <proyecto>/hmi, headerExists }` |
| `selectSketchDirectory()` | Igual que `getLinkedSketch()`: el "sketch" es siempre el proyecto abierto |
| `writeGeneratedHeader(text, overwrite)` | Escribe `hmi/JWPLC_HMI_Generated.h` (sin pedir confirmación) |
| `writeProject(fileName, text, overwrite)` | Escribe `hmi/JWPLC_HMI.jwhmi` (ignora `fileName`, sin confirmación) |

Con esto el Designer actual ya funciona dentro de OpenPLC sin cambios.

### 3.2 `window.JWPLCHMIHost` (nuevo)

```ts
JWPLCHMIHost.version   // 1
JWPLCHMIHost.mode      // "openplc"
getContext(): Promise<{ projectName: string; device: string; display: { width: 320; height: 170 } }>
listVariables(): Promise<HostVariable[]>
loadProject(): Promise<object | null>   // contenido de hmi/JWPLC_HMI.jwhmi
```

```ts
interface HostVariable {
  name: string                  // "LIMITE"
  scope: 'global' | 'program'
  pou?: string                  // "main" si scope = program
  iecType: string               // "INT", "BOOL", "REAL", ...
  address: string | null        // "%MW1" (aliases ya resueltos) o null
  hmiTypes: Array<'bool' | 'float' | 'int32_t'>  // tipos HMI que puede alimentar
  access: 'R' | 'RW' | null     // máximo permitido; null = no enlazable
  reason?: string               // por qué no es enlazable (sin dirección, %MX, ...)
}
```

- `listVariables()` devuelve **todas** las variables globales y de programas
  instanciados; las no enlazables vienen con `access: null` y `reason`, para
  mostrarlas deshabilitadas ("asígnale una `%MW`").
- La lista es la del momento de abrir el Designer; si el programa cambia, se
  vuelve a abrir (refresco en vivo: más adelante).
- `loadProject()` también lo usa OpenPLC al abrir: si existe
  `hmi/JWPLC_HMI.jwhmi` se carga en el Designer. (Desde 1.1 lo abre el propio
  Designer con `JWPLCHMINative.readProject()`; `loadProject()` queda disponible.)

## 4. Compilación en OpenPLC

1. Lee `hmi/JWPLC_HMI_Generated.h` (obligatorio si hay HMI).
2. Toma los enlaces de `plcBinding` en `hmi/JWPLC_HMI.jwhmi` y de `hmi/bindings.json`.
   Una misma variable HMI enlazada dos veces es error.
3. Valida cada enlace con las reglas ya probadas en placa (tabla de §5). Cualquier
   problema es error de compilación con el motivo.
4. Si `JWPLC_HMI.jwhmi` es más nuevo que el header: advertencia "pulsa Actualizar
   HMI en el Designer". El número de revisión compartido queda para después.

## 5. Tipos admitidos (lo probado en placa)

| HMI | PLC | Dirección | Acceso |
|---|---|---|---|
| `bool` | BOOL | `%IX`, `%QX` | R |
| `float`, `int32_t` | INT, UINT, WORD | `%IW`, `%QW` | R |
| `float`, `int32_t` | INT, UINT, WORD | `%MW` | R / RW |
| `float`, `int32_t` | DINT, UDINT, DWORD, REAL | `%MD` | R / RW |
| `float`, `int32_t` | REAL | `%ID`, `%QD` | R |

Todavía no: variables sin dirección, `%MX` (no hay buffer en el runtime), por eso
tampoco `EDIT_BOOL` en RW; TEXT (`char[]`); valores en vivo en el lienzo.

## 6. Reparto y aceptación

| Lado | Entrega | Aceptación |
|---|---|---|
| OpenPLC (Alpha14, F3) | Botón "Abrir HMI Designer"; ventana con el preload de §3; `listVariables`; guardar/cargar en `hmi/`; compilar con `plcBinding` | Con el Designer actual: abrir, diseñar, "Actualizar HMI" y "Guardar" escriben en `hmi/`; compilar y ver la pantalla en la placa |
| Designer (Alpha15, A15-6) | Si existe `JWPLCHMIHost` con `mode: "openplc"`: selector "Variable PLC" con `listVariables()`, `plcBinding` en el `.jwhmi`, sin edición de nombre/tipo/valor inicial | Proyecto del gate F0c rehecho eligiendo `CONTADOR`, `LIMITE`, `CONSIGNA` y `LLENO` de la lista, sin `bindings.json` |

Cambios a este contrato: se acuerdan entre Alpha14 y Alpha15 y se anotan aquí.

## 7. Versión 1.1: Designer de 2.1.0-alpha.14 dentro de OpenPLC (24/10/2026)

El Designer de 2.1.0-alpha.14 amplió `JWPLCHMINative` de 4 a unas 20 funciones. En OpenPLC el
«sketch» vinculado sigue siendo la carpeta `hmi/` del proyecto abierto, así que el anfitrión expone
sólo lo que tiene sentido ahí y el Designer, en modo OpenPLC (`JWPLCHMIHost.mode === 'openplc'`),
oculta el resto.

Ramas: `v2.1.0-alpha.15/OpenPLC-HMI/feature/designer-alpha14-en-openplc` en openplc-editor
(`src/main/modules/hmi-designer/index.ts`, `src/main/modules/preload/hmi-designer-preload.ts`,
helpers puros en `src/backend/shared/compile/jwplc-hmi-host.ts`) y en platform-jwplc (Designer).

### 7.1 `JWPLCHMINative` en OpenPLC

| Función | En OpenPLC 1.1 |
|---|---|
| `getLinkedSketch()` / `selectSketchDirectory()` | `{ ok, name: <proyecto>, path: <proyecto>/hmi, headerExists, projectFile: 'JWPLC_HMI.jwhmi', projectExists }` (`describeHmiFolder`) |
| `readProject()` | `{ ok, text, fileName }` de `hmi/JWPLC_HMI.jwhmi` (o el único `.jwhmi`); si no hay, `{ ok: false, missing: true }`. Con `projectExists` el Designer lo abre solo al iniciar |
| `writeGeneratedHeader(text)` | Escribe `hmi/JWPLC_HMI_Generated.h`. Ya no guarda además el `.jwhmi`: lo escribe el Designer a continuación |
| `writeProject(fileName, text)` | Escribe `hmi/JWPLC_HMI.jwhmi` (ignora `fileName`) |
| `openSketchFolder()` | Abre `hmi/` en el explorador (la crea si no existe) |
| `registerCloseGuard(cb)` / `confirmClose()` | Cerrar la ventana con cambios sin guardar pregunta (Guardar / No guardar / Cancelar). Sin respuesta en 2 s, cierra. Si se cierra OpenPLC, se guarda el `.jwhmi` sin preguntar |
| `registerSerialPortPicker()` / `chooseSerialPort()` | Selector de puertos del Designer para Live Preview y el flasheo del Live Bridge. La ventana usa su propia sesión (`persist:jwplc-hmi-designer`), así que los permisos serie no tocan a OpenPLC |
| `writeGuide` | **No se expone.** En OpenPLC no hay `.ino`: no se escribe `JWPLC_HMI_LEEME.md` |
| `chooseSaveTarget`, `applySaveTarget` | **No se exponen.** Siempre hay destino (`hmi/`): «Guardar» = «Actualizar HMI» |
| `createSketch`, `unlinkSketch`, `setLinkedSketch`, `openSketchIno`, `onSketchOpened` | **No se exponen.** «Nuevo» vacía el diseño sin desvincular |
| `listRecentProjects`, `openRecentProject`, `clearRecentProjects`, `openProjectFile` | **No se exponen.** «Abrir» usa el selector de archivos del navegador; lo abierto se guarda en `hmi/` |

### 7.2 El Designer en modo OpenPLC

- Botón «OpenPLC: <proyecto> ▾» (en vez de «Sketch: …»), con un menú de dos acciones: «Recargar HMI guardado» y
  «Abrir carpeta hmi/».
- «Actualizar HMI» (en vez de «Escribir en sketch») y Ctrl+S: escriben el header y `JWPLC_HMI.jwhmi`, sin guía.
- «Variable PLC» en la pestaña Datos (`designer-openplc-host.js`, sin cambios desde 1.0).
- Los textos los escriben `designer-sketch-menu.js`, `designer-native-project.js`,
  `designer-project-integration-fix.js` y `desktop-responsive.js` con la misma regla, para que no parpadeen.
- El modo Arduino y el `.exe` no cambian (regla 6 del `CLAUDE.md`).

### 7.3 Qué Designer abre OpenPLC

OpenPLC toma los archivos de `JWPLC_HMI_DESIGNER_DIR` o del checkout vecino
`../platform-jwplc/tools/jwplc-hmi-designer/poc`. Desde 1.1 exige `designer-save-flow.js` (Designer
2.1.0-alpha.14) y `designer-openplc-host.js` (modo OpenPLC). Si falta alguno, no abre un Designer viejo: avisa
que hay que cambiar platform-jwplc a `v2.1.0-alpha.15/OpenPLC-HMI/main`.

### 7.4 Verificación

- `[ejecutado]` platform-jwplc: `tools/jwplc-hmi-designer/tests/openplc-host-smoke.cjs` (puente 1.1 simulado,
  también en `run-all.cjs`).
- `[ejecutado]` openplc-editor: `jwplc-hmi-host.test.ts` (100 % de `jwplc-hmi-host.ts`), `tsc`, `eslint`.
- `[no verificado]` en la app y en la placa: guía `A14_F3_1_DESIGNER_ALPHA14_EN_OPENPLC_20261024.md`.

## 8. Versión 1.2: variables sin dirección (24/10/2026)

Hasta 1.1 sólo se enlazaban variables con dirección (`%IX`/`%QX`/`%IW`/`%QW`/`%MW`/`%MD`/`%ID`/`%QD`). Las que crea el
Ladder no tienen dirección, así que la lista «Variable PLC» quedaba vacía. Desde 1.2 se enlazan también las
variables **sin dirección** de los programas instanciados y las globales: el HMI las lee y escribe por la tabla
de depuración que STruC++ ya compila en el firmware.

| Tema | Regla |
|---|---|
| Tipos | BOOL; SINT, USINT, BYTE, INT, UINT, WORD, DINT, UDINT, DWORD; REAL. Otros (TIME, LREAL, STRING, tipos de usuario) aparecen deshabilitados con el motivo |
| Acceso | `RW` siempre: son memoria del programa. La escritura **no fuerza** (`handle_write`): si el depurador fuerza la variable, manda el forzado |
| `listVariables()` | `address: null`, `access: 'RW'`, sin `reason`. El Designer muestra «variable del programa» |
| Ruta | Globales: `NOMBRE`; variables de programa: `<INSTANCIA>.<NOMBRE>` (primera instancia del programa). `VAR_EXTERNAL` es la global; `VAR_TEMP` no se enlaza |
| Las que tienen dirección | Igual que antes: buffers del runtime (F0–F0c en placa) |

Cómo se arma al compilar (openplc-editor):

1. `generateJwplcHmi()` escribe `jwplc_hmi.h` con marcas `@@JWPLC_HMI_VAR(<ruta>,<TIPO>)@@` para cada enlace sin
   dirección y los ayudantes `jwplcHmiReadVar` / `jwplcHmiWriteVar`.
2. Tras STruC++, el pipeline llama `finalizeFirmwareSkeleton` → `finalizeJwplcHmiSkeleton()`: reemplaza cada marca
   por `arr, elem` de `debug-map.json` y comprueba el tipo. Si falta la variable o cambió de tipo, error con el
   nombre y no se llama a arduino-cli.
3. El runtime Arduino exporta `openplc_debug_write` (escritura sin forzar) junto a `openplc_debug_read`
   (`resources/sources/arduino/arduino_runtime_glue.{h,cpp}`).

Además, en 1.2:

- Un `hmi/JWPLC_HMI.jwhmi` sin `JWPLC_HMI_Generated.h` ya no detiene la compilación: aviso y la placa muestra la IDLE.
- Al cambiar de proyecto o cerrar OpenPLC, el anfitrión guarda el header **y** el `.jwhmi`.

Verificación:

- `[ejecutado]` openplc-editor: tests de `jwplc-hmi.ts` y `jwplc-hmi-host.ts` al 100 %, gancho del pipeline;
  tests de `compile` (284) en verde, `tsc`.
- `[ejecutado]` compilación ESP32 del HAL del VPP + `jwplc_hmi.h` con los 7 enlaces del gate F0c sin dirección
  (índices resueltos, tabla de depuración simulada): PASS, 448 525 B.
- `[no verificado]` en la app y en la placa con un programa Ladder real (guía `A14_F3_1_…` paso 4).

## 9. Versión 1.3: el Designer en una pestaña de OpenPLC (27/10/2026)

Pedido del propietario: ver el HMI integrado en OpenPLC, no en una ventana aparte. Desde 1.3 el Designer se abre
como una pestaña más del editor («HMI Designer», en el árbol del proyecto bajo las pantallas de la placa, y con el
botón «Abrir HMI Designer» de *Configuration*).

| Tema | Regla |
|---|---|
| Dónde corre | Un `<webview>` en la pestaña. El proceso principal sólo deja adjuntar la página del Designer servida en 127.0.0.1 y le impone el preload y la sesión `persist:jwplc-hmi-designer` (`will-attach-webview`) |
| Cambiar de pestaña | La pestaña queda montada mientras está abierta: el diseño y el historial de deshacer no se pierden |
| Cambios sin guardar | `JWPLCHMIHost.setDirty(bool)`: el Designer los informa y la pestaña se marca como cualquier archivo del proyecto |
| Guardar | Ctrl+S en la pestaña, «Guardar proyecto» y el guardado automático antes de compilar escriben el header y `JWPLC_HMI.jwhmi` en `hmi/`; después el Designer cuenta el diseño como guardado (`JWPLCHMISaveFlow.markSaved()`) |
| Cerrar la pestaña | Con cambios pregunta el modal de OpenPLC (Guardar / No guardar / Cancelar). Ya no hay guarda de cierre propia: `registerCloseGuard` / `confirmClose` no se exponen |
| Cerrar OpenPLC | Se guarda el diseño sin preguntar, como antes (`hmiDesignerHost.flush()`, máximo 3 s) |
| Variables | `JWPLCHMIHost.onVariablesChanged(cb)`: al editar las variables del proyecto, OpenPLC avisa y el Designer vuelve a pedir `listVariables()` sin reabrir. Sin variables, «Variable PLC» explica dónde declararlas |
| Detección | `JWPLCHMIHost.embedded === true` en la pestaña |

Verificación:

- `[ejecutado]` platform-jwplc: `tests/openplc-host-smoke.cjs` (marca de cambios, guardado desde OpenPLC,
  variables en vivo y lista vacía).
- `[ejecutado]` openplc-editor: `tsc`, `eslint`, tests de `tabs/utils`, del adaptador de dispositivo y de
  `hmi-designer-save`.
- `[no verificado]` en la app instalada (pestaña, guardado, puerto serie del Live Preview dentro de la pestaña).
