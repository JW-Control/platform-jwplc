# JWPLC mBlock PoC - Gate 1

## Objetivo

Determinar si mBlock 5.4.3 dispone de una ruta nativa de
compilación y upload compatible con ESP32/JWPLC Basic.

## Entorno

~~~text
mBlock: 5.4.3
branch: experiment/mblock-jwplc-poc
base Gate 1: ac68645e14009aa13feffa28b54cf7193e8fdc00
~~~

## Resultado

~~~text
GATE1_DISCOVERY=PASS

MBLOCK_ARDUINO_CODE_GENERATOR=PASS

MBLOCK_NATIVE_AVR_COMPILER=PASS
MBLOCK_NATIVE_ESP32_COMPILER=NO

MBLOCK_BUNDLED_ARDUINO_CLI=NO
MBLOCK_BUNDLED_XTENSA_TOOLCHAIN=NO

MBLOCK_ESPTOOL_EXECUTABLE=PASS
MBLOCK_CUSTOM_UPLOAD_DRIVER_API=PASS
MBLOCK_CUSTOM_UPLOAD_MIDDLEWARE_API=PASS

JWPLC_NATIVE_MBLOCK_COMPILER_ROUTE=NOT_VIABLE
JWPLC_CUSTOM_MIDDLEWARE_ROUTE=VIABLE_CANDIDATE
~~~

## Hallazgos

El compilador Arduino incluido por mBlock utiliza:

~~~text
platforms/arduino/index.js
    -> avr-ino.js
    -> compiler.js
    -> AVR toolchain
~~~

No se encontraron:

~~~text
xtensa-esp32-elf-g++.exe
xtensa-esp32-elf-gcc.exe
arduino-cli.exe
~~~

Sí se encontró:

~~~text
platforms/esptool/bin/esptool.exe
platforms/esptool/esptoolByExec.js
~~~

mBlock expone además APIs para registrar:

~~~text
registerUploadDriver()
registerUploadMiddleware()
~~~

y dispone de generación de código:

~~~text
ArduinoC
~~~

## Decisión PoC

No adaptar el compilador AVR interno de mBlock.

Siguiente arquitectura candidata:

~~~text
Bloques mBlock
    -> Arduino C
    -> middleware JWPLC
    -> Arduino CLI externo
    -> jwplc:esp32:jwplcbasic
    -> compile/upload
    -> JWPLC Basic
~~~

Para el primer PoC, Arduino CLI realizará tanto compilación
como upload.

No se adopta todavía el uploader sptoolByExec de mBlock
como uploader final JWPLC.

Motivo:

- trabaja con un único firmware binario;
- usa un único startIndex;
- JWPLC no debe asumir app-only como upload por defecto;
- bootloader/configuración final continúan sujetos a la
  configuración validada del package.

## Evidencia

~~~text
RAW_LOG=GATE1_MBLOCK_ESP32_ROUTING.log
RAW_LOG_BYTES=4859386
RAW_LOG_SHA256=81F24BF549CF82F307248DA88C43C81C58C380A40A9ABBF7E4A45F76B69A9F5E
~~~

## Próximo gate

~~~text
GATE2_CUSTOM_MIDDLEWARE_PROCESS_EXECUTION
~~~

Objetivo:

Comprobar si una extensión mBlock puede ejecutar un proceso
externo controlado desde un Upload Middleware.

Primera prueba objetivo:

~~~text
arduino-cli version
~~~

sin compilar ni modificar hardware.
