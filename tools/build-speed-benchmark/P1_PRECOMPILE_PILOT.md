# Alpha4 P1 — piloto de librerías precompiladas

Fecha: 2026-08-09

## Objetivo

Reducir el `cold build` del JWPLC Basic sin retirar periféricos del autoload y sin alterar APIs públicas.

D1 ya redujo el `warm build` del sketch `01_empty` de ~36–40 s a ~14 s. P1 ataca un problema distinto: la recompilación desde fuente de librerías estables durante un build frío.

## Mecanismo

Arduino soporta `precompiled=full` en `library.properties`.

Con ese modo:

- si existe un archivo precompilado compatible en `src/{build.mcu}/lib*.a`, Arduino lo enlaza y evita recompilar los fuentes de esa librería;
- si no existe el binario compatible, Arduino vuelve automáticamente a los fuentes como fallback.

Para el ESP32 actual del JWPLC Basic, `{build.mcu}` es `esp32`, por lo que P1 genera:

```text
<library>/src/esp32/lib<Library>.a
```

El `platform.txt` JWPLC ya incluye `compiler.libraries.ldflags` en la receta de link, requisito para enlazar librerías precompiladas.

## Alcance P1 inicial

Se habilita `precompiled=full` únicamente para librerías cuyo código objeto puede compartirse de forma segura entre los perfiles actuales Basic/Core:

```text
JW_RTC
JW_FRAM
JW_SD
JW_MatrixButtons
JWPLC_ModbusRTU
```

## Librerías deliberadamente fuera de P1 inicial

Por estabilidad, todavía no se precompilan:

```text
JWPLC_GlobalPeripherals
JWPLC_Ethernet
JWPLC_RS485
JWPLC_Display
```

Motivo: contienen o consumen decisiones de hardware/perfil que pueden depender de macros del board (`JWPLC_HAS_*`, pines/defaults o configuración equivalente). El mecanismo estándar `src/esp32/lib*.a` selecciona por MCU, no por nombre de board, así que un único archivo podría terminar reutilizado tanto por Basic como por Basic Core.

Antes de precompilar ese grupo se debe:

1. demostrar que el objeto generado es idéntico entre perfiles; o
2. sacar del binario precompilado las decisiones dependientes del perfil; o
3. definir otro mecanismo seguro de selección.

No se aceptará simplemente compilarlas como Basic y reutilizarlas a ciegas en Core.

## Generador

Script:

```text
tools/build-speed-benchmark/Build-JWPLCPrecompiledLibraries.ps1
```

El script:

1. elimina sólo los `.a` P1 seleccionados si ya existen;
2. hace un build fuente limpio con `jwplc_local`;
3. toma los `.o` producidos por las recetas reales de Arduino CLI;
4. localiza el mismo `xtensa-esp32-elf-gcc-ar` usado por el package;
5. genera un `.a` por librería en `src/esp32/`;
6. hace un segundo build limpio;
7. verifica que las librerías P1 ya no se compilen desde fuente;
8. deja log y `P1_SUMMARY.md` con tiempos, tamaños y SHA-256.

No se recrean manualmente flags de compilación aproximados: los objetos salen del build real de Arduino CLI.

## Ejecución

Desde:

```powershell
cd C:\Users\jeykc\Documentos\GitHub\platform-jwplc
git switch v2.1.0-alpha.4/feature/build-speed-cache
git pull
cd tools\build-speed-benchmark
```

Ejecutar:

```powershell
.\Build-JWPLCPrecompiledLibraries.ps1
```

Si termina correctamente, ejecutar el benchmark P1:

```powershell
.\Run-JWPLCBuildBenchmark.ps1 -PackageNamespace jwplc_local -Targets Basic -Sketches 01_empty -RunLabel alpha4-precompile-p1 -SkipExplicitBuild -SkipUploads
```

## Baselines para comparar

Alpha3 publicada (`jwplc`):

```text
managed_cold          136.509 s
managed_warm_nochange  34.360 s
managed_warm_touch     34.638 s
```

D1 (`jwplc_local`):

```text
managed_cold          121.732 s
managed_warm_nochange  14.157 s
managed_warm_touch     14.074 s
```

La comparación principal P1 debe hacerse contra D1 usando el mismo namespace `jwplc_local`.

## Criterios P1

P1 debe cumplir simultáneamente:

- compilar `01_empty`;
- mantener tamaño de aplicación equivalente;
- mantener D1 (~14 s warm) o mejorarlo;
- reducir compilaciones reales del cold build;
- compilar `03_autoload_contract`;
- conservar fuentes como fallback;
- no romper Basic Core;
- validar hardware antes de publicar los `.a` como parte del package.

## Binarios generados

Los `.a` generados por el script quedan inicialmente **sin commit**.

Primero se mide P1. Sólo si el resultado es favorable se incorporarán como artefactos versionados del package y se validarán en Basic/Core y hardware físico.

## Pendientes posteriores

- [ ] Ejecutar generador P1.
- [ ] Benchmark `01_empty` P1.
- [ ] Verificar número de compilaciones cold.
- [ ] Smoke `03_autoload_contract`.
- [ ] Verificar Basic Core.
- [ ] Decidir si ampliar precompilación a librerías dependientes del perfil.
- [ ] Resolver reproducibilidad de Adafruit/Ethernet duplicadas.
- [ ] Medir upload full vs app-only.
