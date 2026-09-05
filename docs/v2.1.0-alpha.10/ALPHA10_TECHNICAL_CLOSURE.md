# Alpha10 - Cierre técnico

## Resumen

Alpha10 se redefine como hotfix de compatibilidad para resolver un fallo real observado durante un taller con Arduino IDE: una copia antigua de `JWPLC_Ethernet` instalada en el sketchbook del usuario podía ser seleccionada por Arduino Library Resolver antes que la copia distribuida con el package JWPLC.

El fallo reproducido terminaba en linker error porque la copia antigua no implementaba las APIs usadas por `JWPLC_Display`:

```text
JWPLC_EthernetClass::diagnosticCode() const
JWPLC_EthernetClass::runtimeState() const
```

Commit técnico validado:

```text
c0e5c621cec71977b86becfc8d7acb26ca21e906
fix(alpha10): aislar JWPLC_Ethernet del sketchbook
```

## Causa raíz

```text
ALPHA10_TRIGGER=USER_LIBRARY_SHADOWING
ALPHA10_REPRODUCTION=PASS
ALPHA10_ROOT_CAUSE=CONFIRMED
PRIMARY_FAILURE=STALE_JWPLC_ETHERNET_SELECTED
```

El problema no era la instalación del platform ni el FQBN: el entorno afectado utilizaba el package JWPLC correcto, pero Arduino resolvía una librería homónima desde el sketchbook.

## Solución adoptada

Se añade un marker exclusivo del package:

```text
JWPLC_Bundled_JWPLC_Ethernet.h
```

`JWPLC_GlobalPeripherals_Auto.h` lo incluye únicamente durante `JWPLC_LIBRARY_DISCOVERY_PHASE`, de forma que Arduino Builder descubra primero la copia bundled validada de `JWPLC_Ethernet`.

También se actualiza la metadata de la librería:

```text
JWPLC_Ethernet 1.0.0 -> 1.0.1
```

No se modifican APIs públicas ni implementación runtime.

## Verificador

`Verify-JWPLCUnifiedEthernetSelection.ps1` se corrigió para:

- aceptar rutas del árbol fuente del repositorio;
- aceptar `Arduino15/packages/jwplc/...`;
- aceptar `Arduino15/packages/jwplc_local/...`;
- normalizar paths Windows escapados en `compile_commands.json`;
- detectar sketchbooks Arduino en layouts no estándar;
- detectar específicamente una `JWPLC_Ethernet` del sketchbook.

Resultado final:

```text
JWPLC_Ethernet unificado: SELECCIONADO OK
Backend legacy separado: NO SELECCIONADO OK
Ethernet homonima externa/Espressif: NO SELECCIONADA
Ethernet del sketchbook: IGNORADA OK
JWPLC_Ethernet del sketchbook: IGNORADA OK
JWPLC_ETHERNET_UNIFIED_SELECTION=PASS
```

## Regresión hostil

Se creó una `JWPLC_Ethernet` intencionalmente incompatible en el sketchbook con un `#error` para demostrar que no era seleccionada.

```text
COMPILE_EXIT_CODE=0
PACKAGE_ETHERNET_SEEN=True
USER_ETHERNET_SEEN=False
HOSTILE_ETHERNET_TRIGGERED=False
UNDEFINED_REFERENCE=False
ETHERNET_SHADOWING_FIX=PASS
```

La librería temporal se eliminó al terminar la prueba.

## Matriz final de compilación

El estado exacto adoptado se volvió a validar con:

```text
DigitalIO_Basic          PASS
Buttons_Basic            PASS
Display_HMI_Fields       PASS
Ethernet_Diagnostics     PASS
RemoteIO_Slave_RTU       PASS
```

Resultado:

```text
COMPILE_TOTAL=5
COMPILE_PASS=5
COMPILE_FAIL=0
UNDEFINED_REFERENCE_HITS=0
FINAL_COMPILE_MATRIX=5/5_PASS
```

## Benchmark y alcance del aislamiento

Se evaluó inicialmente proteger siete librerías homónimas mediante markers. Esa opción fue descartada por coste de library discovery.

| Variante | Warm promedio | Delta |
|---|---:|---:|
| 0 markers | 22.094 s | base |
| Ethernet únicamente | 23.327 s | +5.6% |
| 4 markers | 26.888 s | +21.7% |
| 7 markers | 30.353 s | +37.4% |

Alpha10 adopta el mínimo conjunto que resuelve la causa primaria reproducida:

```text
ALPHA10_MARKER_SET=JWPLC_ETHERNET_ONLY
GENERALIZED_7_MARKER_OPTION=REJECTED_BUILD_COST
```

No se afirma que Alpha10 blinde todas las librerías JW/JWPLC frente a copias homónimas del sketchbook.

## Compatibilidad

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

Se mantienen integrados Display, Ethernet W5500, microSD, FRAM, RTC, botonera, RS-485, Modbus RTU, TCA/I/O y arbitraje SPI compartido.

OpenPLC continúa externo/opcional al runtime Arduino.

## Decisiones heredadas sin cambio

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

El hotfix ocupa el alcance de Alpha10. Se transfieren explícitamente:

```text
BACKPLANE_RTU_BAUDRATE_UI=PENDING
BACKPLANE_RTU_SERIAL_FORMAT_UI=PENDING
BACKPLANE_RTU_CONFIG_PROPAGATION=PENDING
TON_Q_REFERENCE=REQUIRED
TOF_Q_REFERENCE=REQUIRED
TP_Q_REFERENCE=REQUIRED
FB_MEMBER_TYPE_VALIDATION=REQUIRED
FB_MEMBER_AUTOCOMPLETE=REQUIRED
OPENPLC_EDITOR_SOURCE_FREEZE=PENDING
MULTIBIT_SIMULTANEOUS=NOT_TESTED
HMI_TO_OPENPLC_LADDER=PENDING
GENERALIZED_LIBRARY_ISOLATION_LOW_COST=PENDING
```

## Estado

```text
ALPHA10_ROOT_CAUSE=CONFIRMED
ALPHA10_MINIMUM_SAFE_FIX=PASS
ALPHA10_BUILD_BENCHMARK=PASS_WITH_SCOPED_FIX
ALPHA10_ETHERNET_SELECTION_VERIFIER=PASS
ALPHA10_FINAL_COMPILE_MATRIX=5/5_PASS
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_STATUS=TECHNICALLY_CLOSED
PUBLICATION=PENDING_PR_CI_RELEASE
```
