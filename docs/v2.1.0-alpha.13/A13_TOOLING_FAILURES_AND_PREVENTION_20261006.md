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

## Estado

```text
NEXT_FAILURE_ID=F106
SYNC_TO_PROJECT_FAILURES_SOURCE=PENDING
```
