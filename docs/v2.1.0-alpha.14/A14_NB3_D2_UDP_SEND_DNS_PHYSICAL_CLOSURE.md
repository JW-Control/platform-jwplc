# Alpha14 — NB3-D2: cierre físico DNS con UDP SEND cooperativo

## Objetivo

Validar físicamente que el motor UDP SEND cooperativo reduce el hold observado en `DNSClient::beginResolveAsync()` sin romper resolución DNS, timeout cooperativo ni convivencia SPI.

## Baseline NB2

```text
DNS_BEGIN_HOLD_MAX_US=6033
DNS_POLL_HOLD_MAX_US=369
LOOP_GAP_MAX_US=2383
SPI_LOCK_ERRORS=0
```

## Resultado NB3-D2

```text
DUT_IP_EFFECTIVE=192.168.0.159
SUCCESS_RESULT_IP=10.20.30.40
SUCCESS_DURATION_MS=1
SUCCESS_POLL_COUNT=8
SUCCESS_POLL_PENDING_COUNT=7

TIMEOUT_DURATION_MS=453
TIMEOUT_POLL_COUNT=7160
TIMEOUT_POLL_PENDING_COUNT=7159

DNS_BEGIN_HOLD_MAX_US=1571
DNS_POLL_HOLD_MAX_US=373
LOOP_GAP_MAX_US=2363
SPI_LOCK_ERRORS=0

DNS_VALID_QUERY_COUNT=1
DNS_TIMEOUT_QUERY_COUNT=1
DNS_OTHER_QUERY_COUNT=0
VISUAL_SPI_EVENTS=0
```

## Mejora

```text
DNS_BEGIN_HOLD_BASELINE_US=6033
DNS_BEGIN_HOLD_MAX_US=1571
DNS_BEGIN_HOLD_REDUCTION_US=4462
DNS_BEGIN_HOLD_REDUCTION_PCT=74
```

El hold inicial cayó de aproximadamente 6.0 ms a 1.57 ms.

El poll se mantuvo prácticamente en el mismo orden que NB2:

```text
NB2: 369 us
NB3-D2: 373 us
```

El loop gap también se mantuvo estable:

```text
NB2: 2383 us
NB3-D2: 2363 us
```

## Conclusión

El SEND UDP cooperativo elimina el hold prolongado atribuido al antiguo `endPacket()/socketSendUDP()` síncrono dentro del camino DNS async.

```text
NB3_DNS_DYNAMIC_IP=PASS
NB3_DNS_VALID_RESOLUTION=PASS
NB3_DNS_TIMEOUT_PENDING=PASS
NB3_DNS_UDP_SEND_COOPERATIVE=PASS
NB3_DNS_BEGIN_HOLD_TARGET_5MS=PASS
NB3_DNS_BEGIN_HOLD_IMPROVED_FROM_6033US=PASS
NB3_DNS_POLL_HOLD_5MS=PASS
NB3_DNS_LOOP_GAP_10MS=PASS
NB3_DNS_SPI_LOCK_ERRORS=ZERO
NB3_DNS_VISUAL_SPI=PASS
A14_NB3_D2_UDP_SEND_DNS_PHYSICAL=PASS
```

## Pendiente inmediato

NB3-D3 debe verificar que el mismo backend no introduzca regresión funcional o de integridad en UDP TX raw.

No se modifica todavía `parsePacket()`.
