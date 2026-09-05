# v2.1.0-alpha.10 - JWPLC Arduino package

## Resumen

Alpha10 optimiza el ciclo de compilación eliminando un guard de library discovery que protegía una instalación manual/paralela de `JWPLC_Ethernet` en el sketchbook.

El package adopta el modelo:

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
```

La variante inicial de Alpha10 había incorporado un marker de `JWPLC_Ethernet`. Ese marker resolvía el shadowing observado en una laptop con una copia antigua de la librería, pero añadió aproximadamente `+5.6%` al warm build del host de benchmark.

## Protecciones que permanecen

Se conservan los markers de:

```text
Adafruit ST77xx
Adafruit GFX
Adafruit BusIO
```

porque son dependencias externas vendorizadas/precompiladas y pueden coexistir legítimamente con otras versiones instaladas mediante Arduino Library Manager.

## Compatibilidad

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
OPENPLC_AUTOLOAD_INTEGRATION=NO
OTA=NOT_DEFINED
```

Se mantienen integrados:

- Display;
- Ethernet W5500;
- microSD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA/I/O;
- arbitraje SPI compartido.

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

Resultado Alpha10:

```text
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
```

Ethernet y RS-485/Modbus no fueron sometidos a un nuevo stress físico porque Alpha10 no modifica esos runtimes. Se conserva la evidencia física cerrada en Alpha6/Alpha7/Alpha9 y se revalidó compilación mediante `Ethernet_Diagnostics` y `RemoteIO_Slave_RTU`.

## Empaquetado para Boards Manager

El archive de publicación debe contener una única carpeta raíz:

```text
2.1.0/
  boards.txt
  platform.txt
  cores/
  libraries/
  variants/
  ...
```

El workflow oficial debe ejecutarse con:

```text
source_folder=JWPLC/2.1.0
archive_root_mode=folder
```

Durante el cierre de Alpha10 se detectó y documentó que `archive_root_mode=contents` genera un ZIP que Arduino CLI rechaza por no tener una raíz única. Esa publicación fue descartada y no constituye el artefacto final.

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

## Artefacto

El workflow de publicación genera y registra automáticamente el ZIP, `SHA-256` y tamaño en `package_jwplc_index_dev.json` y en el asset del GitHub PreRelease.

```text
ZIP=jwplc-esp32-2.1.0-alpha.10.zip
ARTIFACT_METADATA=GENERATED_BY_RELEASE_WORKFLOW
PUBLIC_INDEX_UPDATE=NO
```

## Estado técnico antes del gate publicado

```text
TECHNICAL_COMMIT_SHA=35385c7286c8a4fdf33aec1af1175b8bb4f45e64
ALPHA10_PROTECTION_AUDIT=PASS
ALPHA10_BUILD_BENCHMARK=PASS_WITH_HOST_VARIATION
ALPHA10_BINARY_SIZE_PARITY=PASS
ALPHA10_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_PUBLISHED_PACKAGE_GATE=REQUIRED_BEFORE_CLOSED_PUBLISHED
```
