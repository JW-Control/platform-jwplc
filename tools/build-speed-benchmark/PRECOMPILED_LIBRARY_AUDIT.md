# Auditoria global de compatibilidad de librerias precompiladas JWPLC

Fecha: 2026-08-23 00:39:40

Toolchain nm: `C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc_local\tools\esp-x32\2601\bin\xtensa-esp32-elf-nm.exe`

Archives auditados: 7

Criterio bloqueante: cualquier archive `libraries/*/src/esp32/lib*.a` reutilizable por targets con `build.mcu=esp32` no debe conservar dependencias externas `jwplc_*` generadas por los remapeos del core JWPLC.

| Libreria | Archive | Undefined | Dependencias externas `jwplc_*` | Estado |
|---|---|---:|---|---|
| Adafruit_ST7735_and_ST7789_Library | libAdafruit_ST7735_and_ST7789_Library.a | 111 | - | PASS |
| FS | libFS.a | 51 | - | PASS |
| JW_FRAM | libJW_FRAM.a | 22 | - | PASS |
| JW_RTC | libJW_RTC.a | 12 | - | PASS |
| JWPLC_ModbusRTU | libJWPLC_ModbusRTU.a | 19 | - | PASS |
| SPI | libSPI.a | 49 | - | PASS |
| Wire | libWire.a | 31 | - | PASS |

Resultado global: **PASS**. No se detectaron dependencias externas `jwplc_*` en los archives ESP32 presentes.

Nota: `nm -u` reporta simbolos indefinidos por miembro del archive. Para evitar falsos positivos, esta auditoria descuenta simbolos `jwplc_*` que el mismo archive define y bloquea solo dependencias externas reales.
