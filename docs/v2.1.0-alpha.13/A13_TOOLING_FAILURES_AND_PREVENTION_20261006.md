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

## F112 — cierre Git G4: Python/PowerShell, HEAD local y EOL LF/CRLF (2026-10-10)

**Clasificación:** `HARNESS / FLUJO DE COLABORACIÓN / PORTABILIDAD WINDOWS`.
**Estado:** `CLOSED_PREVENTION_REGISTERED` tras cierre G4 real. No es fallo
de `JWPLC_TFT`, de `core.a` ni de la prueba física.

### Incidentes comprobados en el mismo cierre G4

| Manifestación | Evidencia compartida | Causa o clasificación |
|---|---|---|
| Ventana de Windows para elegir programa antes de iniciar el finalizador | Primer lanzamiento interrumpido tras mostrar `LOCAL_HEAD` y `REMOTE_HEAD` | Fallo del método de invocación de Python en PowerShell; la operación precisa que abrió la ventana no quedó registrada, **no atribuirla sin evidencia** a un mecanismo específico de asociaciones |
| Invocación con tres rutas de Python concatenadas | `PYTHON_EXE=C:\Windows\system32\python C:\Users\...\Python311\python.exe C:\Users\...\WindowsApps\python.exe`; PowerShell no reconoció el conjunto | Uso de `(Get-Command python -CommandType Application).Source` sin seleccionar una sola aplicación: salida múltiple convertida en cadena |
| `HEAD local inesperado` en el primer comando de cierre | Local `b15c59c...`, mientras la instrucción solo admitía `bc9e7c9...` | Suposición incorrecta sobre el HEAD local tras integración de tooling; faltaba validar la cadena de commits permitida |
| `LOCAL_TOOLING_NOT_HEAD_IDENTICAL_tools/alpha13/gates/a13_g4_finalize.py` | El finalizador había validado Git, producto y evidencia; se detuvo antes de autorización | Comparación de `git show HEAD:path` **byte a byte** contra `Path.read_bytes()`; susceptible a falsos negativos por conversiones LF/CRLF. No se recuperó un volcado de EOL local que permita atribuir la discrepancia exclusivamente a CRLF |

Estos abortos **no invalidaron** el gate G4: anteriormente se habían
obtenido `PASS_PHYSICAL_AND_REBUILT_NORMAL_ARCHIVE`, 50/50 ciclos,
cuatro regresiones, evidencia visual del operador y tres fuentes/binarios
TFT adoptados localmente. Durante los abortos de cierre no se hizo
upload nuevo, rollback destructivo ni commit productivo.

### Corrección aplicada y verificación observada

- Invocación explícita del ejecutable existente
  `C:\Users\jeykc\AppData\Local\Programs\Python\Python311\python.exe`,
  **no** una lista de `Get-Command` y **no** una asociación de `.py`.
- Script extraído del commit remoto a `%TEMP%`; igualdad de blob Git
  verificada antes de pasarlo como argumento a `python.exe -B`.
- HEAD local admitido únicamente si pertenece al conjunto de commits
  ancestrales esperado, validando padres y alcance remoto **solo tooling**.
  No ejecutar `git pull` sobre los tres archivos productivos modificados.
- Identidad de textos: `git diff --quiet HEAD -- path` y
  `git diff --cached --quiet HEAD -- path`, y comparación del blob
  indexado `git rev-parse :path` con `git rev-parse HEAD:path`.
  **No** comparar bytes crudos de archivos de texto de checkout contra
  blobs Git. Para `.a` y otros binarios conservar SHA-256 de bytes reales.
- Última ejecución del operador verificó:
  `LOCAL_TOOLING=PASS_GIT_NORMALIZED_EOLS`,
  `REMOTE_PREFLIGHT=PASS_3_TOOLING_COMMITS_ONLY`,
  `READY_FOR_OPERATOR_AUTHORIZATION=YES`, y posteriormente
  `PUSH=PASS_NON_FORCE`, `STATUS=CLOSED_PASS`.
  Commits: producto `46a695756ecae205f585e59bf8e129493fa51d91`,
  merge `dae9e674b3bc1212210fe9ca780b3000c72cbab2` y
  cierre `bd1581035af09eff47a08cae959312f8eae1d06f`.

### Prevención obligatoria desde F112

1. **Un solo intérprete:** localizar `python.exe` como archivo real,
   exigir exactamente una ruta y ejecutar mediante `& $pythonExe -B $runner`.
   Si no existe, abortar. No inferir que `python` o `py` en el PATH
   resuelven inequívocamente, ni llamar directamente a archivos `.py`.
2. **Preflight del launcher:** comprobar invocación `--version`,
   sintaxis `ast.parse`, integridad `git hash-object` frente al blob
   remoto y salida de un autodiagnóstico antes de enviar al usuario
   un comando que haga commits.
3. **HEAD no supuesto:** obtener `LOCAL_HEAD` y `REMOTE_HEAD` en vivo,
   validar ancestros exactos, padres y diff allowlisted. No basarse en
   un HEAD histórico porque aparezca en GitHub Desktop o un chat.
4. **EOL normalizado por Git:** para texto usar índice y `git diff`;
   para archive precompilado, hashes de bytes físicos. Prohibido usar
   `git show ... == Path.read_bytes()` como única condición de limpieza.
5. **Worktree intencionalmente sucio:** conservar los productos probados,
   usar `fetch` (no `pull/reset/checkout/clean`), extraer finalizador
   a `%TEMP%`, y realizar commit solo con autorización.
6. **Prueba de regresión previa:** ejecutar
   `tools/alpha13/gates/a13_finalizer_portability_selftest.py` con
   Python 3.11+ sobre un Git temporal; exige aceptación CRLF/LF,
   rechazo de modificaciones reales, HEAD ancestral correcto y
   ejecución explícita de un único intérprete. Además probar el
   finalizador concreto con HEAD base y HEAD con tooling ya incorporado.
   El self-test **no sustituye** esa prueba de integración.
7. **Nada de microreintentos ciegos:** ante `REVIEW_STOP`, clasificar
   `HARNESS/ENVIRONMENT/PRODUCT`, inspeccionar evidencia y corregir
   la causa; no crear nuevos F-ID por la misma cadena de errores.

```text
F112=HARNESS_WINDOWS_PYTHON_GIT_FINALIZER
G4_PHYSICAL=PASS_50_OF_50
G4_CLOSURE=CLOSED_PASS
F112_STATUS=CLOSED_PREVENTION_REGISTERED
PYTHON_EXECUTABLE=EXPLICIT_SINGLE_PATH
RAW_GIT_BLOB_EQUALS_WORKTREE_TEXT=FORBIDDEN_AS_SOLE_GUARD
HEAD_POLICY=EXACT_VERIFIED_ANCESTRY_AND_SCOPE
CRLF_POLICY=GIT_DIFF_AND_INDEX_BLOB
NEXT_FAILURE_ID=F113
```

La prevención se incorpora transversalmente a
`docs/JWPLC_COLLABORATION_WORKFLOW.md`. Documentar estas causas
no demuestra por sí solo que jamás puedan reaparecer; los siguientes
finalizadores deben ejecutar el self-test y su propia prueba de
integración **antes** de proponerse al operador.

## F113 — Finalizador G5 bloqueado por archivos no rastreados ajenos (2026-10-11)

**Categoría:** HARNESS / ergonomía del preflight Git.
**Estado:** CORRECCIÓN_VERSIONADA; **cierre operativo pendiente de nueva ejecución del finalizador**.

### Evidencia
El gate integrado G5 terminó con `STATUS=PASS_PHYSICAL_NORMAL_SOURCE`,
las pruebas PC↔JWPLC y 3/3 compilaciones de regresión PASS,
y adoptó exactamente dos archivos `JWPLC_Ethernet` localmente.
El siguiente intento de repetir el gate se detuvo por worktree sucio,
como correspondía. En el primer intento del finalizador auditado:
```text
LOCAL_HEAD=ab269503fd25b942faecc9256b7bf35dc5ccbc03
REMOTE_HEAD=80c0af5cb4febcd0cd6a11fec32a60d9d0253f11
FINALIZER_INTEGRITY=PASS
G5_FINALIZER_SYNTAX=PASS
HITO=A13_G5_FINALIZE
STATUS=REVIEW_STOP
REASON=UNTRACKED_FILES
NO_RESET_NO_CHECKOUT_NO_CLEAN_NO_BLIND_RETRY=YES
```
La salida no enumera los archivos no rastreados. La ventana CMake de VS Code
observada anteriormente podría haber generado artefactos, **pero esa
causalidad no está comprobada**. No eliminar ni ignorar sus archivos a ciegas.

### Causa
El finalizador G5 exigía un repositorio sin ningún archivo untracked:
`git ls-files --others --exclude-standard` vacío. Esta restricción
es innecesaria cuando los únicos cambios de producto permitidos están
identificados y **cada `git add` usa pathspecs explícitos**, con posterior
comprobación exacta del índice y del commit.

