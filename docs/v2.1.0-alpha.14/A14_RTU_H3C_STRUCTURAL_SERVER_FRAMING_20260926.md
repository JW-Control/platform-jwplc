# Alpha14 — RTU-H3C — Framing estructural del Slave

Fecha: 2026-09-26

## Evidencia de H3B.3

H3B.3 reprodujo el fallo con:

```txt
RTU_STARTED=406909
RTU_SUCCESS=406905
RTU_FAILED=4
RTU_TIMEOUTS=4
REQUEST_PATH_GAP=4
RESPONSE_PATH_GAP=0
MASTER_TX_BYTES=3255272
SLAVE_RX_BYTES=3255272
SLAVE_TX_BYTES=3662145
MASTER_RX_BYTES=3662145
REQUEST_BYTE_GAP=0
RESPONSE_BYTE_GAP=0
SLAVE_DISCARDED_TAILS=8
SLAVE_DISCARDED_BYTES=32
MASTER_CRC=0
SLAVE_CRC=0
```

Los bytes llegaron al motor RTU del Slave, pero parte del flujo fue descartado
por `pollServer()` antes de `processServerFrame()`.

El benchmark usa FC03 request de 8 bytes. Los 32 bytes descartados coinciden en
magnitud con cuatro requests completas, igual que los cuatro timeouts
observados. La correspondencia evento-a-evento no está instrumentada, pero la
evidencia descarta pérdida pre-parser en esta corrida.

## Hipótesis H3C

El framing legacy decide fin de trama sólo por:

```txt
micros() - lastByteUs >= frameGapUs
```

Con `frameGapUs=100`, una request local puede quedar temporalmente visible en
fragmentos para el software y ser descartada antes de que llegue el resto al
parser.

H3C agrega framing estructural opcional del Slave:

1. si una request local conocida está completa, se procesa inmediatamente;
2. si sólo existe un prefijo local incompleto, no se descarta a los 100 us;
3. el prefijo se conserva hasta completarse o hasta 1750 us;
4. tráfico desconocido conserva el framing legacy por gap.

## Compatibilidad

La API es aditiva:

```cpp
void setEarlyServerDispatchEnabled(bool enabled);
bool earlyServerDispatchEnabled() const;
```

Default:

```txt
EARLY_SERVER_DISPATCH=false
```

Por tanto, ninguna API ni comportamiento histórico cambia antes de cerrar la
qualification.

## Perfil del gate

```txt
BAUD=500000
CLOCK=APB_FORCED
FRAME_GAP_US=100
RX_FIFO_FULL=1
RX_MODE=BULK
MASTER_SERVER_FRAMING=GAP
SLAVE_SERVER_FRAMING=STRUCTURAL
MOTOR=ASYNC
TX_MODE=QUEUED
RTU_TIMEOUT_MS=25
TCP=500 req/s
DURATION=600 s
BUCKET=60 s
W5500_SPI_HZ=26000000
FULL_RUNTIME=ACTIVE
```

El Master mantiene framing GAP. Sólo el Slave cambia a STRUCTURAL.

## Criterio de cierre

```txt
RTU_HZ >= 650
RTU_FAILED=0
RTU_TIMEOUTS=0
REQUEST_PATH_GAP=0
RESPONSE_PATH_GAP=0
REQUEST_BYTE_GAP=0
RESPONSE_BYTE_GAP=0
SLAVE_DISCARDED_TAILS=0
SLAVE_DISCARDED_BYTES=0
MASTER_CRC=0
SLAVE_CRC=0
TCP_TARGET_PCT >= 99
cada bucket TCP >= 495 req/s
SD_CLEAN=YES
PERIPHERAL_FAILURE_COUNT=0
TFT Master/Slave estable
```

PASS esperado:

```txt
A14_RTU_H3C=PASS_STRUCTURAL_LONGRUN_600S
```

Si H3C pasa, se considera corregida la causa observada en H3B.2/H3B.3. La
decisión sobre convertir STRUCTURAL y/o Bulk RX en defaults se toma en un gate
posterior, sin mezclarla con esta validación causal.

Además, H3C puede aumentar throughput porque una request estructuralmente
completa ya no espera los 100 us de frame gap antes de ser atendida. Ese aumento
se medirá, pero no es requisito para declarar corregido el fallo.
