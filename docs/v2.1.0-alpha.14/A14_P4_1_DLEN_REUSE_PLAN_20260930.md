# Alpha14 — P4.1 DLEN_REUSE — plan 2026-09-30

## Objetivo

Reducir el coste repetido de programación de longitud del helper
`jwplcSpiReadBytesReuseFifoNL()` sin cambiar ninguna otra variable del
transporte W5500.

Baseline actual por chunk:

```cpp
dev->mosi_dlen.usr_mosi_dbitlen = (c_len * 8U) - 1U;
dev->miso_dlen.usr_miso_dbitlen = (c_len * 8U) - 1U;
```

P4.1 conserva el último `c_len` programado únicamente durante una invocación
del helper y evita reescribir DLEN cuando el siguiente chunk tiene la misma
longitud.

No existe cache persistente entre transacciones ni entre dueños del bus.

## Única variable

```text
ONLY_VARIABLE=DLEN_PROGRAMMING_POLICY

BASELINE:
  write DLEN every chunk

CANDIDATE:
  write DLEN only when c_len changes
```

Constantes:

```text
W5500_SPI_HZ=26000000
FIFO_REUSE=ON_BOTH
DIRECT_RX=OFF_BOTH
RX_COMMIT=IMMEDIATE
COPY_OUT=UNCHANGED
BATCH_POLICY=UNCHANGED
```

El candidato se introduce como:

```text
JWPLC_SPI_FIFO_REUSE_DLEN_CACHE=0
```

por defecto. No se promociona antes del resultado del gate.

## Motivación

El microperfil P4 anterior obtuvo:

```text
CHUNK_COUNT=87227
BYTES=4504177
AVG_CHUNK_BYTES=51.637

SETUP_TOTAL_US=72830
WIRE_WAIT_TOTAL_US=2051640
COPY_OUT_TOTAL_US=157270
OTHER_TOTAL_US=52038

SETUP_PCT=3.121
WIRE_WAIT_PCT=87.911
COPY_OUT_PCT=6.739
OTHER_PCT=2.230
```

P4.1 sólo puede atacar directamente el bloque SETUP. No se espera que elimine
el START_WAIT_EXCESS dominante.

## Gate

`a14_p4_1_w5500_dlen_reuse_ab.py` ejecuta:

1. verificación FNV del candidato;
2. microperfil BASELINE vs DLEN_REUSE;
3. A/B de performance alternado;
4. revisión física;
5. clasificación sin promoción automática.

Modo inicial:

```text
3 runs por variante
15 s por run
```

Modo de confirmación si el efecto queda pequeño/inconcluso:

```text
--confirmation
5 runs por variante
```

## Criterios

Integridad obligatoria:

```text
FNV actual = FNV esperado
TRANSPORT_ERRORS=0
TCP_SPI_LOCK_ERRORS=0
UNEXPECTED_RESETS=0
```

El mecanismo se considera demostrado sólo si el coste de setup por chunk baja
al menos 2%.

Una ganancia de sistema requiere simultáneamente:

```text
PAYLOAD_DELTA >= +0.25%
US_PER_BYTE_DELTA <= -0.25%
```

Una regresión material:

```text
PAYLOAD_DELTA <= -0.50%
OR
US_PER_BYTE_DELTA >= +0.50%
```

El gate distingue:

```text
DLEN_REUSE_GAIN_CONFIRMED
DLEN_REUSE_NO_MATERIAL_SYSTEM_GAIN
DLEN_REUSE_SMALL_OR_INCONCLUSIVE_EFFECT
DLEN_REUSE_REPEATABILITY_INSUFFICIENT
DLEN_REUSE_REGRESSION
DLEN_REUSE_MECHANISM_NOT_CONFIRMED
```

## Decisión de flujo

```text
H3E_R=CLOSED_PASS
P4_1=NEXT
P4_2_BLOCKED_UNTIL_P4_1_DECISION=YES
```
