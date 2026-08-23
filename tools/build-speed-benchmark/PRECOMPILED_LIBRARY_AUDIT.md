# Auditoria de compatibilidad de librerias precompiladas JWPLC

Fecha: 2026-08-23 00:00:07

Toolchain nm: `C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc_local\tools\esp-x32\2601\bin\xtensa-esp32-elf-nm.exe`

Criterio bloqueante: un archive `src/esp32/lib*.a` no debe depender de simbolos internos `jwplc_*` si puede ser reutilizado por targets que comparten `build.mcu=esp32` pero usan cores distintos.

| Libreria | Archive | Undefined | Simbolos `jwplc_*` | Estado |
|---|---|---:|---|---|
| JW_RTC | presente | 12 | - | PASS |
| JW_FRAM | presente | 22 | - | PASS |
| JW_SD | presente | 37 | jwplc_digitalRead, jwplc_pinMode | FAIL |
| JWPLC_ModbusRTU | presente | 19 | - | PASS |

Resultado global: **FAIL / REVISAR**.

- JW_SD: jwplc_digitalRead, jwplc_pinMode

Nota: otros simbolos indefinidos de Arduino/ESP-IDF son normales en una libreria estatica y se resuelven durante el link final. Esta auditoria se concentra en el acoplamiento interno `jwplc_*` que causo la regresion de `JW_MatrixButtons`.
