# Gate A15 I2 · Fecha/hora del RTC en VALUE

```text
A15_I2_HOST_PARITY=PASS        (tests/value-format-parity.cjs: 55 relojes firmware == Designer)
A15_I2_CODEGEN=PASS            (golden i2Clock.h: sin variable ni setter para los relojes)
A15_I2_ESP32_COMPILE=PASS      (arduino-cli, jwplc_local:esp32:jwplcbasic)
A15_I2_PHYSICAL=[no verificado]
```

## Qué mirar en la TFT

| Paso | Se espera |
|---|---|
| 1. RTC ajustado, con pila | Hora `HH:MM:SS` con marco a ×3 y fecha `DD/MM/AAAA` |
| 2. Mirar 2 minutos | La hora avanza cada segundo sin parpadeo del resto. La fecha no se repinta |
| 3. Pasar la medianoche (ajustar a 23:59:50) | Cambian la hora y la fecha |
| 4. Sin pila o RTC sin ajustar (`valid == false`) | `--:--:--` y `--/--/----` |

Criterio PASS: pasos 1–4 como en la tabla y sin restos de dígitos en ningún cambio.
