# Alpha14 — RTU-H1B — Sanity de baudrate y reloj UART

Fecha: 2026-09-26

## Motivo

RTU-H1 produjo un resultado no monotonicamente explicable por velocidad:

```txt
115200 -> PASS
230400 -> FAIL severo por CRC
500000 -> PASS
```

Antes de optimizar el gap a 500 kbaud se aisla la frontera de reloj UART.

## Hallazgo estatico en el core ESP32 actual

`esp32-hal-uart.c` define:

```txt
REF_TICK_BAUDRATE_LIMIT = 250000
```

En ESP32 clasico, cuando no hay una fuente de reloj UART elegida explicitamente:

- baud <= 250000 usa UART_SCLK_REF_TICK;
- baud > 250000 usa UART_SCLK_APB.

Por ello:

| Baud | Zona de reloj esperada |
|---:|---|
| 230400 | REF_TICK |
| 250000 | REF_TICK |
| 460800 | APB |
| 500000 | APB |

Esta observacion es una pista, no una conclusion. H1B la contrasta fisicamente.

## Diagnostico nuevo de package

Se anade sin romper la API existente:

```cpp
JWPLC_RS485.effectiveBaudRate();
JWPLC_ModbusRTU.effectiveBaudRate();
```

`baudRate()` sigue reportando el baud solicitado/configurado. La nueva funcion
consulta al driver UART el baud efectivo derivado de su reloj/divisor real.

## Sweep

```txt
MOTOR=ASYNC
TX=QUEUED
FRAME_GAP=500 us
TCP=OFF
DURATION=30 s
```

| Comando benchmark | Baud |
|---|---:|
| 8 | 230400 |
| 0 | 250000 |
| 6 | 460800 |
| 9 | 500000 |

Se mide requested baud, effective baud, error porcentual y estabilidad RTU.

## Interpretacion esperada

- Si 230400 falla y 250000 pasa, el borde REF_TICK/divisor queda fuertemente
  implicado.
- Si 460800 y 500000 pasan, APB queda reforzado como zona adecuada para high
  baud en este ESP32.
- Si 460800 falla pero 500000 pasa, se investigara precision/divisor especifico
  dentro de APB.
- No se cambia aun el baudrate default del package.

Despues de H1B, si 500000 se reafirma limpio, se pasa al tuning de gap a
500 kbaud.
