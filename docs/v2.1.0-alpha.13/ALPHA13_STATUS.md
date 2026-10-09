# v2.1.0-alpha.13 — Estado operativo y continuidad

Actualizado: 2026-10-09 — G2 / A13-002 CLOSED_PASS

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
NEXT_GATE=TFT-PRE4
OBJECTIVE=temp-only deferred-DISPON candidate transform preflight
STATE=READY_TO_RUN_AFTER_PULL
PREREQUISITE=TFT_PRE3_PASS
```

No modificar todavía código productivo de G2 hasta completar su preflight
dirigido y fijar el contrato del gate.

## G2 — A13-002 TCA startup / EN_IO

```text
G2_PRE1=PASS
G2_CLASSIFICATION=ROBUSTNESS_FIX
G2_PRIORITY=P0
G2_CONFIDENCE=HIGH
G2_STATUS=CLOSED_PASS
G2_P1_R2=PASS
G2_P2_R5=PASS
G2_P3=PASS
G2_P4=PASS
G2_P5=PASS
PRODUCT_CHANGE=COMMITTED_PUSHED
PRODUCT_COMMIT=6a585693
REMOTE_PUSH=PASS
WORKTREE_AFTER_COMMIT=CLEAN
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
jwplc_peripherals.cpp blob=c3d53566d274e95b7dda111327140b5393f7db35
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
G2-P2 R3 — REVIEW_HARNESS:

```text
A13_APPLY_SYNTAX=PASS
CANDIDATE_BLOB_MISMATCH=YES
FILE=jwplc_peripherals.cpp
ACTUAL=2e54c7950e2d23db2c19548e56f5b81fd483f92f
EXPECTED=875a50fd64552e8c4a4300e07b9494d2c32605d7
CATCH_RESTORE=EXECUTED
PHYSICAL_GATE_EXECUTED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Diagnóstico byte-a-byte:

```text
wrapper forced powershell.exe (PowerShell legacy) + UTF-8 without BOM
todavía -> todavÃ­a
están   -> estÃ¡n
MOJIBAKE_VARIANT_BLOB=2e54c7950e2d23db2c19548e56f5b81fd483f92f
MATCHES_OBSERVED_ACTUAL=YES
```

Corrección R4:

```text
product-generating PS1 literals -> ASCII-only
new expected jwplc_peripherals.cpp blob=c3d53566d274e95b7dda111327140b5393f7db35
candidate bytes are generated and hashed under %TEMP% first
repo product files are not touched unless all 3 temp blobs match
combined BAT parses common + apply + physical gate before apply
```

Entorno PowerShell confirmado por el desarrollador:

```text
PSEdition=Core
PSVersion=7.6.6
EXECUTABLE=pwsh
```

Regla aplicada desde G2-P2-R4:

```text
material BAT wrappers -> require pwsh / PowerShell Core 7+
no silent fallback to powershell.exe
wrapper prints resolved pwsh version before parser/apply/gate
```

G2-P2 R4 — REVIEW_HARNESS por parser de evidencia:

```text
PWSH_VERSION=7.6.6
CANDIDATE_APPLY=PASS
DIRTY_COUNT=3
STAGED_COUNT=0
6/6 COMPILE=PASS
SOURCE_CORE=True
STUB_CORE=False
ARCHIVE_LINKED=False
FULL_PROFILE=True
6/6 UPLOAD=PASS
6/6 CLIENT_EXIT=0
FAULT_STEPS_1_3_4_5 CONTRACT=PASS
STEP0/STEP2 FALSE_NEGATIVE=YES
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
```

Evidencia visible del producto en R4:

```text
CONTROL:
attempt=31
ok=31
EN_IO high requested=YES
EN_IO latch=HIGH
peripherals initialized=YES
IO ready=YES

FAULT 1:
attempt=1
ok=0
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 2:
attempt=3
ok=1
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 3:
attempt=7
ok=3
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 4:
attempt=15
ok=7
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO

FAULT 5:
attempt=31
ok=15
EN_IO high requested=NO
EN_IO latch=LOW
peripherals initialized=NO
IO ready=NO
```

R5 cambia sólo el harness: el cliente emite claves canónicas
`A13_G2_CLIENT_*` del único bloque que ya validó y el gate consume sólo esas
claves. El candidato productivo local no cambia.

G2-P2 R5 — cierre source-first:

```text
STATUS=PASS
REASON=CANDIDATE_SOURCE_FIRST_PASS
6/6 COMPILE=PASS
6/6 UPLOAD=PASS
6/6 CLIENT=PASS
6/6 CONTRACT=PASS
SOURCE_CORE=True
STUB_CORE=False
ARCHIVE_LINKED=False
FULL_PROFILE=True
TRACKED_DIRTY_FINAL=3
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

