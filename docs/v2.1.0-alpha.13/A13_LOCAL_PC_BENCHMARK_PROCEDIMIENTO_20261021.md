# Alpha13 — Benchmark FINAL en la PC del operador (Windows)

**Estado final (21/10/2026):** `LOCAL_PC_BENCHMARK=PASS_LOCAL_PC_THREE_PAIRS_TWO_FQBN`. La campaña fue ejecutada por el operador en su **PC Windows** y el `MANIFEST.json` fue conservado en [evidencia versionada](evidence/A13_LOCAL_PC_BENCHMARK_MANIFEST_20261021.json); resultados, muestras y limitaciones en [informe de cierre](A13_LOCAL_PC_BENCHMARK_RESULTADOS_20261021.md). Medianas frías: Basic **52,120 → 40,190 s (−22,89 %)** y Basic Core **54,964 → 47,958 s (−12,75 %)**. El comparativo CI sigue siendo independiente.

## Qué mediremos

Se compara **el mismo sketch consumidor G10P_COM4_Master** en cuatro escenarios:

| Perfil | Variante de bibliotecas | Repeticiones |
|---|---|---|
| JWPLC Basic | Ethernet + Modbus TCP **fuentes** (`.cpp`) | 3 frías + 3 calientes |
| JWPLC Basic | Ethernet + Modbus TCP **precompiladas** (`.a`) | 3 frías + 3 calientes |
| JWPLC Basic Core | Ethernet + Modbus TCP **fuentes** (`.cpp`) | 3 frías + 3 calientes |
| JWPLC Basic Core | Ethernet + Modbus TCP **precompiladas** (`.a`) | 3 frías + 3 calientes |

**Definiciones:** compilación fría = primera compilación sobre un build-path individual limpiado; caliente = recompilar inmediatamente el mismo sketch sin cambios sobre ese build-path. El runner compara medianas, incluye ELF y prueba la procedencia de biblioteca (`runtime.platform.path`, presencia de `.cpp.o` en source y ausencia en precompiled). Estos tiempos son de **compilación**, NO miden tiempo de upload, ni demuestran mejora del arranque del PLC.

Para comparar únicamente el cambio Alpha13 F137, el resto de las librerías/autoload queda idéntico en ambos escenarios. El runner nunca elimina los `.a` originales: solo los quita de una **copia temporal** source-first y mantiene los reales en la copia precompiled.

## Preparar el entorno una sola vez

1. En Windows, entra a tu clon de `JW-Control/platform-jwplc`. Guarda cualquier trabajo sin commit antes de cambiar de rama; el runner se negará a empezar con archivos rastreados modificados.
2. Descarga la **rama del PR #126**. Mientras siga Draft, sus archivos no están en BASE. Desde Git Bash/PowerShell:

   ```powershell
   git fetch origin
   git switch --track origin/codex/alpha13-c-global-release-like-20261021
   git pull --ff-only
   git status --short
   ```

   Si ya tienes esa rama local, sustituye el comando `git switch --track ...` por `git switch codex/alpha13-c-global-release-like-20261021`. No mezclar ni fusionar BASE.
3. Confirma `arduino-cli version` (objetivo **1.5.1**) y `arduino-cli core list`. Debe estar disponible el toolchain `jwplc:esp32` utilizado por Alpha13; si no se encuentra, el benchmark se detendrá y **no** instalará ni actualizará nada por su cuenta.
4. Ten Python 3 disponible como `py -3` o `python`. No conectes placas para este benchmark; no hace falta enchufar COM4/COM14.

## Ejecución

Desde el directorio raíz del repositorio, ejecuta:

```powershell
.\tools\alpha13\RUN_ALPHA13_LOCAL_PC_BENCHMARK.bat
```

Primero se verifica el HEAD, el árbol base N3, la ausencia de cambios funcionales respecto al package precompilado, los dos SHA-256 congelados y la disponibilidad de Arduino CLI. Después se crean carpetas temporales **fuera del repositorio** y se compilan los cuatro escenarios.

El script da `REVIEW_STOP` si una biblioteca no se selecciona desde la copia aislada, si una librería precompilada vuelve a compilar su `.cpp`, si source-first no genera objetos, si se rompe la compilación o si cambia un archivo productivo no autorizado.

**El ejecutor no hace** `git pull`, escritura al package instalado, USB/COM, upload, flashing, cambios de `boards.txt`, `platform.txt` ni llamadas de Release.

## Qué devolver para cerrar el benchmark

Al terminar, la consola informa:

```text
A13_LOCAL_BENCH=PASS_THREE_PAIRS_BOTH_FQBN
RESULT_ROOT=C:\Users\...\AppData\Local\Temp\jwplc_alpha13_pc_benchmark_...
```

En `RESULT_ROOT` se guardan:
- `MANIFEST.json` — commit HEAD, árbol de package, hashes, 24 mediciones y medianas/ahorros;
- `*_cold.log` y `*_warm.log` — logs completos de compilación.

**Comparte primero** las líneas `A13_LOCAL_BENCH=...`, `RESULT_ROOT=...` y el `MANIFEST.json`; solo pediremos logs específicos si hay un fallo. No hace falta compartir archivos `.bin`, `.elf` ni información sensible del PC.

## Criterios para darlo por cerrado

- [x] `A13_LOCAL_BENCH=PASS_THREE_PAIRS_BOTH_FQBN` en Windows del operador, 12/12 runs PASS.
- [x] Medianas de fría/caliente Basic/Core calculadas y separadas explícitamente de las muestras CI.
- [x] Dos hashes congelados confirmados; compilación desde fuentes 12+2 `.cpp.o`, desde archives 0+0, procedencia desde copias aisladas.
- [x] HEAD `158d12a2`, metodología, 24 tiempos y manifest preservado en repo; README principal actualizado. Logs completos siguen en la carpeta TEMP de la PC del operador.
- [x] **UPLOAD_BENCHMARK=NOT_MEASURED** declarado explícitamente; no se requirió USB/COM.

El benchmark no autoriza merge ni Release por sí solo. Tras incorporar el resultado local a `README.md`, `PRE_RELEASE.md`, `ALPHA13_STATUS.md` y checklist se solicita autorización del operador para cerrar el PR #126.
