# Alpha12 — Conclusión técnica

Fecha: 2026-10-05

Versión objetivo:

```text
v2.1.0-alpha.12
```

Rama:

```text
v2.1.0-alpha.12/feature/modbus-tcp
```

## Conclusión

Alpha12 queda técnicamente cerrada como release candidate del package Arduino
para JWPLC Basic v2.0.0.

El alcance principal consolida comunicaciones Ethernet/Modbus, runtime
cooperativo, precompilación segura y autocontención del package sin retirar
periféricos del autoload normal.

```text
ALPHA12_TECHNICAL_CLOSURE=PASS
PACKAGE_CONTENT_FREEZE=f060d0d88c57b473d94b55955b265b58d2f9fe1f
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

## Modbus TCP

Server y Client cooperativos quedan soportados con:

```text
FC01
FC02
FC03
FC04
FC05
FC06
FC15
FC16
```

La implementación Client fue consolidada en un único translation unit sin
cambiar la API pública soportada.

```text
MODBUS_TCP_SERVER=PASS
MODBUS_TCP_CLIENT=PASS
ALL_8_FUNCTION_CODES_LINK=PASS
```

## Modbus RTU y RS-485

Se mantiene el motor RTU ASYNC como recomendado y SYNC para compatibilidad.
RS-485 conserva la ruta queued aditiva sobre el hardware AutoDirection.

```text
MODBUS_RTU_ASYNC=SUPPORTED
MODBUS_RTU_SYNC=SUPPORTED
RS485_QUEUED_TX=SUPPORTED
RTU100HZ=OPERATIONAL_CONTRACT_NOT_HARD_REALTIME
```

## Ethernet / W5500

Se conserva la API Arduino compatible y se consolidan rutas cooperativas para
conexión, cierre, flush y transmisión, junto con hardening de RX y recuperación.

Perfil cualificado actual:

```text
W5500_SPI_HZ=26000000
TCP_RX_POLICY=POLLING_C0
FIFO_REUSE=ON
DLEN_CACHE=ON
COPY_OUT_64=ON
```

Las variantes INT/D2/D3/E1 no quedan promovidas como defaults de producto.

## Display / TFT

`JWPLC_Display` utiliza el backend propio `JWPLC_TFT`.

Se acepta antes de adopción externa la incompatibilidad del tipo explícito:

```text
Adafruit_ST7789& -> JWPLC_TFTClass&
```

El patrón recomendado continúa siendo:

```cpp
auto &tft = JWPLC_Display.tft();
```

No se reintroduce el backend Adafruit sólo para conservar ese tipo histórico.

## Autocontención

Se retiraron del package activo:

```text
Adafruit_GFX_Library
Adafruit_ST7735_and_ST7789_Library
```

Se conserva `Adafruit_BusIO` porque `JW_FRAM` utiliza
`Adafruit_SPIDevice`, y se fuerza su discovery desde el package.

```text
BUNDLED_BUSIO_SELECTED=True
EXTERNAL_BUSIO_SELECTED=NO
LEGACY_GFX_SELECTED=False
LEGACY_ST77XX_SELECTED=False
STRICT_AUTOCONTAINMENT_CLI=PASS
STRICT_AUTOCONTAINMENT_ARDUINO_IDE=PASS
```

## Precompilados

La auditoría final post-autocontención cubrió 12 archives.

`JW_FRAM` fue regenerado después del cambio de discovery:

```text
JW_FRAM_CLASS=REGENERATED
JW_FRAM_BYTES=126440
JW_FRAM_SHA256=b8734763bfa1287167feda72a5c9b30df97340d41ca0c3fd000321e218632ceb
JW_FRAM_SOURCE_FRESHNESS_PASS=True
```

Resultado global:

```text
P7A_POST_FREEZE_GLOBAL_AUDIT=PASS
P7B_RELEASE_LIKE_ACTIVATION=PASS
PRECOMPILED_ARCHIVE_IDENTITIES=PASS
PRECOMPILED_POLICY_ACTIVE=PASS
NORMAL_AUTOLOAD_COMPLETE=PASS
PRECOMPILED_FREEZE=PASS
```

## Build speed

La matriz final post-autocontención contiene 12/12 celdas PASS.

Cambio estructural frente al rerun post-P8:

```text
BASIC_COLD_COMPILER_INVOCATIONS=20 -> 16
CORE_COLD_COMPILER_INVOCATIONS=83 -> 79
COMBINED_COLD_AVG_DELTA=-7.94%
COMBINED_WARM_AVG_DELTA=-2.53%
BUILD_STRUCTURE_IMPROVED=YES
```

No se retiró ningún periférico del autoload normal para conseguir esta mejora.

## Gates finales

Arduino CLI:

```text
PASS_COUNT=6
FAIL_COUNT=0
WARNINGS=0
ERRORS=0
FINAL_REPO_HYGIENE=PASS
FINAL_ARDUINO_CLI_GATE=PASS
```

Arduino IDE 2.3.4, board JWPLC Basic:

```text
FINAL_ARDUINO_IDE_GATE=PASS
BUNDLED_ADAFRUIT_BUSIO_SELECTED=YES
FLASH_USED_BYTES=431957
RAM_USED_BYTES=29396
RAM_FREE_BYTES=298284
```

No se repitió upload físico porque el cambio final era de resolución/build y
los periféricos afectados no requerían una nueva matriz física.

## Decisiones explícitamente diferidas

```text
OPENPLC_RUNTIME_AUTOLOAD=NO
OTA=NOT_DEFINED
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
BOOTLOADER_BIN_FINAL=NO
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL_NOT_DEFAULT
```

No se publica `bootloader.bin` como definitivo.

## Estado de release

```text
ALPHA12_TECHNICAL_CLOSURE=PASS
ALPHA12_RELEASE_CANDIDATE=READY
PR=NEXT
MERGE=PENDING
PRE_RELEASE_PUBLICATION=PENDING
DEV_INDEX=PENDING
ISOLATED_PUBLISHED_PACKAGE_GATE=PENDING
ALPHA12_STATUS=NOT_YET_CLOSED_PUBLISHED
```
