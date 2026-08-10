# JWPLC v2.1.0-alpha.4 — tabla formal de tiempos de compilación

## Alcance

Este documento consolida los tiempos medidos durante la optimización de compilación de `v2.1.0-alpha.4` para `JWPLC Basic`, manteniendo el autoload normal y sin retirar periféricos por motivos de velocidad.

FQBN de trabajo:

```txt
jwplc_local:esp32:jwplcbasic
```

Sketch de referencia:

```txt
tools/build-speed-benchmark/sketches/01_empty
```

Los cold controlados recientes se ejecutaron con build path dedicado y limpieza explícita:

```powershell
arduino-cli compile -b jwplc_local:esp32:jwplcbasic -j 0 -v --build-path <BUILD_PATH> --clean tools/build-speed-benchmark/sketches/01_empty
```

> Nota: los tiempos dependen del host. Para comparar etapas se priorizan runs cold controlados del mismo entorno. Los runs `source-layout` existen para validar equivalencia de layout y no se usan como prueba aislada de mejora de rendimiento. P7 fue validado en un segundo host y su comparación principal es contra el P6 medido en ese mismo host.

## Tabla consolidada

| Etapa | Cold | Compiles/TUs | `g++ -E` | App / bin observado | Estado / lectura |
|---|---:|---:|---:|---:|---|
| Alpha3 oficial instalado | 136.509 s | 102 | — | — | Baseline histórico oficial. |
| Local pre-D1 | 148.649 s | 102 | — | — | Baseline local previo a optimizaciones. |
| D1 discovery | 121.732 s | 102 | — | — | Validado. Reduce costo de discovery sin retirar autoload. |
| P1 librerías JWPLC precompiladas | 105.940 s | 97 | — | — | Validado. |
| P2 core precompilado | 104.223 s | 34 | 51 | — | Validado para Basic; compatibilidad Arduino IDE validada posteriormente con link explícito. |
| P3 histórico Display | 95.172 s | 32 | 49 | 406016 B | Válido históricamente, pero con dependencias no deterministas. |
| P4 GlobalPeripherals | 97.121 s | 31 | 48 | — | Técnicamente válido, rechazado por no mejorar P3 histórico. |
| P3 determinista | 101.677 s | 32 | 49 | 406016 B | Referencia determinista posterior al bundling de dependencias. |
| P5A Ethernet W5x00 | 90.587 s | 24 | 41 | 406032 B | Validado estructuralmente. |
| P6A-2 ST77xx precompiled | 84.544 s | 20 | 37 | 405712 B | Validado estructuralmente. |
| P6B-1 GFX `src/` source-only | 83.327 s | 20 | 37 | 405712 B | Equivalencia de layout; no se toma como mejora aislada. |
| P6B-2 GFX precompiled | 77.907 s | 16 | 33 | 405472 B | Validado estructuralmente. |
| P6C-1 BusIO `src/` source-only | 73.382 s | 16 | 33 | 405472 B | Equivalencia de layout; no se toma como mejora aislada. |
| **P6C-2 / P6D full Adafruit stack — PC principal** | **67.322 s** | **12** | **29** | **404912 B bin** | **Cerrado / validado estructuralmente.** |
| **P6 baseline — laptop** | **107.170 s** | **12** | — | **404912 B bin** | Baseline del segundo host previo a P7. |
| **P7 FS+SD precompiled — laptop** | **63.870 s** | **7** | — | **404761 B app / 404912 B bin** | **Cerrado: CLI + mapa + Arduino IDE + microSD física PASS.** |

## Validación real en Arduino IDE — P2 compatible con caché

El 2026-08-10 se detectó que el diseño P2 original, que sustituía `{build.path}/core/core.a` mediante un hook posterior, no cubría el caso de Arduino IDE 2.x cuando el IDE enlazaba un `core.a` desde su caché global bajo `%LOCALAPPDATA%\arduino\cores`.

