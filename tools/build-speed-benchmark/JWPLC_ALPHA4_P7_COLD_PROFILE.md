# JWPLC v2.1.0-alpha.4 — perfil cold P7 en laptop

## Objetivo

Registrar el perfil de compilación cold de `JWPLC Basic` en el segundo equipo de validación, una vez cerradas las optimizaciones P1–P6 y validado el perfilador por unidad de compilación.

Este documento no representa todavía una optimización P7 aplicada. Es una línea base para decidir qué atacar a continuación sin retirar periféricos del autoload normal.

FQBN:

```txt
jwplc_local:esp32:jwplcbasic
```

Sketch:

```txt
tools/build-speed-benchmark/sketches/01_empty
```

Perfilador:

```txt
tools/build-speed-benchmark/Measure-JWPLCColdCompileBreakdown.ps1
```

El perfilador usa `compile_commands.json` y ejecuta cada unidad de compilación mediante su `arguments[]` nativo con `ProcessStartInfo.ArgumentList`. No reconstruye una línea de comandos de texto, para conservar correctamente argumentos como `-DARDUINO_BOARD="JWPLCBASIC"`.

## Host de validación

Segundo equipo utilizado para la validación cruzada de Arduino IDE:

- Intel Core i5-13420H;
- 15.7 GB de RAM;
- SSD NVMe;
- Arduino IDE 2.3.10;
- Windows;
- package local enlazado al mismo branch versionado.

## Run completo de referencia

Run:

```txt
tools/build-speed-benchmark/compile-profile-work/20260810_105835
```

Resultados:

| Métrica | Resultado |
|---|---:|
| Preparación/discovery + `compile_commands.json` | **87.286 s** |
| TUs detectados desde fuente | **12** |
| Suma secuencial de TUs | **36.673 s** |
| Cold completo normal `-j 0` | **107.170 s** |

La suma secuencial de TUs es una métrica de profiling y **no debe sumarse** al cold normal. Los TUs se ejecutan uno a uno deliberadamente para poder atribuir costo por archivo; el cold normal usa paralelismo.

Un run previo del mismo host había medido **89.570 s** en preparación/discovery, por lo que el nuevo resultado de **87.286 s** confirma que esa fase es consistentemente costosa en este equipo.

## Tiempo por unidad de compilación

| Posición | Grupo | Archivo | Tiempo |
|---:|---|---|---:|
| 1 | Wire | `Wire.cpp` | 4.477 s |
| 2 | FS | `vfs_api.cpp` | 4.245 s |
| 3 | SD | `SD.cpp` | 4.103 s |
| 4 | FS | `FS.cpp` | 4.047 s |
| 5 | JWPLC_GlobalPeripherals | `JWPLC_GlobalPeripherals.cpp` | 3.773 s |
| 6 | sketch | `01_empty.ino.cpp` | 3.677 s |
| 7 | SD | `sd_diskio.cpp` | 3.160 s |
| 8 | JWPLC_Ethernet | `JWPLC_Ethernet.cpp` | 2.977 s |
| 9 | JWPLC_RS485 | `JWPLC_RS485.cpp` | 2.951 s |
| 10 | SPI | `SPI.cpp` | 2.760 s |
| 11 | core | `p2_core_stub.c` | 0.257 s |
| 12 | SD | `sd_diskio_crc.c` | 0.244 s |

## Tiempo agregado por grupo

| Grupo | Tipo | TUs | Total secuencial | Promedio | Máximo |
|---|---|---:|---:|---:|---:|
| FS | library | 2 | **8.292 s** | 4.146 s | 4.245 s |
| SD | library | 3 | **7.507 s** | 2.502 s | 4.103 s |
| Wire | library | 1 | 4.477 s | 4.477 s | 4.477 s |
| JWPLC_GlobalPeripherals | library | 1 | 3.773 s | 3.773 s | 3.773 s |
| sketch | sketch | 1 | 3.677 s | 3.677 s | 3.677 s |
| JWPLC_Ethernet | library | 1 | 2.977 s | 2.977 s | 2.977 s |
| JWPLC_RS485 | library | 1 | 2.951 s | 2.951 s | 2.951 s |
| SPI | library | 1 | 2.760 s | 2.760 s | 2.760 s |
| jwcontrol_p2 | core | 1 | 0.257 s | 0.257 s | 0.257 s |

`FS + SD` suman **15.799 s**, equivalentes a aproximadamente **43.1 %** de la suma secuencial de los 12 TUs. Son por tanto el primer conjunto candidato para un experimento P7 controlado.

## Comparación cruzada de cold controlado

El cold P6 final de la PC principal fue:

```txt
67.322 s
```

El cold normal `-j 0` del segundo equipo fue:

```txt
107.170 s
```

La laptop tarda **39.848 s más**, aproximadamente **59.2 %** sobre el tiempo de la PC principal. Esto refuerza el objetivo de alpha4: las mejoras deben evaluarse también en equipos donde la compilación inicial es sensiblemente más costosa.

No se debe comparar directamente `87.286 s` de preparación/discovery con `107.170 s` como si fueran fases aditivas de una misma ejecución: son ejecuciones independientes del perfilador con finalidades distintas.

## Lectura P7

El perfil deja dos líneas de investigación separadas:

1. **TUs todavía compilados desde fuente.** `FS` y `SD` son el mayor hotspot secuencial; después aparecen `Wire`, `JWPLC_GlobalPeripherals`, `SPI`, `JWPLC_Ethernet` y `JWPLC_RS485`.
2. **Preparación/discovery.** Dos mediciones consecutivas en la laptop dieron 89.570 s y 87.286 s. Esta fase merece instrumentación específica antes de atribuirla a una librería concreta.

La siguiente prueba P7 debe ser incremental y reversible. El primer candidato recomendado es evaluar `FS + SD` como bloque, midiendo simultáneamente:

- número de TUs restantes;
- tiempo de preparación/discovery;
- cold completo `-j 0`;
- tamaño de app;
- equivalencia estructural y funcional.

No se considera válido adoptar P7 sólo porque reduzca la suma secuencial de TUs. Debe demostrar una mejora real del cold completo y mantener compatibilidad con Arduino IDE y el autoload normal.

## Estado

- Perfilador con `arguments[]`: **validado**.
- Reutilización de `compile_commands.json`: **validada**.
- Perfil cold completo en segundo equipo: **cerrado**.
- P7 aplicado: **pendiente**.
- Primer candidato de experimento: **FS + SD**.
- Investigación separada de preparation/discovery: **pendiente**.
