# v2.1.0-alpha.12 — Comunicaciones, runtime y autocontención

## Estado

PreRelease publicada y validada el 2026-10-05.

```text
TAG=v2.1.0-alpha.12
PUBLISHED_PACKAGE_SOURCE_SHA=1011f2588fe02bdc67b14bef8c33ad3624426cb6
ZIP=jwplc-esp32-2.1.0-alpha.12.zip
SIZE=24383662
SHA256=412079a9e01cb0eaccdf6ec530b04183db1c245eb043e846fe9f0f7e2d5eb1b5
PACKAGE_ROOT=2.1.0/
GITHUB_PRERELEASE=PASS
INDEX_DEV_PR=#101
INDEX_DEV_PR_STATUS=MERGED
ALPHA12_STATUS=CLOSED_PUBLISHED
```

## Modbus TCP

Nueva librería `JWPLC_ModbusTCP` con Server y Client cooperativos.

Function Codes soportadas:

```text
FC01 Read Coils
FC02 Read Discrete Inputs
FC03 Read Holding Registers
FC04 Read Input Registers
FC05 Write Single Coil
FC06 Write Single Register
FC15 Write Multiple Coils
FC16 Write Multiple Registers
```

```text
MODBUS_TCP_SERVER=PASS
MODBUS_TCP_CLIENT=PASS
MODBUS_TCP_ALL_8_FC=PASS
```

## Modbus RTU y RS-485

Se consolida el motor Master seleccionable:

```cpp
JWPLC_ModbusRTU.motor(ASYNC); // recomendado
JWPLC_ModbusRTU.motor(SYNC);  // compatibilidad/commissioning
```

Se mantiene TX queued aditivo en RS-485 y control de frame gap en microsegundos.

## Ethernet / W5500

Perfil cualificado:

```text
W5500_SPI=26 MHz
TCP_RX_POLICY=POLLING_C0
FIFO_REUSE=ON
DLEN_CACHE=ON
COPY_OUT_64=ON
```

Se endurecieron DHCP, recuperación de link, TCP cooperativo, UDP y coexistencia
sobre SPI compartido.

## Display y TFT

`JWPLC_Display` utiliza el backend propio `JWPLC_TFT`.

Patrón recomendado:

```cpp
auto &tft = JWPLC_Display.tft();
```

La compatibilidad de uso normal se conserva. Se acepta antes de adopción externa
el cambio de tipo explícito `Adafruit_ST7789&` a `JWPLC_TFTClass&`.

Los ejemplos distribuidos quedaron migrados al backend soportado. El CI final
corrigió además residuos legacy en FlappyBird y ejemplos Ethernet:

```text
DISPLAY_FLAPPYBIRD_COMPAT=PASS
ACTIVE_ST77XX_RESIDUE_COUNT=0
CI_JWPLC_PACKAGE_SMOKE_RUN_944=PASS
```

## Package autocontenido

Se retiraron del package activo:

```text
Adafruit_GFX_Library
Adafruit_ST7735_and_ST7789_Library
```

`Adafruit_BusIO` permanece integrada porque `JW_FRAM` la requiere.

```text
STRICT_AUTOCONTAINMENT_CLI=PASS
STRICT_AUTOCONTAINMENT_ARDUINO_IDE=PASS
PUBLISHED_BUNDLED_BUSIO_SELECTED=PASS
EXTERNAL_BUSIO_REQUIRED=NO
```

## Precompilación

`JW_FRAM` fue regenerado después de la corrección de autocontención:

```text
JW_FRAM_SIZE=126440
JW_FRAM_SHA256=b8734763bfa1287167feda72a5c9b30df97340d41ca0c3fd000321e218632ceb
```

Core publicado:

```text
CORE_SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Auditoría global y enlace release-like:

```text
P7A_POST_FREEZE_GLOBAL_AUDIT=PASS
P7B_RELEASE_LIKE_ACTIVATION=PASS
PRECOMPILED_ARCHIVE_IDENTITIES=PASS
PRECOMPILED_POLICY_ACTIVE=PASS
NORMAL_AUTOLOAD_COMPLETE=PASS
PRECOMPILED_FREEZE=PASS
```

## Build speed final

Matriz final: 12/12 fases PASS, `Jobs=0`, sin uploads.

```text
JWPLC Basic cold compiler invocations:      20 -> 16
JWPLC Basic Core cold compiler invocations: 83 -> 79

COMBINED_COLD_AVG_DELTA=-7.94%
COMBINED_WARM_AVG_DELTA=-2.53%
BUILD_STRUCTURE_IMPROVED=YES
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

## Validación final local

```text
FINAL_ARDUINO_CLI_GATE=PASS
PASS_COUNT=6
FAIL_COUNT=0
WARNINGS=0
ERRORS=0
FINAL_REPO_HYGIENE=PASS

FINAL_ARDUINO_IDE_GATE=PASS
ARDUINO_IDE=2.3.4
BOARD=JWPLC Basic
BUNDLED_ADAFRUIT_BUSIO_SELECTED=YES
```

Verify final:

```text
FLASH=431957 / 4063232 bytes (10%)
RAM=29396 bytes (8%)
RAM_FREE=298284 bytes
```

## Validación del package publicado

Índice:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Entorno aislado:

```text
%TEMP%\jwplc-alpha12-published-gate
FQBN=jwplc:esp32:jwplcbasic
VERSION=2.1.0-alpha.12
```

Resultado:

```text
ALPHA12_PUBLISHED_INDEX=PASS
ALPHA12_PUBLISHED_INSTALL=PASS
ALPHA12_PUBLISHED_AUTOCONTAINMENT=PASS
ALPHA12_PUBLISHED_ARCHIVE_PARITY=PASS
ALPHA12_PUBLISHED_CI_FIXES_PRESENT=PASS
ALPHA12_PUBLISHED_COMPILE=PASS
JWPLC_LOCAL_SELECTED=False
```

Upload físico mínimo desde el package publicado:

```text
PORT=COM4
COMPILE_UPLOAD_EXIT=0
PUBLISHED_PLATFORM_SELECTED=True
JWPLC_LOCAL_SELECTED=False
ALPHA12_PUBLISHED_UPLOAD=PASS
```

Runtime post-upload:

```text
DISPLAY_READY=1
RTC_OK=1
Q0_0=1/0 alternando
ALPHA12_PUBLISHED_RUNTIME=PASS
ALPHA12_PUBLISHED_TFT_RUNTIME_READY=PASS
```

## Periféricos integrados

Se mantienen en el autoload normal:

```text
Display
Ethernet W5500
microSD
FRAM
RTC
botonera
RS-485
Modbus RTU
TCA/I/O
SPI compartido
```

## Decisiones que no cambian

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
OPENPLC_RUNTIME_AUTOLOAD=NO
```

No se publica `bootloader.bin` como definitivo.

## Cierre

```text
ALPHA12_TECHNICAL_CLOSURE=PASS
ALPHA12_RELEASE_PUBLICATION=PASS
ALPHA12_PUBLISHED_PACKAGE_GATE=PASS
ALPHA12_STATUS=CLOSED_PUBLISHED
```
