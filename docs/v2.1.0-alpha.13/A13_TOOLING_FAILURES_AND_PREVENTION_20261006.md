# Alpha13 — Incidentes de tooling y prevención

Fecha: 2026-10-06

Este documento registra las clases de error nuevas observadas durante G1 y
complementa la fuente viva `JWPLC_ASSISTANT_FAILURES_AND_PREVENTION`.
Deben sincronizarse con esa fuente en la próxima actualización de fuentes del
Proyecto.

## F093 — escape de cmd.exe dentro de powershell -Command

Síntoma:

```text
token literal ^| recibido por PowerShell
ParserError antes de ejecutar el gate físico
```

Causa:

Se aplicó escape de `cmd.exe` a una expresión que debía parsear PowerShell.

Clasificación:

```text
PROCESS_FAILURE=YES
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
```

Prevención:

```text
BAT corto
evitar metacaracteres cross-shell
parser preflight de .ps1
fallo de wrapper antes de firmware => nunca clasificar producto
```

## F094 — artefactos de build dentro del working tree

Síntoma:

GitHub Desktop mostró decenas de `.o/.d/.bin/.elf/.map` generados por el gate.

Causa:

El build path se creó dentro de `tools/alpha13/results/<run>/build`.

Clasificación:

```text
PROCESS_FAILURE=YES
PRODUCT_FAILURE=NO
```

Prevención:

```text
build reproducible -> %TEMP%
results del repo -> sólo evidencia/logs deliberados
.gitignore defensivo en tools/<alpha>/results
```

## F095 — reader serial dependiente del encoding de consola

Síntoma:

```text
UnicodeEncodeError
console encoding=cp1252
serial byte inválido -> U+FFFD
thread serial_reader termina
```

Causa:

El cliente decodificaba bytes seriales con reemplazo Unicode y los imprimía
directamente a una consola que no podía representar ese carácter.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
```

Prevención:

```text
captura serial tolerante a bytes arbitrarios de boot
usar representación ASCII escapada/backslashreplace
un byte inválido nunca debe matar el thread de captura
```

## F096 — promover una hipótesis de timing a causa raíz sin evidencia suficiente

Síntoma:

Tras R2 se documentó como causa una posible pérdida del marcador READY antes
de demostrar que Ethernet hubiera alcanzado READY.

Evidencia posterior:

Durante esos intentos todavía faltaba la precondición física completa de
Ethernet. Al conectar RJ45 a una LAN con DHCP, R5 mostró:

```text
INI -> DHC -> READY
IP=192.168.0.31
```

Clasificación:

```text
ANALYSIS_FAILURE=YES
PRODUCT_FAILURE=NO
```

Prevención:

```text
hipótesis != causa raíz
instrumentar estado antes de afirmar causalidad
en gates Ethernet declarar/preflight:
USB serial + RJ45 link + DHCP/LAN cuando aplique
```

## F097 — usar digitalRead como readback contractual de un GPIO output-only

Síntoma:

```text
G2-P1 control leg
EN_IO_HIGH_REQUESTED=YES
OP_OK_MASK=31
digitalRead(EN_IO)=LOW
```

Las cinco piernas de fallo sí reprodujeron el defecto esperado, pero la pierna
control quedó REVIEW porque el harness interpretó ese LOW como estado físico
contractual.

Causa:

`initPeripherals()` configura GPIO27 mediante:

```cpp
gpio_set_direction((gpio_num_t)EN_IO, GPIO_MODE_OUTPUT);
```

Ese modo no habilita necesariamente el camino de entrada del pad. El propio
HAL Arduino advierte que `digitalRead()` / `gpio_get_level()` puede dar una
lectura inconsistente cuando el pin no está configurado como GPIO input.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
G2_BASELINE_DEFECT_EVIDENCE_LOST=NO
```

Prevención:

```text
para verificar una salida configurada output-only:
- comprobar GPIO_ENABLE_REG;
- comprobar el latch GPIO_OUT_REG;
- tratar gpio_get_level/digitalRead sólo como diagnóstico no contractual;
- no cambiar la configuración productiva del GPIO para satisfacer al harness.
```

Validación R2:

```text
CONTROL:
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=HIGH
EN_IO_PAD_READBACK=LOW
CONTRACT_PASS=True

FAULT_LEGS_1_TO_5:
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=LOW
CONTRACT_PASS=True

F097_RESOLVED=YES
```

## F098 — depender de Get-FileHash en un gate cuando ya existe helper portable

Síntoma:

```text
A13_G2_CANDIDATE_APPLY=FAIL
Get-FileHash no se reconoce como cmdlet
```

Causa:

El aplicador G2-P2 introdujo una dependencia nueva del cmdlet
`Get-FileHash` en lugar de reutilizar `Get-A13Sha256` de
`tools/alpha13/gates/common.ps1`, ya validado en gates anteriores.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
PATCH_APPLIED=NO
```

Prevención:

```text
reutilizar helpers versionados ya validados;
evitar introducir cmdlets nuevos sin preflight de disponibilidad;
hashes del harness Alpha13 -> Get-A13Sha256.
```

## F099 — gate versionado con PowerShell sintácticamente inválido

Síntoma:

```text
A13-G2-P2 parser preflight=FAIL
ValueExpressionRequired after -and
UnexpectedToken if
missing closing parenthesis/brace
```

Causa:

Una transformación automática dejó simultáneamente el fragmento antiguo del
contrato y el bloque nuevo, produciendo `... -and if (...)`.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
PHYSICAL_GATE_EXECUTED=NO
```

Prevención:

```text
BAT parser preflight remains mandatory;
after generated text replacements, inspect the replaced range for duplicate
control-flow fragments before publication;
parser failure before compile/upload never counts against product.
```

## F100 — usar un patch contextual como mecanismo de aplicación del candidato

Síntoma:

```text
baseline blob guards=PASS
git apply --check=FAIL
jwplc_peripherals.cpp patch does not apply
jwplc_peripherals.h patch does not apply
```

El fallo ocurrió antes de `git apply`; por tanto no hubo cambio productivo.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
PATCH_APPLIED=NO
ROOT_CAUSE_OF_CONTEXT_MISMATCH=NOT_REQUIRED_FOR_PRODUCT_DECISION
```

Prevención:

```text
cuando el baseline ya está fijado por blob SHA:
- aplicar transformaciones exact-once sobre anchors controlados;
- verificar después los blob SHA exactos del candidato;
- restaurar archivos en catch;
- no depender de matching contextual de un .patch para un gate local.
```

## F101 — allowlist de topología incompleto para el propio gate

Síntoma:

```text
STATUS=REVIEW
REASON=UNEXPECTED_HEAD_TOPOLOGY
unexpected:
  A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md
  run_a13_g2_p2_apply_candidate.bat
```

Causa:

El gate no incluyó en su allowlist dos rutas que ya formaban parte de la cadena
versionada necesaria para ejecutarlo.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
PHYSICAL_GATE_EXECUTED=NO
```

Prevención:

```text
antes de publicar un gate con topología cerrada:
- enumerar git diff --name-only BASE..HEAD;
- comparar esa lista completa contra el allowlist;
- incluir wrappers, docs y registros de fallos del propio gate;
- si el apply falla, no ejecutar manualmente el test físico.
```

## F102 — wrapper forzó PowerShell legacy al generar bytes productivos

Síntoma:

```text
A13_G2_CANDIDATE_APPLY=FAIL
FILE=jwplc_peripherals.cpp
actual=2e54c7950e2d23db2c19548e56f5b81fd483f92f
expected=875a50fd64552e8c4a4300e07b9494d2c32605d7
```

Causa demostrada:

El entorno interactivo del desarrollador es PowerShell Core 7.6.6, pero los
wrappers BAT de G2-P2 invocaban explícitamente `powershell.exe`. En Windows
eso desvió la ejecución al host PowerShell legacy. El `.ps1` versionado
estaba en UTF-8 sin BOM y contenía literales acentuados usados para construir
bytes del archivo productivo; ese host reinterpretó los caracteres con la code
page local:

```text
todavía -> todavÃ­a
están   -> estÃ¡n
```

La variante mojibake produce exactamente:

```text
SHA=2e54c7950e2d23db2c19548e56f5b81fd483f92f
```

igual al hash observado en el host.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
PHYSICAL_GATE_EXECUTED=NO
WORKTREE_RESTORED_BY_CATCH=YES
```

Prevención:

```text
si un script PowerShell genera bytes productivos:
- host estándar JWPLC = `pwsh` / PowerShell Core 7+;
- los BAT materiales deben resolver/requerir `pwsh` explícitamente;
- imprimir la versión de PowerShell al inicio del gate;
- no hacer fallback silencioso a `powershell.exe`;
- usar literales ASCII-only o construir Unicode explícitamente;
- no depender de la interpretación de encoding del host;
- generar primero el candidato bajo %TEMP%;
- verificar allí los blob SHA exactos;
- sólo después copiar bytes al working tree;
- parsear todos los .ps1 del flujo antes de tocar producto.
```

## F103 — parsear todo el ruido serial en vez del bloque validado por el cliente

Síntoma:

```text
G2-P2 R4
STEP0_CLIENT_EXIT=0
STEP2_CLIENT_EXIT=0
A13_G2_CLIENT_BLOCK=PASS
pero STEP0_CONTRACT_PASS=False y STEP2_CONTRACT_PASS=False
summary keys vacíos
```

Causa demostrada:

El cliente serial sí encontró y validó un bloque completo. Sin embargo, el gate
PowerShell volvió a parsear toda la salida del cliente, incluyendo líneas de
boot y bloques parciales/repetidos impresos antes del bloque aceptado.

`Get-A13LogValue()` exige exactamente una ocurrencia por clave. Cuando el
stream contenía dos `OP_OK_MASK`, `EN_IO_*` o `IO_VIEW_READY`, el helper
devolvía `null` aunque el cliente hubiera cerrado `PASS`.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
CLIENT_VALIDATED_BLOCK=PASS
SOURCE_FIRST_BEHAVIOR_VISIBLE=PASS
FORMAL_G2_P2=REVIEW_UNTIL_R5
```

