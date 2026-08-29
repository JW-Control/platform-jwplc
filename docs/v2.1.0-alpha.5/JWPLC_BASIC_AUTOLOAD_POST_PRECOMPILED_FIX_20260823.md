# Alpha5 - Validacion JWPLC Basic autoload post-correccion de precompilados

Fecha: 2026-08-23

## Objetivo

Validar que `JWPLC Basic` conserva la compilacion normal con autoload completo despues de retirar los archives ESP32 incompatibles detectados por la auditoria global de simbolos.

La prueba se ejecuta despues de volver a compilacion desde fuente:

- `JW_MatrixButtons`;
- `JW_SD`;
- `SD`;
- `Adafruit_BusIO`;
- `Adafruit_GFX_Library`;
- `JWPLC_Display`;
- `JWPLC_Ethernet_W5x00_Backend`.

No se quitan perifericos del autoload y no se modifica la API publica.

## Configuracion observada

```txt
Sketch: tools/build-speed-benchmark/sketches/01_empty/01_empty.ino
FQBN: jwplc_local:esp32:jwplcbasic
Board: jwplcbasic
Core: jwcontrol_p2
Variant: jwplcbasic
Package local: 2.1.0-dev
JWPLC_BASIC: definido
JWPLC_HAS_RTC=1
JWPLC_HAS_FRAM=1
JWPLC_HAS_SD=1
JWPLC_HAS_ETHERNET=1
```

La configuracion observada durante esta prueba incluye:

```txt
CPU: 240 MHz
Flash size: 4 MB
Flash frequency: 40 MHz
Flash mode: DIO
Partition: jwplc_max_app_4mb
```

Esta evidencia describe el estado actual de la rama y no fija por si sola una configuracion universal futura.

## Autoload detectado

El discovery del sketch vacio incorpora las capas esperadas del JWPLC Basic:

- `JWPLC_Display`;
- `JWPLC_GlobalPeripherals`;
- `JW_RTC`;
- `JW_FRAM`;
- `JW_SD` + `SD`;
- `JW_MatrixButtons`;
- `JWPLC_Ethernet`;
- `JWPLC_Ethernet_W5x00_Backend`;
- `JWPLC_RS485`;
- `JWPLC_ModbusRTU`;
- soporte TCA/I/O por el core JWPLC.

Por tanto, la correccion de compatibilidad no se basa en retirar perifericos del autoload normal.

## Dependencias bundled seleccionadas

Arduino IDE selecciono las copias incluidas en `platform-jwplc/JWPLC/2.1.0/libraries` para las dependencias relevantes:

```txt
JWPLC_Display 1.0.1
JWPLC_GlobalPeripherals 1.0.0
Adafruit ST7735 and ST7789 Library 1.11.0
Adafruit GFX Library 1.12.4
Adafruit BusIO 1.17.4
Wire 3.3.8
SPI 3.3.8
JW_RTC 1.0.2
JW_FRAM 1.0.3
JW_SD 1.0.2
SD 3.3.8
FS 3.3.8
JW_MatrixButtons 1.0.5
JWPLC_Ethernet 1.0.0
JWPLC Ethernet W5x00 Backend 2.0.2
JWPLC_RS485 1.0.1
JWPLC_ModbusRTU 1.0.0
```

En particular, `SD.h` resolvio a:

```txt
C:\Users\jeykc\Documentos\GitHub\platform-jwplc\JWPLC\2.1.0\libraries\SD
```

y no a las copias alternativas instaladas en Arduino15 o en el sketchbook.

Esto cierra el matiz observado en la prueba JWPLC Laundry, donde una dependencia Adafruit generica podia resolverse desde el sketchbook del usuario.

## Librerias que vuelven a fuente

La salida verbose confirma compilacion normal para las librerias retiradas del esquema precompilado comun, incluyendo:

```txt
JWPLC_Display
Adafruit GFX Library
Adafruit BusIO
JW_SD
SD
JW_MatrixButtons
JWPLC Ethernet W5x00 Backend
```

No se reutilizan archives `src/esp32/lib*.a` para esas librerias.

## Archives que permanecen aprobados

La compilacion utiliza exactamente los siete archives que superaron la auditoria global de compatibilidad:

```txt
Adafruit_ST7735_and_ST7789_Library
Wire
SPI
JW_RTC
JW_FRAM
FS
JWPLC_ModbusRTU
```

Este conjunto coincide con el resultado global 7/7 PASS de `Audit-JWPLCPrecompiledLibraries.ps1`.

## Resultado de enlace

Resultado:

```txt
PASS
```

El link completo genero correctamente:

```txt
01_empty.ino.elf
01_empty.ino.bin
01_empty.ino.merged.bin
```

No se reportaron referencias `jwplc_*` no resueltas.

## Memoria reportada

```txt
Sketch: 405553 bytes (9%) de 4063232 bytes
RAM global: 27908 bytes (8%) de 327680 bytes
RAM libre estimada para variables locales: 299772 bytes
```

Estos valores constituyen una nueva referencia post-correccion. No deben compararse directamente con timings o tamanos historicos Alpha4 como si el conjunto de precompilados fuera identico.

## Conclusion

Se valida que `JWPLC Basic` conserva compilacion y autoload completo despues de retirar los precompilados incompatibles de MatrixButtons, SD, Display, Ethernet backend y capas Adafruit afectadas.

La estrategia de correccion mantiene:

- compatibilidad Arduino IDE;
- perifericos integrados en el autoload;
- APIs existentes;
- los archives que demostraron ser neutrales al core;
- el core precompilado especifico de `JWPLC Basic` mediante `jwcontrol_p2`.

## Pendientes antes de cerrar el ajuste de compatibilidad

1. Recompilar `JWPLC Basic Core` con el estado post-auditoria global, porque su PASS anterior fue previo al retiro de `JWPLC_Display`, `Adafruit_GFX_Library` y `Adafruit_BusIO` precompilados.
2. Validar comportamiento fisico de la botonera en un JWPLC Basic real.
3. Revalidar funcionalmente microSD en JWPLC Basic despues de volver `JW_SD` y `SD` a compilacion desde fuente.
4. Ejecutar un benchmark de tiempos nuevo antes de reutilizar cifras historicas Alpha4.
