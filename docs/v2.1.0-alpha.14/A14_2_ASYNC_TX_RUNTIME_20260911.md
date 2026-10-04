# A14.2 — Runtime físico de TX TCP cooperativo

Fecha: `2026-09-11`

## Objetivo

Validar que el envío TCP usado por el futuro Client/Master Modbus TCP no depende del `EthernetClient::write()` bloqueante y que el W5500 puede completar `Sock_SEND -> SEND_OK` mediante pasos cooperativos cortos.

## Configuración

```text
JWPLC_IP=192.168.0.31
PC_LISTENER=192.168.0.4:15021
PAYLOAD_BYTES=1024
PAYLOAD_PATTERN=byte[i] = i & 0xFF
```

## Evidencia PC

```text
LISTENER_READY=PASS
LISTENING=0.0.0.0:15021
EXPECTED_BYTES=1024
TCP_ACCEPT=PASS
REMOTE=192.168.0.31:56774
RX_BYTES=1024
PATTERN_MISMATCHES=0
CHECKSUM=130560
PAYLOAD_PATTERN=PASS
TX_ACK=PC_ACK
LISTENER_RESULT=PASS
```

## Evidencia JWPLC

```text
A14_2_ASYNC_TX_RUNTIME=PASS
PAYLOAD_BYTES=1024
TX_BEGIN_US=1612
TX_COMPLETE_MS=2
TX_POLLS=1
LOOPS_WHILE_PENDING=2
MAX_TX_POLL_US=42
```

El resumen fue repetido por Serial sin resets ni cambio de resultado:

```text
A14_2_ASYNC_TX_RUNTIME=PASS bytes=1024 tx_begin_us=1612 tx_complete_ms=2 polls=1 loops_pending=2 max_poll_us=42
```

## Conclusión

```text
A14_2_ASYNC_TX_API_COMPILE=PASS
A14_2_ASYNC_TX_RUNTIME=PASS
TCP_PAYLOAD_INTEGRITY=PASS
TCP_SEND_OK_COOPERATIVE=PASS
UNEXPECTED_RESET=0
```

El `begin()` del helper carga el frame y dispara `Sock_SEND`; la finalización se observa después mediante `poll()`. El payload completo llegó al PC sin discrepancias y el `loop()` continuó ejecutándose mientras el SEND seguía pendiente.

Con este gate quedan físicamente validadas las dos primitivas de transporte requeridas por el Client/Master A14.2:

```text
ASYNC_TCP_CONNECT=PASS
ASYNC_TCP_TX=PASS
```