Prevención:

```text
el cliente que valida framing serial debe emitir un resumen canónico con
prefijo único A13_G2_CLIENT_*;
el gate debe consumir sólo ese resumen normalizado;
el ruido serial bruto permanece en logs como evidencia, pero no como fuente
contractual para parsear resultados.
```

## F104 — usar GPIO_ENABLE_REG/GPIO_OUT_REG para un pin >= 32

Síntoma:

```text
TFT-PRE1
TFT_CS=GPIO33
TFT_CS_OUTPUT_ENABLE=NO
TFT_CS_OUTPUT_LATCH=LOW
```

Causa:

El probe reutilizó el patrón de observabilidad de EN_IO (GPIO27) y leyó
`GPIO_ENABLE_REG` / `GPIO_OUT_REG`, que cubren GPIO0..31. Para GPIO33 debe
usarse el banco alto (`GPIO_ENABLE1_REG` / `GPIO_OUT1_REG`) y desplazar
`pin - 32`. Por tanto las dos líneas de CS de PRE1 no representan el estado
real del pin.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
TFT_PRE1_TIMING_INVALIDATED=NO
TFT_PRE1_PASS_INVALIDATED=NO
CS_DIAGNOSTIC_INVALID=YES
```

El contrato de TFT-PRE1 no dependía del estado de CS; `SETUP_ENTRY_MS`,
`DISPLAY_READY`, `IO_READY` y las verificaciones de ruta precompilada siguen
siendo válidas.

Prevención:

```text
GPIO 0..31  -> GPIO_ENABLE_REG / GPIO_OUT_REG
GPIO 32+    -> GPIO_ENABLE1_REG / GPIO_OUT1_REG
no reutilizar una máscara 1UL << pin sin seleccionar primero el banco
si el dato es sólo diagnóstico, marcarlo explícitamente y no elevarlo a causa
```

Validación preventiva:

El probe TFT-PRE1 se corrige para seleccionar el banco de registros según el
número de GPIO. No se exige repetir PRE1 porque el dato CS no era contractual.

## F105 — asumir nombres simbólicos concretos en ST7789_Init.h

Síntoma:

```text
TFT-PRE3
TFT_ESPI_VERSION=2.5.43
ST7789_DELAY_VALUES_MS=120,10,120,120,120,10,120,120
ST7789_SLPOUT_COUNT=0
ST7789_NORON_COUNT=0
ST7789_DISPON_COUNT=0
STATUS=REVIEW
REASON=ST7789_INIT_SEQUENCE_UNEXPECTED
```

Causa:

El parser de PRE3 buscó únicamente los identificadores `TFT_SLPOUT`,
`TFT_NORON` y `TFT_DISPON`. La implementación seleccionada de TFT_eSPI
2.5.43 puede expresar esos mismos comandos como `ST7789_SLPOUT`,
`ST7789_NORON`, `ST7789_DISPON` o como valores literales `0x11`, `0x13`,
`0x29`. Por tanto los conteos cero no prueban ausencia de esos comandos.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
ENVIRONMENT_FAILURE=NO
SOURCE_ATTRIBUTION_VALID=YES
VERSION_AND_HASHES_VALID=YES
SEQUENCE_CLASSIFICATION=PENDING_R2
```

Prevención:

```text
parsear llamadas writecommand(...) y normalizar equivalentes semánticos
SLPOUT = TFT_SLPOUT | ST7789_SLPOUT | 0x11
NORON  = TFT_NORON  | ST7789_NORON  | 0x13
DISPON = TFT_DISPON | ST7789_DISPON | 0x29
reportar también la expresión capturada y el delay posterior
no usar presencia de un nombre de macro como contrato funcional
```

## F106 — usar JavaScript String.replace con `$'` dentro de PowerShell generado

Síntoma:

```text
TFT-PRE3 R2
A13_TFT_PRE3_SYNTAX=FAIL
PowerShell: falta cierre de ')' / '}'
script contiene cola duplicada después del bloque finally
PHYSICAL_OR_PRODUCT_ACTION=NO
```

Causa demostrada:

