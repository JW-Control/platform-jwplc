# v2.1.0-alpha.13 — Estado operativo y continuidad

Actualizado: 2026-10-07 — G2-P1 baseline reproducido

> Fuente viva de continuidad del alpha. Un chat nuevo debe verificar el estado real
> del repositorio y continuar desde `NEXT_GATE`.

## Identidad

```text
RELEASE_VERSION=v2.1.0-alpha.13
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
BASELINE_HEAD=20f1b66075dac04160311b7f40c939e88534073f
ALPHA12_STATUS=CLOSED_PUBLISHED
ALPHA13_STATUS=IN_PROGRESS
```

## Alcance

```text
PURPOSE=cleanup + technical debt + robustness + safe optimization
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
```

## P0

```text
A13_P0=PASS_WITH_KNOWN_BASELINE_DEFECT
P0A=PASS
P0B=PASS
P0C=PASS
P0D1=PASS_WITH_KNOWN_BASELINE_DEFECT
P0D2=PASS
```

Known baseline defect:

```text
Adafruit_BusIO source fallback -> Wire.h unresolved
release/precompiled path affected=NO
```

## G1 — A13-001 DNS truncation

```text
G1_PRE1=PASS
G1_P1=PASS
G1_P2=PASS
G1_P3_ATTEMPT_1=REVIEW_ENVIRONMENT
G1_P3_R1=REVIEW_HARNESS
G1_P3_R2=REVIEW_HARNESS
G1_P3_R3=REVIEW_PRECONDITION
G1_P3_R4=REVIEW_HARNESS
G1_P3_R5=PASS
G1_STATUS=CLOSED_PASS
```

Candidato cerrado:

```text
FILE=JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp
SHA256=08291b4b89274f014e1ce6073bd16303192e5bfe1b4e2fdeaa21d133fb783995
PRODUCT_COMMIT=8bc48b73074859d367b2d949e333f0b16c83cbd7
COMMIT_MESSAGE=fix(ethernet): evitar bloqueo DNS con respuestas truncadas
REMOTE_PUSH=PASS
WORKTREE_AFTER_COMMIT=CLEAN
```

Evidencia cerrada:

```text
BASELINE_TRUNCATED_LOOP_REPRODUCED=True
PATCH_COMPILE=PASS
VALID_DNS_PACKETS=3/3
TRUNCATED_PREFIXES=136/136
```

P3 intento 1:

```text
DNS_NORMAL_PATH_CONTRACT=True
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
PORTS_VISIBLE=COM1
REQUESTED_PORT=COM4
UPLOAD_EXIT=1
PHYSICAL_REGRESSION=NOT_EXECUTED
PRODUCT_FAILURE=NO
ENVIRONMENT_FAILURE=YES
```

P3 R1 — wrapper de sintaxis:

