# v2.1.0-alpha.13 — Estado operativo y continuidad

Actualizado: 2026-10-06 — cierre G1

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
NEXT_GATE=G2 / A13-002
OBJECTIVE=TCA startup / EN_IO
STATE=READY_FOR_READ_ONLY_PREFLIGHT
PREREQUISITE=G1_CLOSED_PASS
```

No modificar todavía código productivo de G2 hasta completar su preflight
dirigido y fijar el contrato del gate.

## Gates restantes

```text
G2  A13-002 TCA startup / EN_IO                  READY
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
