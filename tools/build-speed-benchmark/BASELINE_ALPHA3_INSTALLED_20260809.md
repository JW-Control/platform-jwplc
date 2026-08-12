# Baseline oficial de compilación — v2.1.0-alpha.3 instalada — 2026-08-09

## Estado

Medición oficial de referencia para `v2.1.0-alpha.4/feature/build-speed-cache`, ejecutada contra el package instalado mediante Arduino Boards Manager:

```text
jwplc:esp32 2.1.0-alpha.3
```

No corresponde al namespace local de desarrollo.

## Entorno

```text
CPU: 13th Gen Intel Core i5-13400F
Logical cores: 16
RAM: ~23.8 GiB
OS: Windows 10 Pro 10.0.19045
PowerShell: 5.1.19041.6456
Arduino CLI: 1.0.2
Package: jwplc:esp32 2.1.0-alpha.3
FQBN: jwplc:esp32:jwplcbasic
Sketch: 01_empty
Jobs: 0
```

## Resultados

| Fase | Tiempo | Compilaciones reales |
|---|---:|---:|
| managed_cold | 136.509 s | 102 |
| managed_warm_nochange | 34.360 s | 1 |
| managed_warm_touch | 34.638 s | 1 |

## Estructura observada

Cold build:

```text
102 invocaciones de compilador
```

Warm build:

```text
17 ResolveLibrary(...)
16 pasadas xtensa-esp32-elf-g++ -E
37 objetos de librería reutilizados
1 core precompilado/reutilizado
1 compilación real del sketch
```

Conclusión: el ciclo incremental sigue gastando ~34.5 s aun cuando las librerías y el core ya están cacheados. El cuello prioritario para el warm build es library discovery/preprocessing; la precompilación queda como línea posterior para reducir el cold build.

## Convención de pruebas desde este punto

```text
jwplc:esp32:jwplcbasic       = referencia publicada/instalada
jwplc_local:esp32:jwplcbasic = experimentos de la rama Alpha4
```

Los cambios D1/P1 y posteriores deben compararse principalmente entre ejecuciones `jwplc_local` del mismo entorno, conservando esta Alpha3 instalada como referencia pública.
