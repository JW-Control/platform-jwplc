# Alpha14 — NB3-E1: cierre apply + compile de hardening UDP parsePacket

## Objetivo

Eliminar el riesgo de loop infinito en `EthernetUDP::parsePacket()` cuando quedan bytes pendientes del datagrama anterior y `read()` falla o drena sólo parcialmente.

## Cambio

Antes:

```cpp
while (_remaining) {
    read((uint8_t *)NULL, _remaining);
}
```

Después:

```cpp
if (_remaining > 0) {
    const int drained = read((uint8_t *)NULL, _remaining);

    if (drained <= 0 || _remaining > 0) {
        return 0;
    }
}
```

El comportamiento pasa de potencialmente no acotado a un máximo de un intento de drenado por llamada a `parsePacket()`.

## Evidencia de source

```text
NB3_E1_OLD_PARSE_DRAIN_PRESENT=True
NB3_E1_NEW_PARSE_DRAIN_PRESENT=False
NB3_E1_PATCH_APPLICATION=APPLIED

UDP_PARSE_VERIFY_SCOPE=PARSE_PACKET_FUNCTION_BODY
UDP_PARSE_REMAINING_WHILE_COUNT=0
UDP_PARSE_SINGLE_DRAIN_GUARD_COUNT=1
UDP_PARSE_RETRY_RETURN_GUARD_COUNT=1
```

Hash resultante:

```text
UDP_CPP_SHA256_AFTER=F5AE79FA9E9642A670D0AF23E029621711D2DD857B776A5208CF2866EDB496E5
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

## Contrato preservado

```text
NB3_UDP_PARSE_UNBOUNDED_REMAINING_LOOP=REMOVED
NB3_UDP_PARSE_VERIFICATION=FUNCTION_SCOPED
NB3_UDP_PARSE_DRAIN_ATTEMPTS_PER_CALL=ONE
NB3_UDP_PARSE_RECV_FAILURE=RETURN_ZERO_RETRY_NEXT_CALL
NB3_UDP_PARSE_PARTIAL_DRAIN=RETURN_ZERO_RETRY_NEXT_CALL
NB3_UDP_PARSE_PUBLIC_API=PRESERVED
NB3_UDP_RX_THROUGHPUT_OPTIMIZATION=NOT_IN_THIS_GATE
NB3_SPI_FREQUENCY_CHANGE=NO
NB3_UPLOAD=NO
```

## Resultado

```text
A14_NB3_E1_UDP_PARSE_PACKET_HARDEN_APPLY_COMPILE=PASS
```

NB3-E1 cierra source/API/compile. La validación física corresponde a NB3-E2.
