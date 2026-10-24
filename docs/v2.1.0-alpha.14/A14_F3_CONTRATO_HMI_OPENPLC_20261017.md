# Contrato HMI Designer ↔ OpenPLC (`plcBinding` y `jwplc-hmi-host/1`)

Fecha: `2026-10-17` · propuesta de Kenyi (Alpha14) para validar con el jefe y André (Alpha15)

```text
ESTADO=PROPUESTA (pendiente de visto bueno del jefe y André)
IMPLEMENTADO_OPENPLC=sí (openplc-editor develop/alpha14-openplc-hmi-v1)
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
  `hmi/JWPLC_HMI.jwhmi` se carga en el Designer.

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

## 7. Pendiente: versión 1.1 (24/10/2026)

El Designer integrado en `v2.1.0-alpha.14/feature/hmi-designer` amplió
`JWPLCHMINative` de 4 a unas 20 funciones (`readProject`, `chooseSaveTarget`,
`applySaveTarget`, `writeGuide`, `registerCloseGuard`, recientes, selector de
puerto serie, ...). Algunas se llaman sin comprobar que existan, así que el
anfitrión de OpenPLC tiene que implementarlas antes de usar ese Designer.

Propuesta de cómo responde cada una en OpenPLC:
`JWPLC_NEW_CHAT_START_ALPHA14_OPENPLC_20261024.md` §3.2. Se acuerda con André y
se anota aquí como `jwplc-hmi-host/1.1`.
