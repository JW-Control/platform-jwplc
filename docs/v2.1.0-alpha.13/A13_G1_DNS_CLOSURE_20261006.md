# Alpha13 — G1 / A13-001 — Cierre DNS parser truncado

Fecha: 2026-10-06

## Resultado

```text
G1=A13-001
STATUS=CLOSED_PASS
PRODUCT_COMMIT=8bc48b73074859d367b2d949e333f0b16c83cbd7
FILE=JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp
SHA256=08291b4b89274f014e1ce6073bd16303192e5bfe1b4e2fdeaa21d133fb783995
```

## Problema

El parser DNS histórico realizaba lecturas de UDP sin verificar que cada
lectura hubiera consumido bytes. En datagramas truncados, una lectura podía
fallar sin modificar el buffer y dejar estado anterior dentro de los bucles de
parsing, permitiendo pérdida de progreso o bloqueo.

## Corrección

`DNSClient::ProcessResponsePacket()` usa una lectura exacta con progreso
obligatorio. Si faltan bytes en header, nombres, TYPE/CLASS, TTL, RDLENGTH,
RDATA o IPv4, retorna `TRUNCATED` en lugar de continuar con estado obsoleto.

No se cambia la API pública.

## Evidencia

```text
G1_PRE1=PASS
G1_P1=PASS
G1_P2=PASS
BASELINE_TRUNCATED_LOOP_REPRODUCED=True
PATCH_COMPILE=PASS
VALID_DNS_PACKETS=3/3
TRUNCATED_PREFIXES=136/136
```

Gate físico final:

```text
GATE=A13-G1-P3-R5
STATUS=PASS
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
```

## Topología física del PASS

```text
USB/serial -> COM4
W5500/RJ45 -> LAN con DHCP
PC -> misma LAN
```

## Cierre Git

```text
FINAL_DIFF_AUDIT=PASS
GIT_DIFF_CACHED_CHECK=PASS
STAGED_FILE_COUNT=1
PRODUCT_COMMIT=8bc48b73074859d367b2d949e333f0b16c83cbd7
REMOTE_PUSH=PASS
WORKTREE_AFTER_COMMIT=CLEAN
```

## Decisión

```text
A13-001=ADOPTED_AND_CLOSED
G1=CLOSED_PASS
NEXT=G2 / A13-002 TCA startup / EN_IO
```
