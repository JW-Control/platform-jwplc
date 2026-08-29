# v2.1.0-alpha.6 — Comparación final de tiempos Alpha5 vs Alpha6

Fecha: 2026-08-28

Branch validado:

```text
v2.1.0-alpha.6/integration/rebase-alpha5-final
```

Base funcional real de Alpha5:

```text
64068556
```

HEAD del benchmark final Alpha6:

```text
379246c9
```

Run Alpha6:

```text
tools/build-speed-benchmark/results/20260828_174534
Label: alpha6-integrated-final-379246c9
```

## Corrección de base

El benchmark anterior `20260828_141058` fue ejecutado sobre la línea Alpha6 previa a corregir su base de integración. Esa evidencia se conserva como referencia histórica, pero no se usa para la decisión final de publicación.

Alpha6 fue trasladada sobre el cierre real de Alpha5 (`64068556`) y se repitieron los gates afectados: build source, adopción/paridad de `JWPLC_Display`, cold de producción y benchmark Basic/Core.

## Resultados

| Target | Fase | Alpha5 (s) | Alpha6 (s) | Delta (s) | Cambio |
|---|---|---:|---:|---:|---:|
| Basic | managed_cold | 54.594 | 60.683 | +6.089 | +11.15 % |
| Basic | managed_warm_nochange | 24.804 | 22.122 | -2.682 | -10.81 % |
| Basic | managed_warm_touch | 23.760 | 22.922 | -0.838 | -3.53 % |
| Basic | explicit_cold | 55.387 | 60.369 | +4.982 | +8.99 % |
| Basic | explicit_warm_nochange | 22.462 | 21.774 | -0.688 | -3.06 % |
| Basic | explicit_warm_touch | 22.219 | 21.813 | -0.406 | -1.83 % |
| Core | managed_cold | 60.717 | 68.545 | +7.828 | +12.89 % |
| Core | managed_warm_nochange | 20.934 | 20.803 | -0.131 | -0.63 % |
| Core | managed_warm_touch | 21.085 | 20.589 | -0.496 | -2.35 % |
| Core | explicit_cold | 58.617 | 62.366 | +3.749 | +6.40 % |
| Core | explicit_warm_nochange | 21.393 | 20.065 | -1.328 | -6.21 % |
| Core | explicit_warm_touch | 21.221 | 19.905 | -1.316 | -6.20 % |

Resultado del run Alpha6:

```text
12/12 fases = OK
Basic cold compilers = 15
Core cold compilers = 78
Warm compilers = 1
Git status = clean
```

Alpha5 registraba 8 compiladores cold para Basic y 71 para Core. Alpha6 integrada suma 7 compilaciones source en ambos targets, asociadas al backend W5500 consolidado dentro de `JWPLC_Ethernet`.

## Lectura agregada

Promedios por target:

| Target | Cold Alpha5 | Cold Alpha6 | Cambio cold | Warm Alpha5 | Warm Alpha6 | Cambio warm |
|---|---:|---:|---:|---:|---:|---:|
| Basic | 54.991 s | 60.526 s | +10.07 % | 23.311 s | 22.158 s | -4.95 % |
| Core | 59.667 s | 65.456 s | +9.70 % | 21.158 s | 20.340 s | -3.86 % |

Promedio combinado:

```text
cold: +9.88 %
warm: -4.43 %
```

Por tanto Alpha6 no conserva exactamente el rendimiento cold de Alpha5: existe una regresión medida cercana al 10 % en builds limpios. En cambio, las recompilaciones warm mejoran en promedio y permanecen en una sola invocación de compilador.

La regresión cold se acepta como costo conocido porque Alpha6 prioriza estabilidad y corrección funcional, consolida el runtime Ethernet/W5500 cooperativo dentro de la librería activa y no elimina periféricos ni debilita el autoload para recuperar tiempo.

## Conclusión

```text
ALPHA6_BUILD_SPEED=PASS
ALPHA6_COLD_REGRESSION=ACCEPTED_KNOWN_COST
ALPHA6_WARM_AVG_IMPROVEMENT=4.43_PERCENT
```

Alpha6 queda publicable con una regresión cold explícitamente documentada y con mejora de las recompilaciones warm. La optimización de esas 7 TUs Ethernet puede retomarse en un alpha posterior sin reabrir la estabilidad funcional de Alpha6.
