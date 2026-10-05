# Alpha14 — NB3-E2: cierre físico partial-drain de UDP parsePacket

## Objetivo

Validar físicamente el hardening de `EthernetUDP::parsePacket()` en el caso específico que podía dejar `_remaining > 0` entre llamadas.

## Escenario

```text
PC envia 256 bytes
JWPLC parsePacket() = 256
JWPLC lee 8 bytes
_remaining = 248
siguiente ciclo parsePacket()
drena restante
_remaining = 0
PC envia NEXT
JWPLC recibe NEXT
```

## Evidencia

```text
PARTIAL_PACKET_SIZE=256
PARTIAL_READ_BYTES=8
PARTIAL_REMAINING_BEFORE_DRAIN=248

DRAIN_PARSE_RETURN=0
DRAIN_REMAINING_AFTER=0
DRAIN_HOLD_US=63

NEXT_PACKET_RECOVERED=YES
SPI_LOCK_ERRORS=0
LOOP_GAP_MAX_US=202
VISUAL_SPI_EVENTS=0
```

## Resultado

```text
NB3_E2_PARTIAL_PACKET_CREATED=PASS
NB3_E2_REMAINING_248_CONFIRMED=PASS
NB3_E2_SINGLE_DRAIN_COMPLETED=PASS
NB3_E2_PARSE_RETURNED_TO_LOOP=PASS
NB3_E2_NEXT_PACKET_RECOVERY=PASS
NB3_E2_DRAIN_HOLD_5MS=PASS
NB3_E2_LOOP_GAP_10MS=PASS
NB3_E2_SPI_LOCK_ERRORS=ZERO
NB3_E2_VISUAL_SPI=PASS
A14_NB3_E2_UDP_PARSE_PACKET_PHYSICAL=PASS
```

NB3-E2 valida robustez y recovery. El rendimiento normal UDP RX se valida por separado en NB3-E3.
