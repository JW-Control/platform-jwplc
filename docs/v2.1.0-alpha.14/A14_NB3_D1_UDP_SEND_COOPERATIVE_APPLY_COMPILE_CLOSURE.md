# Alpha14 — NB3-D1: UDP SEND cooperative engine apply + compile

## Objetivo

Introducir un motor cooperativo para el SEND UDP sin cambiar la frecuencia SPI, sin tocar `parsePacket()` y preservando la API Arduino legacy.

## Estado del candidato

```text
EFFECTIVE_SPI_HZ=26000000
TRACKED_DIRTY_FINAL=11
STAGED_COUNT_FINAL=0
UPLOAD=NO
```

## Contrato de source validado

```text
SOCKET_UDP_DIRECT_SEND_OK_WAIT_COUNT=0
SOCKET_TCP_LEGACY_SEND_OK_WAIT_COUNT=1
SOCKET_UDP_WAIT_SCOPE=FUNCTION_BODY_ONLY
SOCKET_TCP_LEGACY_WAIT=EXPECTED_PENDING_NB3F
```

Primitives backend:

```text
socketBeginSendUDP HEADER_DECL_COUNT=1 CPP_DEF_COUNT=1
socketPollSendUDP  HEADER_DECL_COUNT=1 CPP_DEF_COUNT=1
```

API pública cooperativa UDP:

```text
beginEndPacketAsync       HEADER_DECL_COUNT=1 CPP_DEF_COUNT=1
pollEndPacketAsync        HEADER_DECL_COUNT=1 CPP_DEF_COUNT=1
endPacketAsyncInProgress  HEADER_DECL_COUNT=1 CPP_DEF_COUNT=1
cancelEndPacketAsync      HEADER_DECL_COUNT=1 CPP_DEF_COUNT=1
```

Integración DNS:

```text
DNS_ASYNC_UDP_BEGIN_COUNT=1
DNS_ASYNC_UDP_POLL_COUNT=1
DNS_SYNC_END_PACKET_COUNT=0
DNS_ASYNC_SEND_STATE_COUNT=1
```

## Compilación

```text
RAW_COMPILE_EXIT=0
RAW_REPO_ETHERNET_LIBRARY_USED=True

API_PROBE_COMPILE_EXIT=0
API_PROBE_REPO_ETHERNET_LIBRARY_USED=True

RAW_BIN_COUNT=4
API_PROBE_BIN_COUNT=4
```

El probe Arduino se limita deliberadamente a APIs públicas. Las primitives privadas de `EthernetClass` se validan por firma/definición y por compilación del candidato completo.

## Compatibilidad

```text
NB3_UDP_SEND_SOCKET_ENGINE=COOPERATIVE_BEGIN_POLL
NB3_UDP_ENDPACKET_LEGACY=WRAPPER_PRESERVED
NB3_DNS_ASYNC_UDP_SEND=COOPERATIVE
NB3_DNS_RESPONSE_TIMER_START=AFTER_UDP_SEND_OK
NB3_PARSE_PACKET_CHANGE=NO
NB3_SPI_FREQUENCY_CHANGE=NO
```

## Resultado

```text
A14_NB3_D1_UDP_SEND_COOPERATIVE_APPLY_COMPILE=PASS
```

NB3-D1 cierra únicamente source/API/compile. La validación física pertenece a NB3-D2.