G2-P3 queda desbloqueado. Debe regenerar `core.a` desde los tres blobs
candidatos exactos, usar `%TEMP%` para outputs de build/verify, y demostrar
que el FQBN normal `jwplcbasic` compila con
`jwcontrol_precompiled_stub + core.a`.

Ante cualquier fallo de build/verify, el gate debe restaurar el archive
anterior y dejar sólo los tres sources candidatos dirty.


G2-P3 — refresh de core y enlace normal:

```text
HEAD=0cfec7d38719bea643baab97a5c4d7dac9833c48
PWSH_VERSION=7.6.6
ARDUINO_CLI=1.0.2

SOURCE_BUILD=PASS
SOURCE_BUILD_TIME_S=110.125
SOURCE_COMPILE_DB_ENTRIES=79
SOURCE_JWCONTROL_TUS=64
SOURCE_STUB_TUS=0
SOURCE_PERIPHERALS_INIT_COUNT=1

CORE_BEFORE_BYTES=3042444
CORE_BEFORE_SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
CORE_AFTER_BYTES=3043670
CORE_AFTER_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983

NORMAL_VERIFY=PASS
NORMAL_VERIFY_TIME_S=73.107
NORMAL_COMPILE_DB_ENTRIES=16
NORMAL_SOURCE_TUS=0
NORMAL_STUB_TUS=1
NORMAL_PRECOMPILED_STUB_COUNT=1
NORMAL_JWPLCBASIC_STUB=True
NORMAL_JWPLCBASIC_SOURCE_CORE=False
NORMAL_JWPLCBASIC_CORE_A_LINKED=True
APP_BYTES=411376

BOARDS_LOCAL_UNCHANGED=True
TRACKED_DIRTY_FINAL=4
STAGED_FINAL=0
CANDIDATE_BLOBS_FINAL=True
DIFF_CHECK_FINAL=True
STATUS=PASS
REASON=CORE_REFRESH_AND_NORMAL_LINK_PASS
```

G2-P4 queda desbloqueado. Debe compilar un probe con el FQBN normal
`jwplcbasic`, demostrar nuevamente stub + archive sin source core, subirlo al
JWPLC y verificar físicamente:

```text
JWPLC_IO.ready()=YES
EN_IO output-enable=YES
EN_IO latch=HIGH
lastScanMs>0
```

Los fault steps no se repiten en P4: ya quedaron validados source-first en
G2-P2 y el archive de P3 fue generado desde esos mismos blobs cualificados.

G2-P4 — validación física con package normal precompilado:

```text
HEAD=5cf8dba1495de150bc9c7161a0233732417bb4b7
SERIAL_PORT=COM4
ARDUINO_CLI=1.0.2
COMPILE_EXIT=0
USES_STUB_CORE=True
USES_SOURCE_CORE=False
CORE_A_LINKED=True
SOURCE_TU_COUNT=0
STUB_TU_COUNT=1
PRECOMPILED_STUB_COUNT=1
FULL_PROFILE=True
UPLOAD_EXIT=0
CLIENT_EXIT=0
IO_READY=YES
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=HIGH
LAST_SCAN_MS=2280
CORE_SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
SOURCE_BLOBS_FINAL=True
TRACKED_DIRTY_FINAL=4
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
STATUS=PASS
REASON=NORMAL_PRECOMPILED_PHYSICAL_PASS
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

Cadena A13-002 completada:

```text
baseline defect reproduced -> PASS
source-first candidate physical -> PASS
core.a refresh -> PASS
normal stub + archive link -> PASS
normal precompiled physical -> PASS
```

Pendiente antes del commit:

```text
final unstaged diff audit
git diff --check
explicit staging of exactly 4 product files
staged diff audit
single product commit
push
status closure
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


## Observación mapeada — TFT sucia al energizar por USB

No se clasifica todavía como regresión productiva y no se mezcla con A13-002.

Estado actual del source:

```text
JWPLC_TFT_RST=GPIO14
initPeripherals():
  TFT_CS -> OUTPUT/HIGH
  TFT_RST -> OUTPUT/LOW
JWPLC_TFT.begin():
  TFT_eSPI backend init()
Alpha11 contract:
  TFT_RST_HELD_LOW_DURING_AUTOLOAD=YES
  FIRST_IDLE_FRAME_IMMEDIATE_AFTER_DISPLAY_BEGIN=YES
```

La retención de reset existe y no fue eliminada por A13-002. Sin embargo,
`initPeripherals()` se ejecuta dentro de `loopTask`, después de
`initArduino()`. El backlight del Basic v2 no tiene control por software.
Por ello existe una ventana estrictamente anterior a `initPeripherals()` en
la que el firmware de aplicación todavía no gobierna GPIO14 y el GRAM del
ST7789 puede hacerse visible al energizar.