La caché contenía el archive del stub `jwcontrol_p2` (~3 KB), mientras el core completo precompilado de `JWPLC Basic` medía `3012800 B`. El resultado era una cascada de `undefined reference` durante el link.

El intento de escribir `{archive_file_path}` desde `recipe.hooks.core.postbuild` también se descartó porque Arduino IDE dejó esa propiedad literal dentro del hook.

La solución validada para `JWPLC Basic` mantiene:

```txt
build.core=jwcontrol_p2
```

y añade el core completo de forma explícita al grupo de enlace:

```txt
build.extra_libs="{runtime.platform.path}/precompiled/core/JWPLCBASIC/core.a"
```

Con esta arquitectura Arduino IDE puede seguir usando/cacheando el pequeño `core.a` del stub, pero el linker recibe además el archive JWPLC completo. El log real confirmó simultáneamente:

- `Using core 'jwcontrol_p2'`;
- uso del `core.a` cacheado por Arduino IDE;
- inclusión explícita de `precompiled/core/JWPLCBASIC/core.a` dentro de `--start-group`;
- creación correcta del `.elf` y `.bin`;
- binario de aplicación de `404912 B` en el estado P6 observado;
- subida física a `921600`;
- verificación de hashes de flash y reset final satisfactorio.

El mecanismo P2 dejó de depender de `boards.local.txt`: la configuración quedó promovida a `JWPLC/2.1.0/boards.txt` y el archive `JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a` quedó versionado en el package. `JWPLC Basic Core` sigue pendiente de un archive propio y no reutiliza el de Basic.

### Tiempos observados en Arduino IDE

Las mediciones manuales de experiencia real distinguen dos hitos:

1. **Compilación**: desde pulsar **Subir** hasta que Arduino IDE termina la fase de compilación y comienza la conexión/subida (`Compilación completada` / `Connecting...`). Esta es la métrica principal de velocidad percibida de compilación.
2. **Total end-to-end**: desde pulsar **Subir** hasta `Hard resetting via RTS pin...`. Se conserva como referencia secundaria porque la fase física de flash es prácticamente estable entre runs.

#### PC principal

| Caso observado | Compilación aproximada |
|---|---:|
| Primera carga / entorno sin build reutilizable del sketch | ~65 s |
| Siguiente carga / incremental | ~17–19 s |

Estos tiempos son observacionales de Arduino IDE y no sustituyen el cold controlado de `67.322 s` obtenido con `arduino-cli --clean`.

#### Segundo equipo — laptop de validación cruzada

Hardware reportado:

- Intel Core i5-13420H;
- 15.7 GB de RAM;
- SSD NVMe;
- Intel UHD Graphics.

Arduino IDE 2.3.10, mismo `jwplc_local:esp32:jwplcbasic`, sketch vacío y package obtenido desde la rama versionada.

Antes de P7:

| Run | Compilación | Total hasta `Hard resetting via RTS pin...` |
|---|---:|---:|
| Primera carga | 1:35 (95 s) | 1:43 (103 s) |
| Incremental 1 | 8 s | 16 s |
| Incremental 2 | 9 s | 16 s |
| Incremental 3 | 8 s | 15 s |

La primera carga del segundo equipo es más lenta que en la PC principal, coherente con la creación inicial de caches y objetos todavía compilados desde fuente. En cambio, las tres cargas incrementales convergen a **8–9 s de compilación** y **15–16 s end-to-end**, confirmando que Arduino reutiliza correctamente los objetos no modificados y los archives precompilados.

El log de la laptop confirmó además:

- `FQBN: jwplc_local:esp32:jwplcbasic`;
- `Using core 'jwcontrol_p2'`;
- reutilización de dependencias y objetos cacheados;
- uso de todos los archives P1–P6 esperados;
- enlace simultáneo del core stub cacheado y `precompiled/core/JWPLCBASIC/core.a`;
- bin de `404912 B`;
- carga física satisfactoria y reset por RTS.

Esta validación cruzada demuestra que el estado optimizado ya versionado es reproducible en un segundo equipo y no depende de archives locales no publicados.

## P7 — FS + SD precompilados

