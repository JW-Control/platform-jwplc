# D1 — Library discovery liviano — resultados 2026-08-09

## Objetivo

Reducir el coste del ciclo incremental del JWPLC Basic sin retirar ningún periférico del autoload normal.

D1 usa `{build.library_discovery_phase}` para evitar expandir `JWPLC_GlobalPeripherals.h` durante las pasadas de dependency/library discovery, manteniendo `JWPLC_Display_API.h` visible y restaurando el árbol completo durante la compilación normal.

Commit medido:

```text
a78b14e32c883f2e3a7670b2ddf6ab00415c1641
```

FQBN:

```text
jwplc_local:esp32:jwplcbasic
```

## Equipo

```text
CPU: Intel Core i5-13400F
Logical cores: 16
RAM: ~23.8 GiB
OS: Windows 10 Pro 10.0.19045
PowerShell: 5.1.19041.6456
Arduino CLI: 1.0.2
Jobs: 0
```

## Resultados D1

| Fase | Alpha3/base local | D1 | Reducción |
|---|---:|---:|---:|
| managed_cold | 148.649 s | 121.732 s | 18.1 % |
| managed_warm_nochange | 36.523 s | 14.157 s | 61.2 % |
| managed_warm_touch | 40.524 s | 14.074 s | 65.3 % |

Referencia pública Alpha3 instalada (`jwplc:esp32:jwplcbasic`):

```text
managed_cold:          136.509 s
managed_warm_nochange:  34.360 s
managed_warm_touch:     34.638 s
```

La comparación causal principal debe hacerse contra Alpha3/base local porque mantiene el mismo namespace y ruta de desarrollo.

## Evidencia estructural

### Antes de D1 — warm

```text
ResolveLibrary(...):                17
xtensa-esp32-elf-g++ -E:            16
Using cached library dependencies:  37
Using previously compiled file:     37
Using precompiled core:              1
Compilaciones reales -MMD -c:        1
```

### D1 — warm

```text
ResolveLibrary(...):                17
xtensa-esp32-elf-g++ -E:             3
Using cached library dependencies:  37
Using previously compiled file:     37
Using precompiled core:              1
Compilaciones reales -MMD -c:        1
```

D1 reduce por tanto las pasadas de preprocesamiento del sketch de 16 a 3, una reducción de 81.25 %, sin cambiar la cantidad de librerías que Arduino mantiene en el build.

`ResolveLibrary` permanece en 17 porque las dependencias siguen declaradas y siendo resueltas por las metadata `depends=` de las librerías. Este comportamiento es deseado: D1 evita expansión textual repetida, no elimina dependencias.

## Cold build

El cold D1 sigue mostrando:

```text
102 compilaciones reales
56 pasadas g++ -E
17 ResolveLibrary(...)
```

Es la misma estructura que el baseline previo. Por ello no se atribuye todavía toda la reducción observada de ~27 s en cold a D1; pueden intervenir cachés del sistema, filesystem, antivirus/indexado y variación de ejecución.

La optimización del cold build sigue requiriendo una fase independiente de precompilación.

## Tamaño de firmware

Baseline Alpha3/base local:

```text
Sketch uses 405853 bytes
Global variables use 27908 bytes
```

D1:

```text
Sketch uses 405853 bytes
Global variables use 27908 bytes
```

Resultado: tamaño idéntico. D1 no obtiene la mejora retirando funcionalidad del firmware.

## Interpretación

D1 valida la hipótesis de que el cuello warm estaba dominado en buena parte por dependency discovery/preprocessing repetido.

Resultado práctico en PC principal:

```text
editar sketch -> recompilar
~36–41 s antes
~14 s con D1
```

Esto equivale aproximadamente a 2.6–2.9 veces más rápido en el ciclo warm medido.

## Riesgo pendiente

Durante discovery se oculta el header pesado `JWPLC_GlobalPeripherals.h`, aunque durante el build normal vuelve a estar presente.

Antes de consolidar D1 se debe validar que:

- APIs globales RTC/FRAM/SD/botonera sigan disponibles sin includes manuales;
- API Display siga disponible;
- I/O nativo siga disponible;
- sketch con tipos de librería en funciones de usuario compile correctamente;
- Basic Core no quede roto;
- Arduino IDE compile igual que Arduino CLI.

## Próximo paso

Ejecutar un smoke compile de contrato de autoload antes de iniciar P1 (precompilación).

Estado:

```text
D1 rendimiento 01_empty: APROBADO provisional
D1 contrato autoload: pendiente
P1 precompilación: pendiente
```