```text
BAT_SYNTAX_PREFLIGHT=FAIL
ERROR=PowerShell recibió el token literal ^ antes de |
PHYSICAL_REGRESSION=NOT_EXECUTED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Clasificación conforme al registro de fallos del proyecto:

```text
F091_APPLIES=YES
CHAT_AS_LONG_GATE_EDITOR=DO_NOT_REPEAT
VERSIONED_PS1_BAT_LOGS_STATUS=REQUIRED
```

P3 R2 — carrera READY serial:

```text
A13_GATE_SYNTAX=PASS
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=4
NB3_DNS_READY_TIMEOUT=YES
PHYSICAL_DNS_SEQUENCE=NOT_STARTED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
ENVIRONMENT_FAILURE=YES
```

Hipótesis planteada durante R2, posteriormente NO confirmada como causa raíz:

```text
probe READY = one-shot
gate delay after upload = 800 ms
client wait starts after serial open
posible pérdida del marcador READY
```

La evidencia posterior mostró que el JWPLC todavía no tenía la precondición
Ethernet completa (RJ45 + red con DHCP) durante esos intentos. Por tanto:

```text
R2_READY_RACE_AS_ROOT_CAUSE=NOT_PROVEN
PRODUCT_FAILURE=NO
LESSON=no promover una hipótesis temporal a causa raíz sin instrumentación
```

Corrección R3:

```text
probe Alpha13 propio
READY reemitido cada 500 ms mientras WAIT_COMMAND
sin cambios en Dns.cpp
sin cambios en API/producto
```

P3 R3 — READY no alcanzado:

```text
A13_GATE_SYNTAX=PASS
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=4
NB3_DNS_READY_TIMEOUT=YES
DNS_SEQUENCE_STARTED=NO
PRODUCT_FAILURE=NO
ROOT_CAUSE=PRECONDITION_NOT_YET_CLASSIFIED
```

La reemisión periódica elimina la hipótesis de marcador READY perdido.
R4 añade telemetría serial no invasiva de runtime Ethernet mientras READY=false:

```text
BEGIN_ATTEMPTED
READY
BUSY
runtimeState
lastError
diagnosticCode
```

Esto permite separar LINK_OFF / DHCP / SPI / HW / otro estado antes de
atribuir cualquier regresión a Dns.cpp.

Higiene de evidencia R4:

```text
BUILD_PATH=%TEMP%
RESULTS=logs locales ignorados por Git
GITHUB_DESKTOP=no debe listar objetos .o/.d/.bin/.elf/.map del build
```

Se recupera así el patrón de los gates físicos Alpha14, que compilaban fuera
del working tree.

P3 R4 — fallo del reader serial:

```text
A13_GATE_SYNTAX=PASS
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=4
SERIAL_READER_EXCEPTION=UnicodeEncodeError
CONSOLE_ENCODING=cp1252
INVALID_SERIAL_BYTE_DECODED_AS=U+FFFD
NB2_ETH_DIAG_CAPTURED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

R5 usa un cliente Alpha13 propio. Los bytes seriales inválidos se representan
con escapes ASCII y la escritura a stdout aplica `backslashreplace`; un byte
de arranque corrupto ya no puede matar el thread de captura.

P3 R5 — cierre físico:

