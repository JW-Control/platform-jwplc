# Alpha5 - Auditoria global de precompilados ESP32

Fecha: 2026-08-23

## Objetivo

Extender la verificacion iniciada con `JW_MatrixButtons` y `JW_SD` a todos los archives `libraries/*/src/esp32/lib*.a` presentes en el package, para detectar dependencias externas `jwplc_*` que puedan quedar ocultas dentro de miembros de un archive y aparecer solo cuando un sketch obliga al linker a extraerlos.

El riesgo existe porque varios targets del package comparten `build.mcu=esp32` pero no usan el mismo core. Un archive generado bajo `JWPLC_BASIC` puede contener llamadas remapeadas como `jwplc_pinMode`, `jwplc_digitalWrite` o `jwplc_digitalRead` y luego ser reutilizado por `ESP32 Board`, cuyo core `esp32` no define esos simbolos.

## Herramienta

```txt
tools/build-speed-benchmark/Audit-JWPLCPrecompiledLibraries.ps1
```

La herramienta recorre automaticamente todos los archives bajo:

```txt
JWPLC/2.1.0/libraries/*/src/esp32/lib*.a
```

Para cada archive compara simbolos `jwplc_*` indefinidos con los definidos por el propio archive y bloquea solo dependencias externas reales.

Toolchain observado:

```txt
C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc_local\tools\esp-x32\2601\bin\xtensa-esp32-elf-nm.exe
```

## Resultado global inicial

Archives encontrados: 11.

| Libreria / archive | Dependencias externas `jwplc_*` | Resultado |
|---|---|---|
| `Adafruit_BusIO/libAdafruit_BusIO.a` | `jwplc_digitalWrite`, `jwplc_pinMode` | FAIL |
| `Adafruit_GFX_Library/libAdafruit_GFX_Library.a` | `jwplc_digitalRead`, `jwplc_digitalWrite`, `jwplc_pinMode` | FAIL |
| `Adafruit_ST7735_and_ST7789_Library/libAdafruit_ST7735_and_ST7789_Library.a` | ninguna | PASS |
| `FS/libFS.a` | ninguna | PASS |
| `JW_FRAM/libJW_FRAM.a` | ninguna | PASS |
| `JW_RTC/libJW_RTC.a` | ninguna | PASS |
| `JWPLC_Display/libJWPLC_Display.a` | `jwplc_digitalWrite` | FAIL |
| `JWPLC_Ethernet_W5x00_Backend/libJWPLC_Ethernet_W5x00_Backend.a` | `jwplc_digitalWrite`, `jwplc_pinMode` | FAIL |
| `JWPLC_ModbusRTU/libJWPLC_ModbusRTU.a` | ninguna | PASS |
| `SPI/libSPI.a` | ninguna | PASS |
| `Wire/libWire.a` | ninguna | PASS |

Resultado:

```txt
7 PASS
4 FAIL
```

## Hallazgo adicional previo: SD nativa

Antes de la auditoria global, el sketch `04_sd_source_compat` confirmo que `JW_SD` ya se compilaba correctamente desde fuente, pero el link fallo al extraer `sd_diskio.cpp.o` desde `SD/src/esp32/libSD.a` por una dependencia a:

```txt
jwplc_digitalWrite
```

Esto demuestra que una compilacion cross-board con sketch vacio no basta para validar un archive: un miembro contaminado puede permanecer sin extraerse hasta que una API concreta lo requiera.

Correccion aplicada a la SD nativa:

```txt
f90ecd7 fix(sd): retirar archive nativo acoplado a jwcontrol
348f4af fix(sd): volver SD nativa a compilacion desde fuente
```

## Correccion de los cuatro FAIL globales

Se aplica la politica conservadora ya usada para `JW_MatrixButtons`, `JW_SD` y `SD`:

1. retirar el archive incompatible;
2. retirar `precompiled=full` de `library.properties`;
3. conservar fuentes, APIs, dependencias y comportamiento funcional;
4. no sustituir llamadas Arduino GPIO por accesos nativos ESP32 sin una decision de arquitectura especifica.

Archives retirados:

```txt
Adafruit_BusIO/src/esp32/libAdafruit_BusIO.a
Adafruit_GFX_Library/src/esp32/libAdafruit_GFX_Library.a
JWPLC_Display/src/esp32/libJWPLC_Display.a
JWPLC_Ethernet_W5x00_Backend/src/esp32/libJWPLC_Ethernet_W5x00_Backend.a
```

Commits:

```txt
cccd226 fix(precompile): retirar archives ESP32 acoplados restantes
e46e202 fix(precompile): volver archives acoplados a compilacion desde fuente
```

No se modifican versiones de librerias, APIs publicas, `platform.txt`, `build.mcu`, `ldflags` de Display ni el autoload normal.

## Archives que permanecen aprobados

La auditoria permite mantener precompilados los siguientes archives porque no presentan dependencias externas `jwplc_*`:

```txt
Adafruit_ST7735_and_ST7789_Library
FS
JW_FRAM
JW_RTC
JWPLC_ModbusRTU
SPI
Wire
```

No se retiran por precaucion general: conservar estos 7 mantiene parte de la mejora de tiempo de compilacion de Alpha4 sin repetir el acoplamiento detectado.

## Scripts historicos de Alpha4

Existen herramientas manuales de benchmark que pueden volver a generar algunos de los archives retirados, entre ellas pilotos P3/P5A/P6B2/P6C2.

Se conservan para reproducibilidad historica de Alpha4, pero **no forman parte del flujo valido de Alpha5** y no deben usarse para restaurar el estado precompilado sin una nueva validacion de neutralidad entre cores.

El gate obligatorio antes de aceptar cualquier archive ESP32 pasa a ser:

```powershell
pwsh -NoProfile -File ./tools/build-speed-benchmark/Audit-JWPLCPrecompiledLibraries.ps1
```

Un resultado con cualquier dependencia externa `jwplc_*` es bloqueante.

## Validacion pendiente despues de la correccion

1. Ejecutar de nuevo la auditoria global. Deben quedar 7 archives y todos deben resultar PASS.
2. Repetir `04_sd_source_compat` con `jwplc_local:esp32:esp32`; `JW_SD` y `SD` deben compilar desde fuente y el link debe completar.
3. Compilar `JWPLC Basic` con autoload normal para confirmar que Display, Ethernet, SD y stack Adafruit siguen integrados al volver a source.
4. Mantener pendiente separado el gate funcional fisico de botonera.

## Decision de arquitectura

No se reintroducen como archive comun `esp32` las librerias retiradas hasta demostrar que el binario es independiente del core o definir una estrategia que separe artefactos por configuracion de core/ABI.

La prioridad de este ajuste es compatibilidad y estabilidad. La perdida de algunos segundos de compilacion se medira despues de cerrar los gates funcionales; no se recuperara rendimiento a costa de volver a introducir archives dependientes de `jwplc_*`.
