# Transferencia Alpha10 -> Alpha11

## Regla de avance

Alpha11 no debe iniciar como trabajo de release hasta que Alpha10 quede publicado nuevamente y validado desde el índice dev.

```text
NEXT_ALPHA=BLOCKED_UNTIL_ALPHA10_PUBLISHED
```

## Qué cerró Alpha10

Alpha10 queda acotado a limpieza de build/library discovery:

```text
JWPLC_ETHERNET_SHADOW_GUARD=REMOVED
JWPLC_ETHERNET_VERSION=1.0.0
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
ADAFRUIT_BUNDLED_MARKERS=RETAINED
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

Commit técnico:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

Cierre técnico local:

```text
ALPHA10_BUILD_BENCHMARK=PASS_WITH_HOST_VARIATION
ALPHA10_COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_BINARY_SIZE_PARITY=PASS
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
ALPHA10_TECHNICAL_CLOSURE=PASS
```

## Benchmark de referencia

```text
M0_NONE = 22.094 s
M1_ETH  = 23.327 s
M4      = 26.888 s
M7      = 30.353 s

Candidato Basic/01_empty/managed_warm_touch:
r1 = 24.126 s
r2 = 21.860 s
r3 = 23.866 s
avg r1-r3 = 23.284 s
avg r2-r3 = 22.863 s
```

No usar un porcentaje exacto de recuperación como conclusión del alpha; se registró variación de host.

## Decisiones que Alpha11 hereda

```text
PACKAGE_MANAGED_JW_LIBRARIES=YES
MANUAL_JW_LIBRARY_SHADOWING_GUARD=NO
THIRD_PARTY_ADAFRUIT_SELECTION_GUARDS=YES
OPENPLC_AUTOLOAD_INTEGRATION=NO
OTA=NOT_DEFINED
CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE
FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING
BOOTLOADER_PRECOMPILED=NOT_ADOPTED
BOOTLOADER_BIN_FINAL=NO
APP_ONLY=VALIDATED_DEVELOPMENT_TOOL
APP_ONLY_DEFAULT_UPLOAD=NO
```

Alpha11 no debe reintroducir markers generalizados de librerías JW/JWPLC sin una necesidad reproducida y un benchmark que justifique el coste.

## Evidencia física que se transfiere

Alpha10 revalidó desde Arduino IDE:

- Display;
- RTC;
- FRAM;
- microSD;
- botonera;
- 8 DI;
- 8 DO;
- TFT visual.

Ethernet y RS-485/Modbus no cambiaron de runtime en Alpha10. Se conserva su evidencia física cerrada en Alpha6/Alpha7/Alpha9 y su regresión de compilación Alpha10:

```text
Ethernet_Diagnostics=PASS
RemoteIO_Slave_RTU=PASS
UNDEFINED_REFERENCE_HITS=0
```

## Pendiente antes de habilitar Alpha11

Sólo queda el cierre de publicación de Alpha10:

- CI verde del commit documental final;
- merge de PR #90 a `release/v2.1.x`;
- reemplazo del release/tag Alpha10 interno previo;
- nueva PreRelease `v2.1.0-alpha.10`;
- ZIP/SHA/tamaño nuevos;
- índice dev actualizado;
- índice estable sin cambios;
- instalación/compilación/upload desde el package publicado;
- README y documentos de transferencia finales;
- estado `CLOSED_PUBLISHED`.

## Estado

```text
ALPHA10_TO_ALPHA11_HANDOFF=PREPARED
ALPHA10_TECHNICAL_CLOSURE=PASS
HANDOFF_EXECUTION=PENDING_ALPHA10_PUBLICATION
```