```text
A13_GATE_SYNTAX=PASS
DNS_SHA256=08291b4b89274f014e1ce6073bd16303192e5bfe1b4e2fdeaa21d133fb783995
DNS_NORMAL_PATH_CONTRACT=True
COMPILE_EXIT=0
REPO_ETHERNET_SELECTED=True
DNS_SOURCE_OBJECT_COUNT=1
UPLOAD_EXIT=0
CLIENT_EXIT=0
DUT_IP_EFFECTIVE=192.168.0.31
PC_DNS_SERVER_IP=192.168.0.4
RESULT_CODE=1
PROBE_FAILED=NO
SUCCESS_RESULT_IP=10.20.30.40
SUCCESS_DURATION_MS=1
SUCCESS_POLL_COUNT=5
TIMEOUT_DURATION_MS=453
TIMEOUT_POLL_COUNT=5663
DNS_BEGIN_HOLD_MAX_US=1460
DNS_POLL_HOLD_MAX_US=748
LOOP_GAP_MAX_US=644
SPI_LOCK_ERRORS=0
DNS_VALID_QUERY_COUNT=1
DNS_TIMEOUT_QUERY_COUNT=1
DNS_OTHER_QUERY_COUNT=0
NB3_DNS_CLIENT_PASS=YES
DIRTY_SCOPE_VALID=True
DIFF_CHECK_PASS=True
STATUS=PASS
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

Precondición física confirmada para este gate:

```text
USB/serial -> COM4
W5500/RJ45 -> LAN con DHCP
PC -> misma LAN
```

Cierre Git:

```text
FINAL_DIFF_AUDIT=PASS
STAGED_FILE_COUNT=1
STAGED_FILE=JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp
GIT_DIFF_CACHED_CHECK=PASS
PRODUCT_COMMIT=8bc48b73074859d367b2d949e333f0b16c83cbd7
REMOTE_PUSH=PASS
G1=CLOSED_PASS
```

## Infraestructura G1-P3-R5

Versionada en commits de tooling/docs descendientes de `BASELINE_HEAD`. El gate
acepta esa cadena sólo si los archivos commiteados desde el baseline pertenecen
al allowlist de tooling/docs y confirma que `Dns.cpp` sigue sin commit:

```text
tools/alpha13/gates/common.ps1
tools/alpha13/gates/a13_g1_p3_dns_physical.ps1
tools/alpha13/gates/a13_g1_p3_dns_physical_client.py
tools/alpha13/gates/run_a13_g1_p3_dns_physical.bat
tools/alpha13/firmware/a13_g1_p3_dns_physical_probe/a13_g1_p3_dns_physical_probe.ino
tools/alpha13/results/.gitignore
tools/alpha13/results/.gitkeep
```

Contrato del gate:

```text
SerialPort explícito = prioridad 1
autodetección = sólo un único puerto distinto de COM1
sin candidato inequívoco = REVIEW_ENVIRONMENT
preflight de puerto fallido = no compilar / no subir / no abrir cliente
compile/upload/client/SUMMARY = logs persistentes por run
Dns.cpp = nunca stageado ni commiteado por el gate
wrapper BAT = sin pipes escapados entre cmd.exe y powershell -Command
topología = baseline ancestro + allowlist de commits tooling/docs
```

## NEXT_GATE

```text
NEXT_GATE=G2-P2-R3 / A13-002
OBJECTIVE=apply minimal candidate and run source-first contract
STATE=READY_TO_RETRY_AFTER_HARNESS_FIX
PREREQUISITE=G2_P1_PASS
```

No modificar todavía código productivo de G2 hasta completar su preflight
dirigido y fijar el contrato del gate.

## G2 — A13-002 TCA startup / EN_IO

```text
G2_PRE1=PASS
G2_CLASSIFICATION=ROBUSTNESS_FIX
G2_PRIORITY=P0
G2_CONFIDENCE=HIGH
G2_STATUS=BASELINE_DEFECT_REPRODUCED
G2_P1_R2=PASS
PRODUCT_CHANGE=NO
```

Preflight read-only confirmado sobre HEAD:

```text
HEAD=690ca995bbced130f389b2432814fb85f7feb59b
SOURCE=JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp
```

Hallazgos:

```text
EN_IO starts LOW=YES
TCA connection failure keeps EN_IO LOW=YES
five post-probe TCA configuration results ignored=YES
EN_IO goes HIGH after those calls regardless of result=YES
g_ioState.initialized becomes true before I2C/TCA completion=YES
JWPLC_IO.ready() consumes that initialized flag=YES
```

Operaciones cuyo resultado se ignora actualmente:

```text
1 TCA6424A_writeBank(bank1, 0x00)
2 TCA6424A_writeBank(bank2, 0x00)
3 TCA6424A_setBankDirection(bank0, 0xFF)
4 TCA6424A_setBankDirection(bank1, 0x00)
5 TCA6424A_setBankDirection(bank2, 0xFF)
```

Dependencia de build:

```text
normal jwplcbasic -> jwcontrol_precompiled_stub + precompiled/core/JWPLCBASIC/core.a
jwplcbasic source core direct at normal build=NO
core.a changed since Alpha13 baseline=NO
inherited Alpha12 core.a bytes=3042444
inherited Alpha12 core.a SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Política obligatoria si A13-002 modifica `cores/jwcontrol`:

```text
SOURCE_CHANGE
-> ARCHIVE_INVALIDATED
-> SOURCE-FIRST PASS
-> REBUILD core.a
-> VERIFY normal jwplcbasic stub + archive
-> physical gate
-> product commit
```

G2-P1 debe reproducir baseline mediante instrumentación temporal y segura:

```text
5 failure legs + 1 normal/control leg
compile real peripherals_init.cpp from source using full jwplcbasic profile
do not mutate versioned core.a
temporary build under %TEMP%
restore any temporary source/boards.local mutation byte-for-byte
during failure legs intercept EN_IO HIGH request and keep physical EN_IO LOW
record requested EN_IO state, actual EN_IO state, operation result,
TCA register snapshot and JWPLC_IO.ready()
```