### Prevención implementada
- Conservar y enumerar hasta 12 rutas untracked, sin abrir, borrar,
  sobrescribir, versionar ni añadirlas al índice.
- Detenerse si una ruta no rastreada colisiona con los documentos
  que G5 debe crear o con otros destinos reservados.
- Exigir exactamente dos archivos de producto dirty y ningún cambio
  staged al comenzar.
- `git add -- <dos archivos>` y comprobación del **alcance exacto**
  en el índice/commit; lo mismo con los tres documentos del cierre.
- Al finalizar, exigir worktree **tracked** limpio; se permiten archivos
  untracked preexistentes sin tocarlos. No usar `git add -A`,
  `git clean`, `git reset`, `git checkout` ni `git pull` sobre
  el producto adoptado.
- Registrar cada bloqueo del launcher en FAILURES antes de abrir otro gate.

```text
F113=G5_UNTRACKED_FILES_PREFLIGHT_OVERRESTRICTIVE
F113_ROOT_CAUSE=HARNESS_POLICY_NOT_PRODUCT
F113_FIX=VERSIONED
F113_FINALIZER_RETEST=PENDING
G5_PHYSICAL=PASS
G5_GIT_CLOSURE=PENDING
NEXT_FAILURE_ID=F114
```

## F114 — Finalizador G5: contrato de claves MANIFEST incorrecto (2026-10-11)

**Clasificación:** HARNESS, no fallo productivo. **Estado:** FIX_VERSIONED_PENDING_RETEST.

El finalizador pasó `GIT_PREFLIGHT=PASS` y
`PRODUCT=PASS_2_ETHERNET_SOURCES_AND_G3_G4_ARCHIVES` pero se detuvo
antes de solicitar autorización con `REASON=G5_CANDIDATE_SET_CHANGED`.
Causa confirmada en `a13_g5_integrated.py`: `candidate_sha256` se
genera como `{product_path: sha(candidate_path)}`; el cierre suponía
incorrectamente claves `candidate_path`, y accedía a `[src]`.
Corregido: exigir claves exactas de producto, formato SHA256 y verificar
`candidate_hashes[dest] == sha(src)` para cada par.

Los 42 untracked vistos en la consola son outputs de CMake/VS Code
en la carpeta `build/`: `CMakeCache.txt`, `CMakeFiles`, etc.
Se añadió `/build/` al `.gitignore` raíz **sin eliminar archivos**.

**Prevención:** validar esquemas completos de MANIFEST contra el código
que los genera, incluidos tipos, nombres de claves y significados de
valores; no enviar finalizadores sin esa comprobación. Los dos archivos
Ethernet siguen probados físicamente y pendientes solo de cierre Git.

```text
F114=G5_MANIFEST_KEYS_WRONG
F114_STATUS=FIX_VERSIONED_PENDING_RETEST
G5_PHYSICAL=PASS
G5_GIT_CLOSURE=PENDING
NEXT_FAILURE_ID=F115
```

### F113 y F114 — cierre confirmado por ejecución G5
F113_STATUS=CLOSED_HARNESS_REMEDIATED
F114_STATUS=CLOSED_HARNESS_REMEDIATED
G5_RUN_ID=20261010_175509_dbd543a2
G5_PRODUCT_COMMIT=c45b77f512fd5829da33c491fd512c0a810846b9
G5_GIT_CLOSURE=CLOSED_PASS
NEXT_FAILURE_ID=F115

No se borraron outputs CMake, se respetó /build/ y los archivos
Ethernet previamente probados físicamente.

## F115 — G6 usó silenciosamente Arduino PLC IDE Tools como CLI (2026-10-11)

**Categoría:** HARNESS / PROVENIENCIA DE HERRAMIENTAS, con incidencia
independiente de apertura de COM4. **Estado:** REMEDIATION_VERSIONED;
prueba del CLI corregido y recuperación del puerto todavía pendientes.

### Evidencia real

```text
RUN_ID=20261011_122638_044c92f3
CLI=C:\Program Files\Arduino PLC IDE Tools\arduino-cli.EXE
FQBN=jwplc_local:esp32:jwplcbasic
G6_CANDIDATE_REGRESSIONS=PASS_3_OF_3
G6_PHYSICAL_BUILD=PASS
UPLOAD_G6_EXIT=1
ESPTOOL_VERSION=5.2.0
ESPTOOL_FATAL=Could not open COM4, the port is busy or doesn't exist
WINDOWS_ERROR=PermissionError(13) / sistema not functioning (31)
PRODUCT_CHANGED=FALSE
GIT_PRODUCT_COMMIT=FALSE
G6_PHYSICAL_NETWORK=NOT_RUN
```

El fallback `C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe`
fue introducido por el asistente. La versión histórica de G4 incluso
prefería esa ruta, y las versiones históricas de G5/G6 la usaban
como fallback cuando `arduino-cli` no estaba en PATH. No es el
flujo acordado para el package JWPLC en Arduino IDE; **no debió
seleccionarse sin aprobación ni dejar constancia de procedencia**.

Aunque el ejecutable se llama `arduino-cli` y compiló el FQBN JWPLC
desde sus fuentes, no equivale a afirmar que Arduino PLC IDE se haya
utilizado para desarrollar el sketch. Tampoco se ha demostrado que esa
ruta sea la causa de `PermissionError(13)`: esptool no consiguió
abrir COM4, situación que puede depender de otra aplicación que ocupe
el puerto, del driver CH340 o de conectividad USB. La causa concreta
del acceso fallido está pendiente de diagnóstico local.

### Prevención obligatoria

- El G6 revisado exige `--arduino-cli` con **ruta absoluta a un
  archivo ejecutable** elegido por el operador; no acepta una opción
  vacía ni un nombre resuelto por PATH sin identificarlo.
- Rechaza la carpeta `Arduino PLC IDE Tools`, sin reemplazarla
  silenciosamente por otra.
- Registra la ruta real y SHA-256 del ejecutable utilizado en
  MANIFEST/SUMMARY; ejecuta `version`, `core list` y verifica que
  `jwplc_local:esp32` esté instalado antes de compilar/subir.
- El script `a13_g6_contract_selftest.py` verifica la presencia de
  esas protecciones (no sustituye el diagnóstico local).
- La verificación de la procedencia de Arduino CLI pasa a ser
  obligatoria en gates posteriores. No mezclar instalaciones ni
  reutilizar binarios de una compilación con un CLI diferente sin
  comprobar equivalencia de entorno.
- Investigar COM4 por separado. Cerrar monitores seriales/terminales
  que pudieran ocuparlo y obtener datos de Device Manager/puertos.
  No reflashear hasta asegurar que el puerto está disponible y el
  JWPLC sigue aislado de cargas.
- Conservar los artefactos de G6 R1 como evidencia. No modificar
  fuentes productivas ni crear commits G6 hasta revalidación
  física con el ejecutable acordado.

```text
F115=G6_UNEXPECTED_PLC_IDE_CLI_FALLBACK
F115_CLASS=HARNESS
F115_CODE_FIX=VERSIONED
F115_REAL_ENVIRONMENT_VALIDATION=PENDING
COM4=FAILED_OPEN_PERMISSION_ERROR_13
COM4_CAUSE=NOT_ESTABLISHED
G6_PRODUCT=UNMODIFIED
G6_CLOSURE=NOT_DONE
NEXT_FAILURE_ID=F116
```

## F116 — PowerShell: falso negativo de `arduino-cli core list` por `-notmatch` con arreglo (2026-10-11)

**Tipo:** HARNESS / SCRIPT POWERSHELL. **Estado:** FIX_COMMAND_PREPARED_PENDING_RETEST.
**Producto:** sin modificaciones. **G6:** sigue sin prueba física ni cierre.

### Evidencia y causa reproducible

El operador ya había obtenido `arduino-cli 1.5.1` y la fila
`jwplc_local:esp32 2.1.0-dev` con el CLI oficial de
`C:\Program Files\Arduino CLI\arduino-cli.exe`. Sin embargo, el bloque
PowerShell de reanudación se detuvo en `Core JWPLC local no encontrado`
inmediatamente después de imprimir `arduino-cli Version: 1.5.1`.
No alcanzó el diagnóstico COM4 ni `git pull` ni ejecutó G6.

Causa: `$cores = & $cli core list` es un **arreglo de líneas**.
En PowerShell, aplicar `$cores -notmatch 'jwplc_local:esp32...'`
produce una **colección de líneas no coincidentes**, no un booleano
global. Como la lista contiene otros cores, la colección no está vacía,
la expresión del `if` se interpreta como verdadera y lanza
una falsa ausencia incluso cuando el core JWPLC está instalado.

### Prevención obligatoria

- No aplicar `-match`/`-notmatch` a resultados multilinea sin
  definir si se esperan colecciones o booleanos. Para comprobar una
  coincidencia en **todo el texto**: `$text = (& $cli core list) -join "`n"`
  y luego `$text -match '(?m)^\s*jwplc_local:esp32\s+'`.