Durante la edición remota del gate se usó JavaScript `String.replace(old, replacementString)`.
El replacement contenía expresiones PowerShell/regex que terminaban en `$'`.
En JavaScript, `$'` dentro del replacement string no es literal: significa
`the portion of the input after the matched substring`. El runtime insertó la
cola del archivo dentro del replacement, duplicó grandes bloques y dejó una
cadena regex sin cerrar.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
ENVIRONMENT_FAILURE=NO
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
WRAPPER_SYNTAX_PREFLIGHT_PROTECTED_PRODUCT=YES
TFT_PRE3_R1_EVIDENCE_INVALIDATED=NO
```

Prevención:

```text
al editar código mediante JavaScript:
- no usar String.replace(old, replacementString) si el replacement puede contener `$`;
- usar String.replace(old, () => replacementString), slice/concat o transformación estructural;
- reconstruir desde el último blob/commit sintácticamente validado cuando haya corrupción;
- verificar conteos estructurales de secciones únicas antes de publicar;
- mantener parser preflight pwsh en el BAT antes de cualquier compile/upload/product action.
```

Corrección aplicada:

```text
PRE3-R3 rebuilt from known-good R1 commit=b2b2b2b3
semantic parser retained
replacement mechanism=callback-safe
banner_count=1
switch_count=1
pass_marker_count=1
```

## F107 — inyectar setup TFT_eSPI por una ruta no visible al backend temporal

Síntoma:

```text
TFT-PRE5 R1
COMPILE_EXIT=1
temp JWPLC_TFT selected=YES
temp TFT_eSPI selected=YES
missing TFT_MAD_COLOR_ORDER / TFT_CASET / TFT_PASET / TFT_RAMWR / TFT_DRIVER
STATUS=REVIEW
REASON=SOURCE_FIRST_CANDIDATE_COMPILE_FAILED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
UPLOAD_EXECUTED=NO
```

Causa:

PRE5 R1 construyó una copia temporal completa de TFT_eSPI y trató de fijar
la configuración JWPLC sustituyendo `User_Setup.h` / `User_Setup_Select.h`
dentro de esa copia. Esa estrategia no reprodujo el mecanismo de discovery
con el que TFT_eSPI 2.5.43 fue cualificado originalmente.

El contrato nativo de TFT_eSPI 2.5.43 busca primero `tft_setup.h` mediante
`__has_include(<tft_setup.h>)`. La compilación histórica JWPLC ya dependía de
ese mecanismo. En R1 el backend temporal terminó sin las definiciones de
driver ST7789 visibles durante `TFT_eSPI.cpp`, de ahí la familia de símbolos
de driver no declarados.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
COMPILE_FAILED_BEFORE_UPLOAD=YES
CANDIDATE_RUNTIME_NOT_EXECUTED=YES
```

Prevención:

```text
- no reemplazar User_Setup/User_Setup_Select para este flujo;
- copiar el probe a %TEMP% y colocar tft_setup.h candidato junto al .ino;
- dejar que TFT_eSPI 2.5.43 use su discovery nativo __has_include;
- mantener TFT_eSPI global sin mutaciones;
- exigir selección de librerías temporales y objetos source-first antes de upload.
```

Corrección:

```text
PRE5-R2 setup source=sketch-local tft_setup.h in %TEMP%
candidate setup SHA must equal PRE4 qualified SHA
temporary TFT_eSPI retains its native User_Setup_Select.h
only ST7789_Init.h candidate is patched in temp backend
```

## F108 — salida escalar de Get-ChildItem evaluada como colección en PRE5

Síntoma confirmado en TFT-PRE5 R2:

```text
A13_TFT_PRE5_SYNTAX=PASS
SERIAL_PORT=COM4
STATUS=REVIEW
REASON=UNEXPECTED_GATE_EXCEPTION
EXCEPTION=No se encuentra la propiedad "Count" en este objeto
COMPILE_EXIT=NOT_REACHED
UPLOAD_EXIT=NOT_REACHED
```

Causa más directa identificada en el código: el gate usaba
`$probeIno = Get-ChildItem ... -Filter '*.ino'` seguido de
`$probeIno.Count`. Con una sola coincidencia, PowerShell puede entregar
un objeto escalar sin propiedad `Count` utilizable bajo
`Set-StrictMode -Version Latest`. La ejecución llegó al punto donde
se copia el sketch temporal, después del preflight de puerto y resolución
de fuente TFT_eSPI; no llegó al `COMPILE_EXIT` del candidato.

Clasificación:

```text
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO_EVIDENCE
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
CANDIDATE_COMPILED=NO
UPLOAD_EXECUTED=NO
VISUAL_FIX_CONFIRMED=NO
```

Prevención obligatoria:

```text
- envolver resultados de cmdlets con cardinalidad variable en @(...);
- comprobar .Count sobre arrays explícitos, no sobre objetos ambiguos;
- hacer el preflight de cardinalidad antes de compilaciones costosas;
- informar PHASE, tipo y línea de excepción en el SUMMARY;
- asegurar selección de ambos sources temporales + .o exactos;
- introducir guardia de compilación para comprobar que TFT_eSPI.cpp
  ve ST7789_DRIVER y JWPLC_TFT_DEFER_DISPON;
- verificar SHA de core.a y de archives originales antes/después;
- fallar cerrado antes de upload si cualquier contrato no se cumple;
- reservar el PASS visual para evidencia posterior a power-cycle físico.
- insertar identificador único del candidato en cada bloque serial canónico,
  verificar SHA de identidad del firmware recibido y rechazar una lectura
  procedente del antiguo probe PRE1 o de un firmware no actualizado.
```

