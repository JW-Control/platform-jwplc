# Alpha11 — Estado final

Fecha de cierre: 2026-09-09.

## Resumen

`v2.1.0-alpha.11` cierra JWPLC HMI Designer V1, la integración experimental con Arduino IDE 2.3.4, el autocompletado contextual de `JWPLC_Display`, la robustez de botonera y el runtime sin necesidad de `delay(1)` en loops que escriben continuamente E/S.

Alpha11 fue publicado como PreRelease, instalado desde el índice dev oficial en un entorno aislado, compilado, subido y validado en hardware real.

## Alcance funcional

```text
HMI_DESIGNER_V1=PASS_USER
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
PIXELMAP=PASS
MULTIPAGE=PASS
LIVE=PASS
CODEGEN=PASS
ARDUINO_IDE_2_3_4=PASS_USER
AUTOCOMPLETE_JWPLC_DISPLAY=PASS_USER
```

## Runtime

Se confirmó en hardware real que un loop con:

```cpp
digitalWrite(Q0_0, condicion);
```

puede ejecutarse continuamente sin congelar TFT/RTC ni requerir:

```cpp
delay(1);
```

```text
A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
ALPHA11_RUNTIME_CLOSED_LOOP=PASS_PHYSICAL
ALPHA11_DIGITALWRITE_REPEATED_STATE=PASS_PHYSICAL
ALPHA11_RTC_SERVICE=PASS_PHYSICAL
ALPHA11_DISPLAY_SERVICE=PASS_PHYSICAL
ALPHA11_USER_DELAY_REQUIRED=NO
```

## Core precompilado

```text
ARCHIVE=JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
ARCHIVE_BYTES=3019320
ARCHIVE_SHA256=6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68
CORE_PRECOMPILED_BUILD=PASS
CORE_PRECOMPILED_VERIFY_BASIC=PASS
```

El archive final contiene el runtime Alpha11 probado; se eliminó la desincronización entre source y `core.a` detectada durante el cierre.

## Display precompilado

```text
ARCHIVE=JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
ARCHIVE_BYTES=849596
ARCHIVE_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
DISPLAY_TUS=6
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
ALPHA11_WARM_PERFORMANCE_STABLE=PASS
ALPHA11_BINARY_SIZE_REGRESSION=MATERIAL_NO
ALPHA11_EXACT_SPEEDUP_CLAIM=NOT_USED
```

No se utiliza un porcentaje exacto de speedup como conclusión global debido a variación del host.

## Publicación

```text
PR_TECHNICAL=95
PR_INDEX=96
TAG=v2.1.0-alpha.11
PUBLISHED_PACKAGE_SOURCE_SHA=64ce22447e0a9b5852ed83cb5f3a1bd2de3aa218
ZIP=jwplc-esp32-2.1.0-alpha.11.zip
SIZE=24547524
SHA256=465440cf92491b3c9050aa44b6184afae7bfa8e52993d6bc777487d203a97474
PACKAGE_ROOT=2.1.0/
ALPHA11_RELEASE_PUBLICATION=PASS
ALPHA11_DEV_INDEX=PASS
ALPHA11_STABLE_INDEX_UNCHANGED=PASS
```

## Validación del package publicado

La prueba se realizó con un `directories.data` aislado bajo:

```text
%TEMP%\jwplc-alpha11-published-gate
```

Se instaló exactamente:

```text
jwplc:esp32@2.1.0-alpha.11
```

Los archives publicados coincidieron byte a byte con los previamente validados:

```text
PUBLISHED_CORE_SHA256=6edf40d105936318a2fd8a84d7f0724571657910e8d92e8538640ec613f4dd68
PUBLISHED_DISPLAY_SHA256=2974d42c847c1b7c7ab3a7b74da42e2f17969fb852b47a8d434f57f70da924af
```

Resultado:

```text
ALPHA11_PUBLISHED_INSTALL=PASS
ALPHA11_PUBLISHED_ARCHIVE_PARITY=PASS
ALPHA11_PUBLISHED_COMPILE=PASS
ALPHA11_PUBLISHED_UPLOAD=PASS
ALPHA11_PUBLISHED_RUNTIME=PASS
ALPHA11_PUBLISHED_NO_DELAY_GATE=PASS
```

## Periféricos

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

## Decisiones de configuración

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

## Sincronización final

Debido a squash merges históricos, la igualdad de historial no es requisito. El cierre exige que `main` y `release/v2.1.x` terminen con el mismo árbol/contenido.

```text
TREE_PARITY_CRITERION=PASS
GIT_ANCESTRY_PARITY=NOT_REQUIRED
ALPHA11_RELEASE_MAIN_TREE_PARITY=PASS
```

## Estado final

```text
ALPHA11_FUNCTIONAL_SCOPE=PASS
ALPHA11_DESIGNER_V1=PASS_USER
ALPHA11_DISPLAY_PRECOMPILED=PASS
ALPHA11_CORE_PRECOMPILED=PASS
ALPHA11_BUILD_SPEED=PASS_WITH_HOST_VARIATION
ALPHA11_RUNTIME_REGRESSION_GATE=PASS_PHYSICAL
ALPHA11_AUTOCOMPLETE=PASS_USER
ALPHA11_RELEASE_PUBLICATION=PASS
ALPHA11_DEV_INDEX=PASS
ALPHA11_STABLE_INDEX_UNCHANGED=PASS
ALPHA11_PUBLISHED_PACKAGE_GATE=PASS
ALPHA11_RELEASE_MAIN_TREE_PARITY=PASS
ALPHA11_TECHNICAL_CLOSURE=PASS
ALPHA11_STATUS=CLOSED_PUBLISHED
NEXT_ALPHA=UNBLOCKED
```
