# Alpha12 — Comparación final de tiempos de compilación

Fecha: 2026-10-04

Branch:

```text
v2.1.0-alpha.12/feature/modbus-tcp
```

HEAD del benchmark:

```text
1cd1c576e4050476bda5b864ab6b006379a27337
```

Run:

```text
tools/alpha12/results/final_build_speed_benchmark/20261004_135521
```

## Entorno Alpha12

```text
Host: PC-MASTER-RACE
CPU: 13th Gen Intel Core i5-13400F
Logical cores: 16
RAM: 25593896960 bytes (~23.8 GiB)
OS: Windows 10 Pro 10.0.19045
PowerShell: 7.6.6
Arduino CLI: 1.0.2
Package namespace: jwplc_local
Jobs: 0
```

Este host coincide con la PC principal usada en las comparaciones formales de
Alpha4/Alpha5. Alpha5 documentó explícitamente `PC-MASTER-RACE`,
`i5-13400F`, 16 logical cores y Arduino CLI 1.0.2.

## Resultado Alpha12

### JWPLC Basic

| Fase | Tiempo | Compiladores |
|---|---:|---:|
| managed cold | 66.983 s | 20 |
| managed warm no-change | 18.004 s | 1 |
| managed warm touch | 17.988 s | 1 |
| explicit cold | 66.986 s | 20 |
| explicit warm no-change | 17.893 s | 1 |
| explicit warm touch | 17.663 s | 1 |

### JWPLC Basic Core

| Fase | Tiempo | Compiladores |
|---|---:|---:|
| managed cold | 75.242 s | 83 |
| managed warm no-change | 17.277 s | 1 |
| managed warm touch | 17.143 s | 1 |
| explicit cold | 71.672 s | 83 |
| explicit warm no-change | 17.282 s | 1 |
| explicit warm touch | 17.215 s | 1 |

Resultado:

```text
ALPHA12_FINAL_BUILD_SPEED_BENCHMARK=PASS
ALPHA12_BUILD_SPEED_MATRIX_COMPLETE=YES
RESULT_ROW_COUNT=12
JOBS=0
UPLOADS=SKIPPED
PRECOMPILED_FREEZE=PASS
```

## Comparación Alpha5 vs Alpha12

Alpha5 fue medida formalmente en la misma PC principal.

### Basic

| Fase | Alpha5 | Alpha12 | Delta | Cambio |
|---|---:|---:|---:|---:|
| managed cold | 54.594 s | 66.983 s | +12.389 s | +22.69 % |
| managed warm no-change | 24.804 s | 18.004 s | -6.800 s | -27.41 % |
| managed warm touch | 23.760 s | 17.988 s | -5.772 s | -24.29 % |
| explicit cold | 55.387 s | 66.986 s | +11.599 s | +20.94 % |
| explicit warm no-change | 22.462 s | 17.893 s | -4.569 s | -20.34 % |
| explicit warm touch | 22.219 s | 17.663 s | -4.556 s | -20.50 % |

Estructura cold:

```text
Alpha5 Basic  = 8 TUs
Alpha12 Basic = 20 TUs
Delta         = +12 TUs
```

### Core

| Fase | Alpha5 | Alpha12 | Delta | Cambio |
|---|---:|---:|---:|---:|
| managed cold | 60.717 s | 75.242 s | +14.525 s | +23.92 % |
| managed warm no-change | 20.934 s | 17.277 s | -3.657 s | -17.47 % |
| managed warm touch | 21.085 s | 17.143 s | -3.942 s | -18.70 % |
| explicit cold | 58.617 s | 71.672 s | +13.055 s | +22.27 % |
| explicit warm no-change | 21.393 s | 17.282 s | -4.111 s | -19.22 % |
| explicit warm touch | 21.221 s | 17.215 s | -4.006 s | -18.88 % |

Estructura cold:

```text
Alpha5 Core  = 71 TUs
Alpha12 Core = 83 TUs
Delta        = +12 TUs
```

Lectura:

- Alpha12 tiene regresión cold de ~21–24 % frente a Alpha5.
- Alpha12 mejora todos los warm de Alpha5 en ~17–27 %.
- El aumento cold coincide con +12 TUs source en ambos targets.
- Warm mantiene una sola invocación de compilador.

## Comparación Alpha6 vs Alpha12

Alpha6 ya había aceptado +7 TUs source por Ethernet respecto a Alpha5.

### Basic

| Fase | Alpha6 | Alpha12 | Cambio |
|---|---:|---:|---:|
| managed cold | 60.683 s | 66.983 s | +10.38 % |
| managed warm no-change | 22.122 s | 18.004 s | -18.61 % |
| managed warm touch | 22.922 s | 17.988 s | -21.52 % |
| explicit cold | 60.369 s | 66.986 s | +10.96 % |
| explicit warm no-change | 21.774 s | 17.893 s | -17.82 % |
| explicit warm touch | 21.813 s | 17.663 s | -19.02 % |

Estructura:

```text
Alpha6 Basic  = 15 TUs
Alpha12 Basic = 20 TUs
Delta         = +5 TUs
```

### Core

| Fase | Alpha6 | Alpha12 | Cambio |
|---|---:|---:|---:|
| managed cold | 68.545 s | 75.242 s | +9.77 % |
| managed warm no-change | 20.803 s | 17.277 s | -16.95 % |
| managed warm touch | 20.589 s | 17.143 s | -16.74 % |
| explicit cold | 62.366 s | 71.672 s | +14.92 % |
| explicit warm no-change | 20.065 s | 17.282 s | -13.87 % |
| explicit warm touch | 19.905 s | 17.215 s | -13.51 % |