- No duplicar las verificaciones que ya ejecuta `a13_g6_integrated.py`.
  Ese runner aplica `re.search(..., core_list)` sobre una cadena,
  registra CLI real y SHA, y aborta antes de compilar si falta el core.
- Mantener la selección explícita del CLI oficial y un diagnóstico
  de COM4 sin escritura. Conservar los resultados G6 del intento
  anterior para analizar el error COM4 por separado.
- Los comandos de PowerShell deben revisar el **tipo real** devuelto
  por cmdlets y ejecutables nativos y tratar cada código de salida
  individualmente; no asumir comportamiento escalar de expresiones
  sobre arreglos. Prevenir reintentos innecesarios y prompts duplicados.

```text
F116=POWERSHELL_ARRAY_NOTMATCH_FALSE_NEGATIVE
F116_CLASS=HARNESS_LAUNCHER
F116_ROOT_CAUSE=ARRAY_NOTMATCH_RETURNS_NONMATCHING_ELEMENTS
F116_PRODUCT_CHANGED=NO
F116_G6_RESTARTED=NO
F116_FIX=NO_DUPLICATE_CORE_CHECK_IN_POWERSHELL
F116_RETEST=PENDING
NEXT_FAILURE_ID=F117
```

## F117 — G6: fallo TX UDP sin diagnóstico de longitud/IP/contenido (2026-10-11)

**Tipo:** GATE-PHYSICAL-REVIEW con defecto de observabilidad del harness.
**Estado:** OPEN_DIAGNOSTIC_PREPARED. **Producto:** sin mutaciones.

### Evidencia aportada
```text
ARDUINO_CLI_EXE=C:\Program Files\Arduino CLI\arduino-cli.exe
ARDUINO_CLI_SHA256=1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571
CLI_VERSION_EXIT=0
CLI_CORES_EXIT=0
PORT_PRECHECK=COM4_OPEN_PASS_COM4_CLOSE_PASS
COMPILE_OVERLAY_ETH_STATIC_EXIT=0
COMPILE_OVERLAY_ETH_SPI_EXIT=0
COMPILE_OVERLAY_MODBUS_CLIENT_EXIT=0
COMPILE_G6_UDP_PHYSICAL_EXIT=0
APP_SHA256=03ea7fbefab1269c3f6de6a51efa39e26d7c50ff3e8cdfd8b3f955a3dc490581
UPLOAD_G6_EXIT=0
G6_PHYSICAL_PC_EXIT=3
G6_PC_ERROR=TX_PAYLOAD_MISMATCH
PRODUCT_ADOPTION=NOT_EXECUTED
GIT_PRODUCT_COMMIT=NO
```

El fallo COM4 del intento anterior no se repitió usando el CLI oficial:
COM4 pudo abrirse antes del test y esptool completó upload.
Ahora el error aparece en la verificación de UDP del PC.

### Distinción crítica

El script original `a13_g6_udp_pc.py` combina con `or`:
`address[0] != dut` **o** `data != EXPECTED_TX`, y muestra
`G6_PC_ERROR=TX_PAYLOAD_MISMATCH` en ambos casos. Con la evidencia
disponible no puede determinarse si el datagrama era más corto,
de otro origen, contenía bytes diferentes o tenía otra longitud.
**No concluir** que el W5500, el SPI o el candidato UDP están
defectuosos sin separar esas condiciones.

### Diagnóstico preparado
Se versionó `tools/alpha13/gates/a13_g6_udp_tx_diagnose.py`, que
usa el **firmware ya cargado** y el token de MANIFEST de la última
ejecución elegible. No recompila, no carga firmware, no adopta fuentes
ni hace commits. Inspecciona: IP/puerto de origen, longitud real,
longitud esperada, SHA-256, primeras diferencias y flags seriales
`G6_TX_*`. Conserva evidencia en
`tools/alpha13/results/g6_integrated_*/G6_TX_DIAGNOSTIC.json`.
El origen del fallo queda `PENDING_DIAGNOSIS`.

Prevención para G6 y posteriores: una aserción de payload físico
**debe desglosar** causa (origen, longitud, contenido, primer offset
distinto, estado serial del emisor), sin convertirlas en un solo
`MISMATCH` genérico. Reusar el firmware de prueba cuando sea seguro,
sin nuevos flashes para incrementar observabilidad del host.

```text
F117=G6_UDP_TX_PAYLOAD_MISMATCH_UNCLASSIFIED
F117_HARNESS_OBSERVABILITY=INSUFFICIENT_R1
F117_DIAGNOSTIC=VERSIONED_NOT_EXECUTED
G6_PREVIOUS_UPLOAD=PASS
G6_UDP_NETWORK=REVIEW
G6_PRODUCT_MODIFIED=NO
G6_GIT_CLOSURE=NO
NEXT_FAILURE_ID=F118
```

### Diagnóstico F117 confirmado: división UDP por MTU y receptor demasiado estricto

El diagnóstico de solo lectura, con firmware físico ya cargado,
observó `DUT_IP=192.168.0.31`, `PC_IP=192.168.0.4`, dos datagramas
desde `192.168.0.31:5020` de **1472** y **576** bytes.
`DATAGRAM_CONCAT_BYTES=2048` y
`DATAGRAM_CONCAT_MATCH=True`, con emisor serial:
`G6_TX_ACCEPTED_BYTES=2048`, `G6_TX_END_PACKET=1`,
`G6_TX_ERRORS=0`.

La documentación oficial de WIZnet explica que el W5500 divide
automáticamente los envíos UDP que exceden el MTU. Se confirma, por
tanto, el origen del falso fallo de la expectativa del harness:
**no se recibió un solo datagrama de 2048 bytes, sino dos mensajes
UDP distintos cuyos contenidos concatenados son correctos**.
No equivale a reensamblaje IP transparente. El contrato público
sobre conservar un datagrama único más allá de 1472 bytes deberá
documentarse como limitación de hardware/red.

La prueba host se corrigió para aceptar exclusivamente un datagrama
de 2048 bytes o la forma `1472,576`, verificando IP/puerto idénticos,
orden y contenido exacto. Ocho fixtures comprueban casos válidos e
inválidos. Se preparó `a13_g6_resume_uploaded.py` para repetir
únicamente el tramo físico, continuar con regresiones y cierre Git
automático **si pasa todo**, sin recompilar ni reflashear.

```text
F117_ROOT_CAUSE=MTU_1472_W5500_UDP_SPLIT
F117_HARNESS_FIX=VERSIONED
F117_PHYSICAL_FULL_RETEST=PENDING
G6_TX_DATA_CORRUPTED=NO
G6_REUPLOAD_REQUIRED=NO
```

### F115–F117 — resultado posterior del cierre real de G6 (2026-10-11)

El operador ejecutó `a13_g6_resume_uploaded.py` y confirmó:
```text
G6_RESUME_UDP_FIXTURES=8_OF_8_PASS
G6_RESUME_ORIGINAL_EVIDENCE=PASS_3_COMPILES_UPLOAD_AND_DIAGNOSTIC
G6_PHYSICAL_PC_EXIT=0
PHASE=PHYSICAL_NETWORK_PASS
COMPILE_PRODUCT_ETH_STATIC_EXIT=0
COMPILE_PRODUCT_ETH_SPI_EXIT=0
COMPILE_PRODUCT_MODBUS_CLIENT_EXIT=0
PHASE=FINAL_AUDIT_PASS
PHASE=AUTO_GIT_CLOSURE_PASS
PUSH=PASS_NON_FORCE
STATUS=CLOSED_PASS
G6_RESUME_NO_NEW_UPLOAD=YES
```

Resultado confirmado:
- `F115=TOOLING_CLOSED_CLI_OFFICIAL_VALIDATED`: Arduino CLI oficial
  1.5.1 fue usado y el upload funcionó. **La causa histórica exacta
  del primer PermissionError(13) en COM4 no se pudo demostrar.**
- `F116=HARNESS_CLOSED`: el falso negativo de la colección
  PowerShell se eliminó, el core y CLI fueron validados después.
- `F117=HARNESS_CLOSED`: receptor MTU dividido corregido y
  prueba PC↔JWPLC completa PASS, sin nueva subida.
- `NEXT_FAILURE_ID=F118`.

Esto actualiza los estados vigentes sin borrar la evidencia de
los intentos fallidos ni atribuir al firmware un fallo del harness.

## F118 — G7: autoprueba de secuencia usa etiqueta inexistente (2026-10-11)

**Clase:** HARNESS / CONTRATO DE TEST; no incidencia productiva.
**Estado:** FIX_VERSIONED_PENDING_RETEST.

Evidencia real aportada en el primer intento de G7:

