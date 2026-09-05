# v2.1.0-alpha.10 - JWPLC Arduino package

## Resumen

Alpha10 optimiza el ciclo de compilación retirando un guard de library discovery específico para una instalación manual/paralela de `JWPLC_Ethernet` en el sketchbook.

El flujo soportado queda definido como:

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
```

No se retiran periféricos ni se modifican APIs públicas o runtimes.

## Cambio principal

```text
JWPLC_Bundled_JWPLC_Ethernet.h=REMOVED
JWPLC_Ethernet_VERSION=1.0.0
JWPLC_GlobalPeripherals_Auto=RESTORED_TO_ALPHA9_BEHAVIOR
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

La variante inicial de Alpha10 había incorporado un marker de `JWPLC_Ethernet`. Ese marker resolvía el shadowing observado en una laptop con una copia antigua de la librería, pero añadió aproximadamente `+5.6%` al warm build del host de benchmark.

## Protecciones que permanecen

Se conservan los markers de Adafruit ST77xx, GFX y BusIO porque son dependencias externas vendorizadas/precompiladas y pueden coexistir legítimamente con versiones instaladas mediante Arduino Library Manager.

## Compatibilidad

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
OPENPLC_AUTOLOAD_INTEGRATION=NO
OTA=NOT_DEFINED
```

Se mantienen integrados Display, Ethernet W5500, microSD, FRAM, RTC, botonera, RS-485, Modbus RTU, TCA/I/O y arbitraje SPI compartido.

OpenPLC continúa externo/opcional al runtime Arduino.

## Benchmark

Evidencia histórica:

| Configuración | Warm promedio | Delta |
|---|---:|---:|
| 0 markers JW/JWPLC | 22.094 s | base |
| Ethernet únicamente | 23.327 s | +5.6% |
| 4 markers | 26.888 s | +21.7% |
| 7 markers | 30.353 s | +37.4% |

Tres réplicas del candidato final:

```text
Basic cold = 15 compiladores
Core cold  = 78 compiladores
Warm       = 1 compilador
COMPILER_STRUCTURE_PARITY=PASS
```

`Basic / 01_empty / managed_warm_touch`:

```text
r1 = 24.126 s
r2 = 21.860 s
r3 = 23.866 s
avg r1-r3 = 23.284 s
avg r2-r3 = 22.863 s
historical M0 = 22.094 s
historical M1 = 23.327 s
```

No se reclama una recuperación porcentual exacta por variación del host. El warm estabilizado vuelve al entorno de M0 y se conserva la estructura esperada de compilación.

## Paridad binaria

```text
Basic / 01_empty    = 4618688 x3
Basic / 02_io_basic = 4618784 x3
Core  / 01_empty    = 4574464 x3
Core  / 02_io_basic = 4574576 x3
ALPHA10_BINARY_SIZE_PARITY=PASS
```

## Matriz funcional

```text
DigitalIO_Basic=PASS
Buttons_Basic=PASS
Display_HMI_Fields=PASS
Ethernet_Diagnostics=PASS
RemoteIO_Slave_RTU=PASS
COMPILE_TOTAL=5
COMPILE_PASS=5
COMPILE_FAIL=0
UNDEFINED_REFERENCE_HITS=0
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
```

## Arduino IDE y hardware

El gate físico local fue compilado y subido desde Arduino IDE usando el autoload normal del package.

```text
ALPHA4_DISPLAY_READY=PASS
ALPHA4_RTC=PASS
ALPHA4_FRAM=PASS
ALPHA4_SD=PASS
ALPHA4_BUTTONS=PASS
ALPHA4_INPUTS=PASS
ALPHA4_OUTPUTS=PASS
ALPHA4_DISPLAY_VISUAL=PASS
ALPHA4_LOCAL_PHYSICAL_GATE=PASS
```

Ethernet y RS-485/Modbus no fueron sometidos a un nuevo stress físico porque Alpha10 no modifica esos runtimes. Se conserva la evidencia física cerrada en Alpha6/Alpha7/Alpha9 y se revalidó compilación mediante `Ethernet_Diagnostics` y `RemoteIO_Slave_RTU`.

## Empaquetado para Boards Manager

Durante el cierre se detectó que un ZIP creado con `archive_root_mode=contents` no tiene una raíz única y Arduino CLI lo rechaza. Ese artefacto fue descartado.

La publicación final usa y valida una única raíz:

```text
2.1.0/
  boards.txt
  platform.txt
  cores/
  libraries/
  variants/
  ...
```

El workflow manual quedó simplificado a un único input editable (`version`) y fija internamente la política correcta de empaquetado.

## Artefacto final

```text
PUBLISHED_PACKAGE_SOURCE_SHA=f365738e8b0903bca9f93f5c42dfee8310e074b2
TAG=v2.1.0-alpha.10
ZIP=jwplc-esp32-2.1.0-alpha.10.zip
SIZE=24464282
SHA256=5ca5a71d6de0ddd25c81442d7ea4f840ad48603dd024afcd2925235dc4d1b0bf
PACKAGE_ROOT=2.1.0/
PUBLIC_INDEX_UPDATE=NO
DEV_INDEX_UPDATE=PASS
```

Validación desde el package publicado:

```text
ALPHA10_PUBLISHED_EXACT_INSTALL=PASS
ALPHA10_PUBLISHED_COMPILE=PASS
ALPHA10_PUBLISHED_UPLOAD=PASS
ALPHA10_PUBLISHED_RUNTIME=PASS
```

## Sincronización final

El contenido final de Alpha10 fue sincronizado a `main` con PR #93 mediante Squash and merge. En el momento del sync, ambos branches quedaron con el mismo árbol:

```text
MAIN_COMMIT_AFTER_SYNC=d25db91db2f327b4e97bef1ff339fc514548a632
MAIN_TREE_AFTER_SYNC=7d0358741d525445630814894dde6ea2cab37dbd
RELEASE_TREE_AT_SYNC=7d0358741d525445630814894dde6ea2cab37dbd
ALPHA10_RELEASE_MAIN_TREE_PARITY=PASS
```

La paridad se evalúa por árbol/contenido; no se exige ancestría literal por los squash merges históricos.

## Decisiones de configuración

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
```

No se publica `bootloader.bin` como definitivo mientras la configuración final siga pendiente.

## Estado final

```text
TECHNICAL_COMMIT_SHA=35385c7286c8a4fdf33aec1af1175b8bb4f45e64
ALPHA10_PROTECTION_AUDIT=PASS
ALPHA10_BUILD_BENCHMARK=PASS_WITH_HOST_VARIATION
ALPHA10_BINARY_SIZE_PARITY=PASS
ALPHA10_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
ALPHA10_PUBLISHED_INSTALL=PASS
ALPHA10_PUBLISHED_COMPILE=PASS
ALPHA10_PUBLISHED_UPLOAD=PASS
ALPHA10_PUBLISHED_RUNTIME=PASS
ALPHA10_RELEASE_PUBLICATION=PASS
ALPHA10_RELEASE_MAIN_TREE_PARITY=PASS
ALPHA10_STATUS=CLOSED_PUBLISHED
NEXT=ALPHA11
```