El perfilador por unidad de compilación identificó a `FS` + `SD` como el mayor grupo restante en el estado P6 de la laptop. La optimización P7 añadió `precompiled=full` y archives ESP32 para ambas librerías.

### Comparación controlada en el mismo host

| Métrica | P6 laptop | P7 FS+SD | Reducción |
|---|---:|---:|---:|
| Preparación/discovery | 87.286 s | **50.004 s** | **37.282 s / 42.71 %** |
| TUs desde fuente | 12 | **7** | **5 TUs menos** |
| Suma secuencial de TUs | 36.673 s | **13.351 s** | **23.322 s / 63.59 %** |
| Cold normal `-j 0` | 107.170 s | **63.870 s** | **43.300 s / 40.40 %** |

La reducción del cold normal de **107.170 s a 63.870 s** es la métrica principal P7 porque compara el mismo host y el mismo método. La suma secuencial de TUs se usa únicamente para profiling.

### Archives P7

| Archive | Miembros | Bytes | SHA-256 |
|---|---:|---:|---|
| `libFS.a` | 2 | 415104 | `CBEA33C505D28E9B3A6A2E3ABDDEA6CB4C384B8BE919ED456C00ED0D8A31C327` |
| `libSD.a` | 3 | 275694 | `45D1D9B27701403CE7D380838AD723194A3730DB5F2859B90D0D01D75FA040FD` |

El mapa de enlace confirmó extracción real de:

```txt
libFS.a(FS.cpp.o)
libFS.a(vfs_api.cpp.o)
libSD.a(SD.cpp.o)
libSD.a(sd_diskio.cpp.o)
libSD.a(sd_diskio_crc.c.o)
```

### Arduino IDE y gate físico P7

El sketch `04_p7_sd_gate.ino` se compiló y subió desde Arduino IDE en la laptop:

| Hito | Tiempo |
|---|---:|
| Fin de compilación / inicio de conexión | **73 s** |
| `Hard resetting via RTS pin...` | **80 s** |
| Conexión/upload/reset | **~7 s** |

La primera ejecución del gate reportó `No card` por un detalle físico de soldadura en CARD DETECT. Después de resoldar, GPIO39 quedó estable con la lógica esperada: `1` sin microSD y `0` con microSD. No se realizó cambio de software para resolverlo.

La segunda ejecución pasó completamente:

```txt
[P7-SD] Card type: 3
[P7-SD] Card size bytes: 15730212864
[P7-SD] Readback: JWPLC_P7_FS_SD_OK
[P7-SD] PASS write/read/verify/remove
P7_SD_GATE=PASS
JWPLC_Display inicializado
```

P7 FS+SD queda por tanto cerrado en rendimiento, estructura, Arduino IDE y función física de microSD.

Integración versionada:

```txt
1a60176 perf(build): precompilar FS y SD para P7
```

## Runs preservados principales

- P3 determinista: `tools/build-speed-benchmark/p3-deterministic-work/20260809_190321/p3-Basic`
- P5A Ethernet: `tools/build-speed-benchmark/p5a-ethernet-work/20260809_224628/p5a-Basic`
- P6A-2 ST77xx: `tools/build-speed-benchmark/p6a2-st77xx-precompiled-work/20260809_232946/p6a2-Basic`
- P6B-1 GFX source-layout: `tools/build-speed-benchmark/p6b-gfx-layout-work/20260809_234653/p6b-layout-Basic`
- P6B-2 GFX precompiled: `tools/build-speed-benchmark/p6b2-gfx-precompiled-work/20260809_235326/p6b2-Basic`
- P6C-1 BusIO source-layout: `tools/build-speed-benchmark/p6c-busio-layout-work/20260810_000915/p6c-layout-Basic`
- P6C-2 BusIO / full stack: `tools/build-speed-benchmark/p6c2-busio-precompiled-work/20260810_002028/p6c2-Basic`
- P7 perfil baseline laptop: `tools/build-speed-benchmark/compile-profile-work/20260810_105835`
- P7 FS+SD: `tools/build-speed-benchmark/compile-profile-work/20260810_114917`
- P7 generación/verificación FS+SD: `tools/build-speed-benchmark/precompile-work/20260810_112117`