```text
CLI_EXE=C:\Program Files\Arduino CLI\arduino-cli.exe
CLI_SHA256=1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571
PREFLIGHT=PASS
F112_PORTABILITY_SELFTEST_EXIT=0
G7_CONTRACT_SELFTEST_EXIT=2
G7_CONTRACT_BENCH_SAFETY_ONLY_CONFIRMATION=PASS
G7_CONTRACT_SELFTEST=FAIL
G7_CONTRACT_REASON=ValueError('substring not found')
STATUS=REVIEW_STOP
REASON=G7_CONTRACT_SELFTEST_FAILED
GIT_PARTIAL_COMMIT=False
FRAM_RESTORE_VERIFIED=False
```

Causa directa: el autodiagnóstico `a13_g7_contract_selftest.py`
buscaba literalmente `regression(cli,FRAM,"PRODUCT")`, que **no
existe**: el runner invoca `regression(cli,FRAM,"PRODUCT_REGRESSIONS")`.
El uso de `.index()` sin verificar presencia arrojó `ValueError`
y no informó qué etapa tenía la discrepancia. El ejecutor reportó
la categoría genérica `PRODUCT_OR_ENVIRONMENT` para una caída de
su propia autoprueba, aunque era `HARNESS`.

Prevención versionada:

- Verificar los argumentos contra llamadas reales del runner.
  `find()` y una comprobación explícita por etapa emiten ahora
  `G7_CONTRACT_SEQUENCE_STAGE_*_PRESENT` antes del orden global,
  sin `ValueError` opaco.
- `call()` clasifica fallo de `G7_CONTRACT_SELFTEST` o F112
  como `HARNESS`, no producto/ambiente.
- Dos problemas latentes identificados antes del siguiente intento:
  `sample_sketch()` creaba el mismo directorio TEMP en cada
  compilación, lo que fallaría en la segunda llamada; ahora es
  idempotente. La ruta a `gcc-ar` no se imprimía necesariamente
  en el verbose de CLI: ahora también se resuelve junto al `g++`
  del toolchain realmente usado, como en el gate de Alpha12.
- El primer intento NO llegó a `SOURCE_REBUILD`, `UPLOAD`,
  `FRAM_PHYSICAL`, `PRODUCT_ADOPTION` ni Git commits;
  por tanto, no hay escritura FRAM ni restauración pendiente.
- Retestar selftest completo **antes** de iniciar ninguna operación
  física. Nunca repetir la subida por una falla del harness
  que pueda diagnosticarse offline.

```text
F118=G7_SELFTEST_WRONG_PRODUCT_REGRESSIONS_LABEL
F118_CLASS=HARNESS
F118_CODE_FIX=VERSIONED
F118_LOCAL_TEST=PENDING
G7_UPLOAD=NOT_EXECUTED
G7_FRAM_WRITES=NONE
G7_PRODUCT_CHANGED=NO
G7_GIT_CLOSURE=NOT_EXECUTED
NEXT_FAILURE_ID=F119
```

## F119 — G7: falso negativo del enlace FRAM por buscar el nombre del archive (2026-10-11)

**Clase:** HARNESS / VERIFICACIÓN PRECOMPILADO; **Estado:** FIX_VERSIONED_PENDING_RETEST.
No hay incidencia de hardware ni prueba física iniciada en este intento.

### Evidencia aportada en G7 R2

```text
G7_CONTRACT_SELFTEST_EXIT=0
SOURCE_OVERLAY=PASS
COMPILE_FRAM_SOURCE_EXIT=0
SOURCE_REBUILD=PASS
ARCHIVE_CREATE_EXIT=0
ARCHIVE_MEMBERS_EXIT=0
ARCHIVE_EXTRACT_EXIT=0
SOURCE_OBJECT_SHA256=52b4b3f5154e7b0b45f6db5098830167c30db79fa03e12667d1f36fa5ad0501e
ARCHIVE_MEMBER_BYTE_PARITY=PASS
NEW_FRAM_ARCHIVE_SHA256=477ba0ccc3b20e638eccd7dda7d22b209d1ce73003df34917dc9ea7d476d83dd
ARCHIVE_REBUILD=PASS
COMPILE_CANDIDATE_LINK_EXIT=0
STATUS=REVIEW_STOP
REASON=PRECOMPILED_ARCHIVE_LINK_MISSING_CANDIDATE_LINK
PRODUCT_CHANGED=NO
FRAM_UPLOAD=NO
FRAM_WRITE=NO
```

El operador inspeccionó el log del linker con PowerShell y corroboró:

```text
SELECTED_LIB=TEMP\candidate\JW_FRAM
LINKER_LIBRARY_FLAG=-lJW_FRAM
LINKER_DIRECTORY_FLAG=-LTEMP\candidate\JW_FRAM\src\esp32
ARCHIVE_LITERAL_IN_VERBOSE=False
```

El build usó correctamente `-L` hacia la carpeta que contiene el
`libJW_FRAM.a` candidato y `-lJW_FRAM`. El harness esperaba
erróneamente el literal `libJW_FRAM.a` en la salida del linker.
Eso no prueba un enlace ausente: Arduino CLI expone el `-l` y
`-L` del compilador. La correspondencia real de contenido entre
archivo fuente, objeto y archive ya estaba verificada.

### Prevención / correcciones versionadas

- Nuevo `assert_fram_archive_link()`: busca comando real `.elf`
  del linker, requiere `-lJW_FRAM` y directorio `-L` exacto,
  existencia de `src/esp32/libJW_FRAM.a` y SHA-256 igual al del
  archive creado; **no** se satisface por la sola línea
  `Using library`.
- Sustituir las tres validaciones de nombre literal:
  candidato, regresiones y prueba física.
- Autoprueba con fixtures positivo y negativos: directorio incorrecto,
  `-l` ausente y SHA incorrecto. Estas pruebas ejercitan el
  validador real y no solo buscan cadenas de texto.
- Reanudación `--resume-from-run g7_20261011_162621_6ca7668e`
  limitada a ese RUN_ID, con validación del manifiesto antiguo,
  ausencia de upload/cambios previos, huellas de las fuentes,
  identidad del objeto y archive, selección previa y opciones
  reales de linker. Copia el archive ya construido a un TEMP nuevo
  y repite desde `CANDIDATE_LINK`; no rehace la compilación de
  fuentes ni archiva de nuevo.
- Si faltan artefactos temporales o difieren los hashes, detener
  `REVIEW` antes de compilar o tocar el equipo, no reconstruir
  automáticamente un candidato distinto.

```text
F119=G7_FRAM_LINK_LITERAL_ASSUMPTION
F119_CLASS=HARNESS
F119_SOURCE_REBUILD=PASS_PREVIOUS_RUN
F119_ARCHIVE_PARITY=PASS_PREVIOUS_RUN
F119_LINK_FLAGS=-L_EXACT_AND_lJW_FRAM
F119_FIX=VERSIONED
F119_RESUME=PREPARED_NOT_EXECUTED
G7_UPLOAD=NO
G7_FRAM_WRITE=NO
G7_PRODUCT_MUTATION=NO
NEXT_FAILURE_ID=F120
```

## F120 — G7: timeout de READY y reloj TFT detenido después del upload (2026-10-11)

**Estado:** DIAGNOSIS_PENDING. **Producto:** sin adopción.
El intento G7 R3 confirmó reconstrucción source/archive, enlace
`-L/-lJW_FRAM`, regresiones candidato, compilación del sketch
físico y subida `UPLOAD_G7_EXIT=0`. Tras la subida:
`FRAM_PHYSICAL_SERIAL_EXIT=3`,
`G7_SERIAL_ERROR=READY_TIMEOUT`.
El operador observó hora TFT congelada en 16:46:54.

Causa **todavía desconocida**: el sketch habría de transmitir
`G7_FRAM_READY` cada 700 ms sin orden previa. El host no recibió
la marca y no envió `G7_FRAM_RUN`. El paquete posee autoload de
RTC/display (`jwplcSystemTask`) fuera del sketch; por ello la
explicación inicial de TFT inmóvil sólo por falta de dibujo desde
`loop()` es insuficiente.

Se creó `a13_g7_boot_readonly.py` para lectura UART pasiva, con
DTR/RTS en false y sin escrituras, resets, comandos FRAM ni flash.
Conservar manifiesto, logs de subida, y cualquier marca NVS intacta.

```text
F120_CLASS=UNKNOWN
F120_FLASH_UPLOAD=PASS
F120_DEVICE_READY=NOT_OBSERVED
F120_FRAM_RUN_SENT=NO
F120_PRODUCT_MUTATED=NO
F120_PASSIVE_DIAG=VERSIONED_NOT_EXECUTED
NEXT_FAILURE_ID=F121
```

### F120 — Nueva evidencia: recuperación tras reconexión y UART pasiva (2026-10-11)

El operador había desconectado el JWPLC tras el fallo de G7.
Al conectarlo nuevamente, reportó que TFT/RTC se actualizaban con
normalidad. `a13_g7_boot_readonly.py`, sin enviar comandos,
abrió COM4 a 115200 con DTR=RTS=False y registró:

