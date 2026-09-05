# Alpha10 - Benchmark de compilación

## Objetivo actual

Revalidar `v2.1.0-alpha.10` después de retirar el guard de shadowing de `JWPLC_Ethernet` y comprobar que el ciclo normal de Arduino IDE / Arduino CLI vuelve al entorno de tiempos de Alpha9 sin sacrificar periféricos ni compatibilidad.

## Evidencia histórica que motivó el cambio

Durante el Alpha10 inicial se midió el coste de markers bundled en `tools/build-speed-benchmark/sketches/01_empty`:

| Variante | Markers adicionales JW/JWPLC | Warm promedio | Delta vs M0 |
|---|---:|---:|---:|
| `M0_NONE` | 0 | 22.094 s | base |
| `M1_ETH` | 1 | 23.327 s | +1.233 s / +5.6% |
| `M4_OBSERVED_STALE` | 4 | 26.888 s | +4.794 s / +21.7% |
| `M7_ALL` | 7 | 30.353 s | +8.259 s / +37.4% |

La estructura de compilación y el tamaño binario se mantuvieron equivalentes. El efecto observado fue principalmente coste de library discovery/warm build.

## Decisión de esta revisión

Se retira `M1_ETH` y se vuelve al comportamiento de Alpha9 para `JWPLC_GlobalPeripherals_Auto.h`.

```text
ALPHA10_MARKER_SET_JW_JWPLC=NONE
JWPLC_ETHERNET_SHADOW_GUARD=REMOVED
JWPLC_ETHERNET_LIBRARY_VERSION=1.0.0
ADAFRUIT_BUNDLED_MARKERS=RETAINED
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

Los markers Adafruit no pertenecen a esta eliminación: protegen dependencias externas precompiladas que pueden coexistir legítimamente con copias del sketchbook.

Commit candidato:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
```

## Matriz de revalidación requerida

Usar el mismo host, mismo Arduino CLI, mismo `Jobs` y el namespace local enlazado al árbol bajo prueba.

Targets:

```text
jwplc_local:esp32:jwplcbasic
jwplc_local:esp32:jwplcbasiccore
```

Sketches de benchmark:

```text
tools/build-speed-benchmark/sketches/01_empty
tools/build-speed-benchmark/sketches/02_io_basic
```

Fases relevantes:

```text
managed_cold
managed_warm_nochange
managed_warm_touch
explicit_cold
explicit_warm_nochange
explicit_warm_touch
```

Hacer al menos tres réplicas del candidato para distinguir el cambio real de la variación normal del host.

## Comando candidato

Desde la raíz del repositorio:

```powershell
cd tools\build-speed-benchmark

.\Run-JWPLCBuildBenchmark.ps1 `
    -ArduinoCli "C:\Program Files\Arduino PLC IDE Tools\arduino-cli.exe" `
    -PackageNamespace jwplc_local `
    -Targets Basic,Core `
    -Sketches 01_empty,02_io_basic `
    -RunLabel alpha10-cleanup-r1
```

Repetir como `alpha10-cleanup-r2` y `alpha10-cleanup-r3` sin cambiar configuración del host.

## Matriz funcional corta

Además del benchmark, Alpha10 debe volver a compilar:

```text
JWPLC_GlobalPeripherals/examples/01.DigitalIO_Basic
JWPLC_GlobalPeripherals/examples/02.Buttons_Basic
JWPLC_Display/examples/02.Display_HMI_Fields
JWPLC_Ethernet/examples/03.Ethernet_Diagnostics
JWPLC_ModbusRTU/examples/JWPLC_RemoteIO_Slave_RTU
```

El workflow general ya usa esta matriz. La validación local/Arduino IDE debe comprobar también que no aparece una dependencia inesperada del sketchbook.

## Criterios de aceptación

- la matriz funcional termina 5/5;
- no aparecen undefined references;
- Basic y Basic Core conservan la estructura esperada de compilación;
- no se retira ningún periférico del autoload;
- el tamaño de firmware no presenta una diferencia funcional inexplicada;
- el warm build mejora respecto al Alpha10 inicial con `M1_ETH`;
- el resultado queda razonablemente alineado con Alpha9/M0 teniendo en cuenta la variación del host;
- Arduino IDE y Arduino CLI permanecen operativos.

## Resultados nuevos

Pendientes de la ejecución física/local del candidato:

| Run | Target | Sketch | Cold | Warm no-change | Warm touch | Resultado |
|---|---|---|---:|---:|---:|---|
| r1 | Basic | `01_empty` | PENDING | PENDING | PENDING | PENDING |
| r2 | Basic | `01_empty` | PENDING | PENDING | PENDING | PENDING |
| r3 | Basic | `01_empty` | PENDING | PENDING | PENDING | PENDING |
| r1-r3 | Core | `01_empty` | PENDING | PENDING | PENDING | PENDING |
| r1-r3 | Basic/Core | `02_io_basic` | PENDING | PENDING | PENDING | PENDING |

## Estado

```text
ALPHA10_BUILD_BENCHMARK_HISTORICAL_EVIDENCE=RECORDED
ALPHA10_CLEANUP_CANDIDATE=READY_FOR_LOCAL_BENCHMARK
ALPHA10_BUILD_BENCHMARK_FINAL=PENDING
```
