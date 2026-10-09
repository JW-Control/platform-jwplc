# Alpha13 — TFT-PRE6 — Integración reproducible del arranque ST7789

Fecha: 2026-10-09
Estado: `P0_PREPARED_NOT_EXECUTED`.

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
