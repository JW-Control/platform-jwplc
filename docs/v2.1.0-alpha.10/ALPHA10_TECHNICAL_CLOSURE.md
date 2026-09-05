# Alpha10 - Cierre técnico

Fecha de cierre: 2026-09-05.

## Resumen

`v2.1.0-alpha.10` cierra la limpieza de library discovery iniciada al analizar el shadowing de `JWPLC_Ethernet`. El guard añadido en la primera variante resolvía una instalación manual/paralela de esa librería, pero añadía coste al warm build. El flujo soportado queda definido como package-managed:

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
```

No se retiran periféricos ni se modifican APIs públicas o runtimes.

## Cambio técnico

```text
TECHNICAL_COMMIT_SHA=35385c7286c8a4fdf33aec1af1175b8bb4f45e64
JWPLC_Bundled_JWPLC_Ethernet.h=REMOVED
JWPLC_GlobalPeripherals_Auto.h=RESTORED_TO_ALPHA9_BEHAVIOR
JWPLC_Ethernet_VERSION=1.0.0
Verify-JWPLCUnifiedEthernetSelection.ps1=REMOVED
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

Se conservan `JWPLC_Bundled_Adafruit_ST77xx.h`, `JWPLC_Bundled_Adafruit_GFX.h`, `JWPLC_Bundled_Adafruit_BusIO.h` y `JWPLC_LIBRARY_DISCOVERY_PHASE` por corresponder a dependencias externas vendorizadas/precompiladas.

## Benchmark

Tres réplicas del candidato final conservaron la estructura esperada:

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

No se reclama una recuperación porcentual exacta por variación del host. El overhead conocido del marker JW/JWPLC fue retirado y el warm estabilizado volvió al entorno de M0.

Paridad binaria:

```text
Basic / 01_empty    = 4618688 x3
Basic / 02_io_basic = 4618784 x3
Core  / 01_empty    = 4574464 x3
Core  / 02_io_basic = 4574576 x3
ALPHA10_BINARY_SIZE_PARITY=PASS
```

Detalle completo: `ALPHA10_BUILD_BENCHMARK.md`.

## Matriz funcional y hardware

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

Gate físico:

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

Ethernet y RS-485/Modbus RTU no recibieron un nuevo stress porque Alpha10 no cambia sus runtimes. Se conserva la evidencia física de Alpha6/Alpha7/Alpha9 y se revalidó compilación de `Ethernet_Diagnostics` y `RemoteIO_Slave_RTU`.

## Hallazgo de empaquetado

La primera reedición del release se generó con `archive_root_mode=contents`. Arduino CLI la rechazó con:

```text
no unique root dir in archive
```

Ese artefacto se descartó. El workflow final usa y valida una única raíz:

```text
2.1.0/
  boards.txt
  platform.txt
  cores/
  libraries/
  variants/
  ...
```

El workflow manual quedó reducido a un único input editable (`version`) y valida la raíz y los archivos requeridos antes de publicar.

## Publicación final validada

```text
PUBLISHED_PACKAGE_SOURCE_SHA=f365738e8b0903bca9f93f5c42dfee8310e074b2
TAG=v2.1.0-alpha.10
ZIP=jwplc-esp32-2.1.0-alpha.10.zip
SIZE=24464282
SHA256=5ca5a71d6de0ddd25c81442d7ea4f840ad48603dd024afcd2925235dc4d1b0bf
PACKAGE_ROOT=2.1.0/
```

Validación desde el package publicado:

```text
ALPHA10_PUBLISHED_EXACT_INSTALL=PASS
ALPHA10_PUBLISHED_COMPILE=PASS
ALPHA10_PUBLISHED_UPLOAD=PASS
ALPHA10_PUBLISHED_RUNTIME=PASS
```

El tag permanece apuntando al commit fuente del package probado (`f365738e...`). Los commits posteriores sólo sincronizan automatización, índices y documentación, por lo que no requieren republicar el ZIP validado.

## README y sincronización final

El README raíz fue reorganizado tomando Alpha1 como referencia de distribución. Los dos índices JSON quedaron en la primera sección útil, se actualizó la lista real de librerías y se eliminó la descripción obsoleta del guard `JWPLC_Bundled_JWPLC_Ethernet.h` como comportamiento vigente.

El sync de contenido se realizó con PR #93 mediante un branch creado directamente desde `main` y un commit cuyo árbol coincidía con `release/v2.1.x`. Después del merge:

```text
MAIN_COMMIT_AFTER_SYNC=d25db91db2f327b4e97bef1ff339fc514548a632
MAIN_TREE_AFTER_SYNC=7d0358741d525445630814894dde6ea2cab37dbd
RELEASE_TREE_AT_SYNC=7d0358741d525445630814894dde6ea2cab37dbd
ALPHA10_RELEASE_MAIN_TREE_PARITY=PASS
```

La historia Git puede seguir divergente por los squash merges históricos; el criterio de cierre es la paridad de árbol/contenido.

## Decisiones heredadas

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
OPENPLC_AUTOLOAD_INTEGRATION=NO
```

No se publica `bootloader.bin` como definitivo.

## Estado final

```text
ALPHA10_SCOPE=BUILD_SPEED_CLEANUP
ALPHA10_PROTECTION_AUDIT=PASS
ALPHA10_BUILD_BENCHMARK=PASS_WITH_HOST_VARIATION
ALPHA10_COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_BINARY_SIZE_PARITY=PASS
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
ALPHA10_PUBLISHED_INSTALL=PASS
ALPHA10_PUBLISHED_COMPILE=PASS
ALPHA10_PUBLISHED_UPLOAD=PASS
ALPHA10_PUBLISHED_RUNTIME=PASS
ALPHA10_RELEASE_PUBLICATION=PASS
ALPHA10_RELEASE_MAIN_TREE_PARITY=PASS
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_STATUS=CLOSED_PUBLISHED
NEXT=ALPHA11
```