## Reducción acumulada hasta P6 en PC principal

Tomando `67.322 s` como cold final P6 de la PC principal:

| Comparación | Reducción | Mejora |
|---|---:|---:|
| Alpha3 oficial `136.509 -> 67.322 s` | 69.187 s | 50.68 % |
| Local pre-D1 `148.649 -> 67.322 s` | 81.327 s | 54.71 % |
| D1 `121.732 -> 67.322 s` | 54.410 s | 44.70 % |
| P1 `105.940 -> 67.322 s` | 38.618 s | 36.45 % |
| P2 `104.223 -> 67.322 s` | 36.901 s | 35.41 % |
| P3 determinista `101.677 -> 67.322 s` | 34.355 s | 33.79 % |
| P5A `90.587 -> 67.322 s` | 23.265 s | 25.68 % |
| P6A-2 `84.544 -> 67.322 s` | 17.222 s | 20.37 % |
| P6B-2 `77.907 -> 67.322 s` | 10.585 s | 13.59 % |

No se calcula una reducción acumulada Alpha3 -> P7 usando `63.870 s`, porque P7 fue medido en un host diferente. La mejora P7 válida es **107.170 -> 63.870 s (40.40 %)** dentro de la laptop.

## Estado técnico del stack precompilado al cierre P7

Además de P1–P6, P7 incorpora `FS` y `SD` como `precompiled=full` con source fallback preservado.

Las tres librerías bundled Adafruit permanecen preparadas con layout `src/` y `precompiled=full` para ESP32:

- `Adafruit_ST7735_and_ST7789_Library`: archive con 4 miembros; el firmware Basic extrae sólo los objetos necesarios para ST7789/ST77xx.
- `Adafruit_GFX_Library`: archive con 4 miembros; el enlazador evita componentes no usados como `Adafruit_GrayOLED.cpp.o`.
- `Adafruit_BusIO`: archive con 4 miembros; el firmware Basic actual extrae `Adafruit_SPIDevice.cpp.o` y deja fuera los miembros no utilizados.

P7 añade:

- `FS`: 2 miembros, ambos requeridos por el firmware actual;
- `SD`: 3 miembros, los tres requeridos por el firmware actual.

Los gates estructurales verificaron selección de librerías, símbolos globales, mapa de enlace y objetos externos relevantes; no se usa igualdad byte-a-byte del `.bin` como requisito cuando cambia la semántica de extracción de archives.

## Conclusión de esta fase

P7 FS+SD queda cerrado e integrado en `v2.1.0-alpha.4/feature/build-speed-cache`.

En la laptop de validación, el cold normal baja de **107.170 s a 63.870 s**, una mejora de **40.40 %**, mientras los TUs compilados desde fuente bajan de **12 a 7**. La mejora conserva el autoload normal y pasó gate estructural, compilación/subida real en Arduino IDE y prueba física de microSD con write/read/verify/remove.

El estado P6 de la PC principal permanece como referencia histórica de **67.322 s**. Debe ejecutarse un benchmark P7 equivalente en la PC principal si se desea obtener una comparación acumulada P6 -> P7 dentro de ese host.

## Pendientes de cierre de alpha relacionados

- Generar/validar el archive de core para `JWPLC Basic Core`, separado del archive de `JWPLC Basic`.
- Dejar explícita la decisión o pendiente sobre configuración final, incluyendo Flash Frequency.
- Auditar `#ifdef JWPLC_HAS_*` frente a `#if JWPLC_HAS_*`.
- Ejecutar `03_autoload_contract` final.
- Ejecutar gate físico final de TFT/periféricos integrados.
- Corregir gates antiguos de igualdad cruda de payload que ya no representan correctamente la semántica de archives.
- Opcional para comparación cruzada: repetir P7 cold controlado en la PC principal.