Criterio de reproducción baseline:

```text
for each injected failed operation:
OP_RESULT=FAIL
EN_IO_HIGH_REQUESTED=YES
EN_IO_ACTUAL=LOW   # safety interlock of harness
IO_READY=TRUE      # demonstrates current false-ready behavior
PRODUCT_FAILURE=REPRODUCED_BASELINE_DEFECT
```

La lectura de registros se conserva como evidencia, pero no se exige energizar
salidas para demostrar el defecto.

No implementar todavía el fix A13-002 hasta cerrar G2-P1.

Infraestructura G2-P1 versionada:

```text
tools/alpha13/gates/a13_g2_p1_tca_startup_baseline.ps1
tools/alpha13/gates/a13_g2_tca_startup_client.py
tools/alpha13/gates/run_a13_g2_p1_tca_startup_baseline.bat
tools/alpha13/firmware/a13_g2_tca_startup_probe/a13_g2_tca_startup_probe.ino
```

Salvaguardas:

```text
product source instrumentation=TEMPORARY_ONLY
source restored before physical uploads=REQUIRED
boards.local restored byte-for-byte=REQUIRED
versioned core.a mutation=FORBIDDEN
builds=%TEMP%
failure legs physical EN_IO=FORCED_LOW
control leg physical EN_IO=normal
```

G2-P1 R1 — evidencia:

```text
6/6 source-core compiles=PASS
source profile full jwplcbasic=PASS
peripherals_init.cpp count=1
stub core=0
versioned core.a linked=NO
source restore=PASS
boards.local restore=PASS
core.a SHA preserved=PASS
failure legs 1..5 contract=PASS
control leg contract=REVIEW
```

Las cinco piernas inyectadas reprodujeron de forma consistente:

```text
EN_IO_HIGH_REQUESTED=YES
IO_VIEW_READY=YES
OP_OK_MASK=30/29/27/23/15
```

El único fallo de contrato fue la pierna control porque `digitalRead(EN_IO)`
devolvió LOW pese a que el baseline había solicitado HIGH y toda la
configuración TCA era correcta. Esto se clasifica como F097: el harness usó
readback de pad para un GPIO configurado `GPIO_MODE_OUTPUT` sin input-enable.

R2 corrige sólo la observabilidad:

```text
EN_IO_OUTPUT_ENABLE <- GPIO_ENABLE_REG
EN_IO_OUTPUT_LATCH  <- GPIO_OUT_REG
EN_IO_PAD_READBACK  <- diagnóstico no contractual
PRODUCT_CHANGE=NO
```

G2-P1 R2 — cierre baseline:

```text
HEAD=0dd5e236604391c497839ae08452e507a131db78
A13_GATE_SYNTAX=PASS
COMPILE_LEGS=6/6 PASS
SOURCE_CORE=YES
STUB_CORE=NO
VERSIONED_CORE_A_LINKED=NO
FULL_JWPLCBASIC_PROFILE=YES
SOURCE_RESTORED=True
BOARDS_LOCAL_RESTORED=True
CORE_SHA256_PRESERVED=True
CONTROL_STEP_CONTRACT=PASS
FAULT_STEP_1_CONTRACT=PASS
FAULT_STEP_2_CONTRACT=PASS
FAULT_STEP_3_CONTRACT=PASS
FAULT_STEP_4_CONTRACT=PASS
FAULT_STEP_5_CONTRACT=PASS
STATUS=PASS
REASON=BASELINE_DEFECT_REPRODUCED_SAFELY
PRODUCT_FAILURE=REPRODUCED_BASELINE_DEFECT
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

Control normal:

```text
OP_OK_MASK=31
EN_IO_HIGH_REQUESTED=YES
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=HIGH
IO_VIEW_READY=YES
```

En las cinco fallas inyectadas el baseline conserva el defecto:

```text
EN_IO_HIGH_REQUESTED=YES
EN_IO_OUTPUT_LATCH=LOW   # interlock de seguridad del harness
IO_VIEW_READY=YES
PERIPHERALS_INITIALIZED=YES
```

Por tanto A13-002 deja de ser sólo riesgo estático: queda reproducido en
hardware con fault injection controlado.

Siguiente paso: diseñar el cambio mínimo productivo. Debe mantener `EN_IO`
en LOW y `JWPLC_IO.ready()==false` ante cualquiera de las cinco fallas, y
sólo declarar ready después de completar correctamente la configuración TCA.

No regenerar todavía `core.a`: primero debe pasar source-first el candidato.

Diseño G2-P2 fijado:

```text
PRODUCT_FILES=3
1 cores/jwcontrol/peripherals_init.cpp
2 cores/jwcontrol/jwplc_peripherals.cpp
3 cores/jwcontrol/jwplc_peripherals.h
PUBLIC_API_BREAK=NO
CORE_A_REFRESH=NOT_YET
```

Cambios mínimos:

```text
jwplcSystemInitState() -> IO ready=false
post-probe TCA ops -> every return checked
any failed op -> return with EN_IO still LOW
successful startup -> EN_IO HIGH -> settle -> peripheral init true -> IO ready=true
non-Basic path -> explicit IO ready=true after system state init
```

Identidad esperada del candidato local:

```text
peripherals_init.cpp blob=23efb3935a34e6b5649875b57c804e60538827cd
jwplc_peripherals.cpp blob=875a50fd64552e8c4a4300e07b9494d2c32605d7
jwplc_peripherals.h blob=288667f1caa08142e2a155b8c85f24b2aa5beb44
staged=0
commit=NO
core.a SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Infraestructura G2-P2:

```text
tools/alpha13/gates/apply_a13_g2_p2_candidate.ps1
tools/alpha13/gates/run_a13_g2_p2_apply_candidate.bat
tools/alpha13/gates/run_a13_g2_p2_apply_and_test.bat
tools/alpha13/gates/a13_g2_p2_tca_startup_candidate.ps1
tools/alpha13/gates/a13_g2_tca_startup_candidate_client.py
tools/alpha13/gates/run_a13_g2_p2_tca_startup_candidate.bat
tools/alpha13/firmware/a13_g2_tca_startup_candidate_probe/a13_g2_tca_startup_candidate_probe.ino
```

Contrato source-first:

```text
control:
attempt=31
ok=31
EN_IO latch=HIGH
peripherals initialized=YES
IO ready=YES

fault step N:
attempt=(1<<N)-1
ok=(1<<(N-1))-1
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO
```

El gate vuelve a compilar las seis piernas desde `cores/jwcontrol` con el
perfil completo `jwplcbasic`, preserva el `core.a` versionado y restaura la
instrumentación temporal al candidato exacto antes de cualquier upload.

G2-P2 R1 — REVIEW_HARNESS antes de ejecutar producto:

```text
APPLY_SYNTAX=PASS
APPLY_RESULT=FAIL
APPLY_REASON=Get-FileHash unavailable in local PowerShell environment
PATCH_APPLIED=NO
PRODUCT_CHANGE_EXECUTED=NO

CANDIDATE_GATE_SYNTAX=FAIL
PHYSICAL_GATE_EXECUTED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Corrección R2:

```text
SHA helper -> Get-A13Sha256 from versioned common.ps1
Get-FileHash dependency -> removed
duplicate malformed contract block in candidate gate -> removed
BAT parser preflight -> retained
candidate still must start from clean baseline
```

G2-P2 R2 — REVIEW_HARNESS:

```text
APPLY_SYNTAX=PASS
BASELINE_BLOBS=PASS
CORE_ARCHIVE_SHA=PASS
GIT_APPLY_CHECK=FAIL
PATCH_APPLIED=NO
PRODUCT_CHANGE_EXECUTED=NO

