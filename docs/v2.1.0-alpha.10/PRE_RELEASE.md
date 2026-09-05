# v2.1.0-alpha.10 - JWPLC Arduino package

> Borrador de PreRelease para la reedición de Alpha10. No publicar hasta completar benchmark, matriz funcional y validación física.

## Resumen

Alpha10 optimiza el ciclo de compilación eliminando un guard de library discovery que protegía una instalación manual/paralela de `JWPLC_Ethernet` en el sketchbook.

El package adopta un modelo administrado para sus librerías propias:

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
```

No se retiran periféricos ni se modifican APIs públicas.

## Cambio principal

```text
JWPLC_Bundled_JWPLC_Ethernet.h=REMOVED
JWPLC_Ethernet_VERSION=1.0.0
JWPLC_GlobalPeripherals_Auto=RESTORED_TO_ALPHA9_BEHAVIOR
```

La versión previa de Alpha10 había incorporado un marker de `JWPLC_Ethernet`. Ese marker resolvía el shadowing observado, pero añadió aproximadamente `+5.6%` al warm build del host de benchmark.

## Protecciones que permanecen

Se conservan los markers de:

```text
Adafruit ST77xx
Adafruit GFX
Adafruit BusIO
```

porque corresponden a dependencias externas vendorizadas/precompiladas y pueden coexistir legítimamente con otras versiones instaladas en Arduino Library Manager/sketchbook.

## Compatibilidad

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
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

Resultados del candidato final:

```text
BASIC_01_EMPTY_COLD=PENDING
BASIC_01_EMPTY_WARM=PENDING
BASIC_01_EMPTY_WARM_TOUCH=PENDING
CORE_01_EMPTY_COLD=PENDING
CORE_01_EMPTY_WARM=PENDING
CORE_01_EMPTY_WARM_TOUCH=PENDING
FUNCTIONAL_COMPILE_MATRIX=PENDING
PHYSICAL_SMOKE=PENDING
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
```

No se publica `bootloader.bin` como definitivo mientras la configuración final siga pendiente.

## Artefacto

Completar después del workflow final:

```text
ZIP=jwplc-esp32-2.1.0-alpha.10.zip
SIZE=PENDING
SHA-256=PENDING
```

## Estado previo a publicación

```text
TECHNICAL_COMMIT_SHA=35385c7286c8a4fdf33aec1af1175b8bb4f45e64
ALPHA10_PROTECTION_AUDIT=PASS
ALPHA10_BUILD_BENCHMARK=PENDING
ALPHA10_FUNCTIONAL_MATRIX=PENDING
ALPHA10_PHYSICAL_VALIDATION=PENDING
ALPHA10_TECHNICAL_CLOSURE=PENDING
DO_NOT_PUBLISH_YET=TRUE
```