Acción ejecutada: TFT-PRE5 R3 reforzó validaciones de compilación
y regresión sin cambiar la identidad candidata PRE4. Resultado verificado:

```text
A13_TFT_PRE5_SYNTAX=PASS
PROBE_INO_COUNT=1
BACKEND_CONFIG_GUARD=ENABLED
BACKEND_SOURCE_CONFIG_GUARD=ENABLED
COMPILE_EXIT=0
JWPLC_TFT_TEMP_SELECTED=True
TFT_ESPI_TEMP_SELECTED=True
JWPLC_TFT_SOURCE_OBJECT_COUNT=1
TFT_ESPI_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=0
RUNTIME_CANDIDATE_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
DISPLAY_READY=YES
IO_READY=YES
WORKTREE_FINAL=CLEAN
STATUS=PASS
REASON=SOURCE_FIRST_CANDIDATE_FLASHED
USER_VISUAL_WHITE_BACKGROUND=NOT_OBSERVED
```

PRE5 confirma que la corrección temporal de `DISPON` y borrado de
GRAM funciona visualmente, sin nuevas fallas. No abrir un identificador
adicional por una ejecución que pasó. Mantener `NEXT_FAILURE_ID=F109`.
El archive TFT productivo requiere regeneración reproducible y prueba
normal antes de cerrar la observación a nivel release.

### Prevención aplicada a TFT-PRE6-P0 (sin fallo nuevo)

```text
P0_INPUT=objetos fuente ya validados físicamente en PRE5_R3
P0_OUTPUT=archive autocontenido únicamente en %TEMP%
ARRAY_CARDINALITY=usar @(...) para todos los resultados de búsqueda
PRECOMPILED_MEMBER_SET=JWPLC_TFT.cpp.o,TFT_eSPI.cpp.o
PRECOMPILED_MEMBER_SHA_PARITY=REQUIRED
NORMAL_COMPILATION_CASES=3
GLOBAL_TFT_ESPI_SELECTION=FORBIDDEN
CORE_A_AND_DISPLAY_A_AND_OLD_TFT_A=HASH_GUARDED
NO_UPLOAD_UNTIL_PRODUCT_ARCHIVE_AND_SOURCE_REFRESH=YES
NO_NEW_FAILURE_ID_WITHOUT_NEW_FAILURE=YES
```

Importante: un archive construido desde los objetos temporales PRE5
puede confirmar que las unidades compiladas se empaquetan y enlazan,
pero **todavía no demuestra** que se puedan reconstruir de forma
autónoma desde sources canónicos. Esa demostración corresponde a P1
y nunca se reemplaza por el PASS de P0.

### Prevención aplicada a TFT-PRE6-P1A (sin fallo nuevo)

```text
INPUT_SOURCE_GIT_BLOBS=PINNED
TFT_ESPI_VERSION=2.5.43
BACKEND_CPP_HEADER_INIT_SHA256=PINNED
WINDOWS_CHECKOUT_NEWLINES=ONLY_RAW_OR_CRLF_TO_LF_NORMALIZATION
CANDIDATE_CPP_SETUP_INIT_SHA256=MUST_EQUAL_PHYSICALLY_VALIDATED_PRE4
TARGET=FRESH_OS_TEMP_CHILD
INSTALLED_TFT_ESPI_MUTATION=FORBIDDEN
PRODUCT_REPO_MUTATION=FORBIDDEN
NO_COMPILE=YES
NO_UPLOAD=YES
SOURCE_BUILD_ARCHIVE_REFRESH=BLOCKED_UNTIL_P1A_PASS
```

P1A implementa la transformación en Python con anclas únicas,
reemplazos deterministas y comprobación de los tres SHA exactos antes
de crear el candidato. La instrumentación adicional del backend se
escribe sólo en la copia temporal, preservando bytes originales.
Se mantiene `NEXT_FAILURE_ID=F109`; el éxito de P0 no genera un
nuevo identificador de fallo.

## F109 — dependencia de un SUMMARY.log local ausente entre gates

La primera corrida TFT-PRE6-P1A R1 reportó:

```text
STATUS=REVIEW
REASON=P0_PASS_PROOF_NOT_FOUND
PHASE=P0_PROVENANCE
HARNESS_FAILURE=YES
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
PRODUCT_REPO_MODIFIED=NO
```

El diagnóstico ampliado R2 (2026-10-09) aisló el motivo **observado**:

```text
PROOF_PARSER_SELF_TEST=PASS
PROOF_SEARCH_FILTER=tft_pre6_p0_*
PROOF_SEARCH_DIRECTORY_COUNT=1
PROOF_SUMMARY_MISSING=tools/alpha13/results/tft_pre6_p0_20261009_123147/SUMMARY.log
P1A_R2=REVIEW_HARNESS
```

