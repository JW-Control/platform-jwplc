# Alpha10 - Checklist de cierre

Fecha de cierre: 2026-09-05.

## Base y alcance

- [x] Alpha10 redefinido como limpieza de library discovery y recuperación de build speed.
- [x] Modelo soportado: `PACKAGE_MANAGED` para librerías JW/JWPLC.
- [x] Overrides manuales JW/JWPLC en sketchbook declarados fuera del flujo soportado.
- [x] No se retiran periféricos del autoload.
- [x] No se modifican APIs públicas ni runtimes por este cambio.

## Cambio técnico

- [x] Retirado `JWPLC_Bundled_JWPLC_Ethernet.h`.
- [x] `JWPLC_GlobalPeripherals_Auto.h` restaurado al comportamiento de Alpha9.
- [x] `JWPLC_Ethernet` restaurada a `1.0.0`.
- [x] Eliminado el verificador específico de shadowing de `JWPLC_Ethernet`.
- [x] Archives precompilados preservados.
- [x] Markers Adafruit ST77xx/GFX/BusIO conservados.

```text
TECHNICAL_COMMIT_SHA=35385c7286c8a4fdf33aec1af1175b8bb4f45e64
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

## Benchmark y paridad

- [x] Tres réplicas completas ejecutadas.
- [x] Cold, warm y warm-touch evaluados.
- [x] Tabla final documentada en `ALPHA10_BUILD_BENCHMARK.md`.
- [x] Estructura de compilación preservada.
- [x] BinaryBytes idénticos entre réplicas.
- [x] No se reclama porcentaje exacto de recuperación por variación del host.

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
historical M0 = 22.094 s
historical M1 = 23.327 s

Basic / 01_empty    BinaryBytes = 4618688 x3
Basic / 02_io_basic BinaryBytes = 4618784 x3
Core  / 01_empty    BinaryBytes = 4574464 x3
Core  / 02_io_basic BinaryBytes = 4574576 x3
ALPHA10_BINARY_SIZE_PARITY=PASS
```

## Matriz funcional y Arduino IDE

- [x] `DigitalIO_Basic` = PASS.
- [x] `Buttons_Basic` = PASS.
- [x] `Display_HMI_Fields` = PASS.
- [x] `Ethernet_Diagnostics` = PASS.
- [x] `RemoteIO_Slave_RTU` = PASS.
- [x] Undefined references = 0.
- [x] Arduino IDE compila y sube el candidato local.

```text
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_LOCAL_COMPILE_GATE=PASS
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
```

## Validación física

- [x] TFT listo.
- [x] RTC operativo.
- [x] FRAM operativo.
- [x] microSD operativo.
- [x] Botonera 6/6.
- [x] Entradas digitales 8/8.
- [x] Salidas/relés 8/8.
- [x] Confirmación visual TFT.

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
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
```

Ethernet y RS-485/Modbus RTU no fueron sometidos a un nuevo stress de runtime porque Alpha10 no modifica esos runtimes. Se conserva la evidencia física cerrada en Alpha6/Alpha7/Alpha9 y se revalidó compilación mediante los ejemplos correspondientes.

## Empaquetado y publicación

- [x] Primer ZIP con `archive_root_mode=contents` identificado como inválido para Boards Manager.
- [x] Publicación inválida descartada.
- [x] `archive_root_mode=folder` fijado como política del release.
- [x] Workflow manual simplificado a un único input editable: `version`.
- [x] Validación de raíz única + `boards.txt` + `platform.txt` añadida.
- [x] Workflow final #19 = SUCCESS.
- [x] PreRelease `v2.1.0-alpha.10` regenerada.
- [x] ZIP final con raíz única `2.1.0/`.
- [x] SHA-256 y tamaño final registrados.
- [x] PR #92 del índice dev integrada a `main`.
- [x] Índice estable permanece en `2.0.0`.
- [x] Índice dev final sincronizado en `main` y `release/v2.1.x`.
- [x] Instalación exacta desde package publicado = PASS.
- [x] Compilación desde package publicado = PASS.
- [x] Upload físico desde package publicado = PASS.
- [x] Gate físico post-upload desde package publicado = PASS.

```text
PUBLISHED_PACKAGE_SOURCE_SHA=f365738e8b0903bca9f93f5c42dfee8310e074b2
ZIP=jwplc-esp32-2.1.0-alpha.10.zip
SIZE=24464282
SHA256=5ca5a71d6de0ddd25c81442d7ea4f840ad48603dd024afcd2925235dc4d1b0bf
PACKAGE_ROOT=2.1.0/
ALPHA10_PUBLISHED_INSTALL=PASS
ALPHA10_PUBLISHED_COMPILE=PASS
ALPHA10_PUBLISHED_UPLOAD=PASS
ALPHA10_PUBLISHED_RUNTIME=PASS
```

## README y documentación

- [x] README raíz revisado de forma general.
- [x] Distribución reorganizada tomando Alpha1 como referencia.
- [x] `package_jwplc_index_dev.json` y `package_jwplc_index.json` movidos a la primera sección útil.
- [x] Eliminadas referencias obsoletas que presentaban `JWPLC_Bundled_JWPLC_Ethernet.h` como protección vigente.
- [x] Lista de librerías JW/JWPLC actualizada según `JWPLC/2.1.0/libraries/`.
- [x] Modelo `PACKAGE_MANAGED` documentado para usuarios y talleres.
- [x] PreRelease final actualizada con ZIP/SHA/tamaño reales.
- [x] Handoff Alpha10 -> Alpha11 actualizado.
- [x] PR #93 de sincronización final a `main` integrada mediante Squash and merge.

## Decisiones de configuración

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

## Sincronización de ramas

El sync final se realizó mediante un branch construido directamente desde `main` y un commit cuyo árbol era el árbol final de `release/v2.1.x`. Esto evitó mezclar historiales divergentes.

- [x] PR #93 integrada con Squash and merge.
- [x] `main` quedó con tree SHA `7d0358741d525445630814894dde6ea2cab37dbd` inmediatamente después del sync.
- [x] Ese tree SHA coincidía exactamente con `release/v2.1.x` en el momento del merge.
- [x] La ancestría literal no se usa como criterio de paridad debido a squash merges históricos.

Los commits documentales de cierre posteriores deben mantenerse idénticos en ambas ramas para conservar la paridad de contenido.

## Estado final

```text
ALPHA10_PROTECTION_AUDIT=PASS
ALPHA10_BUILD_BENCHMARK=PASS_WITH_HOST_VARIATION
ALPHA10_BINARY_SIZE_PARITY=PASS
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PUBLISHED_INSTALL=PASS
ALPHA10_PUBLISHED_COMPILE=PASS
ALPHA10_PUBLISHED_UPLOAD=PASS
ALPHA10_PUBLISHED_RUNTIME=PASS
ALPHA10_RELEASE_PUBLICATION=PASS
ALPHA10_DEV_INDEX=PASS
ALPHA10_STABLE_INDEX_UNCHANGED=PASS
ALPHA10_RELEASE_MAIN_TREE_PARITY=PASS
ALPHA10_STATUS=CLOSED_PUBLISHED
NEXT=ALPHA11
```
