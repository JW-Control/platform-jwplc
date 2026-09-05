# PR Alpha10 - aislar JWPLC_Ethernet del sketchbook

## Resumen

Alpha10 corrige un fallo real de compatibilidad con Arduino Library Resolver: una copia antigua de `JWPLC_Ethernet` instalada en el sketchbook podía tener prioridad sobre la copia bundled del package JWPLC.

El problema se detectó durante un taller con Arduino IDE y terminó en linker error sobre:

```text
JWPLC_EthernetClass::diagnosticCode() const
JWPLC_EthernetClass::runtimeState() const
```

## Causa raíz

```text
ALPHA10_TRIGGER=USER_LIBRARY_SHADOWING
ALPHA10_REPRODUCTION=PASS
ALPHA10_ROOT_CAUSE=CONFIRMED
PRIMARY_FAILURE=STALE_JWPLC_ETHERNET_SELECTED
```

## Cambios

- añade `JWPLC_Bundled_JWPLC_Ethernet.h` como marker exclusivo del package;
- lo carga sólo durante `JWPLC_LIBRARY_DISCOVERY_PHASE`;
- actualiza `JWPLC_Ethernet` de `1.0.0` a `1.0.1`;
- mejora `Verify-JWPLCUnifiedEthernetSelection.ps1` para layouts de repo, `jwplc`, `jwplc_local`, paths Windows escapados y sketchbooks no estándar;
- añade detección específica de `JWPLC_Ethernet` del sketchbook.

Commit técnico:

```text
c0e5c621cec71977b86becfc8d7acb26ca21e906
```

## Validación funcional

Prueba hostil:

```text
COMPILE_EXIT_CODE=0
PACKAGE_ETHERNET_SEEN=True
USER_ETHERNET_SEEN=False
HOSTILE_ETHERNET_TRIGGERED=False
UNDEFINED_REFERENCE=False
ETHERNET_SHADOWING_FIX=PASS
```

Verificador:

```text
JWPLC_Ethernet unificado: SELECCIONADO OK
Backend legacy separado: NO SELECCIONADO OK
Ethernet homonima externa/Espressif: NO SELECCIONADA
Ethernet del sketchbook: IGNORADA OK
JWPLC_Ethernet del sketchbook: IGNORADA OK
JWPLC_ETHERNET_UNIFIED_SELECTION=PASS
```

Matriz final:

```text
DigitalIO_Basic          PASS
Buttons_Basic            PASS
Display_HMI_Fields       PASS
Ethernet_Diagnostics     PASS
RemoteIO_Slave_RTU       PASS

FINAL_COMPILE_MATRIX=5/5_PASS
UNDEFINED_REFERENCE_HITS=0
```

## Benchmark y decisión de alcance

La primera propuesta añadía markers a siete librerías. El benchmark mostró una penalización warm creciente:

| Variante | Markers | Warm promedio | Delta |
|---|---:|---:|---:|
| M0 | 0 | 22.094 s | base |
| M1 | 1 (`JWPLC_Ethernet`) | 23.327 s | +5.6% |
| M4 | 4 | 26.888 s | +21.7% |
| M7 | 7 | 30.353 s | +37.4% |

Una contraprueba Alpha10 -> Alpha9 confirmó que el coste M7 no era sólo efecto del orden:

```text
ALPHA10_WARM_AVG_S=30.996
ALPHA9_WARM_AVG_S=22.227
WARM_DELTA_PCT=39.5
```

Por ello se adopta el mínimo seguro que resuelve la causa primaria reproducida:

```text
ALPHA10_MARKER_SET=JWPLC_ETHERNET_ONLY
GENERALIZED_7_MARKER_OPTION=REJECTED_BUILD_COST
```

La protección general de otras librerías homónimas no se considera resuelta por este PR.

## Compatibilidad

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

No se retiran Display, Ethernet, microSD, FRAM, RTC, botonera, RS-485, Modbus RTU, TCA/I/O ni el arbitraje SPI compartido.

OpenPLC continúa externo/opcional al runtime Arduino.

## Decisiones heredadas

```text
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
OTA=NOT_DEFINED
```

## Pendientes transferidos a Alpha11

- UI de baudrate RTU del Backplane;
- UI de serial format RTU;
- propagación de configuración RTU al HAL;
- `TON0.Q`, `TOF0.Q`, `TP0.Q`;
- validación/autocomplete tipado de miembros FB;
- source freeze reproducible del fork OpenPLC Editor;
- HMI Arduino hacia Ladder/OpenPLC;
- prueba multibit simultánea;
- estrategia de aislamiento general de librerías con menor coste de discovery.

## Estado

```text
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_STATUS=READY_FOR_RELEASE_PR
```
