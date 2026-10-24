# Benchmark de compilación: 2.1.0-alpha.13 frente a 2.1.0-alpha.14 (24/10/2026)

```text
A14_BENCHMARK_NUBE=PASS             [ejecutado 24/10] tabla de abajo
A14_BENCHMARK_PC_JEFE=PASS          [ejecutado 24/10] WAR-MACHINE, 12 núcleos, arduino-cli 1.5.2-rc.1
```

## 1. Qué se mide

Los sketches de referencia de siempre (`tools/build-speed-benchmark/sketches/`):
`01_empty` (sólo el autoload del JWPLC) y `02_io_basic` (una entrada y una salida), en JWPLC Basic y Basic Core.

- **Frío**: caché de build aislada y vacía y build-path nuevo (primer Compilar tras instalar o cambiar de placa).
- **Caliente**: misma caché y build-path; sólo cambia la fecha del `.ino` (Compilar otra vez en el IDE).

Las dos versiones se alternan en cada repetición y se reporta la mediana.

## 2. Procedimiento

### PC del jefe (Windows, el que vale para las notas)

1. Arduino IDE / arduino-cli **1.5.1**; alpha.13 instalada desde Boards Manager (`jwplc:esp32@2.1.0-alpha.13`).
2. `jwplc_local` (junction en Arduino15) apuntando a un checkout de `v2.1.0-alpha.14/HmiDesigner/main`.
3. Sketchbook sin copias de librerías JWPLC (las de `JW_RS485`/`JW_ModbusRTU` del Laundry HMI también tapan al package:
   moverlas fuera mientras dura la medida).
4. Cerrar el Arduino IDE (comparte la caché) y correr:

   ```powershell
   cd tools\build-speed-benchmark
   .\Compare-JWPLCBuildBenchmark.ps1 -ArduinoCli "C:\Program Files\Arduino CLI\arduino-cli.exe" `
       -BaseNamespace jwplc -HeadNamespace jwplc_local -Targets Basic,Core -Repetitions 3
   ```

5. Pegar `results\compare_<fecha>\COMPARE.md` en §3 y la fila resumen en `PRE_RELEASE.md`.

El script llama a `Run-JWPLCBuildBenchmark.ps1` (fases `managed_*`, sin subida) para cada versión y repetición.

### Linux: nube, WSL o CI

```bash
tools/alpha15/regenerate_libJWPLC_TFT.sh setup          # una vez: arduino-cli + tools del package
tools/build-speed-benchmark/compare_build_times.sh --reps 3 --targets "basic core"
# opciones: --base-ref <ref git> --base-label <txt> --head-label <txt> --sketches "<lista>" --out <dir>
```

Arma la base desde un ref de git (por defecto `origin/release/v2.1.x`, la alpha.13 publicada) en un worktree temporal
y la candidata desde el árbol de trabajo; las enlaza como `jwplc_base`/`jwplc_head` en un sketchbook vacío propio.
Sirve igual en un runner de GitHub Actions (`ubuntu-latest`) con esas dos líneas; no se dejó un workflow fijo para no
sumar CI que corra en cada push.

## 3. Resultados

### Nube `[ejecutado 24/10]`

Contenedor Linux de 4 núcleos, arduino-cli 1.5.2-rc.1 (el que instala el setup), base `22e7531` (release alpha.13),
candidata = `JWPLC/` de `9a27b13`. 3 repeticiones, mediana, 0 fallos.

| Placa | Sketch | Frío alpha.13 | Frío alpha.14 | Caliente alpha.13 | Caliente alpha.14 | Flash alpha.13 | Flash alpha.14 |
|---|---|---|---|---|---|---|---|
| Basic | 01_empty | 30,3 s | 38,9 s | 4,9 s | 4,4 s | 415 805 B | 422 753 B |
| Basic | 02_io_basic | 29,0 s | 36,0 s | 4,3 s | 4,8 s | 415 913 B | 422 861 B |
| Core | 01_empty | 38,4 s | 45,9 s | 4,3 s | 5,1 s | 373 779 B | 380 727 B |
| Core | 02_io_basic | 38,4 s | 46,0 s | 4,7 s | 4,6 s | 373 883 B | 380 831 B |

Lectura:

- **Caliente: sin cambios** (4–5 s en ambas; las diferencias están dentro del ruido del contenedor, ±0,5 s).
- **Frío: +7 a +8 s en alpha.14.** La única diferencia en la lista de librerías precompiladas que usa el build es
  `JWPLC_ModbusRTU`: en alpha.13 estaba `precompiled=full` y en alpha.14 va desde la fuente `[visto en los logs]`,
  junto con `JW_ModbusRTU` y `JW_RS485`, que ahora envuelve y tampoco están precompiladas `[visto en código]`. Es coherente con los +24 s medidos en la PC
  al pasarla a fuente (traspaso §2) `[inferido]`.
- Flash +6 948 B por los envoltorios RS-485/Modbus RTU.
- El ruido entre repeticiones del contenedor llega a ±5 s en frío; los números absolutos no se comparan con la PC.

Si se quiere recuperar el tiempo en frío, la vía es volver a precompilar `JWPLC_ModbusRTU` (y sus `JW_*`) en una
alpha posterior, con su `.a` y chequeo ABI; no se hace en esta.

### PC del jefe `[ejecutado 24/10]`

`Compare-JWPLCBuildBenchmark.ps1`, equipo WAR-MACHINE (12 núcleos lógicos), arduino-cli 1.5.2-rc.1, base `jwplc`
(2.1.0-alpha.13 instalada), candidata `jwplc_local` (`v2.1.0-alpha.14/HmiDesigner/main`). 3 repeticiones, mediana,
0 fallos.

| Placa | Sketch | Frío alpha.13 | Frío alpha.14 | Caliente alpha.13 | Caliente alpha.14 |
|---|---|---|---|---|---|
| Basic | 01_empty | 40,4 s | 48,3 s | 11,0 s | 10,0 s |
| Basic | 02_io_basic | 40,9 s | 47,9 s | 9,9 s | 10,2 s |
| Core | 01_empty | 49,5 s | 60,4 s | 9,3 s | 10,0 s |
| Core | 02_io_basic | 48,1 s | 60,6 s | 10,3 s | 10,6 s |

Lectura:

- **Caliente: sin cambios** (9–11 s en ambas versiones; diferencias de ±1 s).
- **Frío: +7 a +8 s en Basic (+18 %) y +11 a +12,5 s en Core (+23 a +26 %)**, la misma tendencia que en la nube:
  `JWPLC_ModbusRTU` y sus `JW_ModbusRTU`/`JW_RS485` se compilan desde la fuente en la primera compilación `[inferido]`.
- Frente a la referencia del traspaso (alpha.13 con arduino-cli 1.5.1: 41,5 s frío, 9,0 s caliente), la alpha.13
  medida hoy da 40,4 s y 9,9–11 s: el mismo orden, con otra versión de arduino-cli y otra carga del equipo.
