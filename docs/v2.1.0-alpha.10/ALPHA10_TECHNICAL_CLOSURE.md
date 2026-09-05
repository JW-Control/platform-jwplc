# Alpha10 - Cierre técnico

Fecha de cierre técnico: 2026-09-05.

## Resumen

`v2.1.0-alpha.10` se redefine como un ciclo de limpieza de library discovery y recuperación de build speed.

La primera variante de Alpha10 añadió un guard para impedir que una copia antigua/manual de `JWPLC_Ethernet` instalada en el sketchbook tuviera prioridad sobre la copia del package. El fix funcionó, pero el benchmark mostró un coste warm aproximado de `+5.6%` usando un solo marker.

Dado que JWPLC distribuye sus librerías propias dentro del package, se adopta el contrato:

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
```

Una instalación manual/paralela de `JW_*` o `JWPLC_*` debe corregirse en el entorno del usuario y no añade coste permanente al autoload normal.

## Cambio técnico

Commit principal:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
perf(alpha10): retirar guard de shadowing de JWPLC_Ethernet
```

Cambios:

```text
JWPLC_Bundled_JWPLC_Ethernet.h=REMOVED
JWPLC_GlobalPeripherals_Auto.h=RESTORED_TO_ALPHA9_BEHAVIOR
JWPLC_Ethernet_VERSION=1.0.0
Verify-JWPLCUnifiedEthernetSelection.ps1=REMOVED
```

No cambia:

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

## Protecciones heredadas auditadas

Se conservan:

```text
JWPLC_Bundled_Adafruit_ST77xx.h=KEEP
JWPLC_Bundled_Adafruit_GFX.h=KEEP
JWPLC_Bundled_Adafruit_BusIO.h=KEEP
JWPLC_LIBRARY_DISCOVERY_PHASE=KEEP
```

Motivo: Adafruit es una dependencia externa vendorizada/precompilada y puede coexistir legítimamente con otras versiones instaladas mediante Arduino Library Manager. Alpha5 registró además una selección real de BusIO desde el sketchbook durante un gate genérico.

Detalle: `ALPHA10_PROTECTION_AUDIT.md`.

## Benchmark local

Runs:

```text
20260905_110955
20260905_112529
20260905_113956
```

Estructura:

```text
Basic cold = 15 compiladores
Core cold  = 78 compiladores
Warm       = 1 compilador
COMPILER_STRUCTURE_PARITY=PASS
```

Referencia principal, `Basic / 01_empty / managed_warm_touch`:

```text
r1 = 24.126 s
r2 = 21.860 s
r3 = 23.866 s
avg r1-r3 = 23.284 s
avg r2-r3 = 22.863 s
```

Referencias históricas:

```text
M0_NONE = 22.094 s
M1_ETH  = 23.327 s
M4      = 26.888 s
M7      = 30.353 s
```

El primer run presenta deriva de host, por lo que no se reclama una recuperación porcentual exacta. La conclusión defendible es:

```text
JW_JWPLC_DISCOVERY_MARKER_OVERHEAD=REMOVED
COMPILER_STRUCTURE_PARITY=PASS
WARM_BEHAVIOR_RETURNED_TO_M0_RANGE=PASS_WITH_HOST_VARIATION
EXACT_PERCENT_RECOVERY_CLAIM=NOT_USED
```

La tabla completa está en `ALPHA10_BUILD_BENCHMARK.md`.

## Paridad binaria

```text
Basic / 01_empty    = 4618688 x3
Basic / 02_io_basic = 4618784 x3
Core  / 01_empty    = 4574464 x3
Core  / 02_io_basic = 4574576 x3
ALPHA10_BINARY_SIZE_PARITY=PASS
```

## Matriz funcional local

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
ALPHA10_LOCAL_COMPILE_GATE=PASS
```

## Arduino IDE y validación física

Se compiló y subió desde Arduino IDE el gate histórico:

```text
tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate/06_alpha4_local_physical_gate.ino
```

Resultado:

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

Por tanto:

```text
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_LOCAL_PHYSICAL_GATE=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
```

El gate reutilizado no prueba Ethernet ni RS-485/Modbus físicamente. Alpha10 no modifica esos runtimes, así que se conserva la evidencia física ya cerrada en Alpha6/Alpha7/Alpha9 y se exige únicamente regresión de compilación en este alpha:

```text
Ethernet_Diagnostics=PASS
RemoteIO_Slave_RTU=PASS
ETHERNET_RUNTIME_CHANGED=NO
MODBUS_RTU_RUNTIME_CHANGED=NO
```

Detalle: `ALPHA10_PHYSICAL_VALIDATION_20260905.md`.

## CI

`CI JWPLC Package Smoke` terminó `SUCCESS` para los estados benchmarkeados/documentados hasta:

```text
09ba7395450ce9d85a174dbd96a57f255371590c
```

El commit documental de cierre debe volver a quedar verde antes del merge de PR #90.

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

No se publica `bootloader.bin` como definitivo mientras la configuración final siga pendiente.

OpenPLC continúa externo/opcional al runtime Arduino.

## Cierre técnico

```text
ALPHA10_SCOPE=BUILD_SPEED_CLEANUP
ALPHA10_PROTECTION_AUDIT=PASS
ALPHA10_BUILD_BENCHMARK=PASS_WITH_HOST_VARIATION
ALPHA10_COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_BINARY_SIZE_PARITY=PASS
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_LOCAL_COMPILE_GATE=PASS
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_PUBLICATION_REPLACEMENT=PENDING
NEXT_ALPHA=BLOCKED_UNTIL_ALPHA10_PUBLISHED
```

La publicación Alpha10 interna anterior todavía debe reemplazarse. El alpha sólo pasa a `CLOSED_PUBLISHED` después de merge, nueva PreRelease/ZIP/SHA, actualización del índice dev y validación desde el package publicado.
