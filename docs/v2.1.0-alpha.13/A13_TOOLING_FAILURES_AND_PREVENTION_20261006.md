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

## Estado

```text
NEXT_FAILURE_ID=F098
SYNC_TO_PROJECT_FAILURES_SOURCE=PENDING
```
