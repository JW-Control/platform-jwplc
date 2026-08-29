# Alpha5 - Impacto del perfil energético en laptop

Fecha: 2026-08-24

Rama:

`v2.1.0-alpha.5/feature/esp32-precompiled-compatibility`

Commit:

`707416026ddd7403b3be825702a4c6b2fd550e83`

Arduino CLI:

`1.5.1`

Package:

`jwplc_local:esp32 2.1.0-dev`

Sketch:

`tools/build-speed-benchmark/sketches/01_empty`

## 1. Objetivo

Caracterizar el efecto del estado de alimentación y del perfil energético
de Windows sobre los tiempos de compilación de Alpha5 en una laptop de
validación.

La comparación controlada principal mantiene el perfil Equilibrado y cambia
únicamente entre cargador conectado y funcionamiento con batería.

---

## 2. Comparación controlada: Equilibrado AC vs batería

Runs:

- AC: `20260824_163912`
- Batería: `20260824_165541`

### JWPLC Basic

| Fase | AC + Equilibrado | Batería + Equilibrado | Diferencia | Penalización |
|---|---:|---:|---:|---:|
| managed cold | 66.475 s | 87.032 s | +20.557 s | +30.92 % |
| managed warm no-change | 9.062 s | 11.886 s | +2.824 s | +31.16 % |
| managed warm touch | 9.884 s | 11.658 s | +1.774 s | +17.95 % |
| explicit cold | 76.694 s | 87.533 s | +10.839 s | +14.13 % |
| explicit warm no-change | 8.630 s | 11.286 s | +2.656 s | +30.78 % |
| explicit warm touch | 8.749 s | 11.556 s | +2.807 s | +32.08 % |

En todos los cold de JWPLC Basic se observaron:

`8 compilaciones`

Por tanto, la diferencia de tiempo no proviene de un cambio en la cantidad
de TUs compiladas.

### JWPLC Basic Core

| Fase | AC + Equilibrado | Batería + Equilibrado | Diferencia | Penalización |
|---|---:|---:|---:|---:|
| managed cold | 83.996 s | 103.960 s | +19.964 s | +23.77 % |
| managed warm no-change | 8.389 s | 11.881 s | +3.492 s | +41.63 % |
| managed warm touch | 8.371 s | 11.510 s | +3.139 s | +37.50 % |
| explicit cold | 76.500 s | 102.384 s | +25.884 s | +33.84 % |
| explicit warm no-change | 7.999 s | 10.928 s | +2.929 s | +36.62 % |
| explicit warm touch | 8.048 s | 10.823 s | +2.775 s | +34.48 % |

En los cold de JWPLC Basic Core se observaron:

`71 compilaciones`

El deterioro bajo batería también afecta al target Core, por lo que no se
considera un comportamiento específico de la arquitectura precompilada de
JWPLC Basic.

---

## 3. Lectura práctica para JWPLC Basic

En el ciclo típico de edición:

`editar sketch -> compilar/subir nuevamente`

la métrica `explicit warm touch` pasó de:

`8.749 s`

con cargador y perfil Equilibrado a:

`11.556 s`

en batería con el mismo perfil.

Esto representa:

`+2.807 s / +32.08 %`

Aun funcionando con batería, el ciclo incremental permanece alrededor de
11.6 segundos para el sketch de referencia.

---

## 4. Comparación estructural Basic vs Core

### AC + Equilibrado

Managed cold:

| Target | Tiempo | Compilaciones |
|---|---:|---:|
| JWPLC Basic | 66.475 s | 8 |
| JWPLC Basic Core | 83.996 s | 71 |

JWPLC Basic evita 63 compilaciones y tarda 17.521 s menos en este run.

El `explicit cold` de este mismo run quedó prácticamente empatado:

| Target | Tiempo |
|---|---:|
| JWPLC Basic | 76.694 s |
| JWPLC Basic Core | 76.500 s |

La diferencia de 0.194 s no se considera significativa y se interpreta
como dispersión de una ejecución individual.

### Batería + Equilibrado

| Target | explicit cold | Compilaciones |
|---|---:|---:|
| JWPLC Basic | 87.533 s | 8 |
| JWPLC Basic Core | 102.384 s | 71 |

En este run JWPLC Basic tarda 14.851 s menos.

---

## 5. Otros perfiles observados

También se realizaron ejecuciones exploratorias con otros perfiles.

| Escenario | Basic managed cold | Basic explicit cold | Basic explicit warm touch |
|---|---:|---:|---:|
| AC + máxima potencia | 87.190 s | 73.784 s | 9.653 s |
| AC + Equilibrado | 66.475 s | 76.694 s | 8.749 s |
| Batería + Equilibrado | 87.032 s | 87.533 s | 11.556 s |
| Batería + ejecución equilibrada previa | 103.831 s | 102.577 s | 13.066 s |
| Batería + mínimo consumo | 131.207 s | 133.458 s | 14.011 s |

Estos runs muestran una influencia importante del estado energético del
equipo, pero no se utilizan para calcular una relación causal exacta entre
perfiles debido a la dispersión observada entre ejecuciones individuales.

En particular, un único run de Máxima potencia no resultó consistentemente
más rápido que uno Equilibrado.

---

## 6. Conclusión

La comparación controlada Equilibrado AC vs batería confirma que el estado
de alimentación de la laptop afecta de forma material los tiempos de
compilación.

Para JWPLC Basic:

- managed cold: +30.92 % en batería;
- explicit cold: +14.13 % en batería;
- explicit warm no-change: +30.78 % en batería;
- explicit warm touch: +32.08 % en batería.

El número de unidades compiladas permanece idéntico:

`8 TUs`

Por tanto, la variación observada corresponde al rendimiento disponible del
host y no a un cambio de comportamiento del package.

La recomendación para benchmarks formales de velocidad es:

`LAPTOP_BENCHMARK_POWER=AC_CONNECTED`

Para uso cotidiano en batería, Alpha5 mantiene compilaciones incrementales
del sketch de referencia cercanas a 11-12 segundos bajo perfil Equilibrado.

Resultado:

`ALPHA5_LAPTOP_POWER_COMPARISON=PASS`

`AC_BALANCED_VS_BATTERY_BALANCED=VALID_COMPARISON`

`BATTERY_PERFORMANCE_PENALTY=CONFIRMED`
