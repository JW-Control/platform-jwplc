# Alpha5 - reproducibilidad del core precompilado

Fecha: 2026-08-24

## Objetivo

Determinar si `JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a` puede regenerarse desde la fuente canonica `JWPLC/2.1.0/cores/jwcontrol` y definir que criterio de reproducibilidad es valido para Alpha5.

## Resultado del generador normalizado

El gate ejecutado con `Build-JWPLCPrecompiledCore.ps1` confirmo:

- build fuente `jwplc_local:esp32:jwplcbasic`: PASS;
- `Using core 'jwcontrol'`: confirmado;
- `compile_commands.json`: 72 entradas;
- TUs de `cores/jwcontrol`: 64;
- TUs del stub precompilado: 0;
- `peripherals_init.cpp`: compilado una vez;
- `boards.local.txt`: restaurado correctamente;
- archive candidato: 3,011,212 bytes.

El archive versionado validado antes del gate tenia SHA-256:

`7c2c0149fc19e5f363a46d49c9a805db268b2ab72ae0a4ebf2b679e781a2d669`

La regeneracion produjo un archive del mismo tamaño con SHA-256:

`ec3398aa36e8573f8fac0dd58349660910fac5a2200fa5532a886cc2e0e6a8a9`

## Diagnostico miembro por miembro

Se compararon ambos `core.a` con `xtensa-esp32-elf-gcc-ar`.

Resultado:

- miembros HEAD: 64;
- miembros regenerados: 64;
- diferencias de nombres u orden: 0;
- miembros duplicados: 0;
- diferencias de metadata reportada por `ar tv`: 0;
- objetos binariamente identicos: 62/64;
- objetos diferentes: 2/64.

Los dos objetos diferentes fueron exclusivamente:

1. `chip-debug-report.cpp.o`
2. `firmware_msc_fat.c.o`

## Causa de las dos diferencias

### `chip-debug-report.cpp.o`

`chip-debug-report.cpp` usa `__DATE__` y `__TIME__` para reportar la fecha y hora reales de compilacion dentro de `printSoftwareInfo()`.

Por ello, recompilar el mismo source tree en otro instante produce deliberadamente bytes distintos en ese TU.

### `firmware_msc_fat.c.o`

`firmware_msc_fat.c` usa `__DATE__` y `__TIME__` para construir la fecha/hora de las entradas FAT generadas por el soporte Firmware MSC.

Por ello, este TU tambien cambia deliberadamente entre compilaciones realizadas en instantes distintos.

## Decision Alpha5

No se modificaran ni congelaran `__DATE__`/`__TIME__` con el unico objetivo de obtener un SHA-256 bit-a-bit repetible.

Hacerlo cambiaria semantica util del core y no aporta una mejora funcional ni de compatibilidad Arduino.

Tampoco se considera que `compiler.ar.flags` sea la causa observada: `ar tv` no mostro diferencias de metadata entre ambos archives.

La reproducibilidad valida para Alpha5 queda definida asi:

- `CORE_ARCHIVE_STRUCTURE_REPRODUCIBLE=PASS`
- `CORE_OBJECTS_DETERMINISTIC=62/64`
- `CORE_TIME_DEPENDENT_OBJECTS=2/64_EXPECTED`
- `CORE_SEMANTIC_REPRODUCIBILITY=PASS`
- `CORE_BIT_FOR_BIT_REPRODUCIBILITY=N/A_BY_DESIGN`

Las dos excepciones temporales permitidas son exactamente:

- `chip-debug-report.cpp.o`
- `firmware_msc_fat.c.o`

Si una futura regeneracion muestra un tercer objeto diferente, el resultado debe volver a `REVIEW` hasta identificar la causa.

## Consecuencia para el verificador

`Verify-JWPLCPrecompiledCore.ps1` deja de usar la equivalencia binaria de una app fuente previa como criterio principal.

El gate normalizado valida directamente la arquitectura de build:

### Basic normal

Debe cumplir:

