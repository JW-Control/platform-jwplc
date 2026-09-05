# v2.1.0-alpha.10 - JWPLC Arduino package

## Resumen

Alpha10 es un hotfix de compatibilidad para Arduino IDE / Arduino CLI.

Corrige un caso real donde una versión antigua de `JWPLC_Ethernet` instalada en el sketchbook del usuario podía ser seleccionada antes que la copia incluida en el package JWPLC. En el entorno afectado esto produjo errores de linker porque la librería antigua no implementaba:

```text
JWPLC_EthernetClass::diagnosticCode() const
JWPLC_EthernetClass::runtimeState() const
```

## Corrección

Se incorpora un marker exclusivo del package:

```text
JWPLC_Bundled_JWPLC_Ethernet.h
```

El marker se carga sólo durante `JWPLC_LIBRARY_DISCOVERY_PHASE` para que Arduino Builder descubra primero la copia bundled validada de `JWPLC_Ethernet`.

También se actualiza:

```text
JWPLC_Ethernet 1.0.0 -> 1.0.1
```

No cambia la API pública ni la implementación runtime.

## Validación de shadowing

Prueba hostil final:

```text
COMPILE_EXIT_CODE=0
PACKAGE_ETHERNET_SEEN=True
USER_ETHERNET_SEEN=False
HOSTILE_ETHERNET_TRIGGERED=False
UNDEFINED_REFERENCE=False
ETHERNET_SHADOWING_FIX=PASS
```

Verificador de selección:

```text
JWPLC_Ethernet unificado: SELECCIONADO OK
Backend legacy separado: NO SELECCIONADO OK
Ethernet homonima externa/Espressif: NO SELECCIONADA
Ethernet del sketchbook: IGNORADA OK
JWPLC_Ethernet del sketchbook: IGNORADA OK
JWPLC_ETHERNET_UNIFIED_SELECTION=PASS
```

## Matriz final

```text
DigitalIO_Basic          PASS
Buttons_Basic            PASS
Display_HMI_Fields       PASS
Ethernet_Diagnostics     PASS
RemoteIO_Slave_RTU       PASS

COMPILE_TOTAL=5
COMPILE_PASS=5
COMPILE_FAIL=0
UNDEFINED_REFERENCE_HITS=0
```

## Benchmark de compilación

Se evaluó ampliar el mecanismo bundled a más librerías, pero el coste warm creció de forma casi lineal:

| Configuración | Warm promedio | Delta |
|---|---:|---:|
| 0 markers | 22.094 s | base |
| `JWPLC_Ethernet` únicamente | 23.327 s | +5.6% |
| 4 markers | 26.888 s | +21.7% |
| 7 markers | 30.353 s | +37.4% |

Por estabilidad de tiempos de compilación, Alpha10 adopta sólo el marker necesario para la causa primaria reproducida.

```text
ALPHA10_MARKER_SET=JWPLC_ETHERNET_ONLY
GENERALIZED_7_MARKER_OPTION=REJECTED_BUILD_COST
```

No se afirma que Alpha10 aísle todas las librerías JW/JWPLC frente a copias homónimas del sketchbook.

## Compatibilidad

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
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
- TCA / I/O;
- arbitraje SPI compartido.

OpenPLC continúa externo/opcional al runtime Arduino.

## Decisiones heredadas

Alpha10 no modifica:

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
```

No se publica `bootloader.bin` como definitivo.

## Pendientes transferidos a Alpha11

- configuración de baudrate RTU en Backplane;
- configuración de serial format RTU en Backplane;
- propagación de esos parámetros al HAL;
- referencias tipadas `TON0.Q`, `TOF0.Q`, `TP0.Q`;
- autocomplete/validación de miembros de FB;
- source freeze reproducible del fork OpenPLC Editor;
- exposición HMI hacia Ladder/OpenPLC;
- prueba multibit simultánea;
- estrategia general de aislamiento de librerías con menor coste de discovery.

## Estado previo a publicación

```text
TECHNICAL_COMMIT_SHA=c0e5c621cec71977b86becfc8d7acb26ca21e906
ALPHA10_ROOT_CAUSE=CONFIRMED
ALPHA10_MINIMUM_SAFE_FIX=PASS
ALPHA10_BUILD_BENCHMARK=PASS_WITH_SCOPED_FIX
ALPHA10_FINAL_COMPILE_MATRIX=5/5_PASS
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_PUBLICATION=PENDING_PR_CI_RELEASE
```