```text
ORIGINAL_RUN=g7_20261011_164228_e5e86ddc
FIRMWARE_SHA256=30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a
ORIGINAL_UPLOAD=True
FRAM_TEST_COMMAND=NOT_SENT
G7_DIAG_UART_LINES=29
G7_DIAG_UART_BYTES=1247
G7_DIAG_READY_EXACT_COUNT=29
G7_DIAG_BOOT_PANIC_MARKER_COUNT=0
STATUS=PASSIVE_CAPTURE_COMPLETE_NOT_A_G7_PASS
```

**Conclusión delimitada**: el firmware G7 sí ejecuta setup/loop
tras arranque frío, COM4 funciona y el freeze TFT/RTC desapareció.
No afirmar que el origen sea DTR/RTS, un fallo de RTC,
FRAM o un reset, porque no se tiene traza de arranque del
timeout original. F120 queda **open root cause unknown**.
No hubo mensaje `G7_FRAM_RUN` en el intento anterior,
por lo que `fram_restored=False` no demuestra alteración FRAM.

Acción: `a13_g7_fram_serial.py` ahora usa apertura serial
DTR/RTS=False. `a13_g7_resume_physical_only.py`
corrobora el manifiesto del upload, evidencia pasiva de READY
y token, crea un registro *exclusivo*, envía solo una orden de
ensayo y exige PASS de restauración completa. No compila,
no sube sketch y no hace commits. Ante cualquier fallo después
de enviar orden, detener: **podría existir respaldo NVS pendiente**.

```text
F120_PHYSICAL_SYMPTOM=RECOVERED_ON_POWER_CYCLE
F120_UART_READY=PASS_29_OF_29
F120_PANIC_OBSERVED=NO
F120_CAUSE=UNKNOWN
F120_PHYSICAL_FRAM_TEST=NOT_YET_EXECUTED
F120_RESUME_SCRIPT=VERSIONED_NOT_EXECUTED
NEXT_FAILURE_ID=F121
```

### F120 — El ensayo físico de FRAM finalmente pasó; freeze post-upload sin causa determinada

Después de abrir COM4 con DTR=RTS=False, el operador ejecutó
una única orden `G7_FRAM_RUN` sobre el firmware ya cargado:
```text
G7_RECOVER_RUN_ID=g7_20261011_164228_e5e86ddc
G7_RECOVER_SOURCE=ALREADY_FLASHED_NO_NEW_UPLOAD
G7_SERIAL_READY_MATCH=PASS
G7_SERIAL_G7_FRAM_STALE_RECOVERY=1
G7_SERIAL_G7_FRAM_BACKUP_PASS=1
G7_SERIAL_G7_FRAM_NULL_AND_BOUNDS_ERRORS=0
G7_SERIAL_G7_FRAM_VALID_BLOCK_ERRORS=0
G7_SERIAL_G7_FRAM_CORRUPT_BLOCK_ERRORS=0
G7_SERIAL_G7_FRAM_VERSION_ERRORS=0
G7_SERIAL_G7_FRAM_RESTORE_PASS=1
G7_SERIAL_G7_FRAM_TOTAL_ERRORS=0
G7_SERIAL_G7_FRAM_RESULT=PASS
G7_FRAM_PHYSICAL=PASS_BACKUP_RESTORE_CHECKSUM_NULL_BOUNDS
G7_RECOVER_SERIAL_EXIT=0
G7_RECOVER_FRAM_RESTORE_VERIFIED=True
G7_RECOVER_STATUS=PHYSICAL_PASS
```

Esto **cierra la validación física FRAM** y confirma la restauración,
pero no identifica la causa raíz del congelamiento TFT y
`READY_TIMEOUT` inmediatamente posterior al upload anterior.
Distinguir `F120_PHYSICAL_RECOVERY=PASS` de
`F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN`; no cerrar la causa
histórica por inferencia. No volver a ejecutar `G7_FRAM_RUN`.

La evidencia local consta de:
`tools/alpha13/results/g7_fram_g7_20261011_164228_e5e86ddc/G7_FRAM_PHYSICAL_RECOVERY.log`
y `G7_FRAM_PHYSICAL_RECOVERY.json`, con SHA de log/host.
`a13_g7_close_from_physical_pass.py` comprueba ambos y la
identidad del archivo `libJW_FRAM.a` antes de permitir adopción
y regresiones. Posee `--verify-only` no destructivo.
```text
F120_PHYSICAL_FRAM=PASS
F120_POST_UPLOAD_FREEZE_CAUSE=UNKNOWN
G7_CLOSE_PRODUCT_AND_DOCS=PENDING
NEXT_FAILURE_ID=F121
```

## F121 — G7: clave de regresiones duplicada impide cierre documental (2026-10-11)

**Clase:** HARNESS / ESQUEMA DE MANIFIESTO.
**Estado:** FIX_VERSIONED_PENDING_RETEST.

La ejecución de cierre desde la prueba física PASS tuvo:

```text
PHASE=PHYSICAL_EVIDENCE_REVIEW_PASS
PHASE=PRODUCT_ADOPTION_PASS
COMPILE_PRODUCT_REGRESSIONS_GLOBAL_FRAM_EXIT=0
COMPILE_PRODUCT_REGRESSIONS_CONFIG_BLOCK_EXIT=0
PHASE=PRODUCT_REGRESSIONS_PASS
PHASE=FINAL_AUDIT_PASS
PHASE=AUTO_GIT_CLOSURE_RUNNING
ROLLBACK=PASS
STATUS=REVIEW_STOP
REASON='product_regressions'
GIT_PARTIAL_COMMIT=False
G7_CLOSE_NO_UPLOAD_NO_FRAM_WRITE_NO_SERIAL=YES
```

**Causa exacta:** el productor en `a13_g7_integrated.py`
almacenaba `MANIFEST[prefix.lower()+"_regressions"]`,
aunque `prefix` YA es `PRODUCT_REGRESSIONS` o
`CANDIDATE_REGRESSIONS`. Se generaba
`product_regressions_regressions` y el renderizador
`close_documents()` esperaba `product_regressions`,
causando `KeyError('product_regressions')` después del
PASS técnico. El cierre Git no empezó; el rollback protegió
los dos archivos fuente y `libJW_FRAM.a`.

**Corrección:** la función de regresiones ahora exige los
dos prefijos permitidos y persiste `MANIFEST[prefix.lower()]`
sin duplicar sufijos. `close_documents()` comprueba
primero el esquema requerido y las rutas productivas, emitiendo
un error etiquetado `HARNESS`, no un `KeyError` opaco.

**Nueva prevención:** `a13_g7_closure_document_selftest.py`
invoca la función de regresión real con compilación/linker simulados,
comprueba ambas claves del manifiesto y renderiza de verdad el
documento de cierre. Incluye caso negativo sin
`product_regressions`. Esta autoprueba se ejecuta ANTES
de `validate_evidence` y de toda adopción en
`a13_g7_close_from_physical_pass.py`.
No modifica producto ni accede a COM4.

Se debe ejecutar primero el modo `--verify-only` de cierre,
confirmar el selftest PASS y luego reintentar cierre productivo.
No repetir la prueba física, compilar firmware físico ni subir sketch.

```text
F121=G7_DOUBLE_SUFFIX_PRODUCT_REGRESSIONS_MANIFEST
F121_CLASS=HARNESS
F121_ROOT_CAUSE=MANIFEST_KEY_SUFFIX_DUPLICATED
F121_FIX=VERSIONED_NOT_TESTED_WINDOWS
F121_CONTRACT_SELFTEST=PREPARED_NOT_EXECUTED
G7_PHYSICAL_FRAM=PASS_AND_RESTORED
G7_PRODUCT_FILES=ROLLED_BACK_PASS
G7_GIT_CLOSURE=NO
NEXT_FAILURE_ID=F122
```


## F122 — G8: adopción productiva rechazada por diff Git sin detalle (2026-10-11)

**Síntoma:** `run_a13_g8_adopt.bat` llegó a respaldo y copia exacta de
las dos fuentes y `libJWPLC_ModbusRTU.a`, pero abortó antes de las
regresiones productivas:

```text
RUN_ID=g8_adopt_20261011_192007_949944b4
PREFLIGHT=PASS
BACKUP=PASS
PRODUCT_ADOPTION=REVIEW_STOP
REASON=DIRTY_SCOPE_NOT_EXACT_THREE_FILES
ROLLBACK=PASS
PRODUCT_REGRESSIONS=NOT_EXECUTED
```

**Evidencia y límite:** el verificador comparaba
`git diff --name-only -z` con un conjunto fijo de tres rutas, pero no
registró cuáles rutas observó Git. Los hashes de bytes del candidato
habían superado el control anterior; NO hay prueba para identificar
qué ruta faltó/sobró, si hubo retraso del índice o si influyó alguna
particularidad de Windows. No atribuir causa específica sin esos datos.

**Clasificación:**