CANDIDATE_GATE_SYNTAX=PASS
CANDIDATE_GATE_STATUS=REVIEW
REASON=UNEXPECTED_HEAD_TOPOLOGY
UNEXPECTED_COMMITTED=
  docs/v2.1.0-alpha.13/A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md
  tools/alpha13/gates/run_a13_g2_p2_apply_candidate.bat
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Corrección R3:

```text
git apply candidate mechanism -> removed
candidate application -> deterministic exact-once transforms
baseline blob guard -> retained
candidate blob guard -> retained
core.a SHA guard -> retained
allowlist -> completed with all G2-P2 tooling/docs paths
combined apply+test runner -> added
test cannot start if apply fails
```

## Observación mapeada — delay() y temporización no bloqueante

No forma parte del fix A13-002 ni abre un gate nuevo en Alpha13.

Estado actual del core:

```text
delay(ms) -> vTaskDelay(ms / portTICK_PERIOD_MS)
BLOCKS_ENTIRE_ESP32=NO
BLOCKS_USER_LOOP_TASK=YES
JWPLC_SYSTEM_TASK_CONTINUES=YES
```

Mientras el sketch está dentro de `delay(ms)`, la tarea independiente
`jwplcSystemTask` puede seguir atendiendo:

```text
I/O scan
RTC
Ethernet service
DataLog service
Display
```

La botonera también tiene su propia tarea de escaneo.

Sí quedan pausados hasta que retorna el `loop()` del usuario:

```text
código secuencial restante del loop
serialEventRun
JWPLC_ModbusTCP.task() autoservice pre/post-loop
JWPLC_ModbusRTU.task() si el usuario lo atiende desde loop
cualquier máquina de estados del sketch
```

Decisión de diseño:

```text
DO_NOT_OVERRIDE_ARDUINO_DELAY=YES
ASYNC_DELAY_API=CANDIDATE_BACKLOG
CURRENT_ALPHA_PRODUCT_SCOPE=NO_CHANGE
```

Dirección propuesta futura: una API de temporizadores no bloqueantes, por
ejemplo `JWPLC_Timer`, basada en `millis()` y segura ante rollover, con
`start()`, `done()`, `every()`, `restart()`, `cancel()` y
`remaining()`. Evitar callbacks/tareas por defecto para no introducir
concurrencia innecesaria en sketches de PLC.

`delayMicroseconds()` merece una revisión separada porque su implementación
espera activamente y no tiene la misma semántica cooperativa de `delay(ms)`.

## Gates restantes

```text
G2  A13-002 TCA startup / EN_IO                  REVIEW_CONFIRMED_RISK
G3  A13-004 TCA RMW/shadow atomicity             PENDING
G4  A13-003 TFT batch task ownership             PENDING
G5  A13-005 + A13-006 TCP correctness            PENDING
G6  A13-007 + review A13-008 datagram robustness PENDING
G7  A13-009 + A13-013 FRAM                       PENDING
G8  A13-010 + A13-011 serial                     PENDING
G9  A13-012 + A13-014 + A13-017 cleanup          PENDING
G10 final regression/freeze                       PENDING
```

## KNOWN_RISKS

```text
Adafruit_BusIO source fallback baseline defect
stale precompiled archives can mask source changes
serial COM can change between sessions
```

## DO NOT DO

```text
DO_NOT_REPEAT_P0
DO_NOT_REPEAT_G1_PRE1_P1_P2_WITHOUT_NEW_EVIDENCE
DO_NOT_REMOVE_NORMAL_AUTOLOAD_PERIPHERALS
DO_NOT_ASSUME_OPENPLC_INTEGRATED
DO_NOT_ASSUME_OTA_DEFINED
DO_NOT_FIX_FINAL_FLASH_FREQ_WITHOUT_DECISION
DO_NOT_PUBLISH_BOOTLOADER_BIN_AS_FINAL
DO_NOT_IMPLEMENT_JW_BUSIO_YET
G1_CLOSED_G2_MAY_START_WITH_READ_ONLY_PREFLIGHT
```
