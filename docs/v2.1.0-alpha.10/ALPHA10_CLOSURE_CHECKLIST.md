# Alpha10 - Checklist de cierre

Fecha de actualización: 2026-09-05.

## Base y alcance

- [x] Rama creada desde `release/v2.1.x`.
- [x] Alpha10 previo identificado como release interno a reemplazar.
- [x] Alcance redefinido a limpieza de library discovery / recuperación de build speed.
- [x] Modelo soportado definido como `PACKAGE_MANAGED` para librerías JW/JWPLC.
- [x] No se retiran periféricos del autoload.

## Cambio técnico

- [x] Retirado `JWPLC_Bundled_JWPLC_Ethernet.h`.
- [x] `JWPLC_GlobalPeripherals_Auto.h` restaurado al comportamiento de Alpha9.
- [x] `JWPLC_Ethernet` restaurada a `1.0.0`.
- [x] Eliminado el verificador específico de shadowing de `JWPLC_Ethernet`.
- [x] API pública preservada.
- [x] Runtime preservado.
- [x] Archives precompilados preservados.

```text
TECHNICAL_COMMIT_SHA=35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

## Auditoría de protecciones

- [x] Guard JWPLC_Ethernet evaluado y retirado.
- [x] `JWPLC_Bundled_Adafruit_ST77xx.h` se conserva.
- [x] `JWPLC_Bundled_Adafruit_GFX.h` se conserva.
- [x] `JWPLC_Bundled_Adafruit_BusIO.h` se conserva.
- [x] `JWPLC_LIBRARY_DISCOVERY_PHASE` se conserva.
- [x] Motivo de cada decisión documentado.

## Benchmark

- [x] Evidencia histórica M0/M1/M4/M7 conservada.
- [x] Candidato r1 ejecutado.
- [x] Candidato r2 ejecutado.
- [x] Candidato r3 ejecutado.
- [x] Basic `01_empty` comparado.
- [x] Core `01_empty` comparado.
- [x] Basic/Core `02_io_basic` comparado.
- [x] Cold evaluado.
- [x] Warm no-change evaluado.
- [x] Warm touch evaluado.
- [x] Compiler invocations comparadas.
- [x] `BinaryBytes` comparados.
- [x] Paridad binaria exacta entre r1/r2/r3.
- [x] Tabla final de tiempos completada.
- [x] Conclusión de build speed documentada sin reclamar porcentaje exacto por variación del host.

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

## Matriz funcional

- [x] `DigitalIO_Basic` compila localmente.
- [x] `Buttons_Basic` compila localmente.
- [x] `Display_HMI_Fields` compila localmente.
- [x] `Ethernet_Diagnostics` compila localmente.
- [x] `RemoteIO_Slave_RTU` compila localmente.
- [x] Matriz local final = 5/5 PASS.
- [x] Undefined references = 0.
- [x] Arduino IDE compila y sube el candidato local.

## Validación física

Gate reutilizado:

```text
tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate/06_alpha4_local_physical_gate.ino
```

- [x] Boot y autoload normal.
- [x] TFT listo.
- [x] RTC operativo.
- [x] FRAM operativo.
- [x] microSD write/read/verify/remove.
- [x] Botonera 6/6.
- [x] Entradas digitales 8/8.
- [x] Salidas/relés 8/8.
- [x] Confirmación visual TFT.
- [x] Gate físico local final = PASS.
- [x] No se observaron congelamientos ni resets inesperados durante el smoke.

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

### Ethernet / RS-485 / Modbus

- [x] `Ethernet_Diagnostics` compile regression = PASS.
- [x] `RemoteIO_Slave_RTU` compile regression = PASS.
- [x] Ethernet runtime no fue modificado en Alpha10.
- [x] RS-485/Modbus RTU runtime no fue modificado en Alpha10.
- [x] Se conserva evidencia física Ethernet/SPI cerrada en Alpha6/Alpha7.
- [x] Se conserva evidencia física RS-485/Modbus/Remote I/O cerrada en Alpha7/Alpha9.
- [x] No se repite stress de 10 minutos por no existir cambio de runtime/SPI en Alpha10.

```text
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
```

## Arquitectura y decisiones

- [x] Display permanece en autoload.
- [x] Ethernet permanece en autoload.
- [x] microSD permanece en autoload.
- [x] FRAM permanece en autoload.
- [x] RTC permanece en autoload.
- [x] Botonera permanece en autoload.
- [x] RS-485 permanece en autoload.
- [x] Modbus RTU permanece en autoload.
- [x] TCA/I/O permanece integrado.
- [x] OpenPLC no se declara integrado al runtime Arduino.
- [x] OTA no se asume definido.
- [x] `bootloader.bin` no se publica como definitivo.
- [x] Configuración Flash universal final sigue pendiente.
- [x] App-only continúa como herramienta de desarrollo, no upload por defecto.
- [x] Bootloader precompilado continúa no adoptado.

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

- [x] `CI JWPLC Package Smoke` verde sobre HEAD benchmarkeado.
- [x] CI verde sobre `c696034fd2c1f1dafbeb33fcf41c06be6a8f05f1`.
- [x] CI verde sobre `09ba7395450ce9d85a174dbd96a57f255371590c`.
- [ ] El commit documental de cierre debe quedar verde antes del merge.

## Documentación

- [x] Auditoría de protecciones.
- [x] Benchmark con r1/r2/r3 y BinaryBytes.
- [x] Cierre técnico.
- [x] Validación Arduino IDE/física documentada.
- [x] Checklist actualizado.
- [x] Handoff actualizado para publicación.
- [x] PR en español.
- [x] PreRelease en español.
- [ ] README raíz final se actualiza junto con el artefacto/SHA publicado.
- [ ] Documentos de transferencia del proyecto se actualizan al cerrar publicación.

## Publicación de reemplazo

- [x] Cierre técnico local = PASS.
- [ ] CI verde sobre HEAD documental final.
- [ ] PR #90 lista para review/merge.
- [ ] Release/tag Alpha10 previo retirado después del merge aprobado.
- [ ] PR integrada a `release/v2.1.x`.
- [ ] PreRelease `v2.1.0-alpha.10` regenerada.
- [ ] ZIP nuevo generado.
- [ ] SHA-256 nuevo registrado.
- [ ] Tamaño nuevo registrado.
- [ ] Índice dev actualizado al artefacto nuevo.
- [ ] Índice estable sin cambios.
- [ ] Instalación aislada desde índice publicado.
- [ ] Compilación desde package publicado.
- [ ] Upload físico desde package publicado.
- [ ] Arranque post-upload.
- [ ] README/documentos de transferencia finales.
- [ ] Estado `CLOSED_PUBLISHED`.

## Estado actual

```text
ALPHA10_CLEANUP_SOURCE=PASS
ALPHA10_BENCHMARK_RUNS=3_PASS
ALPHA10_COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_WARM_BEHAVIOR=PASS_WITH_HOST_VARIATION
ALPHA10_BINARY_SIZE_PARITY=PASS
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=5/5_PASS
ALPHA10_LOCAL_COMPILE_GATE=PASS
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
ALPHA10_TECHNICAL_CLOSURE=PASS
ALPHA10_PUBLICATION_REPLACEMENT=PENDING
NEXT_ALPHA=BLOCKED_UNTIL_ALPHA10_PUBLISHED
```