```text
F122_CLASS=HARNESS_OBSERVABILITY_GAP
PRODUCT_FAILURE=NOT_DEMONSTRATED
PRODUCT_RESTORE=PASS
PRODUCT_COMMIT_OR_UPLOAD=NONE
ROOT_CAUSE_OF_SCOPE_MISMATCH=UNDETERMINED
```

**Prevención:** antes de rechazar una adopción debe registrarse en log
y manifiesto el conjunto exacto observado, esperado y staged; comparar
además blobs `HEAD`→working tree y hashes de candidato; no relajar la
allowlist ni declarar PASS con diferencias no explicadas. Toda falla
posterior al backup exige rollback auditado y no publicar archivos
fuente sin el archive precompilado correspondiente.

## F123 — G8: falso rechazo del índice por parser `ls-files -v` (2026-10-11)

**Síntoma:** el segundo intento no pasó del preflight:

```text
RUN_ID=g8_adopt_20261011_192425_df5a28db
PHASE=PREFLIGHT_RUNNING
REASON=INDEX_NOT_TRACKING_ALL_PRODUCT_FILES
BACKUP=NOT_STARTED
ADOPTION=NOT_STARTED
PRODUCT_CHANGE=NONE
```

El diagnóstico independiente de solo lectura, ejecutado sobre la
misma copia local después del intento fallido, mostró:

```text
RUN_ID=g8_d0_20261011_192744_4cb40613
PATH_1_INDEX=True  PATH_1_HEAD=True  V_FLAG=H
PATH_2_INDEX=True  PATH_2_HEAD=True  V_FLAG=H
PATH_3_INDEX=True  PATH_3_HEAD=True  V_FLAG=H
INDEX_TRACKS_ALL=True
CLEAN=True
HEAD_EQUALS_WORKTREE=True (las tres rutas)
STATUS=DIAGNOSTIC_COLLECTED_NO_MUTATION
```

Las tres rutas productivas sí están rastreadas: `JWPLC_ModbusRTU.cpp`,
`JWPLC_RS485.cpp` y `libJWPLC_ModbusRTU.a`. No había flags
`skip-worktree` ni `assume-unchanged` en esas tres rutas.
**No pedir al operador reset, checkout, add, cambio de índice ni
copia manual de archivos.**

**Causa demostrada a nivel de sistema:** el rechazo de preflight
contradice los datos de Git obtenidos por la herramienta D0; por
tanto, el chequeo introducido en el harness generó un falso negativo.
**Mecanismo preciso no demostrado:** la ejecución fallida no dejó
la salida cruda de `ls-files -v`, por lo que no se puede establecer
si hubo una anomalía de decodificación, parsing, entorno o contenido
temporal del proceso.

**Corrección:** sustituir el parser de líneas por tokens delimitados
en NUL con `git ls-files -v -z`, contrastar con
`git ls-files --full-name -z` y `git ls-tree -z HEAD`,
emitir observados/esperados y errores de parsing **antes** del
assert; validar en CI Windows y Linux con el mismo helper real.
Devolver `REVIEW_STOP` con evidencia si hay discrepancia verdadera,
sin saltar controles de SHA, Git o rollback.

**Clasificación:**

```text
F123_CLASS=HARNESS_FALSE_NEGATIVE
G8_D0=PASS_READ_ONLY
PRODUCT_FAILURE=NO_EVIDENCE
PRODUCT_MODIFIED_IN_SECOND_ATTEMPT=NO
PRODUCT_COMMIT_OR_UPLOAD=NONE
F123_FIX=VERSIONED_PENDING_WINDOWS_RETEST
A13-010=CANDIDATE_VALIDATED_NOT_ADOPTED
A13-011=CANDIDATE_VALIDATED_NOT_ADOPTED
G8=CURRENT_NOT_CLOSED
NEXT_FAILURE_ID=F124
```

**Regla transversal:** cada nueva precondición de Git que pueda
bloquear un gate debe tener un caso positivo sobre checkout normal
y otro sobre archivos realmente modificados, ejecutados en Windows
y Linux antes de pedir al operador que repita el BAT. Diferenciar
siempre `CI_PASS`, `WINDOWS_PASS`, adopción productiva, prueba
física y commit/push. Las notas F122/F123 también deben
sincronizarse con la fuente viva de FAILURES del Proyecto en la
próxima actualización de sus archivos.


### Reconciliación definitiva F122/F123 — separación de rutas Windows (2026-10-11)

