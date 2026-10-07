# v2.1.0-alpha.13 — Estado operativo y continuidad

Actualizado: 2026-10-06

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
G1_P3_R4=READY
```

Candidato actual:

```text
FILE=JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp
SHA256=08291b4b89274f014e1ce6073bd16303192e5bfe1b4e2fdeaa21d133fb783995
TRACKED_DIRTY_EXPECTED=1
STAGED_EXPECTED=0
COMMIT=NO
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

Causa de harness:

```text
probe READY = one-shot
gate delay after upload = 800 ms
client wait starts after serial open
READY emitted before client open => marker lost permanently
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

## Infraestructura G1-P3-R4

Versionada en commits de tooling/docs descendientes de `BASELINE_HEAD`. El gate
acepta esa cadena sólo si los archivos commiteados desde el baseline pertenecen
al allowlist de tooling/docs y confirma que `Dns.cpp` sigue sin commit:

```text
tools/alpha13/gates/common.ps1
tools/alpha13/gates/a13_g1_p3_dns_physical.ps1
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
NEXT_GATE=A13-G1-P3-R4
OBJECTIVE=physical DNS regression
PREREQUISITE=JWPLC connected and serial port visible
```

Ejecución recomendada después de sincronizar el commit de tooling:

```text
.\tools\alpha13\gates\run_a13_g1_p3_dns_physical.bat
```

Si la autodetección no es inequívoca:

```text
.\tools\alpha13\gates\run_a13_g1_p3_dns_physical.bat COM4
```

Si PASS:

```text
final diff audit
-> stage Dns.cpp only
-> commit productivo A13-001
-> update this file
-> push
-> close G1
-> open G2 / A13-002
```

## Gates restantes

```text
G2  A13-002 TCA startup / EN_IO                  PENDING
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
G1 product candidate is still uncommitted
```

## DO NOT DO

```text
DO_NOT_REPEAT_P0
DO_NOT_REPEAT_G1_PRE1_P1_P2_WITHOUT_NEW_EVIDENCE
DO_NOT_COMMIT_DNS_CPP_BEFORE_G1_P3_R1_PASS
DO_NOT_REMOVE_NORMAL_AUTOLOAD_PERIPHERALS
DO_NOT_ASSUME_OPENPLC_INTEGRATED
DO_NOT_ASSUME_OTA_DEFINED
DO_NOT_FIX_FINAL_FLASH_FREQ_WITHOUT_DECISION
DO_NOT_PUBLISH_BOOTLOADER_BIN_AS_FINAL
DO_NOT_IMPLEMENT_JW_BUSIO_YET
DO_NOT_OPEN_G2_UNTIL_G1_CLOSED
```
