# Alpha15 — Decisión de integración HMI Designer ↔ OpenPLC

Fecha: `2026-10-16`

## Fuentes revisadas

```text
REPO=JW-Control/openplc-editor
BRANCH=develop/alpha12-backplane-closure   (la más reciente, Kenyi, 2026-10-10)
HEAD=1f12dc5
DOCS=docs/jwplc/JWPLC_HMI_PANTALLA_OPENPLC_PROPUESTA.md
     docs/jwplc/JWPLC_HMI_PLAN_INDUSTRIAL.md   (rev. 2, "plan aprobado en dirección")

REPO=JW-Control/platform-jwplc
BRANCH=v2.1.0-alpha.12/feature/openplc-engineering-closure  (HEAD 4e869985, VPP 2.1.0-alpha.29)
VPP=openplc-editor-installers/v4.2.7/vpp/ (manifest.json, hal/jwplcbasic.cpp, screens/)
```

## Decisión

```text
BASE=JWPLC_HMI_PLAN_INDUSTRIAL rev.2 (fork) ADOPTADO
UNA_SOLA_HERRAMIENTA=JWPLC HMI Designer
ANFITRIONES=escritorio (.exe) | Arduino IDE (VSIX) | OpenPLC (ventana -> pestaña)
CONTRATO=jwplc-hmi-host/1
DESIGNER_EN_OPENPLC=dentro del VPP firmado
FORMATO=.jwhmi + plcBinding + revision
BINDINGS=generados por OpenPLC al compilar (paso generate-hmi)
```

El plan rev. 2 coincide con el objetivo de alpha.15: una sola interfaz que
sabe si está en un proyecto Arduino o OpenPLC. Se adopta con las
correcciones de abajo, que salen de revisar el código real.

## Correcciones al plan del fork (verificadas en código)

### C1 — El enlace por nombre no puede usar `get_var_addr(i)`

`resources/sources/arduino/debug.h` declara `get_var_addr(size_t)`, pero el
runtime actual compila con **STruC++** y no lo implementa. La superficie real
es (`resources/sources/arduino/arduino_runtime_glue.h:72-76`):

```c
uint8_t  openplc_debug_array_count(void);
uint16_t openplc_debug_elem_count(uint8_t arr);
uint16_t openplc_debug_size(uint8_t arr, uint16_t elem);
uint16_t openplc_debug_read(uint8_t arr, uint16_t elem, uint8_t* dest);
uint8_t  openplc_debug_set(uint8_t arr, uint16_t elem, uint8_t forcing,
                           const uint8_t* bytes, uint16_t len);
```

Las direcciones son `(arrayIdx, elemIdx)` y salen de `debug-map.json`
(`src/frontend/utils/debug-parser.ts`, `leaves[].path/type/size`), que
STruC++ genera en cada compilación.

Consecuencia para `generate-hmi`:

```cpp
// jwplc_hmi_bindings.cpp (generado por OpenPLC)
static const JWPLCHMIBinding HMI_BINDINGS[] = {
    // field,        arr, elem, tipo IEC,   acceso
    { FIELD_TEMP,    2,   5,    HMI_REAL,   HMI_R  },
    { FIELD_MOTOR,   0,   1,    HMI_BOOL,   HMI_R  },
    { MENU_SP_TEMP,  2,   6,    HMI_REAL,   HMI_RW },
};
```

El HAL lee con `openplc_debug_read()` en `updateOutputBuffers()` y escribe con
`openplc_debug_set(..., forcing=0, ...)` en `updateInputBuffers()`.

```text
VERIFICAR_EN_F0=semántica de handle_set con forcing=false (escritura única vs. force)
VERIFICAR_EN_F0=costo de openplc_debug_read x 32 por scan (< 50 us objetivo)
```

Alternativa para E/S y memoria ubicada (`%IX/%QX/%MW/%MD`): punteros de
`bool_input/bool_output/int_memory/dint_memory` (`resources/sources/arduino/openplc.h`).

### C2 — Lista de variables

- Anfitrión OpenPLC: `listVariables()` sale del store del editor (globales +
  variables de programas, rutas como en el debugger, p. ej. `INSTANCE0.NIVEL`).
- Designer de escritorio vinculado a una carpeta OpenPLC: sólo
  `project.json → configuration.resource.globalVariables`
  (`src/backend/shared/types/PLC/open-plc.ts:271`; campos `name`, `type`,
  `location`). Las variables locales viven como texto `VAR ... END_VAR` en
  `pous/**` y **no** se parsean desde el Designer.

### C3 — Botonera

El plan del fork habla de **7 teclas con F1**. El package define **6**
(`JWPLC_GlobalPeripherals.h`: `BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_ESC, BTN_OK,
BTN_DOWN`, `BTN_COUNT`). La capacidad `hmiDisplay.keys` del VPP debe usar la
lista real.

```text
BOTONERA=6_TECLAS (confirmado por el jefe, 2026-10-16)
EVIDENCIA=JWPLC_GlobalPeripherals.cpp:57-58 matriz ROWS{12,2} x COLS{35,34,36} = 2x3
          docs/v2.1.0-alpha.8/ALPHA8_HMI_BUTTON_VALIDATION.md (6 máscaras 0x01..0x20)
F1=NO_EXISTE -> corregir capabilities.hmiDisplay.keys y docs del fork (Alpha14)
```

### C4 — Numeración

El fork ubica el motor ampliado en "Alpha13 = TFT/Display". Queda sustituido
por la organización vigente: Alpha13 = robustez del package, Alpha14 =
OpenPLC (Kenyi), Alpha15 = HMI Designer (André).

### C5 — Codegen puro antes, no en F6

Si OpenPLC ejecuta el codegen del Designer al compilar, desaparece el riesgo
de header desactualizado. Como alpha.15 ya necesita el codegen puro para
arreglar B1-B6, se adelanta (A15-5) y OpenPLC puede usarlo desde F3.

## Reparto

| Trabajo | Alpha | Responsable |
|---|---|---|
| F1 Designer confiable, CI de compilación, codegen puro | 15 | André |
| F2 `jwplc-hmi-host/1`, `plcBinding`, selector "Variable PLC", modo Arduino/OpenPLC | 15 | André |
| Inspector gráfico por tipo, widget MENU (lado Designer) | 15 | André |
| F0 prueba de concepto en el HAL, F3 ventana, F4 pestaña, `generate-hmi`, Designer en VPP | 14 | Kenyi |
| Regenerar `.a`, primitivas TFT, objetos nuevos en `JWPLC_UI` (MENU, setpoint, texto fijo) | 13 | Jefe |

## Interfaces a acordar entre alphas

```text
jwplc-hmi-host/1   (getContext, loadProject, saveProject, writeGeneratedHeader,
                    listVariables, onLiveValues, onHostCommand, setDirty)
.jwhmi v2          (link.mode, plcBinding, revision, menus)
JWPLC_UI           (objetos nuevos, tabla de bindings {fieldId, puntero/tipo})
```

Cualquier cambio se revisa entre Alpha14 y Alpha15 y se documenta aquí.

## Seguimiento

- 2026-10-18: alpha.12 (VPP 2.1.0-alpha.29) unida a la rama del Designer. Ruta F0 para Kenyi, con primera
  prueba BOOL ↔ relé Q0_0, en `A15_F0_OPENPLC_RUTA_KENYI_20261018.md`.