La ruta y el directorio P0 existen, pero `SUMMARY.log` no fue
accesible mediante `Test-Path -LiteralPath` en R2. **El parser no es
la causa** de R2; la hipótesis R1 sobre `return Get-A13LogValue`
quedó refutada por `PROOF_PARSER_SELF_TEST=PASS`.
No hay pruebas suficientes para afirmar si el archivo fue eliminado,
movido o nunca persistió fuera del proceso previo; no especular sobre
`git clean`, sincronización o herramientas de limpieza sin evidencia.

P0 sigue **CLOSED_PASS**: su salida de terminal fue recibida y su
cierre fue registrado y versionado en
`docs/v2.1.0-alpha.13/A13_TFT_PRE6_P0_CLOSURE_20261009.md`,
con SHA del archive
`ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705`,
paridad de miembros y tres compilaciones normales exitosas.

**Clasificación:** `HARNESS_FAILURE=YES`; dependencia demasiado fuerte
de un único archivo de resultados local no versionado. No es fallo de
TFT, de archivo binario ni del puerto serie.

Prevención:

```text
- separar evidencia durable versionada de logs locales efímeros;
- requerir identidad SHA y contenido del cierre versionado de P0;
- cuando exista archive P0 local, verificar bytes y SHA además del registro;
- no marcar SUMMARY ausente como fallo del P0 anterior;
- no fingir ni regenerar SUMMARY.log histórico;
- detenerse si no hay evidencia durable verificable;
- no repetir builds ya cerrados exclusivamente por pérdida de log;
- mostrar ruta y presencia de archive/log antes de elegir recuperación;
- mantener checks de SHA, source y resultado de gate independientes;
- conservar no compile/no upload/no product mutation en P1A.
```

```text
P1A_R1=REVIEW_HARNESS
P1A_R2=REVIEW_HARNESS
P0_RESULT=CLOSED_PASS
ROOT_CAUSE_CLASS=LOST_OR_INACCESSIBLE_LOCAL_EVIDENCE
PRECISE_FILE_DISAPPEARANCE_CAUSE=NOT_ESTABLISHED
PRODUCT_FAILURE=NO
HARDWARE_FAILURE=NO
```

### F109 — recuperación verificada y prevención de R3

Diagnóstico adicional ejecutado por el usuario:

```text
P0_DIRECTORY_EXISTS=True
P0_SUMMARY_EXISTS=False
P0_ARCHIVE_EXISTS=True
P0_ARCHIVE_BYTES=1091942
P0_ARCHIVE_SHA256=ff9dd89cb267bc270d2ca6fa2d0f1362b76595b8dc6b49f764d05a8e28bb6705
```

La evidencia binaria crítica de P0 **no desapareció**; faltaba
únicamente el archivo histórico `SUMMARY.log` del directorio P0.
Su causa de ausencia no se conoce. P0 conserva el cierre versionado
`A13_TFT_PRE6_P0_CLOSURE_20261009.md`.

R3 incorpora un modo de recuperación **sin fabricar el SUMMARY**:
verifica el blob Git de los documentos versionados P0, PRE4 y PRE3,
el contenido exacto de sus campos críticos y los SHA actuales del
archive P0 y del backend TFT_eSPI. El SHA de los documentos se resuelve
desde `HEAD:<ruta>` para evitar falsos fallos por CRLF del checkout.
El working tree debe estar CLEAN. Ante una discordancia, abortar.

```text
P0_PRIOR_RESULT=CLOSED_PASS
RECOVERY_PROOF=VERSIONED_P0_PRE4_PRE3_DOCS_PLUS_P0_ARCHIVE_BYTES_SHA
P0_SUMMARY_RECREATED=NO
P0_REBUILD_REQUIRED=NO
P1A_R3=PREPARED_NOT_EXECUTED
NEXT_FAILURE_ID=F110
```

### F109 — cierre por recuperación durable verificada

```text
P1A_R3=PASS
P0_HANDOFF_RECOVERED=YES
P0_HISTORICAL_SUMMARY=NOT_REQUIRED_MISSING
SOURCE_OF_TRUTH=VERSIONED_P0_PRE4_PRE3_DOCS_PLUS_P0_ARCHIVE_SHA
CANONICAL_SOURCE_TRANSFORM_REPRODUCED=YES
PRODUCT_REPO_MUTATED=NO
INSTALLED_TFT_ESPI_MUTATED=NO
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
```

La corrida R3 cerró sin omitir comprobaciones de SHA ni reconstruir
el `SUMMARY.log` antiguo. Los tres SHA de los fuentes del candidato
coincidieron con los validados físicamente. F109 queda resuelto
mediante comprobación de evidencia versionada y bytes del archive.

Prevención P1B:
- consumir un manifiesto P1A válido y los fuentes exactos con SHA guardado;
- no depender de un único SUMMARY.log local efímero;
- compilar y verificar los dos objetos desde los fuentes regenerados,
  sin tomar los miembros archive de PRE5 ni P0;
- regenerar el archive sólo bajo `%TEMP%`;
- exigir paridad de miembros y tres compilaciones normales sin TFT_eSPI
  externa;
