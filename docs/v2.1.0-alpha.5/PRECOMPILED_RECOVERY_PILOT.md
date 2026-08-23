# Alpha5 - piloto de recuperación de precompilados

Este documento registra la recuperación incremental de archives compartidos mediante el bridge GPIO genérico validado en Alpha5.

## Estrategia

1. mantener la auditoría estricta como modo por defecto;
2. habilitar explícitamente `-AllowGenericGpioBridge` sólo para pilotos bridge-compatible;
3. permitir únicamente `jwplc_pinMode`, `jwplc_digitalWrite` y `jwplc_digitalRead` como dependencias externas bridge-compatible;
4. reactivar una librería por vez;
5. validar `ESP32 Board`, `JWPLC Basic` y `JWPLC Basic Core` antes de avanzar.

## Piloto 1 - JWPLC_Ethernet_W5x00_Backend

Motivo de prioridad: el backend representa 8 translation units recuperables, por lo que ofrece la mayor ganancia potencial individual.

Se reutiliza el mismo archive binario histórico de Alpha4 identificado por Git blob:

```txt
006fa25c31ba248d806feca986699471fa51c6ca
```

No se modifica el código fuente del backend ni su API pública.

### Gate estático bridge-compatible - PASS

Fecha: 2026-08-23.

Comando:

```powershell
pwsh -NoProfile -File ./tools/build-speed-benchmark/Audit-JWPLCPrecompiledLibraries.ps1 -AllowGenericGpioBridge
```

Resultado:

```txt
Modo bridge GPIO generico: HABILITADO
Archives encontrados: 8
[PASS] Adafruit_ST7735_and_ST7789_Library/libAdafruit_ST7735_and_ST7789_Library.a
[PASS] FS/libFS.a
[PASS] JW_FRAM/libJW_FRAM.a
[PASS] JW_RTC/libJW_RTC.a
[BRIDGE] JWPLC_Ethernet_W5x00_Backend/libJWPLC_Ethernet_W5x00_Backend.a: jwplc_digitalWrite, jwplc_pinMode
[PASS] JWPLC_ModbusRTU/libJWPLC_ModbusRTU.a
[PASS] SPI/libSPI.a
[PASS] Wire/libWire.a
EXIT CODE: 0
```

Interpretación:

- los siete archives previamente neutrales siguen sin dependencias externas `jwplc_*`;
- el backend Ethernet requiere exclusivamente `jwplc_digitalWrite` y `jwplc_pinMode`, ambos cubiertos por el bridge GPIO genérico;
- no apareció ningún otro símbolo `jwplc_*` bloqueante;
- el gate estático del piloto 1 queda aprobado.

### Gate de enlace ESP32 Board - PASS

Fecha: 2026-08-23.

FQBN:

```txt
jwplc_local:esp32:esp32
```

Sketch:

```txt
tools/build-speed-benchmark/sketches/08_ethernet_bridge_link/08_ethernet_bridge_link.ino
```

Evidencia del log:

```txt
Skipping dependencies detection for precompiled library JWPLC Ethernet W5x00 Backend
Library JWPLC Ethernet W5x00 Backend has been declared precompiled:
Using precompiled library in ...\JWPLC_Ethernet_W5x00_Backend\src\esp32
...\cores\esp32\jwplc-gpio-compat.c
Sketch uses 271332 bytes (20%) of program storage space.
Global variables use 22172 bytes (6%) of dynamic memory, leaving 305508 bytes for local variables.
EXIT CODE: 0
```

Interpretación:

- Arduino Builder seleccionó el backend vendorizado del package JWPLC;
- el backend se consumió como archive precompilado y no se recompilaron sus 8 translation units;
- el core genérico compiló `jwplc-gpio-compat.c` en esta corrida, por lo que el gate no dependió de un `core.a` previo que ocultara el bridge;
- el enlace terminó sin referencias `jwplc_*` sin resolver.

Estado: auditoría PASS + ESP32 Board PASS; pendientes JWPLC Basic, JWPLC Basic Core y validación funcional Ethernet antes de adoptar definitivamente el archive.
