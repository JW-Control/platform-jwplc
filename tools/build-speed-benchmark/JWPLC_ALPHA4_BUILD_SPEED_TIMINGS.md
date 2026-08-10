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

> Nota: los tiempos dependen del host. Para comparar etapas se priorizan runs cold controlados del mismo entorno. Los runs `source-layout` existen para validar equivalencia de layout y no se usan como prueba aislada de mejora de rendimiento.

## Tabla consolidada

| Etapa | Cold | Compiles | `g++ -E` | App | Estado / lectura |
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
| **P6C-2 / P6D full Adafruit stack** | **67.322 s** | **12** | **29** | **404912 B** | **Cerrado / validado estructuralmente.** |

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
- app binaria de `404912 B`;
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
- app de `404912 B`;
- carga física satisfactoria y reset por RTS.

Esta validación cruzada demuestra que el estado optimizado ya versionado es reproducible en un segundo equipo y no depende de archives locales no publicados.

## Runs preservados principales

- P3 determinista: `tools/build-speed-benchmark/p3-deterministic-work/20260809_190321/p3-Basic`
- P5A Ethernet: `tools/build-speed-benchmark/p5a-ethernet-work/20260809_224628/p5a-Basic`
- P6A-2 ST77xx: `tools/build-speed-benchmark/p6a2-st77xx-precompiled-work/20260809_232946/p6a2-Basic`
- P6B-1 GFX source-layout: `tools/build-speed-benchmark/p6b-gfx-layout-work/20260809_234653/p6b-layout-Basic`
- P6B-2 GFX precompiled: `tools/build-speed-benchmark/p6b2-gfx-precompiled-work/20260809_235326/p6b2-Basic`
- P6C-1 BusIO source-layout: `tools/build-speed-benchmark/p6c-busio-layout-work/20260810_000915/p6c-layout-Basic`
- P6C-2 BusIO / full stack: `tools/build-speed-benchmark/p6c2-busio-precompiled-work/20260810_002028/p6c2-Basic`

## Reducción acumulada del estado P6 final

Tomando `67.322 s` como cold candidato final de P6:

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

## Estado técnico del stack Adafruit al cierre P6

Las tres librerías bundled quedan preparadas con layout `src/` y `precompiled=full` para ESP32:

- `Adafruit_ST7735_and_ST7789_Library`: archive con 4 miembros; el firmware Basic extrae sólo los objetos necesarios para ST7789/ST77xx.
- `Adafruit_GFX_Library`: archive con 4 miembros; el enlazador evita componentes no usados como `Adafruit_GrayOLED.cpp.o`.
- `Adafruit_BusIO`: archive con 4 miembros; el firmware Basic actual extrae `Adafruit_SPIDevice.cpp.o` y deja fuera los miembros no utilizados.

La reducción de tamaño de la app observada al pasar de objetos directos a archives es consistente con la extracción selectiva del linker. Los gates estructurales verificaron selección de librerías, símbolos globales, mapa de enlace y objetos externos relevantes; no se usa igualdad byte-a-byte del `.bin` como requisito cuando cambia la semántica de extracción de archives.

## Conclusión de esta fase

P6 queda cerrado con un cold candidato de **67.322 s**, frente a **136.509 s** del baseline oficial histórico y **148.649 s** del baseline local pre-D1.

Esto representa aproximadamente **50.68 % menos tiempo frente al baseline oficial** y **54.71 % menos frente al baseline local pre-D1**, manteniendo el autoload normal del JWPLC Basic.

La validación estructural P6 cuenta ahora con validación física real en Arduino IDE en dos equipos distintos para el mecanismo P2 ya promovido a configuración oficial de `JWPLC Basic`. Antes del cierre de alpha debe mantenerse como gate funcional una prueba física de TFT y periféricos integrados bajo la configuración final.

## Pendientes de cierre de alpha relacionados

- Generar/validar el archive de core para `JWPLC Basic Core`, separado del archive de `JWPLC Basic`.
- Dejar explícita la decisión o pendiente sobre configuración final, incluyendo Flash Frequency.
- Auditar `#ifdef JWPLC_HAS_*` frente a `#if JWPLC_HAS_*`.
- Ejecutar `03_autoload_contract` final.
- Ejecutar gate físico final de TFT/periféricos.
- Corregir gates antiguos de igualdad cruda de payload que ya no representan correctamente la semántica de archives.
