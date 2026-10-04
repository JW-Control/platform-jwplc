# Alpha14 — G1 TCP SPI Waste Baseline — PREP

Fecha: 2026-10-01

## Objetivo

Caracterizar cuánto del trabajo del W5500 en TCP corresponde a recepción útil y cuánto
corresponde a pasadas de servicio sin payload antes de evaluar un scheduler guiado por
INT GPIO15.

G1 no implementa INT y no cambia la política TCP normal del producto.

## Instrumentación

La instrumentación existente de `JWPLC_Ethernet` se mantiene detrás de
`JWPLC_ETHERNET_ENABLE_PROFILE_HOOKS`, cuyo default continúa en `0`.

G1 añade únicamente clasificación diagnóstica de:

- `recvAvailableZeroCalls`;
- `recvAvailableNonzeroCalls`.

El firmware de benchmark añade contadores locales de:

- `TCP_SERVICE_PASSES`;
- `TCP_SERVICE_ACTIVE_PASSES`;
- `TCP_SERVICE_EMPTY_PASSES`.

Un `SERVICE_EMPTY` significa que una pasada medida en modo `TCP_RX` no terminó
consumiendo payload RX. No implica que la pasada no haya realizado trabajo válido de
status/lifecycle.

## Cargas

### IDLE

Conexión TCP establecida, modo `TCP_RX`, sin payload durante 15 s.

Permite observar el costo puro de polling/lifecycle.

### CONTROLLED

Modelo RAW TCP con ingress de 12 bytes a 1000 Hz durante 30 s.

Los 12 bytes representan el tamaño de una solicitud Modbus TCP FC03 típica. Este caso
sirve para estudiar la esparsidad temporal de RX, pero **no es un benchmark Modbus TCP
completo**: no valida parsing MBAP, respuesta FC03 ni request-response latency.

### SATURATED

RAW TCP RX con envíos de 4096 bytes en loop durante 30 s.

Permite distinguir el trabajo inevitable de mover payload bajo saturación del polling
que podría evitarse.

## Observer effect

Cada carga se ejecuta una vez con:

- BASE: profile hooks OFF;
- PROFILE: profile hooks ON.

Se reporta el delta PROFILE vs BASE en throughput para CONTROLLED y SATURATED. G1 es
un gate diagnóstico; no promueve ninguna optimización.

## Ventana de medición

La secuencia conserva la disciplina existente:

1. conectar;
2. entrar a `TCP_RX`;
3. resetear counters;
4. ventana quieta sin snapshots periódicos;
5. freeze;
6. snapshot final.

## Decisión posterior

G1 termina con:

`G1_INT_TCP_OPPORTUNITY=REVIEW_REQUIRED`

La decisión HIGH/MIXED/LOW se toma después de revisar:

- service empty %;
- available zero %;
- status calls;
- SPI hold occupancy;
- observer effect;
- diferencias entre IDLE, CONTROLLED y SATURATED.

Sólo después se abre G2 — INT GPIO15 TCP A/B.

## Contrato de producto

- `JWPLC_ETHERNET_ENABLE_PROFILE_HOOKS=0` por defecto.
- No se activa INT.
- No cambia W5500 26 MHz.
- No cambia FIFO_REUSE.
- No cambia DLEN_REUSE.
- No cambia COPY_OUT_64.
- No cambia DIRECT_RX.
- No cambia commit TCP.
- No cambia batching TCP.
- No cambia API Arduino legacy.
