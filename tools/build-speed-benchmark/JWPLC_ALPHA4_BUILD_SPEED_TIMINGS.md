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
| P2 core precompilado | 104.223 s | 34 | 51 | — | Validado para Basic. |
| P3 histórico Display | 95.172 s | 32 | 49 | 406016 B | Válido históricamente, pero con dependencias no deterministas. |
| P4 GlobalPeripherals | 97.121 s | 31 | 48 | — | Técnicamente válido, rechazado por no mejorar P3 histórico. |
| P3 determinista | 101.677 s | 32 | 49 | 406016 B | Referencia determinista posterior al bundling de dependencias. |
| P5A Ethernet W5x00 | 90.587 s | 24 | 41 | 406032 B | Validado estructuralmente. |
| P6A-2 ST77xx precompiled | 84.544 s | 20 | 37 | 405712 B | Validado estructuralmente. |
| P6B-1 GFX `src/` source-only | 83.327 s | 20 | 37 | 405712 B | Equivalencia de layout; no se toma como mejora aislada. |
| P6B-2 GFX precompiled | 77.907 s | 16 | 33 | 405472 B | Validado estructuralmente. |
| P6C-1 BusIO `src/` source-only | 73.382 s | 16 | 33 | 405472 B | Equivalencia de layout; no se toma como mejora aislada. |
| **P6C-2 / P6D full Adafruit stack** | **67.322 s** | **12** | **29** | **404912 B** | **Cerrado / validado estructuralmente.** |

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

La validación actual es estructural. Antes del cierre de alpha debe mantenerse como gate funcional una prueba física de TFT y periféricos integrados.

## Pendientes de cierre de alpha relacionados

- Validar/definir el archive de core para Basic Core, separado del archive de JWPLC Basic cuando corresponda.
- Documentar conclusión de `app-only`.
- Documentar conclusión sobre bootloader precompilado sin publicar `bootloader.bin` como configuración definitiva mientras la configuración final siga abierta.
- Dejar explícita la decisión o pendiente sobre configuración final, incluyendo Flash Frequency.
- Auditar `#ifdef JWPLC_HAS_*` frente a `#if JWPLC_HAS_*`.
- Ejecutar gate físico final de TFT/periféricos.
- Corregir gates antiguos de igualdad cruda de payload que ya no representan correctamente la semántica de archives.
