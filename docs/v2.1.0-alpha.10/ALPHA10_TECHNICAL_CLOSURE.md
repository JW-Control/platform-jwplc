# Alpha10 - Cierre técnico reabierto

## Resumen

El cierre técnico previo de Alpha10 se reabre antes de continuar con otro alpha.

La primera versión de Alpha10 añadió un guard para impedir que una copia antigua de `JWPLC_Ethernet` instalada manualmente en el sketchbook tuviera prioridad sobre la copia del package. El fix funcionó, pero su benchmark mostró un coste warm aproximado de `+5.6%` incluso usando un solo marker.

Dado que el package JWPLC administra sus propias librerías y el despliegue actual puede controlarse, Alpha10 se redefine como un ciclo de limpieza y recuperación de tiempo de compilación.

## Contrato de producto

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
```

No se garantiza que una copia antigua/manual de `JW_*` o `JWPLC_*` instalada en paralelo al package sea ignorada automáticamente por Arduino Builder.

Ese entorno debe corregirse eliminando/renombrando la copia conflictiva o ajustando la instalación, no añadiendo un coste permanente al autoload normal.

## Cambio técnico

Commit:

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

Los markers de Adafruit se conservan:

```text
JWPLC_Bundled_Adafruit_ST77xx.h=KEEP
JWPLC_Bundled_Adafruit_GFX.h=KEEP
JWPLC_Bundled_Adafruit_BusIO.h=KEEP
```

Motivo: corresponden a dependencias externas vendorizadas/precompiladas. Es normal que un usuario tenga otras versiones de Adafruit instaladas mediante Library Manager, y Alpha5 ya registró una selección real de BusIO desde sketchbook durante un gate genérico.

También se conserva `JWPLC_LIBRARY_DISCOVERY_PHASE` porque forma parte del autoload liviano y de la selección reproducible del stack Display.

Detalle: `ALPHA10_PROTECTION_AUDIT.md`.

## Benchmark local del candidato

El 2026-09-05 se ejecutaron tres réplicas completas sobre:

```text
BRANCH=v2.1.0-alpha.10/optimize/remove-shadow-guard
HEAD=456d5b9f55088091fcadcb87e9f33ffb90d3754c
DIRTY_COUNT=0
ALPHA10_CLEANUP_SOURCE=PASS
```

Runs:

```text
20260905_110955
20260905_112529
20260905_113956
```

La estructura de compilación permaneció:

```text
Basic cold = 15 compiladores
Core cold  = 78 compiladores
Warm       = 1 compilador
COMPILER_STRUCTURE_PARITY=PASS
```

Para `Basic / 01_empty / managed_warm_touch`:

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

El primer run evidencia deriva del host, por lo que no se reclama una recuperación porcentual exacta. La conclusión defendible es que el coste específico de los markers JW/JWPLC fue eliminado, se conserva la estructura de compilación y el warm estabilizado vuelve al entorno de M0.

```text
ALPHA10_BENCHMARK_RUNS=3_PASS
ALPHA10_COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_WARM_BEHAVIOR=PASS_WITH_HOST_VARIATION
EXACT_PERCENT_RECOVERY_CLAIM=NOT_USED
```

## Paridad binaria

Los `results.csv` de r1/r2/r3 dieron valores idénticos por target/sketch:

```text
Basic / 01_empty    = 4618688,4618688,4618688
Basic / 02_io_basic = 4618784,4618784,4618784
Core  / 01_empty    = 4574464,4574464,4574464
Core  / 02_io_basic = 4574576,4574576,4574576
ALPHA10_BINARY_SIZE_PARITY=PASS
```

No aparece deriva de `BinaryBytes` entre las réplicas del candidato.

## Matriz funcional local

Sobre `HEAD=c696034fd2c1f1dafbeb33fcf41c06be6a8f05f1` se ejecutó la matriz local:

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

Esto valida compilación de los bloques principales afectados por el autoload/discovery sin introducir dependencias manuales JW/JWPLC externas.

## CI

El workflow `CI JWPLC Package Smoke` de la PR #90 terminó correctamente para el HEAD benchmarkeado y posteriormente para el HEAD documental `c696034fd2c1f1dafbeb33fcf41c06be6a8f05f1`.

```text
CI_JWPLC_PACKAGE_SMOKE=SUCCESS
```

Los commits documentales posteriores deben volver a dejar CI verde antes del merge final.

## Periféricos

Alpha10 mantiene integrados:

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

No se obtiene el benchmark retirando periféricos.

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

Estas conclusiones se revalidan sin cambio para Alpha10. No se publica `bootloader.bin` como definitivo mientras la configuración final siga pendiente.

OpenPLC continúa externo/opcional al runtime Arduino; este alpha no redefine esa arquitectura.

## Validación restante antes del cierre

Antes de volver a declarar Alpha10 cerrado faltan únicamente:

- compilación desde Arduino IDE con el candidato local;
- smoke físico rápido de periféricos integrados;
- CI verde sobre el HEAD documental final;
- README raíz final.

Después del cierre técnico se reemplazará la publicación Alpha10 interna y se validará el package desde el índice dev publicado.

La tabla completa de tiempos y `BinaryBytes` se encuentra en `ALPHA10_BUILD_BENCHMARK.md`.

## Estado actual

```text
ALPHA10_SCOPE=BUILD_SPEED_CLEANUP
ALPHA10_PROTECTION_AUDIT=PASS
ALPHA10_TECHNICAL_CHANGE=COMMITTED
ALPHA10_LOCAL_BENCHMARK=PASS_WITH_HOST_VARIATION
ALPHA10_COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_BINARY_SIZE_PARITY=PASS
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_LOCAL_COMPILE_GATE=PASS
ALPHA10_CI_PR90=PASS_TO_C696034F
ALPHA10_ARDUINO_IDE_VALIDATION=PENDING
ALPHA10_PHYSICAL_VALIDATION=PENDING
ALPHA10_TECHNICAL_CLOSURE=PENDING_PHYSICAL_GATE
ALPHA10_PUBLICATION_REPLACEMENT=PENDING
NEXT_ALPHA=BLOCKED_UNTIL_ALPHA10_CLOSED
```
