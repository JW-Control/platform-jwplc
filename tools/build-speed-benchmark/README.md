# JWPLC Build Speed Benchmark

Banco reproducible para caracterizar tiempos de compilación y subida del package JWPLC Basic durante `v2.1.0-alpha.4`.

## Objetivo

El caso principal es deliberadamente mínimo:

```cpp
void setup() {}
void loop() {}
```

En `JWPLC Basic`, incluso este sketch activa el autoload del ecosistema JWPLC. Por tanto, `01_empty` permite medir el coste real que paga un usuario por recibir Display, RTC, FRAM, SD, Ethernet, botonera, RS-485 y Modbus RTU preintegrados.

No se eliminan periféricos del autoload para mejorar artificialmente los números.

---

## Archivos

```text
tools/build-speed-benchmark/
├── Run-JWPLCBuildBenchmark.ps1
├── PRECOMPILE_STRATEGY.md
└── sketches/
    ├── 01_empty/
    │   └── 01_empty.ino
    └── 02_io_basic/
        └── 02_io_basic.ino
```

Los resultados se crean localmente en:

```text
tools/build-speed-benchmark/results/<YYYYMMDD_HHMMSS>/
```

Cada ejecución genera:

```text
environment.json
arduino-cli-config.txt
arduino-cli-core-list.txt
results.csv
results.json
SUMMARY.md
logs/*.log
build/*
managed-cache/*
```

---

## Requisitos

- Windows PowerShell 5.1 o PowerShell 7.
- Arduino CLI disponible en `PATH`, o indicar su ruta con `-ArduinoCli`.
- Package JWPLC instalado o package local enlazado.
- Para pruebas de upload: JWPLC Basic conectado por USB y puerto COM conocido.

El script no borra el cache global de Arduino ni modifica `Arduino15`.

Para las pruebas de cache gestionada usa una ruta aislada mediante:

```text
ARDUINO_BUILD_CACHE_PATH
```

La variable se restaura al terminar el benchmark.

---

## Prueba 1 — Baseline de Alpha3 instalada

Si `jwplc:esp32` corresponde localmente a `v2.1.0-alpha.3`:

```powershell
cd tools\build-speed-benchmark
.\Run-JWPLCBuildBenchmark.ps1 `
    -PackageNamespace jwplc `
    -Targets Basic,Core `
    -Sketches 01_empty `
    -RunLabel alpha3-installed
```

Revisar después:

```text
arduino-cli-core-list.txt
```

para conservar evidencia de qué versión estaba instalada.

---

## Prueba 2 — Rama local Alpha4

Con el package local enlazado como `jwplc_local` hacia `JWPLC/2.1.0`:

```powershell
.\Run-JWPLCBuildBenchmark.ps1 `
    -PackageNamespace jwplc_local `
    -Targets Basic,Core `
    -Sketches 01_empty `
    -RunLabel alpha4-build-speed-base
```

Esta primera ejecución sobre la rama `build-speed-cache` todavía debe dar valores equivalentes a Alpha3 porque la feature inicia sin cambios funcionales del package.

---

## Prueba 3 — Compilación + upload

Ejemplo con `COM7`:

```powershell
.\Run-JWPLCBuildBenchmark.ps1 `
    -PackageNamespace jwplc_local `
    -Targets Basic `
    -Sketches 01_empty `
    -Port COM7 `
    -RunLabel alpha4-upload-baseline
```

Cuando se proporciona `-Port`, el script mide por separado:

```text
upload_full
upload_app_only
```

`upload_full` usa el flujo normal actual del board.

`upload_app_only` es experimental y sólo escribe la imagen de aplicación en `0x10000`. No modifica permanentemente `boards.txt` ni constituye todavía una decisión de producto.

Por seguridad, los uploads se ejecutan sólo para `Basic` de forma predeterminada. Para incluir `Core`:

```powershell
-UploadCore
```

---

## Prueba 4 — Sketch de I/O mínimo

Después de cerrar el caso vacío:

```powershell
.\Run-JWPLCBuildBenchmark.ps1 `
    -PackageNamespace jwplc_local `
    -Targets Basic,Core `
    -Sketches 01_empty,02_io_basic `
    -RunLabel alpha4-io-comparison
```

`02_io_basic` sólo usa:

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);
digitalRead(I0_0);
digitalWrite(Q0_0, ...);
```

Sirve para comprobar que una optimización no beneficia únicamente al sketch vacío.

---

## Fases medidas

### `managed_cold`

- cache aislada vacía;
- sin `--build-path`;
- Arduino CLI administra su cache normal;
- representa el primer build de esa combinación.

### `managed_warm_nochange`

- mismo sketch;
- misma cache;
- sin cambios en fuentes.

Mide el mejor caso de recompilación repetida.

### `managed_warm_touch`

- misma cache;
- se actualiza únicamente el timestamp del `.ino`;
- el contenido del sketch no cambia.

Representa el caso típico de taller:

```text
editar sketch -> Compilar/Subir otra vez
```

### `explicit_cold`

- `--build-path` fijo;
- `--clean`;
- árbol de compilación vacío.

Permite inspeccionar todos los artefactos generados.

### `explicit_warm_nochange`

- mismo `--build-path`;
- sin `--clean`;
- sin cambios.

Permite observar reutilización incremental de `.o/.d` dentro del mismo árbol.

### `explicit_warm_touch`

- mismo `--build-path`;
- timestamp del `.ino` actualizado.

Es especialmente útil para comprobar si cambiar el sketch provoca recompilar también librerías que deberían permanecer intactas.

### `upload_full`

Carga normal actual:

```text
bootloader
partitions
boot_app0
application
```

### `upload_app_only`

Carga experimental:

```text
application @ 0x10000
```

No incluye compilación dentro de la medición.

---

## Campos principales del CSV

| Campo | Uso |
|---|---|
| `DurationMs` | duración total de la fase |
| `CompilerInvocations` | aproximación de cuántas unidades realmente invocaron gcc/g++ |
| `LinkInvocations` | cantidad de enlaces detectados |
| `BinaryBytes` | suma de `.bin` presentes en el build explícito |
| `Success` | resultado de la fase |
| `LogPath` | evidencia verbose completa |
| `FQBN` | board exacto utilizado |
| `RunLabel` | etiqueta libre para comparar variantes |

`CompilerInvocations` es una métrica auxiliar basada en las líneas `-MMD -c` del log verbose. No reemplaza el tiempo medido.

---

## Uso de CPU

Por defecto:

```powershell
-Jobs 0
```

Arduino CLI usa todos los cores lógicos disponibles.

Para caracterizar una PC limitada también puede probarse, por ejemplo:

```powershell
-Jobs 2
```

No mezclar resultados con distinto `Jobs` en una misma conclusión sin indicarlo.

---

## Criterio para Alpha4

Antes de integrar cambios de compilación deben existir como mínimo mediciones de:

- `01_empty` Basic;
- `01_empty` Basic Core;
- cold build;
- warm sin cambios;
- warm tras edición;
- full upload;
- app-only upload;
- versión de Arduino CLI;
- CPU/RAM del equipo;
- commit del repositorio;
- logs verbose.

Después de aplicar precompilación se repite exactamente la misma matriz.

La optimización se acepta sólo si:

1. reduce significativamente el ciclo de edición normal;
2. no elimina periféricos del autoload;
3. no rompe Arduino IDE;
4. no rompe Arduino CLI;
5. no rompe Basic Core;
6. mantiene las APIs actuales;
7. las diferencias de tamaño de firmware quedan explicadas;
8. los resultados quedan registrados.
