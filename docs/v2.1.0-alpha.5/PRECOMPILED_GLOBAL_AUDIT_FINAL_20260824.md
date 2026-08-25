# Auditoria global de compatibilidad de librerias precompiladas JWPLC

Fecha: 2026-08-24 17:36:24

Toolchain nm: `C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc_local\tools\esp-x32\2601\bin\xtensa-esp32-elf-nm.exe`

Archives auditados: 12

Modo bridge GPIO generico: **HABILITADO**

Criterio: se permiten exclusivamente `jwplc_pinMode`, `jwplc_digitalWrite` y `jwplc_digitalRead` como dependencias externas bridge-compatible. El target genérico debe aportar `cores/esp32/jwplc-gpio-compat.c`. Cualquier otro símbolo externo con prefijo `jwplc` es bloqueante.

| Libreria | Archive | Undefined | Externos `jwplc...` | Bridge GPIO | Bloqueantes | Estado |
|---|---|---:|---|---|---|---|
| Adafruit_BusIO | libAdafruit_BusIO.a | 25 | jwplc_digitalWrite, jwplc_pinMode | jwplc_digitalWrite, jwplc_pinMode | - | BRIDGE |
| Adafruit_GFX_Library | libAdafruit_GFX_Library.a | 72 | jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode | jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode | - | BRIDGE |
| Adafruit_ST7735_and_ST7789_Library | libAdafruit_ST7735_and_ST7789_Library.a | 111 | - | - | - | PASS |
| FS | libFS.a | 51 | - | - | - | PASS |
| JW_FRAM | libJW_FRAM.a | 22 | - | - | - | PASS |
| JW_MatrixButtons | libJW_MatrixButtons.a | 14 | jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode | jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode | - | BRIDGE |
| JW_SD | libJW_SD.a | 37 | jwplc_digitalRead, jwplc_pinMode | jwplc_digitalRead, jwplc_pinMode | - | BRIDGE |
| JWPLC_Ethernet_W5x00_Backend | libJWPLC_Ethernet_W5x00_Backend.a | 171 | jwplc_digitalWrite, jwplc_pinMode | jwplc_digitalWrite, jwplc_pinMode | - | BRIDGE |
| JWPLC_ModbusRTU | libJWPLC_ModbusRTU.a | 19 | - | - | - | PASS |
| SD | libSD.a | 56 | jwplc_digitalWrite, jwplc_pinMode | jwplc_digitalWrite, jwplc_pinMode | - | BRIDGE |
| SPI | libSPI.a | 49 | - | - | - | PASS |
| Wire | libWire.a | 31 | - | - | - | PASS |

Resultado global: **PASS BRIDGE-COMPATIBLE**. No se detectaron dependencias con prefijo `jwplc` bloqueantes.

Archives que requieren el bridge GPIO genérico:
- Adafruit_BusIO/libAdafruit_BusIO.a: jwplc_digitalWrite, jwplc_pinMode
- Adafruit_GFX_Library/libAdafruit_GFX_Library.a: jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode
- JW_MatrixButtons/libJW_MatrixButtons.a: jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode
- JW_SD/libJW_SD.a: jwplc_digitalRead, jwplc_pinMode
- JWPLC_Ethernet_W5x00_Backend/libJWPLC_Ethernet_W5x00_Backend.a: jwplc_digitalWrite, jwplc_pinMode
- SD/libSD.a: jwplc_digitalWrite, jwplc_pinMode

Nota: `nm -u` reporta simbolos indefinidos por miembro del archive. Para evitar falsos positivos, esta auditoria descuenta simbolos con prefijo `jwplc` que el mismo archive define y clasifica sólo dependencias externas reales.
