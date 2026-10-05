# Alpha12 — P8 Examples/API Review

Fecha: 2026-10-04

## Resultado

```text
P8_MODBUS_TCP_SERVER_EXAMPLE=PASS
P8_MODBUS_TCP_CLIENT_EXAMPLE=PASS
P8_MODBUS_RTU_SLAVE_EXAMPLE=PASS
P8_MODBUS_RTU_MASTER_READ_EXAMPLE=PASS
P8_MODBUS_RTU_MASTER_WRITE_EXAMPLE=PASS
P8_ETHERNET_DHCP_EXAMPLE=PASS
P8_ETHERNET_STATIC_EXAMPLE=PASS
P8_ETHERNET_DIAGNOSTICS_EXAMPLE=PASS
P8_RS485_SEND_EXAMPLE=PASS
P8_RS485_ECHO_EXAMPLE=PASS
P8_RS485_STATUS_EXAMPLE=PASS
P8_EXPERIMENTAL_API_DEPENDENCY=NO
P8_EXAMPLES_API_REVIEW=PASS
```

## Evidencia agrupada

Runner:

```text
tools/alpha12/p8_compile_remaining_examples.ps1
HEAD=66a7f6e65297fb1207d177c096cdbb1a44c439e2
DIRTY_COUNT=0
PASS_COUNT=5
FAIL_COUNT=0
P8_REMAINING_EXAMPLES_COMPILE=PASS
```

Los cinco casos agrupados fueron:

```text
ETH_STATIC=PASS EXIT=0
ETH_DIAGNOSTICS=PASS EXIT=0
RS485_SEND=PASS EXIT=0
RS485_ECHO=PASS EXIT=0
RS485_STATUS=PASS EXIT=0
```

Los ejemplos TCP, RTU y DHCP fueron compilados individualmente antes del runner y terminaron igualmente con exit 0.

## APIs experimentales

La búsqueda estática en los directorios de ejemplos recomendados no encontró activaciones `EXPERIMENTAL` ni `#define JWPLC_*` requeridos para compilar. Los ejemplos pasan con la configuración normal de Alpha12.

## Punto todavía abierto antes del freeze

La compatibilidad global no se marca aún como PASS por el cambio de tipo de retorno de Display:

```text
Alpha11: Adafruit_ST7789& JWPLC_Display.tft()/display()
Alpha12: JWPLC_TFTClass& JWPLC_Display.tft()/display()
```

El patrón recomendado `auto &tft = JWPLC_Display.tft();` sigue siendo compatible, pero sketches externos que declaren explícitamente `Adafruit_ST7789&` no lo son.

No se realiza cambio adicional de source en este cierre documental.