Estructura:

```text
Alpha6 Core  = 78 TUs
Alpha12 Core = 83 TUs
Delta        = +5 TUs
```

## Comparación Alpha8 R2 vs Alpha12

Alpha8 R2 se documentó sobre Intel i5-13400F, con la misma estructura de
15 TUs Basic / 78 TUs Core.

### Basic

| Fase | Alpha8 R2 | Alpha12 | Cambio |
|---|---:|---:|---:|
| managed cold | 70.698 s | 66.983 s | -5.25 % |
| managed warm no-change | 24.073 s | 18.004 s | -25.21 % |
| managed warm touch | 23.811 s | 17.988 s | -24.45 % |
| explicit cold | 65.384 s | 66.986 s | +2.45 % |
| explicit warm no-change | 24.376 s | 17.893 s | -26.59 % |
| explicit warm touch | 24.994 s | 17.663 s | -29.33 % |

### Core

| Fase | Alpha8 R2 | Alpha12 | Cambio |
|---|---:|---:|---:|
| managed cold | 84.020 s | 75.242 s | -10.45 % |
| managed warm no-change | 26.761 s | 17.277 s | -35.44 % |
| managed warm touch | 26.112 s | 17.143 s | -34.35 % |
| explicit cold | 77.398 s | 71.672 s | -7.40 % |
| explicit warm no-change | 27.830 s | 17.282 s | -37.90 % |
| explicit warm touch | 26.997 s | 17.215 s | -36.23 % |

Alpha12 es claramente mejor que el run Alpha8 R2 en warm y también mejora tres
de los cuatro cold comparables, pese a compilar 5 TUs más.

## Referencia Alpha4 / Alpha3 en PC principal

### Alpha4 P6

```text
Alpha4 P6 explicit cold = 67.322 s / 12 TUs
Alpha12 explicit cold   = 66.986 s / 20 TUs
Delta                   = -0.336 s / -0.50 %
```

Alpha12 iguala prácticamente el cold P6 de Alpha4 aun compilando 8 TUs más.

### Baseline histórico

```text
Alpha3 oficial managed cold = 136.509 s
Alpha12 managed cold        = 66.983 s
Mejora                      = -69.526 s / -50.93 %
```

Frente al baseline local pre-D1 de Alpha4:

```text
148.649 s -> 66.986 s
Reducción = 81.663 s / 54.94 %
```

## Promedios agregados

Promedio de las dos fases cold por target y cuatro fases warm por target:

| Target | Métrica | Alpha5 | Alpha6 | Alpha12 |
|---|---|---:|---:|---:|
| Basic | cold avg | 54.991 s | 60.526 s | 66.985 s |
| Basic | warm avg | 23.311 s | 22.158 s | **17.887 s** |
| Core | cold avg | 59.667 s | 65.456 s | 73.457 s |
| Core | warm avg | 21.158 s | 20.340 s | **17.229 s** |

Promedio combinado Basic + Core:

```text
Alpha5 cold avg  = 57.329 s
Alpha6 cold avg  = 62.991 s
Alpha12 cold avg = 70.221 s

Alpha5 warm avg  = 22.235 s
Alpha6 warm avg  = 21.249 s
Alpha12 warm avg = 17.558 s
```

Cambio agregado Alpha12:

```text
vs Alpha5 cold: +22.49 %
vs Alpha5 warm: -21.04 %

vs Alpha6 cold: +11.48 %
vs Alpha6 warm: -17.37 %
```

## Interpretación técnica

Alpha12 no supera el mínimo cold de Alpha5 porque la arquitectura actual deja
más unidades de traducción en source.

Sin embargo:

1. El warm de Alpha12 es el mejor de la serie formal comparable.
2. Sólo se recompila 1 TU en todos los warm.
3. Alpha12 conserva todo el autoload normal.
4. No se han precompilado RTC, GlobalPeripherals, Ethernet ni RS485 sólo para
   perseguir una cifra menor.
5. Alpha12 incorpora una cantidad de funcionalidad muy superior a Alpha5:
   Ethernet/W5500 endurecido, Modbus TCP Client/Server, RTU actualizado,
   coexistencia TCP/RTU/UDP, DataLog, Display/TFT migrado y demás cierres de
   runtime de la línea posterior.
6. El cold puede optimizarse en un alpha futuro atacando específicamente las
   20 TUs Basic / 83 TUs Core restantes sin romper la política de estabilidad.

## Conclusión

```text
ALPHA12_BUILD_SPEED_BENCHMARK=PASS
ALPHA12_COLD_VS_ALPHA5=REGRESSION_EXPLAINED_BY_SOURCE_TUS
ALPHA12_COLD_VS_ALPHA6=REGRESSION_EXPLAINED_BY_SOURCE_TUS
ALPHA12_WARM_VS_ALPHA5=IMPROVED
ALPHA12_WARM_VS_ALPHA6=IMPROVED
ALPHA12_WARM_BEST_FORMAL_SERIES=YES
AUTOLOAD_PERIPHERALS_REMOVED=NO
PRECOMPILED_FREEZE=PASS
```

La optimización cold posterior debe centrarse en las TUs source restantes y no
en retirar periféricos ni introducir precompilados arquitectónicamente inseguros.
