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

## Estado

```text
NEXT_FAILURE_ID=F102
SYNC_TO_PROJECT_FAILURES_SOURCE=PENDING
```
