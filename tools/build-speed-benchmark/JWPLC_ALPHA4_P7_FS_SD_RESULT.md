# JWPLC v2.1.0-alpha.4 — resultado P7 FS + SD

## Objetivo

Evaluar si precompilar `FS` y `SD` reduce de forma real el cold build de `JWPLC Basic` en el segundo equipo de validación, manteniendo el autoload normal y sin retirar periféricos.

El experimento se ejecutó sobre el estado P6 ya validado estructuralmente y quedó finalmente integrado después de superar rendimiento CLI, gate estructural, Arduino IDE y gate funcional físico de microSD.

FQBN:

```txt
jwplc_local:esp32:jwplcbasic
```

Sketch de benchmark:

```txt
tools/build-speed-benchmark/sketches/01_empty
```

Sketch de gate físico:

```txt
tools/build-speed-benchmark/sketches/04_p7_sd_gate/04_p7_sd_gate.ino
```

## Cambio P7

Se añadió:

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

La integración quedó versionada en:

```txt
1a60176 perf(build): precompilar FS y SD para P7
```

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

## Gate estructural FS + SD

Archives verificados:

| Archive | Bytes | SHA-256 |
|---|---:|---|
| `libFS.a` | 415104 | `CBEA33C505D28E9B3A6A2E3ABDDEA6CB4C384B8BE919ED456C00ED0D8A31C327` |
| `libSD.a` | 275694 | `45D1D9B27701403CE7D380838AD723194A3730DB5F2859B90D0D01D75FA040FD` |

El `FULL_COLD.log` confirmó que Arduino reconoce ambas librerías como precompiladas:

```txt
Skipping dependencies detection for precompiled library SD
Skipping dependencies detection for precompiled library FS
```

El mapa de enlace `01_empty.ino.map` confirmó extracción real desde ambos archives. Entre las entradas observadas:

```txt
libSD.a(SD.cpp.o)
libSD.a(sd_diskio.cpp.o)
libSD.a(sd_diskio_crc.c.o)
libFS.a(FS.cpp.o)
libFS.a(vfs_api.cpp.o)
```

También aparecen referencias resueltas desde estos miembros, incluyendo `fs::SDFS::begin`, `sd_read_raw`, `CRC7`, `ff_diskio_get_drive`, `f_mount`, `esp_vfs_fat_register`, `fileno`, `setvbuf` y `unlink`.

Esto cierra el gate estructural: `FS` y `SD` no sólo son detectadas como precompiladas, sino que sus miembros se extraen y participan efectivamente en el enlace final.

El full cold reportó:

```txt
Sketch uses 404761 bytes (12%) of program storage space. Maximum is 3145728 bytes.
Global variables use 27908 bytes (8%) of dynamic memory, leaving 299772 bytes for local variables. Maximum is 327680 bytes.
```

El tamaño de app es consistente con el estado P6; no se observa regresión estructural por P7.

## Validación real en Arduino IDE

En el segundo equipo de validación se compiló y subió el sketch físico `04_p7_sd_gate.ino` desde Arduino IDE.

Medición manual desde pulsar **Subir**:

| Hito | Tiempo |
|---|---:|
| Fin de compilación / inicio de conexión | **1:13 (73 s)** |
| `Hard resetting via RTS pin...` | **1:20 (80 s)** |
| Diferencia atribuible a conexión/upload/reset | **~7 s** |

La subida física fue satisfactoria. Esta medición es observacional de Arduino IDE y no sustituye el cold controlado `63.870 s` de Arduino CLI.

## Gate funcional físico de microSD

La primera ejecución informó:

```txt
[P7-SD] FAIL begin: No card
P7_SD_GATE=FAIL
```

La causa fue física: un detalle de soldadura en la señal de detección de tarjeta. Durante el diagnóstico GPIO39 variaba de forma inestable. Después de resoldar, el detector quedó estable con la lógica esperada por el package:

```txt
sin microSD -> GPIO39 = 1
con microSD -> GPIO39 = 0
```

No se requirió cambiar software, pin, polaridad ni formato de la tarjeta para resolver ese fallo.

Con la soldadura corregida, el gate físico produjo:

```txt
=== JWPLC P7 FS+SD PHYSICAL GATE ===
[P7-SD] Iniciando microSD...
[P7-SD] Card type: 3
[P7-SD] Card size bytes: 15730212864
[P7-SD] Readback: JWPLC_P7_FS_SD_OK
[P7-SD] PASS write/read/verify/remove
P7_SD_GATE=PASS
JWPLC_Display inicializado
```

El gate valida en hardware real:

- detección estable de inserción;
- inicialización/montaje de la microSD;
- lectura de tipo y capacidad;
- creación de archivo;
- escritura;
- lectura de retorno;
- comparación exacta del payload;
- eliminación del archivo temporal;
- coexistencia con el autoload normal, incluido Display.

## Lectura técnica

El resultado demuestra dos efectos simultáneos:

1. Los 5 TUs de `FS` y `SD` dejan de compilarse desde fuente.
2. La fase de preparación/discovery cae de 87.286 s a 50.004 s en el mismo host, una reducción de 42.71 %.

Por tanto, el beneficio P7 no proviene únicamente de evitar compilación C/C++; `precompiled=full` también reduce trabajo de discovery/dependency detection en estas librerías.

El cold completo baja de 107.170 s a 63.870 s en la misma laptop, una mejora de 40.40 %. Esta es la comparación principal para decidir P7.

Como referencia cruzada, el P6 final de la PC principal había medido 67.322 s. El P7 de la laptop resulta 3.452 s menor, pero no debe usarse como comparación directa de etapas porque son hosts diferentes.

## Estado de decisión

P7 FS+SD queda **adoptado e integrado** para `JWPLC Basic` en la rama `v2.1.0-alpha.4/feature/build-speed-cache`.

Gates cerrados:

- rendimiento CLI: **PASS**;
- reducción 12 -> 7 TUs: **PASS**;
- gate estructural/mapa de enlace: **PASS**;
- Arduino IDE + subida física: **PASS**;
- microSD write/read/verify/remove: **PASS**;
- CARD DETECT: **PASS tras corrección física de soldadura**;
- archives y `precompiled=full` versionados: **PASS**.

P7 FS+SD queda cerrado. Los gates generales restantes de alpha4 —TFT/periféricos integrados finales, autoload contract y decisiones de configuración— siguen siendo independientes de este cierre específico.