# JWPLC v2.1.0-alpha.4 — contrato final de autoload

## Objetivo

Verificar que las optimizaciones acumuladas hasta P8 no hayan roto el contrato de autoload normal de `JWPLC Basic`.

El sketch de prueba no incluye manualmente headers JWPLC y depende de las APIs expuestas automáticamente por el package.

Sketch:

```txt
tools/build-speed-benchmark/sketches/03_autoload_contract/03_autoload_contract.ino
```

FQBN:

```txt
jwplc_local:esp32:jwplcbasic
```

## Resultado

```txt
Exit = 0
ALPHA4_AUTOLOAD_CONTRACT=PASS
```

Tamaño observado:

```txt
Sketch uses 394709 bytes (12%)
Global variables use 27612 bytes (8%)
```

El tiempo observado del gate fue **53.582 s**. Este valor se conserva únicamente como referencia y no se considera benchmark formal.

## Contrato conservado

El build final resolvió correctamente el stack esperado por el autoload:

- `JWPLC_Display`;
- `JWPLC_GlobalPeripherals`;
- `Adafruit_ST7735_and_ST7789_Library`;
- `Adafruit_GFX_Library`;
- `Adafruit_BusIO`;
- `Wire`;
- `SPI`;
- `JW_RTC`;
- `JW_FRAM`;
- `JW_SD`;
- `SD`;
- `FS`;
- `JW_MatrixButtons`;
- `JWPLC_Ethernet`;
- `JWPLC_Ethernet_W5x00_Backend`;
- `JWPLC_RS485`;
- `JWPLC_ModbusRTU`.

El sketch también pudo usar las APIs públicas de RTC, FRAM, SD, botonera, Display y los símbolos de I/O sin incluir manualmente headers JWPLC.

## Arquitectura P2 y archives

El build utilizó `jwcontrol_p2` como core seleccionado y enlazó además el archive completo:

```txt
precompiled/core/JWPLCBASIC/core.a
```

El log confirmó selección de los archives precompilados correspondientes a las etapas P1–P8. `JWPLC_GlobalPeripherals`, `JWPLC_Ethernet` y `JWPLC_RS485` continuaron compilándose desde fuente en este estado.

También se detectó una instalación adicional de `SD` en el sketchbook del usuario, pero Arduino seleccionó correctamente la versión `SD 3.3.8` incluida dentro del package JWPLC.

## Arduino IDE — experiencia real en PC principal

Después del commit P8 se observaron aproximadamente:

| Caso | Tiempo |
|---|---:|
| Primera compilación | ~57 s |
| Segunda compilación / incremental | ~19 s |

Estas mediciones son observacionales de Arduino IDE y se mantienen separadas de los benchmarks cold controlados.

## Alcance del PASS

Este gate demuestra que el contrato de autoload permanece funcional a nivel de compilación y enlace después de P8.

No sustituye el gate físico integral de Display, Ethernet, SD, FRAM, RTC, botonera, RS-485, Modbus RTU y TCA/I/O.

## Decisión

**Contrato final de autoload Alpha4: CERRADO / PASS.**

No se retiró ningún periférico del autoload normal para obtener las mejoras de tiempo de compilación.
