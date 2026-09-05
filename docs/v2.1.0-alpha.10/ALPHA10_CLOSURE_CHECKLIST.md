# Alpha10 - Checklist de cierre reabierto

## Base y alcance

- [x] Rama nueva creada desde `release/v2.1.x`.
- [x] Alpha10 previo identificado como release interno a reemplazar después de validar el candidato.
- [x] Alcance redefinido a limpieza de library discovery / recuperación de build speed.
- [x] Modelo soportado definido como `PACKAGE_MANAGED` para librerías JW/JWPLC.
- [x] No se retiran periféricos del autoload.

## Cambio técnico

- [x] Retirado `JWPLC_Bundled_JWPLC_Ethernet.h`.
- [x] `JWPLC_GlobalPeripherals_Auto.h` restaurado al comportamiento de Alpha9.
- [x] `JWPLC_Ethernet` restaurada a `1.0.0`.
- [x] Eliminado el verificador que exigía ignorar `JWPLC_Ethernet` del sketchbook.
- [x] API pública preservada.
- [x] Runtime preservado.
- [x] Archives precompilados preservados.

```text
TECHNICAL_COMMIT_SHA=35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

## Auditoría de protecciones

- [x] Guard JWPLC_Ethernet evaluado y retirado.
- [x] Markers Adafruit evaluados.
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
- [ ] Tamaño binario comparado desde `results.csv`.
- [x] Conclusión provisional de build speed documentada.
- [x] No se reclama un porcentaje exacto de recuperación por variación de host.

Estructura observada:

```text
Basic cold = 15 compiladores
Core cold  = 78 compiladores
Warm       = 1 compilador
COMPILER_STRUCTURE_PARITY=PASS
```

Referencia de warm touch Basic `01_empty`:

```text
r1 = 24.126 s
r2 = 21.860 s
r3 = 23.866 s
avg r1-r3 = 23.284 s
avg r2-r3 = 22.863 s
historical M0 = 22.094 s
historical M1 = 23.327 s
```

## Matriz funcional

- [ ] `DigitalIO_Basic` compila localmente.
- [ ] `Buttons_Basic` compila localmente.
- [ ] `Display_HMI_Fields` compila localmente.
- [ ] `Ethernet_Diagnostics` compila localmente.
- [ ] `RemoteIO_Slave_RTU` compila localmente.
- [ ] Matriz local final = 5/5 PASS.
- [ ] Undefined references local = 0.
- [ ] Arduino IDE compila con package local candidato.
- [x] CI `JWPLC Package Smoke` de PR #90 = SUCCESS para `456d5b9f55088091fcadcb87e9f33ffb90d3754c`.

## Validación física

- [ ] Boot y autoload normal.
- [ ] TFT/IDLE operativo.
- [ ] Botonera operativa.
- [ ] RTC visible/avanzando.
- [ ] Ethernet W5500 operativo.
- [ ] microSD/FRAM sin regresión observable en gate elegido.
- [ ] RS-485/Modbus RTU sin regresión observable en gate elegido.
- [ ] Sin congelamientos ni resets inesperados durante el smoke.

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
- [x] Configuración Flash universal sigue pendiente.
- [x] App-only continúa como herramienta de desarrollo, no upload por defecto.
- [x] Bootloader precompilado continúa no adoptado.
- [x] Conclusión de app-only revalidada: `VALIDATED_DEVELOPMENT_TOOL`, no upload por defecto.
- [x] Conclusión de bootloader precompilado revalidada: `NOT_ADOPTED`; generación SDK/ELF automática.
- [x] Configuración final marcada explícitamente: perfil actual validado; configuración Flash universal final `PENDING`.

## Documentación

- [x] Auditoría de protecciones creada.
- [x] Benchmark reabierto y procedimiento actualizado.
- [x] Resultados r1/r2/r3 incorporados.
- [x] Cierre técnico reabierto y actualizado.
- [x] Checklist actualizado.
- [x] Handoff actualizado.
- [x] PR candidata redactada en español.
- [x] PreRelease candidata redactada en español.
- [x] Tabla de tiempos del benchmark local completada.
- [ ] Tabla de `BinaryBytes` completada.
- [ ] README raíz actualizado después de los resultados finales.
- [ ] Documentos de transferencia del proyecto actualizados después del cierre.

## Publicación de reemplazo

- [ ] PR técnica lista para review después de todos los gates locales/físicos.
- [x] CI verde para el estado benchmarkeado `456d5b9f55088091fcadcb87e9f33ffb90d3754c`.
- [ ] CI verde para el HEAD documental final antes del merge.
- [ ] Release/tag Alpha10 previo retirado sólo después de aprobar el candidato.
- [ ] PR integrada a `release/v2.1.x`.
- [ ] PreRelease `v2.1.0-alpha.10` regenerada.
- [ ] ZIP nuevo generado.
- [ ] SHA-256 nuevo registrado.
- [ ] Tamaño nuevo registrado.
- [ ] Índice dev actualizado al artefacto nuevo.
- [ ] Índice estable sin cambios.
- [ ] Instalación aislada desde índice publicado.
- [ ] Compilación aislada.
- [ ] Upload físico desde package publicado.
- [ ] Arranque post-upload.
- [ ] Cierre de publicación documentado.

## Estado actual

```text
ALPHA10_CLEANUP_SOURCE=PASS
ALPHA10_BENCHMARK_RUNS=3_PASS
ALPHA10_COMPILER_STRUCTURE_PARITY=PASS
ALPHA10_WARM_BEHAVIOR=PASS_WITH_HOST_VARIATION
ALPHA10_CI_PR90=PASS
ALPHA10_BINARY_SIZE_CHECK=PENDING
ALPHA10_LOCAL_FUNCTIONAL_MATRIX=PENDING
ALPHA10_PHYSICAL_VALIDATION=PENDING
ALPHA10_TECHNICAL_CLOSURE=PENDING
ALPHA10_PUBLICATION_REPLACEMENT=PENDING
NEXT_ALPHA=BLOCKED
```
