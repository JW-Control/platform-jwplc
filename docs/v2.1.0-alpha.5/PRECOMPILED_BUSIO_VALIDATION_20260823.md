# Alpha5 - validación de Adafruit_BusIO precompilada

Fecha: 2026-08-23.

## Objetivo

Validar la recuperación del archive compartido `Adafruit_BusIO` usando el bridge GPIO genérico de Alpha5, sin modificar la API ni el código fuente de la librería.

El archive histórico recupera 4 translation units y forma parte del stack de display usado por JWPLC Basic.

## Gate estático bridge-compatible - PASS

Auditoría ejecutada con:

```powershell
pwsh -NoProfile -File ./tools/build-speed-benchmark/Audit-JWPLCPrecompiledLibraries.ps1 -AllowGenericGpioBridge
```

Resultado relevante:

```txt
Archives encontrados: 10
[BRIDGE] Adafruit_BusIO/libAdafruit_BusIO.a: jwplc_digitalWrite, jwplc_pinMode
[BRIDGE] Adafruit_GFX_Library/libAdafruit_GFX_Library.a: jwplc_digitalRead, jwplc_digitalWrite, jwplc_pinMode
[BRIDGE] JWPLC_Ethernet_W5x00_Backend/libJWPLC_Ethernet_W5x00_Backend.a: jwplc_digitalWrite, jwplc_pinMode
EXIT CODE: 0
```

No apareció ningún símbolo `jwplc_*` adicional fuera de la lista permitida por el bridge.

## Gate ESP32 Board genérico - PASS

FQBN:

```txt
jwplc_local:esp32:esp32
```

El sketch `tools/build-speed-benchmark/sketches/11_busio_bridge_link` fuerza la copia vendorizada mediante `JWPLC_Bundled_Adafruit_BusIO.h` y toma la dirección de `Adafruit_SPIDevice::begin()` para obligar al linker a extraer código real del archive.

Evidencia:

- se resolvió `Adafruit BusIO@1.17.4` desde el package JWPLC;
- Arduino Builder declaró la librería precompilada;
- el linker incorporó `-lAdafruit_BusIO` desde `src/esp32`;
- el core genérico incluyó `jwplc-gpio-compat.c`;
- no hubo referencias `jwplc_*` sin resolver;
- build final: PASS.

## Gate JWPLC Basic - PASS

FQBN:

```txt
jwplc_local:esp32:jwplcbasic
```

Resultado:

```txt
Library Adafruit BusIO has been declared precompiled:
Using precompiled library in ...\Adafruit_BusIO\src\esp32
Sketch uses 394309 bytes of program storage space.
Global variables use 27612 bytes of dynamic memory.
```

No se detectaron errores de enlace ni `undefined reference`.

## Gate JWPLC Basic Core - PASS

FQBN:

```txt
jwplc_local:esp32:jwplcbasiccore
```

Resultado:

```txt
Library Adafruit BusIO has been declared precompiled:
Using precompiled library in ...\Adafruit_BusIO\src\esp32
Sketch uses 339712 bytes of program storage space.
Global variables use 26780 bytes of dynamic memory.
```

No se detectaron errores de enlace ni `undefined reference`.

## Gate físico de display - PASS

Se reutilizó el sketch:

```txt
tools/build-speed-benchmark/sketches/10_gfx_physical
```

La compilación/subida sobre `jwplc_local:esp32:jwplcbasic` usó simultáneamente:

- `Adafruit ST7735 and ST7789 Library` precompilada;
- `Adafruit GFX Library` precompilada;
- `Adafruit BusIO` precompilada;
- `Wire` precompilada;
- `SPI` precompilada.

El linker incorporó explícitamente `-lAdafruit_BusIO` desde el package JWPLC. La subida a `COM4` terminó correctamente y el contenido fue verificado por esptool.

Salida serial observada:

```txt
JWPLC Alpha5 - gate fisico Adafruit_GFX precompilada
JWPLC_Display inicializado
[GFX] Display ready: YES
[GFX] USER screen dibujada
[GFX] TFT size: 320x170
```

Validación física: la TFT mostró correctamente la misma pantalla de prueba aprobada previamente.

## Conclusión

`Adafruit_BusIO` queda **CERRADO / ADOPTADO** para Alpha5 bajo el modelo bridge-compatible.

Gates aprobados:

- auditoría bridge-compatible: PASS;
- ESP32 Board genérico: PASS;
- JWPLC Basic: PASS;
- JWPLC Basic Core: PASS;
- display físico sobre SPI real: PASS.

La recuperación acumulada de los pilotos adoptados pasa a 16 de los 23 translation units que habían vuelto temporalmente a compilación desde fuente durante la corrección de compatibilidad.

Pendientes por recuperar antes del benchmark final:

- `SD`: 3 TUs;
- `JWPLC_Display`: 2 TUs;
- `JW_MatrixButtons`: 1 TU;
- `JW_SD`: 1 TU.
