# Alpha13 — TFT-PRE6 — Integración reproducible del arranque ST7789

Fecha: 2026-10-09
Estado: `P0_CLOSED_PASS`; `P1A_R3_CLOSED_PASS`; `P1B_PREPARED_NOT_EXECUTED`.

## Punto de partida validado

```text
PRE5_R3=PASS
PHYSICAL_USB_BACKGROUND_WHITE=RESOLVED_IN_TEMP_CANDIDATE
COMPILATION=SOURCE_FIRST_JWPLC_TFT_AND_TFT_ESPI_2_5_43
UPLOAD_COM4=PASS
SERIAL_PROVENANCE=PASS
SETUP_ENTRY_MS=676
PRODUCT_TFT_ARCHIVE_CURRENT=UNCHANGED
```

El source de producto y `libJWPLC_TFT.a` todavía son los anteriores.
**No interpretar PRE5 PASS como release integrado.**

## Plan en fases

### P0 — archive temporal desde los objetos físicos validados

```text
GATE=A13-TFT-PRE6-P0
PRODUCT_MUTATION=NO
HARDWARE_UPLOAD=NO
ARTIFACT_OUTPUT=%TEMP%
```

La primera corrida reutiliza los **dos objetos compilados source-first**
en TFT-PRE5 R3, cuya ejecución física corrigió el síntoma. En lugar de
recompilar fuentes modificadas aún no versionadas:

1. verifica branch, ancestro PRE5, working tree limpio y hashes de los archives
   `core.a`, `libJWPLC_Display.a` y `libJWPLC_TFT.a`;
2. localiza un `SUMMARY.log` de PRE5 en estado PASS con identidad serial;
3. verifica las identidades PRE4 del source `JWPLC_TFT.cpp` y setup temporal;
4. exige exactamente un `JWPLC_TFT.cpp.o` y un `TFT_eSPI.cpp.o` del build
   físico PRE5;
5. crea `libJWPLC_TFT.a` **sólo en %TEMP%** con el `gcc-ar` del toolchain,
   comprueba los dos miembros y su paridad byte a byte;
6. compila **tres consumidores normales** seleccionando el archive temporal:
   `04.Display_TFT_Direct`, `Display_UserUI_Callbacks` y `01_empty`;
7. rechaza cualquier uso de TFT_eSPI externa, fuente directa del backend,
   selección errónea de JWPLC_TFT o pérdida del core stub + archive;
8. exige finalmente working tree limpio y hashes oficiales intactos.

Su resultado PASS no reemplaza el gate P1, ya que P0 depende de los objetos
temporales PRE5 y no es una receta autónoma de reconstrucción desde fuente
productiva.

**Resultado real P0 (2026-10-09):** `STATUS=PASS`, archive de 1 091 942
bytes con SHA-256 `ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705`;
dos miembros bit-a-bit validados y tres compilaciones normales PASS sin TFT_eSPI
externa. Ver `A13_TFT_PRE6_P0_CLOSURE_20261009.md`.

**P1A preparado:** reproducción de los tres SHA exactos del candidato
físico PRE4 desde `JWPLC_TFT.cpp` y `tft_setup.h` canónicos junto a
TFT_eSPI 2.5.43 instalado con SHA fijados, trabajando sólo bajo
`%TEMP%`. P1A no compila ni sube firmware; P1B toma sus outputs para la
recompilación y la generación autónoma del archive.


### P1A — fuente candidata regenerada (cerrado)

```text
GATE=A13-TFT-PRE6-P1A
STATUS=PASS
REASON=CANONICAL_SOURCE_TRANSFORM_REPRODUCED
P0_HANDOFF_RECOVERED=YES
P1A_SOURCE_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
P1A_TFT_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
P1A_ST7789_INIT_SHA256=44873be82fe836084934a328df77f098e9ab88d212dd1d570da5e8aac74671bc
P1A_BACKEND_CPP_INSTRUMENTED_SHA256=3fe3c601830aad09e579e8fec1223c6a0e9b6a31a82f491743fbe166a5b16350
PRODUCT_MUTATED=NO
UPLOAD=NO
```

