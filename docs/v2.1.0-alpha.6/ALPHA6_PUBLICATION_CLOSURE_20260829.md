# v2.1.0-alpha.6 — Cierre de publicación

Fecha: 2026-08-29

## Resultado

`v2.1.0-alpha.6` queda publicada y validada desde el canal dev del package JWPLC.

La validación final no reutilizó la instalación normal de Arduino ni `jwplc_local`: se creó un entorno aislado de Arduino CLI bajo `%TEMP%`, se descargó el índice dev publicado desde `main`, se instaló el package desde GitHub Release, se compiló un sketch mínimo y se realizó una carga física por USB a un JWPLC Basic.

Marcador final:

```text
ALPHA6_PUBLICATION_CLOSURE=PASS
```

## Artefacto publicado

```text
Versión     : 2.1.0-alpha.6
ZIP         : jwplc-esp32-2.1.0-alpha.6.zip
Tamaño      : 24294308 bytes
SHA-256     : cfd81391e80852f26c279ca67885227d6f24e4d3ec6b93d715e072176878c9f1
PreRelease  : v2.1.0-alpha.6
```

El índice público estable permanece reservado para la versión estable y no fue promovido a Alpha6.

## Flujo GitHub validado

- PR técnico corregido Alpha6: `#69`.
- CI del PR: PASS.
- Merge a `release/v2.1.x`: completado.
- Workflow automático de release: PASS.
- GitHub PreRelease: publicada.
- PR automático de índices: `#70`.
- Índice dev actualizado a `2.1.0-alpha.6` en `main`.
- Sincronización histórica `release/v2.1.x -> main`: PR `#71`.
- `release/v2.1.x` queda como ancestro de `main`; la divergencia histórica detectada durante Alpha6 queda cerrada.
- La protección `Require linear history` de `main` fue reactivada después de la sincronización excepcional.

Marcadores:

```text
ALPHA6_BRANCH_DIVERGENCE=RESOLVED
RELEASE_IS_ANCESTOR_OF_MAIN=PASS
MAIN_LINEAR_HISTORY_PROTECTION=RESTORED
```

## Instalación aislada desde el índice dev

Índice utilizado:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Resultado de instalación:

```text
ID          Installed     Latest        Name
jwplc:esp32 2.1.0-alpha.6 2.1.0-alpha.6 JW Control ESP32 Boards
```

Herramientas descargadas por el package aislado:

- `jwplc:esp-x32@2601`
- `jwplc:esptool_py@5.2.0`
- `jwplc:mkspiffs@0.2.3`
- `jwplc:mklittlefs@4.0.2-db0513a`
- `jwplc:esp32-libs@3.3.8`

Resultado:

```text
ALPHA6_ISOLATED_INSTALL=PASS
```

## Compilación aislada

FQBN:

```text
jwplc:esp32:jwplcbasic
```

El sketch mínimo se creó dentro del entorno temporal y no dependió del repositorio local.

Resultado:

```text
COMPILE_EXIT=0
COMPILE_SECONDS=53.027
APP_BIN_EXISTS=True
APP_BIN_BYTES=395120
ALPHA6_ISOLATED_COMPILE=PASS
```

La salida de Arduino CLI confirmó uso de las librerías precompiladas incluidas en el ZIP publicado, entre ellas Display, stack Adafruit, Wire, SPI, FRAM, SD, MatrixButtons y ModbusRTU.

## Carga física aislada

Puerto identificado:

```text
COM14
USB-SERIAL CH340
VID_1A86
PID_7523
```

Dispositivo detectado por `esptool`:

```text
ESP32-D0WD-V3 revision v3.1
Crystal frequency: 40 MHz
```

La carga utilizó el build generado por la instalación aislada, sin recompilar mediante `jwplc_local`.

Resultado:

```text
UPLOAD_EXIT=0
ALPHA6_ISOLATED_PHYSICAL_UPLOAD=PASS
```

`esptool` verificó correctamente los hashes de los bloques escritos y completó el reset final por RTS.

## Decisiones que permanecen vigentes

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO

BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC

CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
```

También permanecen fuera de alcance de Alpha6:

- asumir OpenPLC integrado;
- definir OTA;
- declarar una configuración universal final de FlashFreq;
- publicar un `bootloader.bin` definitivo antes de fijar la configuración final.

## Cierre

Con instalación limpia, compilación aislada y carga física aprobadas, Alpha6 queda cerrada como PreRelease publicada del canal dev.

```text
ALPHA6_TECHNICAL_VALIDATION=PASS
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
ALPHA6_BUILD_SPEED=PASS
ALPHA6_ISOLATED_INSTALL=PASS
ALPHA6_ISOLATED_COMPILE=PASS
ALPHA6_ISOLATED_PHYSICAL_UPLOAD=PASS
ALPHA6_PUBLICATION_CLOSURE=PASS
ALPHA6_STATUS=CLOSED
```