- abortar antes de cualquier modificación al archive productivo;
- mantener `NEXT_FAILURE_ID=F110` hasta un fallo nuevo comprobado.



### Prevención TFT-PRE6-P2A (no es un fallo nuevo)

P1B terminó en PASS de recompilación de fuentes, dos miembros del
archive y tres compilaciones normales. El gate P2A incorpora barreras:

```text
P1B_ARCHIVE_SHA256=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
P1B_ARCHIVE_BYTES=1091990
P1B_MEMBER_PARITY=PASS
P1A_CPP_SHA256=494440b00e74e74a7b23975420574e035ef2138fdf5a0cea2fed51c986c29d25
P1A_SETUP_SHA256=8fa079444ca130772d3a642eaf814035100bc098032b403c922c458d351ae5e1
PRODUCT_MUTATION=ONLY_THREE_TRACKED_JWPLC_TFT_FILES
ORIGINAL_BACKUP=TEMP_BYTE_IDENTICAL
PRECONDITION=GIT_WORKTREE_CLEAN
RELEASE_ARCHIVE_ADOPTION=LOCAL_UNCOMMITTED
NORMAL_COMPILE_CASES=4
ROLLBACK_ON_GATE_FAILURE=YES
PHYSICAL_UPLOAD=NO
GIT_COMMIT=NO
```

- No aceptar un `SUMMARY.log` antiguo como única prueba de procedencia:
  exigir manifiesto P1B y SHA del archive actualmente presente.
- Actualizar juntos `JWPLC_TFT.cpp`, `tft_setup.h` y
  `libJWPLC_TFT.a`. No cambiar sólo el binario.
- Respaldar los tres archivos antes de reemplazarlos; si cualquier
  copia, SHA, diff o compilación falla, restaurar exactamente los
  originales y verificar worktree limpio.
- Inspeccionar la selección exacta de la JWPLC_TFT del repositorio,
  no confundirla con las temporales, y exigir core precompilado.
- Rechazar objetos TFT source-first y dependencia externa TFT_eSPI
  durante compilación de sketch normal.
- Dejar el worktree explícitamente modificado y sin commit después
  de P2A PASS para completar gate físico P2B. No hacer
  `git pull`/reset/checkout de otros cambios mientras esté sucio.
- El archive productivo debe probarse físicamente antes de
  commit/release. F109 permanece cerrado; mantener F110 para fallo
  nuevo verificado.

### Cambio de flujo 2026-10-09 — sin fallo nuevo

```text
P1B=PASS_ARCHIVE_REBUILT_SOURCE_FIRST
P2A=PASS_PRODUCT_LOCAL_ADOPTION
P2A_NORMAL_BUILDS=4/4
P2A_PRODUCT_REMOTE_COMMIT=NO
P2A_PHYSICAL_UPLOAD=NO
P2B=NEXT_PHYSICAL_PENDING
F109=CLOSED
NEXT_FAILURE_ID=F110
```

Se adopta `docs/JWPLC_COLLABORATION_WORKFLOW.md` como regla
transversal: etapas seguras automatizadas dentro de **un gate por hito**,
con cortes sólo en fronteras de seguridad, incertidumbre física o
aprobación del usuario. Preflight sintáctico y semántico antes del
primer comando; logs por fase y manifiesto durable de hashes para
no depender de `SUMMARY.log` efímero; clasificación fiel
`PRODUCT/HARNESS/HARDWARE/ENVIRONMENT/PRECONDITION` cuando haya
evidencia suficiente.

No crear múltiples IDs de fallo por reintentos de la misma causa.
Conservar abort/rollback probado. No declarar PASS visual sin
confirmación humana y no hacer commit/product release antes de
prueba física del archive normal. No volver a recomendar pull o reset
sin revisar el worktree intencionalmente modificado.

La política de **hitos integradores** sustituye la obligación de
interacción de usuario para cada microgate; **NO** elimina pruebas ni
rastreo de resultados, y tampoco crea un nuevo fallo F110.

## Estado

```text
NEXT_FAILURE_ID=F110
SYNC_TO_PROJECT_FAILURES_SOURCE=PENDING
```

## F110 — firmas duplicadas en generación de candidato G3 (2026-10-10)

**Clase:** `HARNESS / GENERACIÓN DE CÓDIGO`, no fallo demostrado del hardware ni de un firmware publicado.

### Reproducción observada

```text
GATE=A13-G3-INTEGRATED
RUN_ID=20261010_084655_3f2ccaf9
PREFLIGHT=PASS
ADOPTION=PASS
BUILD_OFFICIAL_CORE_EXIT=1
STATUS=REVIEW
ROLLBACK=PASS
PRODUCT_COMMIT=NO
UPLOAD=NOT_EXECUTED
```

El `source-Basic.log` aportado por el operador evidencia siete errores de
declaración duplicada en los cuatro archivos de origen adoptados:
`bool bool` (tres funciones TCA), `int int` (jwplcI2C_updateBit) y
`void void` (tres funciones del runtime).
Causa raíz: el transformador usado para generar el candidato preservó
el especificador de retorno original y añadió otro, mientras la revisión
estructural inicial no comprobó firmas exactas.

