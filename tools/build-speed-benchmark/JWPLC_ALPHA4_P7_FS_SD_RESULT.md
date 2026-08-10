# JWPLC v2.1.0-alpha.4 — resultado P7 FS + SD

## Objetivo

Evaluar si precompilar `FS` y `SD` reduce de forma real el cold build de `JWPLC Basic` en el segundo equipo de validación, manteniendo el autoload normal y sin retirar periféricos.

Este experimento se ejecuta sobre el estado P6 ya validado estructuralmente. La adopción P7 sigue condicionada a validación estructural completa, Arduino IDE y gate funcional físico.

FQBN:

```txt
jwplc_local:esp32:jwplcbasic
```

Sketch:

```txt
tools/build-speed-benchmark/sketches/01_empty
```

## Cambio experimental

Se añadió localmente:

```txt
precompiled=full
```

A:

- `JWPLC/2.1.0/libraries/FS/library.properties`
- `JWPLC/2.1.0/libraries/SD/library.properties`

Y se generaron los archives:

- `FS`: 2 objetos, 415104 bytes.
- `SD`: 3 objetos, 275694 bytes.

El generador verificó que, con los archives presentes, ninguna de estas dos librerías volvió a compilarse desde fuente.

## Run de generación / verificación

Run:

```txt
tools/build-speed-benchmark/precompile-work/20260810_112117
```

| Etapa | Tiempo |
|---|---:|
| Build fuente para obtener objetos | 71.126 s |
| Build de verificación con `libFS.a` + `libSD.a` | 63.280 s |

La comparación source → archives dentro de este run reduce 7.846 s, aproximadamente 11.03 %. Este dato es auxiliar porque el cambio `precompiled=full` ya altera la fase de discovery aun cuando Arduino hace fallback a source.

## Perfil cold completo P7

Run:

```txt
tools/build-speed-benchmark/compile-profile-work/20260810_114917
```

Resultados:

| Métrica | P6 laptop | P7 FS+SD | Reducción |
|---|---:|---:|---:|
| Preparación/discovery | 87.286 s | **50.004 s** | **37.282 s / 42.71 %** |
| TUs detectados | 12 | **7** | **5 TUs menos** |
| Suma secuencial de TUs | 36.673 s | **13.351 s** | **23.322 s / 63.59 %** |
| Cold completo normal `-j 0` | 107.170 s | **63.870 s** | **43.300 s / 40.40 %** |

La suma secuencial de TUs es una métrica de profiling y no se suma al cold normal.

## TUs restantes

Con `FS` y `SD` precompilados desaparecen del perfil:

- `FS.cpp`
- `vfs_api.cpp`
- `SD.cpp`
- `sd_diskio.cpp`
- `sd_diskio_crc.c`

Quedan 7 TUs:

| Grupo | Archivo | Tiempo |
|---|---|---:|
| Wire | `Wire.cpp` | 2.674 s |
| JWPLC_GlobalPeripherals | `JWPLC_GlobalPeripherals.cpp` | 2.588 s |
| sketch | `01_empty.ino.cpp` | 2.538 s |
| JWPLC_Ethernet | `JWPLC_Ethernet.cpp` | 1.822 s |
| JWPLC_RS485 | `JWPLC_RS485.cpp` | 1.820 s |
| SPI | `SPI.cpp` | 1.792 s |
| jwcontrol_p2 | `p2_core_stub.c` | 0.117 s |

## Lectura técnica

El resultado demuestra dos efectos simultáneos:

1. Los 5 TUs de `FS` y `SD` dejan de compilarse desde fuente.
2. La fase de preparación/discovery cae de 87.286 s a 50.004 s en el mismo host, una reducción de 42.71 %.

Por tanto, el beneficio P7 no proviene únicamente de evitar compilación C/C++; `precompiled=full` también reduce trabajo de discovery/dependency detection en estas librerías.

El cold completo baja de 107.170 s a 63.870 s en la misma laptop, una mejora de 40.40 %. Esta es la comparación principal para decidir P7.

Como referencia cruzada, el P6 final de la PC principal había medido 67.322 s. El P7 de la laptop resulta 3.452 s menor, pero no debe usarse como comparación directa de etapas porque son hosts diferentes.

## Estado de decisión

P7 FS+SD pasa de candidato a **resultado de rendimiento validado en CLI**, pero todavía no se adopta como cerrado/publicable.

Pendientes antes de integrarlo definitivamente:

- verificar que `libFS.a` y `libSD.a` estén realmente enlazados en el full cold y registrar tamaño/hash;
- confirmar tamaño de app y ausencia de regresión estructural;
- validar compilación/subida real en Arduino IDE;
- ejecutar prueba funcional física de microSD y periféricos relacionados;
- después de esos gates, versionar `precompiled=full` + archives y actualizar la tabla formal de tiempos.