**Nueva evidencia que resuelve la incertidumbre anterior.** La prueba CI
reprodujo el falso negativo usando **la función real del ejecutor G8**
en el runner Windows, con las mismas tres rutas. Log del job Windows
en [GitHub Actions 38188287251](https://github.com/JW-Control/platform-jwplc/actions/runs/38188287251):

```text
G8_ADOPT_INDEX_EXPECTED_PATHS=JWPLC\2.1.0\libraries\...
G8_ADOPT_INDEX_V_PATHS=JWPLC/2.1.0/libraries/...:H
G8_ADOPT_INDEX_TRACKED_PATHS=JWPLC/2.1.0/libraries/...
G8_ADOPT_INDEX_HEAD_PATHS=JWPLC/2.1.0/libraries/...
G8_ADOPT_INDEX_PARSE_ERRORS=[]
RuntimeError: INDEX_EXACT_THREE_PATHS_MISMATCH
```

**Causa raíz demostrada:** el harness usaba
`str(Path.relative_to(ROOT))` para construir conjuntos de comparación
contra resultados de Git. En Windows, `pathlib.Path` produce separadores
`\`, mientras que `git ls-files`, `git ls-tree` y
`git diff --name-only -z` devuelven rutas POSIX con `/`,
independientemente del sistema operativo. Por eso ambos conjuntos
tenían los mismos archivos físicos pero cadenas distintas.
Esto explica los rechazos F122 y F123; **no eran defectos del producto**.

**Corrección:** `Path.relative_to(ROOT).as_posix()` para TODAS las
rutas que se comparen con Git o se pasen como `HEAD:ruta`.
Conservar `str(Path.relative_to(ROOT))` donde sea necesario para leer
las claves del manifiesto P1/P2 ya generado en Windows. No convertir
esas claves a mitad del flujo y romper la procedencia de hashes.

**Resultado del retest:** ambos jobs del workflow [38188441089](https://github.com/JW-Control/platform-jwplc/actions/runs/38188441089) pasaron el helper de índice en estado limpio y la auditoría real de tres rutas modificadas. La validación se hizo sobre checkouts efímeros; no se ha repetido todavía la adopción productiva del operador.

**Prueba preventiva:** Linux y Windows deben ejecutar exactamente
`git_index_marks()` y `audit_git_scope()` del ejecutor real en
un checkout CI temporal, primero limpio y luego con las dos fuentes
más el archive modificados. `CI_WINDOWS_PASS` verificado tras el fix no sustituye la adopción
en el Windows físico. No más diagnósticos especulativos ni
repeticiones sin cobertura de plataforma.

```text
F122_ROOT_CAUSE=WINDOWS_PATH_SEPARATOR_MISMATCH
F123_ROOT_CAUSE=WINDOWS_PATH_SEPARATOR_MISMATCH
F122_F123_NOT_PRODUCT_DEFECT=YES
FIX=GIT_PATHS_AS_POSIX_AND_LEGACY_MANIFEST_KEYS_SEPARATE
WIN_CI_REGRESSION=PASS_RUN_38188441089
WINDOWS_PRODUCT_ADOPTION=NOT_EXECUTED_SINCE_FIX
G8=CURRENT_NOT_CLOSED
NEXT_FAILURE_ID=F124
```


### Confirmación Windows productiva de F122/F123 (2026-10-11)

El operador repitió `run_a13_g8_adopt.bat` después del
fix de rutas POSIX y de los tests CI Windows/Linux:

```text
RUN_ID=g8_adopt_20261011_193839_57f9bb49
INDEX_FLAGS=H_H_H
PRODUCT_ADOPTION=PASS
PRODUCT_REGRESSIONS=PASS_SLAVE_MASTER_RS485_STATUS
FINAL_AUDIT=PASS
ADOPTION_DIFF_PATHS=EXACT_THREE
ADOPTION_BYTES_CHANGED=TRUE_TRUE_TRUE
FINAL_DIFF_PATHS=EXACT_THREE
ARCHIVE_SHA256=f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416
STATUS=PRODUCT_ADOPTED_REGRESSIONS_PASS_PENDING_PHYSICAL_DECISION_AND_COMMIT
GIT_COMMIT_OR_PUSH=NONE
UPLOAD_OR_RS485_ACCESS=NONE
```

**F122 y F123 = CORREGIDOS Y RETEST_WINDOWS_PASS.**
La causa raíz fue mezcla de rutas Windows con `\` y salidas POSIX
de Git con `/`; se corrigió el harness y se pasó la auditoría de
Git y tres regresiones **sobre el producto local real**. No abrir
un nuevo incidente por el mismo caso ni declarar otro fallo de firmware.
Se conservan los incidentes para evitar reincidencias. G8 aún requiere
commit Git exacto y decisión explícita de prueba física.

```text
F122_FIX=VERIFIED_WINDOWS_PRODUCT_ADOPTION_PASS
F123_FIX=VERIFIED_WINDOWS_PRODUCT_ADOPTION_PASS
G8_ARCHIVE_PRODUCT_IN_LOCAL_WORKTREE=YES_PENDING_COMMIT
G8_REMOTE_COMMIT=NONE_AT_THIS_POINT
NEXT_FAILURE_ID=F124
```

## F124 — CI G8 asumía archivos fuente diferentes tras adoptar producto (2026-10-11)

**Síntoma:** después del commit de producto `fb71d65e` de A13-010 y
A13-011, el workflow `alpha13-g8-candidate-native.yml` devolvió FAIL
en [run 38189443172](https://github.com/JW-Control/platform-jwplc/actions/runs/38189443172),
en el paso `Probar alcance Git de las tres rutas productivas en checkout efimero`.

```text
CI_DIFF_PATHS=JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a
CI_BYTES_CHANGED_RTU_CPP=False
CI_BYTES_CHANGED_RS485_CPP=False
CI_BYTES_CHANGED_ARCHIVE=True
REASON=CI_CANDIDATE_SAME_AS_GIT_HEAD
```

**Causa demostrada:** el paso de CI copiaba las dos fuentes candidatas
encima del producto y modificaba artificialmente solo el archive. Ese
test era válido **antes** de adoptar el producto, cuando las dos fuentes
candidatas diferían de HEAD. Después del commit `fb71d65e`, las dos
fuentes ya eran exactamente las candidatas, así que copiarlas era un
no-op. Solo el archive aparecía modificado, mientras
`audit_git_scope()` esperaba tres cambios. El fallo está en el
**fixture de CI no idempotente frente al estado post-adopción**,
no en la corrección de firmware ni en el archive productivo.

**Solución:** mantener intacta la auditoría exacta de tres rutas,
pero en la simulación efímera de GitHub Actions añadir marcadores de
prueba controlados a las **dos fuentes y al archive** después de
validar los 20 escenarios nativos y el contrato de fuentes exactas.
Los marcadores se escriben solo en el runner desechable de CI,
jamás en código productivo, candidatos versionados o PC del operador.
El mismo test pasa antes y después de la adopción.

**Prevención:** todo workflow que simule una diferencia Git debe
asegurar explícitamente que el conjunto de rutas modificadas cumple
la precondición requerida, incluso después de haber integrado los
candidatos. Cubrir ambos estados del producto y separar
`CI_FIXTURE_FAIL` de `PRODUCT_FAIL`.

```text
F124_CLASS=CI_FIXTURE_FALSE_NEGATIVE
PRODUCT_FAILURE=NO
G8_PRODUCT_COMMIT=fb71d65eb2c842b3f46e6949760e693f3b73d95e
G8_PRODUCT_FILES_CHANGED_BY_F124_FIX=NONE
F124_FIX=WORKFLOW_VERSIONED
F124_CI_RETEST=PASS_RUN_38191980973
G8_PHYSICAL_RS485=NOT_EXECUTED
NEXT_FAILURE_ID=F125
```

La CI posterior al fix concluyó PASS en
[GitHub Actions 38191980973](https://github.com/JW-Control/platform-jwplc/actions/runs/38191980973),
incluidas las pruebas C++ nativas, el contrato pre/post adopción y
el alcance exacto de tres rutas en checkout efímero.

```text
F124=RESOLVED_CI_PASS
G8_GIT_PRODUCT=CLOSED_PASS
G8_PHYSICAL_RS485=DECISION_PENDING
```

## F125 — G9: prueba de reversibilidad del NUL usa coincidencia incorrecta (2026-10-11)

**Síntoma:** el primer contrato binario del candidato G9-P2/P3
falló en la CI (run
[38205174547](https://github.com/JW-Control/platform-jwplc/actions/runs/38205174547))
con `SD_NOT_REVERSIBLE_EXACT_ONE_LITERAL`.
La fuente `JW_SD.cpp` sí conserva **un NUL real** en offset
28732, confirmado por G9-P0 local y CI.

**Causa raíz:** la comprobación hacía el cambio correcto del literal
`'<NUL>'` por `'\\0'`, pero intentaba reconstruir el archivo
original mediante `new.replace(SD_NEW,SD_OLD,1)`, lo cual
selecciona **la primera aparición de cualquier escape textual**
`'\\0'` en el archivo, y no necesariamente la aparición recién
transformada. La prueba rechazaba un candidato correcto cuando
`JW_SD.cpp` ya contenía otras comparaciones con NUL textual.

**Corrección:** localizar primero el offset exacto de los tres bytes
de la secuencia original (`'` + NUL + `'`), reemplazar en ese
mismo índice y verificar la reconstrucción inversa **por offset**,
además de conteo de NUL restante cero y cambio de longitud de
+1 byte. El control NO se debilita: ahora valida la ruta precisa.

**Prevención:** las validaciones binarias no deben suponer
unicidad de la representación textual nueva; cuando se modifica
un byte de origen identificado por offset, comparar y revertir
siempre en el mismo offset. Mantener byte-for-byte completo
fuera de esa ventana.

```text
F125_CLASS=HARNESS_CONTRACT_FALSE_NEGATIVE
PRODUCT_MODIFIED=NO
SD_ARCHIVE_MODIFIED=NO
G9_P2P3_PRODUCT_ADOPTION=NOT_EXECUTED
F125_FIX=VERSIONED_SOURCE_OFFSET_REVERSIBILITY
F125_CI_RETEST=PASS_RUN_38205406022
NEXT_FAILURE_ID=F126
```

## F126 — G9: ayuda de runner Python con símbolo no codificable en Windows (2026-10-11)

**Síntoma:** `--help` del ejecutor de candidatos
`a13_g9_p2p3_candidate.py` en el runner Windows falló en
[CI 38205242003](https://github.com/JW-Control/platform-jwplc/actions/runs/38205242003),
mientras Linux completó el contrato de fuentes con PASS.
`UnicodeEncodeError: 'charmap' codec can't encode character '\\u2192'`.

**Causa demostrada:** la descripción del comando incluía una
flecha Unicode `→`. El Python del runner Windows tenía una salida
de consola que no podía codificarla en el paso `--help`.
No llegó a compilar ni tocar librerías.

**Corrección y prevención:** usar ASCII en la descripción
CLI del runner, conservando la documentación española UTF-8.
Probar sintaxis y `--help` **en ambos sistemas** antes de
pedir el BAT al operador; no generalizar el PASS Linux a Windows.

```text
F126_CLASS=HARNESS_WINDOWS_CONSOLE_ENCODING
PRODUCT_FAILURE=NO
PRODUCT_MUTATION=NONE
G9_P2P3_CANDIDATE_BUILD=NOT_YET_EXECUTED_BY_OPERATOR
F126_FIX=CLI_ASCII_DESCRIPTION_VERSIONED
F126_CI_RETEST=PASS_RUN_38205406022
NEXT_FAILURE_ID=F127
```

## F127 — G9: contrato de reemplazos fuente dependía de LF en Windows (2026-10-11)

**Síntoma:** el tercer intento de CI G9 de contrato fuente,
[run 38205311929](https://github.com/JW-Control/platform-jwplc/actions/runs/38205311929),
pasó en Linux y falló en Windows con
`GLOBAL_REPLACEMENT_NOT_EXACT` sobre el comentario de RTC.

**Causa demostrada:** `JWPLC_GlobalPeripherals.cpp` llegó al
checkout Windows con finales de línea `CRLF`, pero las cadenas
de transformación esperaban exclusivamente `LF`. No corresponde
normalizar todo el archivo al parchearlo porque eso generaría
un diff masivo y ocultaría la limpieza real.

**Corrección:** detectar el formato de final de línea del archivo,
rechazar mezclas anómalas y construir los patrones exactos con
su formato original. Aplicar únicamente las cuatro sustituciones
de texto A13-014, conservando los bytes no relacionados. El
control de diff y de RTC/botonera se mantiene estricto.

**Prevención:** para ediciones textuales de fuentes Arduino
en Windows/Linux, hacer pruebas de contrato en ambas plataformas
y conservar el estilo LF/CRLF de entrada.

```text
F127_CLASS=HARNESS_TEXT_EOL_PORTABILITY
PRODUCT_FAILURE=NO
GLOBALPERIPHERALS_PRODUCT_MODIFIED=NO
G9_PRODUCT_COMMIT=NONE
F127_FIX=EOL_PRESERVING_EXACT_REPLACEMENTS
F127_CI_RETEST=PASS_RUN_38205406022
NEXT_FAILURE_ID=F128
```

Los tres incidentes G9 F125-F127 se revisaron y su
retest de contrato concluyó PASS en **Windows y Linux**:
[GitHub Actions 38205406022](https://github.com/JW-Control/platform-jwplc/actions/runs/38205406022).
No se modificaron archivos de firmware productivo ni archives.

```text
G9_P2P3_SOURCE_CONTRACT_CI=PASS_WINDOWS_LINUX
F125=RESOLVED
F126=RESOLVED
F127=RESOLVED
G9_CANDIDATE_FULL_ARDUINO_BUILD=NOT_YET_EXECUTED_ON_OPERATOR_WINDOWS
```

## F128 — G10-P1B: claves POSIX del manifiesto frente a Path Windows (2026-10-12)

**Síntoma confirmado por operador:**

```text
G10_P1B_STATUS=REVIEW_STOP
G10_P1B_REASON=KeyError('JWPLC\\2.1.0\\libraries\\JWPLC_ModbusTCP\\src\\JWPLC_ModbusTCP_Client.cpp')
G10_P1B_RUN_ID=g10_p1b_20261012_122341_30dd9ea2
```

La ejecución ocurrió después de sincronizar HEAD `bd3dc54a`.
Se detuvo **antes** del marcador de fuentes candidatas, sin compilar
Arduino, modificar bibliotecas productivas ni tocar COM4/COM14.

**Causa raíz verificada en código:** `G10-P0` y `G10-P1A`
guardan las rutas de fuente de sus manifiestos como claves POSIX
(con `/`), pero el código P1B realizaba la consulta usando
`str(Path(...))`, que en Windows produce `\\`. Además, el
mapa `old` se generaba con `str(path)`, lo que habría provocado
el mismo problema en otras fases. La CI anterior usó
`--ci-standalone`, saltándose la lectura de los
manifiestos locales, por lo que sus PASS Windows/Linux
**no cubrían esta ruta**.

**Corrección versionada:** `repo_key(path) = path.as_posix()`
para todas las claves del contrato y los candidatos,
incluido el mapa `old`; extraer la comprobación
`verify_local_provenance` común al modo local y al selftest.
Añadir CI Windows/Linux con rutas `PureWindowsPath`/
`PurePosixPath`, caso negativo de SHA incorrecto y
generación íntegra de cuatro fuentes candidatas con el
mismo código que usa el operador.

**Prevención:** las claves de manifiestos y nombres de
archivos relativos a Git deben tener representación estable
POSIX; mantener rutas nativas solo para acceso al
sistema de archivos. Probar **ambos modos**
`--ci-standalone` y local-provenance antes de pedir BAT.
La falla de harness no invalida los P0/P1A anteriores
ni implica defecto nuevo de Modbus TCP.

```text
F128_CLASS=HARNESS_WINDOWS_POSIX_MANIFEST_KEY_MISMATCH
G10_P1B_LOCAL=REVIEW_STOP_BEFORE_CANDIDATE
G10_P1B_PRODUCT_CHANGED=NO
G10_P1B_COM4_COM14_TOUCHED=NO
G10_P0_AND_P1A_EVIDENCE=VALID
F128_FIX=VERSIONED_REPO_KEY_AS_POSIX_AND_FULL_FLOW_SELFTEST
F128_CI_RETEST=PASS_WINDOWS_LINUX_RUN_38284253191
NEXT_FAILURE_ID=F129
```

CI de retest de F128: [38284253191](https://github.com/JW-Control/platform-jwplc/actions/runs/38284253191)
terminó **PASS en Windows y Linux**. Se ejecutaron el
selftest del lookup de los manifests P0/P1A con rutas
`PureWindowsPath/PurePosixPath`, el caso negativo de
hash y el generador íntegro de cuatro fuentes; también
pasaron las pruebas nativas de cierre y el chequeo de
checkout sin cambios. Aún no hay ejecución local
del operador del BAT corregido, adopción de producto
ni flash por esta corrección.

```text
F128=RESOLVED_CI_CONTRACT_ONLY
G10_P1B_LOCAL_RETEST=AWAITING_OPERATOR
G10_P1B_PRODUCT_ADOPTION=NONE
```

## Actualización F128 — retest local PASS (2026-10-12)

El operador confirmó el cierre local del defecto del harness:

```text
G10_P1B_LOCAL_RUN=g10_p1b_20261012_124640_8458554e
G10_P1B_SOURCE_CONTRACT=PASS_FOUR_PRIVATE_CANDIDATES
G10_P1B_STATUS=CANDIDATE_SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN
F128=RESOLVED_CI_AND_LOCAL_RETEST
G10_P1B_ARDUINO_BUILD=NOT_YET_EXECUTED
G10_P1B_PRODUCT_ADOPTION=NONE
```

No se debe interpretar la corrección del harness F128 como
resolución de A13-006. El segundo gate de build P1C compilará
Client y Server contra los candidatos, sin modificar producto.

## F129 — G10-P1C: falso negativo de procedencia de compilación TCP (2026-10-12)

**Salida reportada por el operador, Windows (sin editar producto):**

```text
G10_P1C_PHASE=PREFLIGHT_PASS
G10_P1C_PHASE=OVERLAY_PASS
G10_P1C_COMPILE_BASELINE_SERVER_EXIT=0
G10_P1C_STATUS=REVIEW_STOP
G10_P1C_REASON=RuntimeError('TCP_COMPILE_INPUT_NOT_CORRECT_FOLDER_BASELINE_SERVER_JWPLC_ModbusTCP_Client.cpp')
G10_P1C_RUN_ID=g10_p1c_20261012_134431_10e91e4c
```

**Diagnóstico del gate:** el primer ejemplo Modbus TCP Server
compiló con salida Arduino CLI 0 y el verificador llegó
más allá de selección de biblioteca, comprobación de core
y presencia de ambos archivos `.cpp.o`.
Falla únicamente la búsqueda de la ruta absoluta del
`.cpp` dentro del texto verbose `-v`. Ese texto no
garantiza conservar la ruta exactamente en la
representación calculada por `Path.resolve()`;
el log local completo del operador no se descargó
para demostrar cuál formato alternativo utilizó.
**No declarar fallo del compilador ni de A13-006
a partir de esta excepción**. Tampoco declarar
todavía los builds restantes PASS: no fueron ejecutados.

**Corrección del contrato F129:** sustituir la búsqueda
de texto en `arduino-cli -v` por una comprobación de
los dos archivos `.cpp.o` realmente creados y sus
respectivos archivos de dependencias `.cpp.d`
de GCC `-MMD`, habilitado en el `platform.txt`
JWPLC, validando que cada archivo de dependencias
identifique el source `JWPLC_ModbusTCP/src/<archivo>.cpp`.
Conservar la comprobación independiente de librería
exacta `Using library...`, el enlace al core JWPLC
precompilado, cuatro compilaciones y hashes de
los cinco archives congelados. No aceptar resultados
a partir de un archivo `.o` aislado sin dependencias.

**Pruebas añadidas:** fixture sintético con ambos formatos de dependencia `.cpp.o.d` (observado con GCC `-MMD -o nombre.cpp.o`) y `.cpp.d`, en ambos sistemas
con ruta Windows de backslash, escapes Make/GCC,
rutas con espacios, dos objetos y dependencias,
más negativos de source ajeno y archivo `.d` ausente.
CI no sustituye los cuatro builds reales Arduino.
Si una instalación concreta no genera `.d`, marcar
`REVIEW_STOP` y estudiar su `platform.txt`/log;
no silenciar esa prueba.

**Prevención:** un PASS de selftest sin ejecución de
la rama que falló no acredita un verificador de
procedencia. Preferir artefactos estructurados del
compilador por encima del formato humano de `-v`.
Registrar por separado el resultado del compilador,
la selección de biblioteca y el resultado de la
auditoría de procedencia.

```text
F129_CLASS=HARNESS_FALSE_NEGATIVE_COMPILE_SOURCE_PROVENANCE
F129_TRIGGER=ARDUINO_VERBOSE_ABSOLUTE_CPP_PATH_ASSUMPTION
G10_P1C_FIRST_BASELINE_SERVER_COMPILE_EXIT=0
G10_P1C_FIRST_GATE=REVIEW_STOP
G10_P1C_REMAINING_THREE_BUILDS=NOT_EXECUTED
G10_P1C_PRODUCT_CHANGED=NO
G10_P1C_UPLOAD=NO
F129_FIX=VERSIONED_GCC_MMD_OBJECT_DEPENDENCY_PROVENANCE
F129_CI_RETEST=PASS_WINDOWS_LINUX_RUN_38293634614
G10_P1C_LOCAL_RETEST=AWAITING_OPERATOR
NEXT_FAILURE_ID=F130
```

Retest de la corrección F129:
[CI 38293634614](https://github.com/JW-Control/platform-jwplc/actions/runs/38293634614)
**PASS Windows y Linux**. Se ejecutó el mismo
`verify_source_build()` con fixture `.cpp.o/.cpp.d`
positivo y dos rechazos negativos (source ajeno y
`.d` ausente), además del selftest de selección de
bibliotecas. Todavía **no** se ha repetido P1C con
las cuatro compilaciones reales en Windows.