Interpretación provisional:

```text
dirty pattern only before first IDLE frame -> pre-firmware/power-on window
dirty pattern persists after IDLE/display begin -> DISPLAY REGRESSION
```

Acción:

```text
TFT_STARTUP_DIRTY=OPEN_ROBUSTNESS_OBSERVATION
TFT_PRE1=BASELINE_TIMING_WITH_NORMAL_PACKAGE
TFT_NEW_FEATURES=OUT_OF_SCOPE
PRODUCT_CHANGE_BEFORE_MEASUREMENT=NO
G3_BLOCKED_UNTIL_TFT_PRE1_CLASSIFIED=YES
```

TFT-PRE1 — baseline con package normal:

```text
HEAD=ce7831e214a88551a85acf2be83dad569131881f
POWER_SOURCE=USB_ONLY
24VDC=DISCONNECTED
SERIAL_PORT=COM4
USES_STUB_CORE=True
USES_SOURCE_CORE=False
CORE_A_LINKED=True
SETUP_ENTRY_MS=664
DISPLAY_READY=YES
IO_READY=YES
TFT_RST_OUTPUT_ENABLE=YES
TFT_RST_OUTPUT_LATCH=HIGH
TFT_CS_OUTPUT_ENABLE=NO
TFT_CS_OUTPUT_LATCH=LOW
TRACKED_DIRTY_FINAL=0
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
STATUS=PASS
REASON=BASELINE_TIMING_CAPTURED
```

Interpretación:

```text
first valid display frame occurs before setup()
software autoload before setup <= 664 ms from Arduino millis epoch
multi-second firmware-delay hypothesis=NOT_SUPPORTED_BY_PRE1
exact RST-low and first-frame timestamps=NOT_YET_MEASURED
TFT_CS post-init state=DIAGNOSTIC_ONLY
```

PRE1 no mide el instante exacto de power-on, `initPeripherals()` ni del primer
frame; sólo fija un límite superior antes de `setup()`. TFT-PRE2 debe usar
instrumentación temporal source-first y restaurarla byte-for-byte antes del
upload para medir `app_main`, `initArduino`, entrada a `initPeripherals`, RST,
I2C, RTC, FRAM, SD, botones, Display begin, primer refresh y TCA.

Comparación de backend relevante para el arranque:

```text
ALPHA11_BACKEND=Adafruit_ST7789
ALPHA12_CURRENT_BACKEND=JWPLC_TFT -> TFT_eSPI
CURRENT_JWPLC_TFT_BEGIN=g_backend.init() -> setRotation() -> release
CURRENT_EXPLICIT_BLACK_CLEAR_INSIDE_TFT_BEGIN=NO
FIRST_BLACK_CLEAR=Idle phase 0 after jwplcDisplayBeginCallback() returns
BACKLIGHT_SOFTWARE_CONTROL=NO
RST_HOLD_BEFORE_DISPLAY_BEGIN=YES
```

El código Alpha11 de JWPLC tampoco hacía un `fillScreen(BLACK)` dentro de
`tft.init()`: la corrección histórica fue mantener RST bajo durante el autoload
y hacer la inicialización antes de `setup()`. Por tanto la diferencia actual a
investigar es la secuencia interna del backend TFT_eSPI, no la desaparición de
la protección RST.

TFT-PRE2 cerró PASS con instrumentación source-first:

```text
DISPLAY_BEGIN_DURATION_US=558180
FIRST_REFRESH_DURATION_US=14067
RST_TO_DISPLAY_START_US=13145
BOOT_TO_FIRST_REFRESH_US=659341
SETUP_ENTRY_US=664542
SOURCE_RESTORED=True
CORE_SHA_PRESERVED=True
WORKTREE_FINAL=CLEAN
```

Clasificación:

```text
RTC_FRAM_SD_BUTTONS_AS_MAIN_CAUSE=NO
IDLE_FIRST_REFRESH_AS_MAIN_CAUSE=NO
DISPLAY_BEGIN_BACKEND_WINDOW=CONFIRMED
PRODUCT_CHANGE=NO
```

TFT-PRE3 reutiliza el probe de cualificación TFT_eSPI ya existente para
identificar la instalación 2.5.43 usada por el entorno de mantenimiento,
verificar hashes del source y caracterizar la secuencia ST7789 antes de
construir un candidato.

TFT-PRE3 R1 — atribución válida, parser semántico incompleto:

