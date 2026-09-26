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

## Gate versionado

El gate fuerza una sola compilacion/upload desde las fuentes actuales,
restaura luego el archive precompilado previo con el mismo SHA-256 y ejecuta
cuatro ventanas de 30 s con TCP OFF.

Para cada baud se registran por separado:

```txt
RTU_BAUD            = solicitado por el package
RTU_BAUD_EFFECTIVE  = leido del driver UART
ERROR_PCT           = diferencia efectiva vs solicitada
```

El resultado se agrupa tambien por la zona de reloj esperada segun el core:

```txt
REF_TICK: 230400,250000
APB:      460800,500000
```

Un punto fallido no detiene el sweep; la finalidad es conservar la curva
completa y aislar si el fallo de 230400 esta asociado al regimen de reloj o a
un baud concreto.

### Nota de tooling

Durante la generacion inicial del gate reaparecio el patron ya catalogado
F059: backticks de PowerShell dentro de un template JavaScript. La ejecucion
se detuvo antes de cualquier mutacion remota. El gate final evita
deliberadamente continuaciones PowerShell con backtick.

## Resultado fisico RTU-H1B

| Baud solicitado | Baud efectivo | Error | Reloj esperado | RTU | Fallos | Timeout | CRC Master | CRC Slave | Estado |
|---:|---:|---:|---|---:|---:|---:|---:|---:|---|
| 230400 | 231884 | +0.6441 % | REF_TICK | 282.677 Hz | 4852 | 322 | 4530 | 322 | FAIL |
| 250000 | 250000 | 0.0000 % | REF_TICK | 393.875 Hz | 0 | 0 | 0 | 0 | PASS |
| 460800 | 460929 | +0.0280 % | APB | 450.543 Hz | 0 | 0 | 0 | 0 | PASS |
| 500000 | 500000 | 0.0000 % | APB | 468.040 Hz | 0 | 0 | 0 | 0 | PASS |

Resumen del runner:

```txt
RTUH1B_CLEAN_REQUESTED_BAUDS=250000,460800,500000
RTUH1B_REF_TICK_CLEAN=250000
RTUH1B_APB_CLEAN=460800,500000
RTUH1B_REF_TICK_PATTERN=250000_PASS_230400_FAIL
RTUH1B_APB_PATTERN=460800_AND_500000_PASS
A14_RTU_H1B=PASS_CHARACTERIZED
```

### Conclusion de H1B

El fallo de 230400 es reproducible, pero no representa una limitacion general
de velocidad del hardware RS-485:

- 250000 usa tambien REF_TICK y queda completamente limpio;
- 460800 y 500000 quedan limpios sobre APB;
- Master y Slave reportan el mismo baud efectivo en todos los puntos.

230400 queda como configuracion problematica/no recomendada hasta completar la
revision documental del UART ESP32/ESP-IDF y del MAX13487E. Esa investigacion
se realizara despues del sweep H2 de gap a 500 kbaud.

