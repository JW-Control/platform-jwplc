# Alpha5 - Revalidacion JWPLC Basic Core tras correccion global de precompilados

Fecha: 2026-08-23

## Objetivo

Revalidar el target `JWPLC Basic Core` despues de retirar los archives ESP32 compartidos que mostraban acoplamiento a simbolos internos `jwplc_*`.

Esta prueba complementa la validacion anterior de Basic Core, que se habia ejecutado antes de retirar los precompilados incompatibles de Display/GFX/BusIO/SD/Ethernet backend.

## Configuracion observada

```txt
Sketch: tools/build-speed-benchmark/sketches/01_empty/01_empty.ino
FQBN: jwplc_local:esp32:jwplcbasiccore
Board: jwplcbasiccore
Core: jwcontrol
Variant: jwplcbasic
Package local: 2.1.0-dev
```

Flags de capacidad observados:

```txt
JWPLC_BASIC=1
JWPLC_HAS_RTC=1
JWPLC_HAS_FRAM=0
JWPLC_HAS_SD=0
JWPLC_HAS_ETHERNET=0
JWPLC_FRAM_SIZE_BYTES=0
```

## Resultado

```txt
PASS
```

La compilacion completo el enlace y genero correctamente:

- ELF;
- BIN de aplicacion;
- tabla de particiones;
- merged BIN de 4 MB.

No se observaron errores `undefined reference` a simbolos internos `jwplc_*`.

## Librerias que entran desde fuente

El link final incluye objetos fuente de las librerias previamente retiradas del esquema de archive compartido, entre ellas:

```txt
JWPLC_Display
Adafruit_GFX_Library
Adafruit_BusIO
JW_SD
SD
JW_MatrixButtons
JWPLC_Ethernet_W5x00_Backend
```

Tambien entran desde fuente `JWPLC_GlobalPeripherals`, `JWPLC_Ethernet` y `JWPLC_RS485`.

## Archives aprobados que permanecen

El link conserva los siete archives aprobados por la auditoria global:

```txt
Adafruit_ST7735_and_ST7789_Library
Wire
SPI
JW_RTC
JW_FRAM
FS
JWPLC_ModbusRTU
```

## Resolucion de librerias

Arduino IDE resolvio las copias vendorizadas dentro del repositorio/package para las dependencias relevantes, incluyendo:

```txt
JWPLC/2.1.0/libraries/JWPLC_Display
JWPLC/2.1.0/libraries/Adafruit_GFX_Library
JWPLC/2.1.0/libraries/Adafruit_BusIO
JWPLC/2.1.0/libraries/JW_SD
JWPLC/2.1.0/libraries/SD
JWPLC/2.1.0/libraries/JW_MatrixButtons
JWPLC/2.1.0/libraries/JWPLC_Ethernet_W5x00_Backend
```

Para `SD.h`, Arduino detecto otras instalaciones pero selecciono la copia vendorizada 3.3.8 del package.

## Memoria reportada

```txt
Sketch: 354596 bytes (11%) de 3145728 bytes
RAM global: 27108 bytes (8%) de 327680 bytes
RAM libre estimada para variables locales: 300572 bytes
```

## Observacion no bloqueante

Aunque `JWPLC Basic Core` declara:

```txt
JWPLC_HAS_FRAM=0
JWPLC_HAS_SD=0
JWPLC_HAS_ETHERNET=0
```

el grafo de dependencias del autoload/Display todavia arrastra varias de esas librerias al proceso de compilacion y link.

Esto no invalida la prueba funcional de compilacion: las capacidades siguen deshabilitadas por flags. Sin embargo, representa una oportunidad posterior de optimizacion del grafo de build si puede hacerse sin romper el contrato de autoload ni las APIs actuales.

No se modifica este comportamiento dentro de esta correccion de compatibilidad.

## Conclusion

`JWPLC Basic Core` conserva compatibilidad de compilacion tras la correccion global de precompilados. Con este PASS se cierra el gate de regresion de compilacion para los tres targets usados en la matriz de compatibilidad; quedan pendientes las validaciones fisicas del JWPLC Basic, en especial botonera y microSD tras el fallback a fuente.
