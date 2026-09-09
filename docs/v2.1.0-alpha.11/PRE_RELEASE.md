# v2.1.0-alpha.11 — JWPLC HMI Designer V1

## Estado

PreRelease publicada y validada en hardware real el 2026-09-09.

```text
TAG=v2.1.0-alpha.11
PUBLISHED_PACKAGE_SOURCE_SHA=64ce22447e0a9b5852ed83cb5f3a1bd2de3aa218
ZIP=jwplc-esp32-2.1.0-alpha.11.zip
SIZE=24547524
SHA256=465440cf92491b3c9050aa44b6184afae7bfa8e52993d6bc777487d203a97474
PACKAGE_ROOT=2.1.0/
```

## JWPLC HMI Designer V1

Incluye:

- TEXT;
- VALUE;
- BOOL;
- BAR;
- múltiples páginas;
- selector `NN/TT`;
- PixelMap RGB565;
- capas y herramientas de dibujo;
- LIVE Preview;
- codegen automático;
- proyectos `.jwhmi`;
- aplicación Windows standalone.

```text
HMI_DESIGNER_V1=PASS_USER
PIXELMAP=PASS
MULTIPAGE=PASS
LIVE=PASS
CODEGEN=PASS
```

## Arduino IDE 2.3.4

VSIX final:

```text
jwplc-hmi-launcher-0.1.6.vsix
```

Aporta launcher `JW HMI` y autocompletado contextual de una API curada de `JWPLC_Display`, incluidos los valores válidos de configuración en setters.

```text
ARDUINO_IDE_2_3_4=PASS_USER
AUTOCOMPLETE_JWPLC_DISPLAY=PASS_USER
```

## Runtime

Se cerró el caso donde un loop escribiendo continuamente una salida podía congelar RTC/Display cuando el core precompilado estaba desfasado respecto del source.

Caso soportado:

```cpp
void loop()
{
    digitalWrite(Q0_0, condicion);
}
```

No se requiere `delay(1)`.

```text
A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
ALPHA11_RUNTIME_CLOSED_LOOP=PASS_PHYSICAL
ALPHA11_DIGITALWRITE_REPEATED_STATE=PASS_PHYSICAL
ALPHA11_RTC_SERVICE=PASS_PHYSICAL
ALPHA11_DISPLAY_SERVICE=PASS_PHYSICAL
ALPHA11_USER_DELAY_REQUIRED=NO
```

## Archives precompilados finales

Core JWPLC Basic:

```text
JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
SIZE=3019320
SHA256=6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68
```

JWPLC Display:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
SIZE=849596
SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
```

```text
CORE_PRECOMPILED_BUILD=PASS
CORE_PRECOMPILED_VERIFY_BASIC=PASS
PRECOMPILED_DISPLAY_SOURCE_TUS=0
SOURCE_ARCHIVE_EMPTY_PARITY=PASS
SOURCE_ARCHIVE_HMI_PARITY=PASS
```

## Benchmark

```text
TOTAL_PHASES=72
FAILED_PHASES=0
ALPHA11_BUILD_BENCHMARK_3X=PASS
Basic cold compilers=15
Core cold compilers=78
Warm compilers=1
ALPHA11_COMPILER_STRUCTURE_PARITY=PASS
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
ALPHA11_EXACT_SPEEDUP_CLAIM=NOT_USED
```

## Validación post-publicación

La versión publicada se instaló exclusivamente desde el índice dev oficial usando un entorno Arduino CLI aislado:

```text
%TEMP%\jwplc-alpha11-published-gate
```

Se verificó:

```text
INSTALLED=jwplc:esp32@2.1.0-alpha.11
PUBLISHED_CORE_PARITY=PASS
PUBLISHED_DISPLAY_PARITY=PASS
ALPHA11_PUBLISHED_INSTALL=PASS
ALPHA11_PUBLISHED_COMPILE=PASS
ALPHA11_PUBLISHED_UPLOAD=PASS
ALPHA11_PUBLISHED_RUNTIME=PASS
ALPHA11_PUBLISHED_NO_DELAY_GATE=PASS
```

En hardware real, TFT y RTC permanecieron activos mientras `Q0_0` conmutaba continuamente sin `delay(1)` y sin resets inesperados.

## Periféricos integrados

Se conservan en el autoload normal:

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

No se publica `bootloader.bin` como definitivo. No se fija una FlashFreq universal futura. OpenPLC continúa fuera del autoload Arduino.

## Cierre

```text
ALPHA11_RELEASE_PUBLICATION=PASS
ALPHA11_DEV_INDEX=PASS
ALPHA11_STABLE_INDEX_UNCHANGED=PASS
ALPHA11_PUBLISHED_PACKAGE_GATE=PASS
ALPHA11_RELEASE_MAIN_TREE_PARITY=PASS
ALPHA11_TECHNICAL_CLOSURE=PASS
ALPHA11_STATUS=CLOSED_PUBLISHED
```