- `Using core 'jwcontrol_precompiled_stub'`;
- 0 TUs de `cores/jwcontrol`;
- exactamente 1 TU de `cores/jwcontrol_precompiled_stub`;
- el TU debe ser `precompiled_core_stub.c`;
- el enlace debe incluir `precompiled/core/JWPLCBASIC/core.a`;
- debe producir una app valida.

### Basic Core

Actua como control inverso y debe cumplir:

- `Using core 'jwcontrol'`;
- TUs de `cores/jwcontrol` > 0;
- 0 TUs del stub;
- `peripherals_init.cpp` compilado exactamente una vez;
- no debe enlazar `precompiled/core/JWPLCBASIC/core.a`;
- debe producir una app valida.

El parametro historico `-ReferenceRunPath` se conserva por compatibilidad del script, pero ya no participa del gate Alpha5.

## Resultado de los gates normalizados

### Basic normal - PASS

El gate con `jwplc_local:esp32:jwplcbasic` confirmo:

- tiempo: 67.281 s;
- `compile_commands.json`: 9 entradas;
- TUs de `cores/jwcontrol`: 0;
- TUs de `cores/jwcontrol_precompiled_stub`: 1;
- `precompiled_core_stub.c`: 1;
- `Using core 'jwcontrol_precompiled_stub'`: true;
- enlace de `precompiled/core/JWPLCBASIC/core.a`: true;
- SHA-256 del archive antes y despues del gate: `7c2c0149fc19e5f363a46d49c9a805db268b2ab72ae0a4ebf2b679e781a2d669`;
- `CORE_PRECOMPILED_VERIFY_BASIC=PASS`.

La primera ejecucion del verificador detecto un bug de tooling al enlazar un array de output verbose que contenia lineas vacias. El fix se limito a permitir elementos vacios en el parametro interno del helper; el criterio del gate no se relajo.

### Basic Core - PASS

El control inverso con `jwplc_local:esp32:jwplcbasiccore` confirmo:

- tiempo: 66.530 s;
- `compile_commands.json`: 72 entradas;
- TUs de `cores/jwcontrol`: 64;
- TUs del stub: 0;
- `Using core 'jwcontrol'`: true;
- `Using core 'jwcontrol_precompiled_stub'`: false;
- `peripherals_init.cpp`: 1;
- enlace de `precompiled/core/JWPLCBASIC/core.a`: false;
- app generada: 339,888 bytes;
- SHA-256 del archive versionado permanecio sin cambios;
- `boards.local.txt`: ausente antes y despues del gate;
- `CORE_PRECOMPILED_VERIFY_CORE=PASS`.

## Cierre de arquitectura Alpha5

Con ambos sentidos verificados, queda cerrada la arquitectura:

```text
cores/jwcontrol
      |
      | Build-JWPLCPrecompiledCore.ps1
      v
precompiled/core/JWPLCBASIC/core.a
      |
      | target normal jwplcbasic
      v
cores/jwcontrol_precompiled_stub
```

El target `jwplcbasiccore` mantiene el camino fuente directo sobre `cores/jwcontrol` y actua como control inverso.

Estado:

- `ALPHA5_CORE_PRECOMPILED_BASIC_GATE=PASS`
- `ALPHA5_CORE_SOURCE_CONTROL=PASS`
- `ALPHA5_CORE_PRECOMPILED_ARCHITECTURE=PASS`

No se requieren mas cambios de arquitectura del core en Alpha5 salvo que aparezca una regresion nueva.

## Evidencia local

Logs de diagnostico y gates generados durante la validacion:

- `tools/build-speed-benchmark/results/manual-logs/20260824_022959_31_precompiled_core_generator.log`
- `tools/build-speed-benchmark/results/manual-logs/20260824_095146_34_core_precompiled_verify_basic_binder_fix.log`
- `tools/build-speed-benchmark/results/manual-logs/20260824_100150_35_core_precompiled_verify_source_core.log`
- archive regenerado preservado localmente como `20260824_023000_31_JWPLCBASIC_generated_core_ec3398.a` durante el diagnostico.

Los artefactos de `manual-logs` son evidencia local de validacion y no se consideran parte obligatoria del package publicado.