```text
TFT_ESPI_LIB_LIST_VERSION=2.5.43
TFT_ESPI_SELECTED_VERSION=2.5.43
TFT_ESPI_SELECTED_ROOT=C:\Users\jeykc\Documentos\Programacion\Arduino\libraries\TFT_eSPI
TFT_ESPI_HEADER_VERSION=2.5.43
TFT_ESPI_CPP_SHA256=01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1
ST7789_INIT_SHA256=e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10
TFT_ESPI_HEADER_SHA256=b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462
RESET_HIGH_LOW_HIGH_PRESENT=True
RESET_DELAY_150_PRESENT=True
ST7789_DELAY_VALUES_MS=120,10,120,120,120,10,120,120
STATUS=REVIEW
REASON=ST7789_INIT_SEQUENCE_UNEXPECTED
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
ENVIRONMENT_FAILURE=NO
```

F105: el parser asumió únicamente macros `TFT_*`; R2 acepta también
`ST7789_*` y valores literales 0x11/0x13/0x29, y reporta la expresión real
capturada para SLPOUT/NORON/DISPON.

TFT-PRE3 R2 — abortado por syntax preflight:

```text
A13_PWSH_VERSION=7.6.6
A13_TFT_PRE3_SYNTAX=FAIL
COMPILE_EXECUTED=NO
UPLOAD_EXECUTED=NO
PRODUCT_FAILURE=NO
HARNESS_FAILURE=YES
ENVIRONMENT_FAILURE=NO
FAILURE=F106
```

La edición remota R2 se corrompió por semántica de replacement de JavaScript
al insertar texto PowerShell que contenía `$'`. El BAT detectó la sintaxis
inválida antes de ejecutar el gate. R3 fue reconstruido desde el script R1
que sí había ejecutado, conservando el parser semántico corregido con un
mecanismo de replacement seguro.

TFT-PRE3 R3 — cierre:

```text
STATUS=PASS
REASON=TFT_ESPI_2543_SOURCE_ATTRIBUTED
TFT_ESPI_SELECTED_VERSION=2.5.43
TFT_ESPI_CPP_SHA256=01ed6edb0530d38b94ddeac079ba81633aa21d77d049b12da21a37f4bec69ee1
ST7789_INIT_SHA256=e21cae2ac84285dc0e77648eca67ca753f41f7da3c271594ede750b725136c10
TFT_ESPI_HEADER_SHA256=b1b2789ace7ac8fd4a4c414054757d91e6e62649a23d74226ea28ceb8d6f4462
RESET_HIGH_LOW_HIGH_PRESENT=True
RESET_DELAY_150_PRESENT=True
ST7789_SLPOUT_EXPR=ST7789_SLPOUT
ST7789_NORON_EXPR=ST7789_NORON
ST7789_DISPON_EXPR=ST7789_DISPON
ST7789_SLPOUT_BEFORE_DISPON=True
ST7789_FIRST_DELAY_AFTER_DISPON_MS=120
WORKTREE_FINAL=CLEAN
```

Conclusión:

```text
DISPLAY_BEGIN_DURATION_US=558180
backend delay blocks account for nearly all measured duration
visible dirty window begins after backend DISPON
candidate direction=defer DISPON, clear GRAM black, then DISPON
do not mutate installed TFT_eSPI in place
```

TFT-PRE4 sólo genera el candidato bajo `%TEMP%`: no modifica producto ni la
instalación TFT_eSPI. El candidato conserva el delay post-DISPON de 120 ms,
pero lo mueve después de limpiar GRAM a negro.

Referencia externa de drivers:

```text
Adafruit ST7789 generic:
  SWRESET delay 150 ms
  SLPOUT delay 10 ms
  NORON delay 10 ms
  DISPON delay 10 ms

TFT_eSPI ST7789 upstream:
  hardware reset high/low/high + reset wait
  SLPOUT delay 120 ms
  delay 120 ms before DISPON
  DISPON
  delay 120 ms after DISPON
```

Esto es consistente con el video nuevo: la pantalla se hace blanca cerca de
+750 ms y el primer frame limpio empieza ~180 ms después. TFT-PRE2 medirá el
tiempo real de `jwplcDisplayBeginCallback()` ejecutando el archive actual, sin
cambiar aún el producto.

Si el fenómeno es sólo transitorio, la mejora software posible es adelantar
CS=HIGH/RST=LOW al punto seguro más temprano de `app_main()`; eso reduce la
ventana pero no puede eliminar el intervalo anterior al firmware. La solución
eléctrica absoluta requeriría mantener RST definido durante power-on o controlar
backlight, lo cual pertenece a hardware/revisión de placa y no se asumirá sin
revisar el esquemático.

## Gates restantes

```text
G2  A13-002 TCA startup / EN_IO                  CLOSED_PASS
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