La evidencia de P0 fue recuperada mediante el archivo binario con SHA
correcto y los cierres versionados de PRE3/PRE4/P0. No se generó ni
falsificó el SUMMARY.log histórico. Detalle en
`A13_TFT_PRE6_P1A_CLOSURE_20261009.md`.

### P1B — reconstrucción nueva del archive (preparado)

A partir de `MANIFEST.json` de P1A y los cuatro SHA guardados,
compilar en una carpeta distinta:

```text
source JWPLC_TFT.cpp.o = 1
source TFT_eSPI.cpp.o = 1
JWPLC_TFT selected = verified temp P1A
TFT_eSPI selected = verified temp P1A
core stub + core.a = required
Display archive unchanged = required
```

Después construir un archive temporal con `gcc-ar` de la plataforma,
extraer sus dos miembros, comprobar paridad byte a byte y realizar las
tres compilaciones normales ya probadas en P0 sin seleccionar TFT_eSPI
externa. El nuevo archive no tiene por qué tener un SHA idéntico al de P0,
dado que es una recompilación independiente; la identidad de las fuentes,
los miembros y el enlace son los contratos.

```text
P1B=PREPARED_NOT_EXECUTED
PRODUCT_ARCHIVE_MUTATION=NO
UPLOAD=NO
NEXT_AFTER_P1B_PASS=P2_PACKAGE_INTEGRATION
```

### P1 — receta canónica y fuente productiva

Fijar y versionar una receta exacta de mantenimiento para TFT_eSPI 2.5.43,
verificada por SHA. Copiar el backend bajo `%TEMP%` y aplicar allí el
parche `DISPON` condicionado por `JWPLC_TFT_DEFER_DISPON`.
Aplicar a source de producto los cambios mínimos ya probados en PRE4,
con guards de hash y transformación exacta, **sin alterar la instalación
global** ni APIs públicas.

Exigir nueva compilación source-first desde los sources productivos,
comprobar `JWPLC_TFT.cpp.o` / `TFT_eSPI.cpp.o`, producir el nuevo
`libJWPLC_TFT.a`, verificar archivos del archive y su enlace autónomo
antes de sustituir el archive oficial.

### P2 — gate físico de package normal

Compilar y subir usando el FQBN normal `jwplc_local:esp32:jwplcbasic`,
con `JWPLC_TFT` precompilado y `TFT_eSPI` externa no seleccionada.
Validar estado Display/IO y power-cycle USB con video; no confundir el
redraw escalonado de IDLE con el problema de fondo blanco/GRAM.

### P3 — regresiones y cierre

Compilación de consumers Display y LogicRuntime_UI/ejemplos, auditoría de
regresión y hashes, diff y stage exacto de fuentes + archive, commit de
producto sólo tras PASS; actualizar checklist de Alpha13 y crear texto
de PR/pre-release en español cuando corresponda.

## Reglas de seguridad y prevención

```text
DO_NOT_CHANGE_CORE_A=YES
DO_NOT_CHANGE_JWPLC_DISPLAY_A=YES
DO_NOT_TOUCH_INSTALLED_TFT_ESPI=YES
DO_NOT_REQUIRE_TFT_ESPI_FOR_NORMAL_ARDUINO_USER=YES
DO_NOT_REMOVE_AUTOLOAD_PERIPHERALS=YES
DO_NOT_CHANGE_PUBLIC_API=YES
SOURCE_HASH_AND_ARCHIVE_MEMBER_PROOF=REQUIRED
F108_ARRAY_CARDINALITY_PREVENTION=REQUIRED
F105_COMMAND_VARIANT_PREVENTION=REQUIRED
F106_SAFE_REPLACEMENT_PREVENTION=REQUIRED
RELEASE_ARCHIVE_NOT_YET_CHANGED=YES
```
