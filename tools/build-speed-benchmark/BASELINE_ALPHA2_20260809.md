# Baseline de compilación — Alpha2 — 2026-08-09

## Estado

Este documento registra una medición recibida durante el desarrollo de `v2.1.0-alpha.4/feature/build-speed-cache`.

La ejecución fue lanzada con la intención de medir Alpha3 instalada, pero la evidencia del propio benchmark mostró que el namespace `jwplc` instalado correspondía realmente a:

```text
jwplc:esp32 2.1.0-alpha.2
```

Por tanto, estos resultados se conservan como **baseline histórico Alpha2** y no sustituyen el baseline requerido de Alpha3.

## Equipo

```text
CPU: 13th Gen Intel Core i5-13400F
Logical cores: 16
RAM: 25,593,896,960 bytes (~23.8 GiB)
OS: Windows 10 Pro 10.0.19045
PowerShell: 5.1.19041.6456
Arduino CLI: 1.0.2
Storage del proyecto: SSD/M.2 según reporte del operador
```

La GPU no se considera relevante para el proceso de compilación.

## Sketch

`01_empty`:

```cpp
void setup()
{
}

void loop()
{
}
```

No contiene includes ni llamadas explícitas al ecosistema JWPLC. Por tanto, las librerías detectadas son consecuencia del autoload del package.

## Resultados JWPLC Basic

| Fase | Tiempo | Invocaciones de compilador |
|---|---:|---:|
| managed_cold | 137.371 s | 102 |
| managed_warm_nochange | 31.777 s | 1 |
| managed_warm_touch | 32.311 s | 1 |
| explicit_cold | 118.825 s | 102 |
| explicit_warm_nochange | 31.916 s | 1 |
| explicit_warm_touch | 31.602 s | 1 |

## Descomposición del cold build

Las 102 invocaciones `-MMD -c` se distribuyeron así:

```text
Sketch:      1
Core:       64
Librerías:  37
Total:     102
```

Principales grupos de objetos de librería:

```text
Ethernet:                           8
Adafruit BusIO:                     4
Adafruit ST7735/ST7789:             4
Adafruit GFX:                       4
SD:                                 3
JWPLC_Display:                      2
FS:                                 2
JWPLC_GlobalPeripherals:            1
JW_RTC:                             1
JW_FRAM:                            1
SPI:                                1
JW_SD:                              1
JW_MatrixButtons:                   1
JWPLC_Ethernet:                     1
JWPLC_RS485:                        1
JWPLC_ModbusRTU:                    1
Wire:                               1
```

## Hallazgo principal: warm build

En el warm build:

- sólo se recompiló el `.ino`;
- los 37 objetos de librerías reportaron `Using previously compiled file`;
- el core reportó `Using precompiled core`;
- aun así el build completo necesitó aproximadamente 32 s.

Esto demuestra que **la recompilación de las librerías no es el único cuello de botella del ciclo normal de edición**.

Precompilar librerías debe reducir de forma importante el cold build, pero no puede asumirse que por sí solo reduzca el warm build de ~32 s a un valor aceptable.

## Library discovery / preprocessing

En el log warm se observaron:

```text
17 llamadas ResolveLibrary(...)
37 entradas Using cached library dependencies
16 ejecuciones del preprocesador xtensa-esp32-elf-g++ con -E
1 compilación real del sketch
1 core.a reutilizado
```

Las ejecuciones `-E` vuelven a procesar `01_empty.ino.cpp` con el conjunto de includes que va creciendo durante el descubrimiento de librerías.

Esto convierte al **library discovery + preprocessing** en un candidato prioritario de optimización.

## Grafo detectado en el sketch vacío

El build reportó las siguientes librerías:

```text
JWPLC_Display
JWPLC_GlobalPeripherals
JW_RTC
JW_FRAM
Adafruit BusIO
SPI
JW_SD
SD
FS
JW_MatrixButtons
JWPLC_Ethernet
Ethernet
JWPLC_RS485
JWPLC_ModbusRTU
Adafruit ST7735 and ST7789 Library
Adafruit GFX Library
Wire
```

La comodidad de autoload se mantiene como requisito. No se propone eliminar estas capacidades sólo para reducir el benchmark.

## Duplicados detectados

El log también indicó múltiples candidatos para:

```text
Adafruit_SPIDevice.h
SD.h
Adafruit_ST7789.h
Adafruit_GFX.h
```

En varios casos se seleccionaron librerías instaladas en el sketchbook del usuario en vez de las copias bundled del package.

Esto debe revisarse por dos motivos:

1. coste adicional de resolución;
2. reproducibilidad del build entre PCs con librerías de usuario diferentes.

No se atribuye todavía una parte concreta de los 32 s a este punto.

## Interpretación inicial

Alpha4 debe trabajar en dos líneas paralelas:

### Línea A — cold build

- precompilar librerías estables;
- evaluar reutilización/precompilación del runtime común;
- medir coste del core;
- mantener fuentes como fallback;
- no eliminar periféricos.

### Línea B — warm build

- reducir library discovery;
- reducir parsing repetido de headers pesados;
- evaluar `build.library_discovery_phase`;
- mantener headers públicos livianos;
- medir por separado preprocessing, linking y generación de binarios.

## CPU

Durante la prueba se observó uso global de CPU bajo/moderado en parte de la ejecución pese a `-j 0`.

No se concluye todavía que exista un problema de paralelismo: discovery, preprocessing, linking y objcopy contienen fases seriales y una captura puntual del Administrador de tareas no permite asignar tiempo a cada fase.

## Pendientes inmediatos

- [ ] Completar resultado Basic Core de la misma corrida Alpha2 si se desea conservar comparativa histórica.
- [ ] Ejecutar baseline contra Alpha3 real o contra `jwplc_local` basado exactamente en `555bfb2` antes de cambios funcionales.
- [ ] Instrumentar tiempos por fase del warm build.
- [ ] Probar optimización de library discovery antes de concluir que `.a` es suficiente.
- [ ] Mantener precompilación como línea prioritaria para cold build.
- [ ] Medir full upload y app-only después del baseline correcto.
