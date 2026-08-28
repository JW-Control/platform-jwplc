# v2.1.0-alpha.6 — Comparación final de tiempos Alpha5 vs Alpha6

Fecha: 2026-08-28

Branch validado:

```text
v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime
```

HEAD del benchmark final:

```text
412b5b99
```

Run Alpha6:

```text
tools/build-speed-benchmark/results/20260828_141058
Label: alpha6-final-412b5b99
```

## Objetivo

Confirmar que Alpha6 conserva el rendimiento de compilación alcanzado en Alpha5 mientras incorpora el runtime Ethernet cooperativo, diagnóstico IDLE ampliado y el archive final de `JWPLC_Display`, sin retirar periféricos del autoload normal.

## Resultados

| Target | Fase | Alpha5 (s) | Alpha6 (s) | Delta (s) | Cambio |
|---|---|---:|---:|---:|---:|
| Basic | managed_cold | 54.594 | 54.912 | +0.318 | +0.58 % |
| Basic | managed_warm_nochange | 24.804 | 21.343 | -3.461 | -13.95 % |
| Basic | managed_warm_touch | 23.760 | 21.264 | -2.496 | -10.51 % |
| Basic | explicit_cold | 55.387 | 54.241 | -1.146 | -2.07 % |
| Basic | explicit_warm_nochange | 22.462 | 20.618 | -1.844 | -8.21 % |
| Basic | explicit_warm_touch | 22.219 | 20.525 | -1.694 | -7.62 % |
| Core | managed_cold | 60.717 | 61.708 | +0.991 | +1.63 % |
| Core | managed_warm_nochange | 20.934 | 20.990 | +0.056 | +0.27 % |
| Core | managed_warm_touch | 21.085 | 20.957 | -0.128 | -0.61 % |
| Core | explicit_cold | 58.617 | 61.543 | +2.926 | +4.99 % |
| Core | explicit_warm_nochange | 21.393 | 20.462 | -0.931 | -4.35 % |
| Core | explicit_warm_touch | 21.221 | 20.989 | -0.232 | -1.09 % |

Resultado del run Alpha6:

```text
12/12 fases = OK
Basic cold compilers = 14
Core cold compilers = 77
Warm compilers = 1
Git status = clean
```

## Lectura

Los builds warm, que representan mejor el flujo normal de edición repetida en Arduino IDE, mejoran en conjunto alrededor de un 6 % frente a Alpha5.

Los builds cold permanecen en el mismo orden de magnitud. La variación media ronda +1.35 %, con el peor caso observado en `Core / explicit_cold` (+4.99 %). No se considera una regresión bloqueante porque:

- no cambia el orden de magnitud del primer build;
- Basic mejora en `explicit_cold`;
- las recompilaciones warm mejoran de forma general;
- los 12 escenarios terminan correctamente;
- no se retiró ningún periférico del autoload para obtener los tiempos.

## Conclusión

```text
ALPHA6_BUILD_SPEED=PASS
```

Alpha6 conserva el rendimiento alcanzado en Alpha5 y mejora de forma general las recompilaciones warm. No se detecta una regresión de compilación que justifique bloquear la publicación.
