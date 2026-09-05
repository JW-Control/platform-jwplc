# PR - Alpha10: retirar guard de shadowing y recuperar build speed

## Resumen

Esta PR redefine `v2.1.0-alpha.10` como un ciclo de limpieza de library discovery y recuperación de build speed.

El guard introducido inicialmente para `JWPLC_Ethernet` resolvía una copia antigua/manual instalada en un sketchbook, pero el benchmark mostró un coste warm aproximado de `+1.233 s / +5.6%` incluso con un único marker.

Se adopta como contrato:

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
```

## Cambios

- elimina `JWPLC_Bundled_JWPLC_Ethernet.h`;
- restaura `JWPLC_GlobalPeripherals_Auto.h` al comportamiento de Alpha9;
- restaura `JWPLC_Ethernet` a `1.0.0`;
- retira el verificador específico de shadowing de `JWPLC_Ethernet`;
- conserva los markers Adafruit heredados;
- no modifica API pública, runtime, archives precompilados ni autoload.

Commit técnico:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

## Por qué se conservan los markers Adafruit

ST77xx, GFX y BusIO son dependencias externas vendorizadas/precompiladas. Es normal que existan otras versiones instaladas mediante Library Manager y Alpha5 ya registró una selección real de BusIO desde sketchbook durante un gate genérico.

## Compatibilidad

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
OPENPLC_AUTOLOAD_INTEGRATION=NO
OTA=NOT_DEFINED
```

Se mantienen integrados Display, Ethernet, microSD, FRAM, RTC, botonera, RS-485, Modbus RTU y TCA/I/O.

## Benchmark

Histórico:

| Configuración | Warm promedio | Delta |
|---|---:|---:|
| 0 markers JW/JWPLC | 22.094 s | base |
| Ethernet únicamente | 23.327 s | +5.6% |
| 4 markers | 26.888 s | +21.7% |
| 7 markers | 30.353 s | +37.4% |

Candidato final, tres réplicas:

```text
Basic cold = 15 compiladores
Core cold  = 78 compiladores
Warm       = 1 compilador
COMPILER_STRUCTURE_PARITY=PASS

Basic / 01_empty / managed_warm_touch:
r1 = 24.126 s
r2 = 21.860 s
r3 = 23.866 s
avg r1-r3 = 23.284 s
avg r2-r3 = 22.863 s
```

No se reclama una recuperación porcentual exacta por variación del host. El warm estabilizado vuelve al entorno de M0.

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

## Arduino IDE / físico

Se compiló y subió desde Arduino IDE el gate físico histórico del autoload normal.

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

```text
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
```

Ethernet y RS-485/Modbus no se someten a un nuevo stress de runtime porque Alpha10 no modifica esos subsistemas. Se conserva la evidencia física cerrada en Alpha6/Alpha7/Alpha9 y se revalida compilación en esta PR.

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

## CI

`CI JWPLC Package Smoke` quedó verde hasta `09ba7395450ce9d85a174dbd96a57f255371590c`.

El commit documental final de cierre debe quedar verde antes del merge.

## Gates técnicos

- [x] limpieza de source;
- [x] benchmark r1/r2/r3;
- [x] comparación Basic/Core;
- [x] estructura de compiladores;
- [x] paridad `BinaryBytes`;
- [x] matriz funcional local 5/5;
- [x] `undefined reference` = 0;
- [x] Arduino IDE compile/upload;
- [x] gate físico local;
- [x] app-only/bootloader/configuración final documentados;
- [x] documentación técnica en español;
- [ ] CI verde sobre commit documental final;
- [ ] merge a `release/v2.1.x`.

## Publicación

La publicación/tag Alpha10 interna anterior se reemplazará sólo después del merge de esta PR.

Después del merge:

1. retirar release/tag Alpha10 previo;
2. regenerar PreRelease `v2.1.0-alpha.10`;
3. registrar ZIP, tamaño y SHA-256 nuevos;
4. actualizar índice dev;
5. mantener índice estable sin cambios;
6. instalar/compilar/subir desde el package publicado;
7. documentar `CLOSED_PUBLISHED`.

```text
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_RELEASE_REPLACEMENT=PENDING_PR_MERGE
```
