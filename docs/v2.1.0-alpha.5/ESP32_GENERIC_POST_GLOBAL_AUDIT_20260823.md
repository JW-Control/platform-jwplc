# Alpha5 - Validacion ESP32 generico posterior a auditoria global

Fecha: 2026-08-23

Rama:

```txt
v2.1.0-alpha.5/feature/esp32-precompiled-compatibility
```

## Objetivo

Comprobar que el firmware JWPLC Laundry vuelve a compilar y enlazar con el target generico del package despues de retirar los archives ESP32 que la auditoria global identifico como acoplados a simbolos internos `jwplc_*`.

Target observado:

```txt
FQBN: jwplc_local:esp32:esp32
Board: esp32
Core: esp32
Variant: esp32
Package local: 2.1.0-dev
```

## Estado estructural previo

La auditoria global posterior a los ajustes encontro 7 archives `libraries/*/src/esp32/lib*.a` y todos pasaron el criterio bloqueante de no depender de simbolos externos `jwplc_*`:

- `Adafruit_ST7735_and_ST7789_Library`;
- `FS`;
- `JW_FRAM`;
- `JW_RTC`;
- `JWPLC_ModbusRTU`;
- `SPI`;
- `Wire`.

Resultado de auditoria:

```txt
PASS 7/7
```

## Compilacion JWPLC Laundry

Se recompilo el firmware `JWPLC_Laundry_1v3v5` con `ESP32 Board`.

El log confirma que `JW_MatrixButtons` se compila desde fuente:

```txt
Compilando librería "JW_MatrixButtons"
...\libraries\JW_MatrixButtons\src\JW_MatrixButtons.cpp
```

Durante el link se utiliza:

```txt
libraries\JW_MatrixButtons\JW_MatrixButtons.cpp.o
```

Los archives aprobados que aparecen en esta compilacion siguen utilizandose como precompilados, entre ellos:

```txt
SPI
JW_FRAM
Wire
```

El enlace final se completa correctamente, se genera el ELF, el BIN de aplicacion y el merged BIN de 4 MB.

Resultado:

```txt
PASS
```

Memoria reportada:

```txt
Sketch: 492255 bytes (37%) de 1310720 bytes
RAM global: 34728 bytes (10%) de 327680 bytes
RAM libre estimada para variables locales: 292952 bytes
```

No aparecen errores `undefined reference` a `jwplc_pinMode`, `jwplc_digitalRead` o `jwplc_digitalWrite`.

## Matiz de resolucion de librerias

Esta compilacion no valida la copia vendorizada de `Adafruit_BusIO` dentro del package, porque Arduino resolvio `Adafruit_SPIDevice.h` desde el sketchbook del usuario:

```txt
Usado: C:\Users\jeykc\Documentos\Programacion\Arduino\libraries\Adafruit_BusIO
No utilizado: C:\Users\jeykc\Documentos\GitHub\platform-jwplc\JWPLC\2.1.0\libraries\Adafruit_BusIO
```

Por tanto, este PASS demuestra la compatibilidad del firmware Laundry y del target generico tras la limpieza global, pero no se usa para cerrar la validacion de la copia vendorizada de BusIO.

La prueba tampoco cierra `JW_SD`/`SD`, porque este firmware no fuerza la ruta especifica del sketch `04_sd_source_compat`.

## Conclusion

El target generico `jwplc_local:esp32:esp32` vuelve a compilar y enlazar correctamente un firmware real representativo despues de retirar los archives incompatibles.

Quedan como gates independientes:

- compilacion de `04_sd_source_compat` con `ESP32 Board`;
- compilacion/autoload de `JWPLC Basic` despues de la limpieza global;
- validacion funcional fisica de botonera en `JWPLC Basic`;
- validacion explicita de las copias vendorizadas que ahora vuelven a source cuando corresponda.