No hubo daño en producto publicado: la compilación se detuvo antes del
upload y el respaldo restauró el core original.
`git status --short` posterior vacío;
`core.a SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983`.

### Corrección y prevención

- Corregir todas las firmas **en los candidatos versionados**, sin parchear
  manualmente las fuentes productivas ni duplicar el gate.
- Añadir preflight previo a adopción que rechace tipos de retorno duplicados,
  verifique firmas exactas y el contrato de mutex y shadow.
- En fallos de construcción, extraer `source-Basic.log` y mostrar errores
  concretos además del código del wrapper PowerShell.
- Calificar nuevamente fuente→core.a→enlace normal→regresiones→prueba física;
  no considerar PASS hasta obtener resultados del nuevo intento.
- Respetar rollback byte-identical, worktree limpio y frontera de cargas
  desconectadas para las dos salidas de relé del ensayo.

```text
F110=HARNESS_CANDIDATE_GENERATION
ROOT_CAUSE=DOUBLE_RETURN_TYPE
CORRECTIVE_TOOLING_COMMIT=4da9285097e982168577a90be770ff1112b84adc
RETEST=NOT_EXECUTED
F110_RECURRENCE=PREVENTION_CHECK_READY
NEXT_FAILURE_ID=F111
```

### F110 — cierre verificado tras el gate integrado G3

F110=CLOSED_HARNESS_REMEDIATED
G3_20261010_091949_a04d2223=20261010_091949_a04d2223
SOURCE_CORE=PASS
NORMAL_PRECOMPILED_LINK=PASS
REGRESSIONS=3_OF_3_PASS
PHYSICAL=150_OF_150_PASS
NEXT_FAILURE_ID=F111

Las siete firmas duplicadas no reaparecieron. No crear nuevos IDs por esta
misma causa sin nueva evidencia.

## F111 — G4: SHA pinneado truncado para ST7789_Init.h (2026-10-10)

**Clasificación:** `HARNESS_FAILURE` (identidad de artefacto fijada incorrectamente en el ejecutor); no fallo del entorno instalado, producto ni TFT físico.

### Evidencia de la segunda ejecución G4

```text
G4_HARNESS_PY_SYNTAX=PASS
PREFLIGHT=PASS
USB_VID_PID=1A86:7523
BACKEND_RECIPE=RUNNING_THEN_REVIEW
BACKEND_INIT_SHA256_ACTUAL=e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10
BACKEND_UPSTREAM_SHA256_ACTUAL=e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10
STATUS=REVIEW
REASON=BACKEND_UPSTREAM_SHA_NOT_PINNED
PRODUCT_ADOPTION=NOT_EXECUTED
PHYSICAL_UPLOAD=NOT_EXECUTED
```

La instalación local de `TFT_eSPI 2.5.43` coincidía **exactamente** con el tag oficial y con el hash `backend_init` del gate TFT-CLOSURE ya publicado. El ejecutor G4 había copiado una constante incompleta, sin la secuencia `67c` en el tramo `...eca67ca...`, y por ello rechazó dos orígenes correctos. La rama de recuperación descargó correctamente el origen oficial, pero comparó contra la misma constante incorrecta.

### Corrección preventiva

- Restaurar el SHA-256 completo de 64 caracteres **sin modificar el backend instalado**.
- Verificar en preflight todos los SHA pinneados y confrontar `backend_init` y `patched_init` contra `a13_tft_closure.py` versionado; fallar con `HARNESS` antes de tocar archivos o red.
- Mantener la receta de parche ST7789 original y comprobación de hash temporal.
- Preservar `core.a`, `Display.a`, autoload y las fuentes/archivos productivos; no abrir gates G1/G2/G3/TFT-CLOSURE.
- El siguiente reintento integrado es la primera oportunidad de validar realmente reconstrucción G4, enlace de TFT y pruebas físicas. No marcar F111 cerrado ni G4 PASS antes de esa evidencia.

```text
F111=HARNESS_BACKEND_PIN_TRUNCATION
ROOT_CAUSE=G4_BAD_PIN_LITERAL
LOCAL_BACKEND_DEFECT=NO
UPSTREAM_SOURCE_DEFECT=NO
G4_PRODUCT_MUTATION=NO
G4_PHYSICAL_UPLOAD=NO
RETEST=NOT_EXECUTED
NEXT_FAILURE_ID=F112
```

### F111 — cerrado tras G4 PASS
F111=CLOSED_HARNESS_REMEDIATED
RUN_ID=20261010_141745_907ce6dc
ROOT_CAUSE=TRUNCATED_SHA_LITERAL_IN_G4_HARNESS
PHYSICAL=50_OF_50_PASS
NORMAL_REGRESSIONS=4_OF_4_PASS
NEXT_FAILURE_ID=F112

El error estaba en el pin del harness, no en el backend instalado.
