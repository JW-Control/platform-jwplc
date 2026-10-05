# Alpha14 — G2-R1 INT TCP repeatability — PREP 2026-10-01

## Objetivo

Confirmar la repetibilidad del candidato TCP guiado por INT GPIO15 antes de
diseñar su integración productiva.

G2 mostró una separación muy fuerte en IDLE/CONTROLLED y una señal de +6.276 %
en RAW SATURATED. Esta última no se toma todavía como mejora causal porque la
sesión absoluta quedó por debajo del ceiling RAW previo de G1.

## Diseño

Se conservan exactamente las dos variantes de G2:

- `POLLING`;
- `INT_GUIDED`.

Ambas:

- profile hooks ON;
- SINGLE_STATUS same-pass ON;
- W5500 26 MHz;
- 8x2KB;
- FIFO_REUSE ON;
- DLEN_REUSE ON;
- COPY_OUT_64 ON;
- DIRECT_RX OFF;
- commit TCP inmediato;
- batch8.

No hay cambio de producto.

## Repeticiones

Se ejecutan 3 corridas por variante en cada carga, con fresh upload por caso.

### CONTROLLED

20 s por corrida.

Orden balanceado:

```text
POLLING
INT_GUIDED
INT_GUIDED
POLLING
POLLING
INT_GUIDED
```

La primera corrida INT ejecuta también el probe disconnect/reconnect.

### SATURATED

30 s por corrida.

Orden balanceado inverso:

```text
INT_GUIDED
POLLING
POLLING
INT_GUIDED
INT_GUIDED
POLLING
```

## Interpretación

CONTROLLED debe demostrar una separación repetible de ocupación SPI y ausencia
de regresión de throughput.

SATURATED debe demostrar al menos no-regresión. Sólo se declarará
`THROUGHPUT_GAIN=CONFIRMED` si la mediana INT supera a POLLING en al menos
2 % dentro de esta batería.

El gate no productiza INT. Si termina:

```text
G2R1_PRODUCTIZATION_DESIGN_READY=YES
```

el siguiente paso será G3: decidir dónde vive la política INT dentro del
package/runtime y cómo compartirla con TCP/UDP sin romper las APIs Arduino
legacy.
