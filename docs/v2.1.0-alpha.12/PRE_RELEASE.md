# v2.1.0-alpha.12 — Comunicaciones, runtime y autocontención

## Estado

Release candidate técnicamente cerrada el 2026-10-05.

La PreRelease todavía no está publicada. Los metadatos de artifact se completan
después del merge y de la ejecución del workflow de publicación.

```text
TAG=v2.1.0-alpha.12
ALPHA12_TECHNICAL_CLOSURE=PASS
ALPHA12_RELEASE_PUBLICATION=PENDING
ZIP=PENDING
SIZE=PENDING
SHA256=PENDING
PACKAGE_ROOT=2.1.0/
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

También se mantiene TX queued aditivo en RS-485 y el control de frame gap en
microsegundos.

## Ethernet / W5500

Alpha12 endurece DHCP, recuperación de link, TCP cooperativo, UDP y coexistencia
del W5500 sobre SPI compartido.

Perfil actual cualificado:

```text
W5500_SPI=26 MHz
TCP_RX_POLICY=POLLING_C0
FIFO_REUSE=ON
DLEN_CACHE=ON
COPY_OUT_64=ON
```

## Display y TFT

`JWPLC_Display` utiliza ahora el backend propio `JWPLC_TFT`.

Patrón recomendado:

```cpp
auto &tft = JWPLC_Display.tft();
```

La compatibilidad de uso normal se conserva. Se acepta antes de adopción externa
el cambio de tipo explícito `Adafruit_ST7789&` a `JWPLC_TFTClass&`.

## Package autocontenido

Se retiraron las librerías gráficas Adafruit legacy que ya no eran necesarias:

```text
Adafruit_GFX_Library
Adafruit_ST7735_and_ST7789_Library
```

`Adafruit_BusIO` permanece integrada porque `JW_FRAM` la requiere, y su
resolución bundled fue demostrada tanto con Arduino CLI como con Arduino IDE
2.3.4 aun existiendo una copia externa en el sketchbook.

```text
STRICT_AUTOCONTAINMENT=PASS
EXTERNAL_BUSIO_REQUIRED=NO
```

## Precompilación

`JW_FRAM` fue regenerado después de la corrección de autocontención:

```text
SIZE=126440
SHA256=b8734763bfa1287167feda72a5c9b30df97340d41ca0c3fd000321e218632ceb
```

La auditoría global y el enlace release-like quedaron en PASS.

## Build speed final

Matriz final: 12/12 fases PASS, `Jobs=0`, sin uploads.

La limpieza del package redujo cuatro invocaciones de compilador en cold build
para ambos targets:

```text
JWPLC Basic:      20 -> 16
JWPLC Basic Core: 83 -> 79
```

Promedios frente al rerun post-P8:

```text
COMBINED_COLD_AVG_DELTA=-7.94%
COMBINED_WARM_AVG_DELTA=-2.53%
```

No se retiraron periféricos del autoload normal para obtener estas cifras.

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

Verify final de Arduino IDE:

```text
FLASH=431957 / 4063232 bytes (10%)
RAM=29396 bytes (8%)
RAM_FREE=298284 bytes
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

```text
AUTOLOAD_PERIPHERALS_REMOVED=NO
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

## Pendiente de publicación

Después del merge se deben completar:

```text
PUBLISHED_PACKAGE_SOURCE_SHA=PENDING
ZIP=PENDING
SIZE=PENDING
SHA256=PENDING
DEV_INDEX=PENDING
ISOLATED_INSTALL=PENDING
ISOLATED_COMPILE=PENDING
PUBLISHED_PACKAGE_VALIDATION=PENDING
```

Hasta entonces:

```text
ALPHA12_STATUS=READY_FOR_PR_NOT_PUBLISHED
```
