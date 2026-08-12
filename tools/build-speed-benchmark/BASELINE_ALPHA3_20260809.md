# Baseline de compilación — Alpha3/base Alpha4 — 2026-08-09

## Estado

Medición ejecutada sobre `jwplc_local:esp32:jwplcbasic` enlazado al árbol `JWPLC/2.1.0` de la rama `v2.1.0-alpha.4/feature/build-speed-cache` antes de cambios funcionales del package.

La rama parte de `release/v2.1.x` en `555bfb2`, estado publicado como `v2.1.0-alpha.3`. Los commits existentes en la feature hasta esta medición sólo agregaban herramientas/documentación de benchmark.

El package público `jwplc:esp32` ya figuraba instalado como `2.1.0-alpha.3`, pero esta corrida usa explícitamente el namespace local para medir el código base de la feature.

## Equipo

```text
CPU: 13th Gen Intel Core i5-13400F
Logical cores: 16
RAM: 25,593,896,960 bytes (~23.8 GiB)
OS: Windows 10 Pro 10.0.19045
PowerShell: 5.1.19041.6456
Arduino CLI: 1.0.2
Storage del proyecto: SSD/M.2 según reporte del operador
Jobs: 0 (todos los cores lógicos disponibles)
```

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

No contiene includes ni llamadas explícitas JWPLC. El grafo de librerías detectado proviene del autoload del package.

## Resultados JWPLC Basic

| Fase | Tiempo | Invocaciones de compilador |
|---|---:|---:|
| managed_cold | 148.649 s | 102 |
| managed_warm_nochange | 36.523 s | 1 |
| managed_warm_touch | 40.524 s | 1 |
| explicit_cold | 125.964 s | 102 |
| explicit_warm_nochange | 36.059 s | 1 |
| explicit_warm_touch | 38.529 s | 1 |

Aplicación generada:

```text
01_empty.ino.bin: 406,000 bytes
bootloader:        25,024 bytes
partitions:         3,072 bytes
merged image:   4,194,304 bytes
```

`BinaryBytes` del CSV suma todos los `.bin` presentes y no debe confundirse con el tamaño de la aplicación.

## Grafo de discovery observado

En warm build se repite la misma estructura detectada en Alpha2:

```text
17 ResolveLibrary(...)
16 ejecuciones g++ -E sobre 01_empty.ino.cpp
37 objetos de librerías reutilizados
1 core precompilado/reutilizado
1 compilación real del sketch
```

Secuencia principal de headers descubiertos durante las pasadas `-E`:

```text
1  JWPLC_Display_Auto.h
2  JWPLC_GlobalPeripherals.h
3  JW_RTC.h
4  JW_FRAM.h
5  Adafruit_SPIDevice.h
6  SPI.h
7  JW_SD.h
8  SD.h
9  FS.h
10 JW_MatrixButtons.h
11 JWPLC_Ethernet.h
12 Ethernet.h
13 JWPLC_RS485.h
14 JWPLC_ModbusRTU.h
15 Adafruit_ST7789.h
16 pasada final con el conjunto de includes resuelto
```

El comando de preprocesamiento va agregando rutas `-I` de forma incremental, por lo que el mismo sketch vacío se vuelve a preprocesar múltiples veces antes de llegar a la compilación real.

## Hallazgo principal

La caché ya elimina casi toda la recompilación de librerías en el ciclo warm, pero no elimina el coste de discovery/preprocessing.

Por ello existen dos problemas distintos:

### Cold build

```text
102 compilaciones reales
~126–149 s
```

Candidato principal: precompilación de core/librerías estables y reutilización de archivos `.a` compatibles.

### Warm build

```text
1 compilación real
~36–41 s
```

Candidato principal: reducir library discovery y parsing repetido de headers durante las pasadas `-E`.

Precompilar librerías por sí solo no debe considerarse suficiente para resolver la experiencia de edición normal.

## Comparación con baseline Alpha2

Alpha2 Basic medido anteriormente:

| Fase | Alpha2 | Alpha3/base | Diferencia observada |
|---|---:|---:|---:|
| managed_cold | 137.371 s | 148.649 s | +11.278 s |
| managed_warm_nochange | 31.777 s | 36.523 s | +4.746 s |
| managed_warm_touch | 32.311 s | 40.524 s | +8.213 s |
| explicit_cold | 118.825 s | 125.964 s | +7.139 s |
| explicit_warm_nochange | 31.916 s | 36.059 s | +4.143 s |
| explicit_warm_touch | 31.602 s | 38.529 s | +6.928 s |

No declarar regresión de Alpha3 con esta única comparación porque cambian simultáneamente:

- ubicación del package (`Arduino15` instalado vs repo enlazado localmente);
- estado de filesystem/cachés del sistema;
- posible intervención de antivirus/indexado;
- ejecución temporal independiente.

La estructura de build sí permanece equivalente: 102 compilaciones cold y 1 compilación warm en ambas corridas.

## Librerías duplicadas detectadas

Arduino resuelve algunas dependencias desde el sketchbook del usuario y no desde la copia bundled del package:

```text
Adafruit BusIO
Ethernet
Adafruit ST7735 and ST7789 Library
Adafruit GFX Library
```

Además reporta múltiples candidatos para:

```text
Adafruit_SPIDevice.h
SD.h
Adafruit_ST7789.h
Adafruit_GFX.h
```

Esto debe revisarse por reproducibilidad y por posible coste adicional de resolución. No se atribuye todavía un tiempo concreto a este punto.

## Basic vs Basic Core — evidencia Alpha2

El baseline completo Alpha2 mostró:

```text
Basic cold: 102 compilaciones
Core  cold: 102 compilaciones
Basic warm: 1 compilación
Core  warm: 1 compilación
```

Tiempos warm:

```text
Basic: ~31.6–32.3 s
Core:  ~32.3–33.9 s
```

Aplicación Alpha2:

```text
Basic: 405,904 bytes
Core:  354,912 bytes
```

Conclusión provisional: Basic Core genera menos firmware por las macros de perfil, pero paga prácticamente el mismo coste de discovery/build. Esto favorece una estrategia de precompilación común con una capa mínima dependiente del perfil cuando sea necesario.

## Próximo experimento recomendado

Antes de generar `.a`, probar una optimización controlada de library discovery usando la propiedad oficial de Arduino:

```text
build.library_discovery_phase
```

Objetivo:

- mantener el autoload completo;
- mantener las APIs públicas;
- evitar que durante discovery se expandan headers pesados innecesarios;
- reducir el número/coste de pasadas `g++ -E`;
- repetir exactamente el benchmark `01_empty`.

Después, atacar el cold build mediante precompilación.

## Estado Alpha4

```text
Baseline Alpha2: registrado
Baseline Alpha3/base: registrado
Optimización discovery: pendiente
Precompilación: pendiente
App-only: pendiente de medición física
Bootloader precompilado: pendiente; no definitivo
FlashFreq: sin decisión final nueva
```
