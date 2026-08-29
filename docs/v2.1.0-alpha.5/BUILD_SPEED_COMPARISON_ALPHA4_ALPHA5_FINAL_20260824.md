# Alpha5 - Comparación final de tiempos frente a Alpha4

Fecha: 2026-08-24

Rama:

`v2.1.0-alpha.5/feature/esp32-precompiled-compatibility`

Commit Alpha5 medido:

`c181a27`

Benchmark Alpha5 final:

`20260824_134822`

Label:

`alpha5-final-c181a27`

## 1. Objetivo

Comparar el rendimiento final de compilación de Alpha5 frente a los hitos
formales de Alpha4, priorizando comparaciones realizadas en el mismo host
y con un método equivalente.

Alpha5 no busca únicamente reducir tiempo de compilación.

También debía recuperar y mantener compatibilidad entre:

- `JWPLC Basic`;
- `JWPLC Basic Core`;
- `ESP32 Board` genérico;
- Arduino IDE;
- librerías precompiladas compartidas.

Sin eliminar periféricos del autoload normal ni romper APIs ya validadas.

---

## 2. Estado final Alpha5

Resultado de recuperación de unidades de traducción:

| Estado | TUs |
|---|---:|
| Precompiladas / recuperadas | 21 |
| Source fallback deliberado | 3 |
| Total estudiado | 24 |

Las tres TUs que permanecen deliberadamente en fuente son:

| Librería | TUs | Motivo |
|---|---:|---|
| `JWPLC_Display` | 2 | Dependencias externas de runtime/SPI JWPLC; no se añade bridge genérico sólo por velocidad. |
| `JW_RTC` | 1 | Dependencias I2C JWPLC; no se añade bridge I2C genérico sólo por velocidad. |

Resultado:

`PRECOMPILED_RECOVERED_TUS=21/24`

`SOURCE_FALLBACK_TUS=3/24`

---

## 3. Benchmark final Alpha5

Sketch:

`tools/build-speed-benchmark/sketches/01_empty`

### JWPLC Basic

| Fase | Tiempo | Compilaciones |
|---|---:|---:|
| managed cold | 54.594 s | 8 |
| managed warm no-change | 24.804 s | 1 |
| managed warm touch | 23.760 s | 1 |
| explicit cold | **55.387 s** | **8** |
| explicit warm no-change | 22.462 s | 1 |
| explicit warm touch | 22.219 s | 1 |

### JWPLC Basic Core

| Fase | Tiempo | Compilaciones |
|---|---:|---:|
| managed cold | 60.717 s | 71 |
| managed warm no-change | 20.934 s | 1 |
| managed warm touch | 21.085 s | 1 |
| explicit cold | **58.617 s** | **71** |
| explicit warm no-change | 21.393 s | 1 |
| explicit warm touch | 21.221 s | 1 |

La diferencia estructural del cold es clara:

| Target | Compilaciones cold |
|---|---:|
| JWPLC Basic | **8** |
| JWPLC Basic Core | **71** |

JWPLC Basic evita por tanto 63 compilaciones respecto al control desde fuente.

---

## 4. Comparación principal Alpha4 vs Alpha5

La comparación principal utiliza el PC principal y cold controlado con
`--build-path` y limpieza explícita.

Esto permite una comparación mucho más fuerte que mezclar hosts distintos.

### Alpha4 P6 vs Alpha5 final

| Métrica | Alpha4 P6 | Alpha5 final | Diferencia |
|---|---:|---:|---:|
| Cold controlado | 67.322 s | **55.387 s** | **-11.935 s** |
| Mejora relativa | — | — | **-17.73 %** |
| TUs compiladas | 12 | **8** | **-4 TUs** |
| Reducción de TUs | — | — | **-33.33 %** |

Resultado:

`ALPHA5_VS_ALPHA4_SAME_HOST_COLD=PASS`

Alpha5 final es aproximadamente 17.7 % más rápido que el último hito
Alpha4 comparable en el mismo PC principal.

Además, reduce de 12 a 8 las unidades compiladas desde fuente.

---

## 5. Evolución completa en el PC principal

La siguiente tabla muestra la evolución de los principales hitos medidos
en el mismo equipo.

| Estado | Cold | TUs |
|---|---:|---:|
| Alpha3 / baseline local pre-D1 | 148.649 s | 102 |
| Alpha4 D1 discovery | 121.732 s | 102 |
| Alpha4 P1 | 105.940 s | 97 |
| Alpha4 P2 core precompilado | 104.223 s | 34 |
| Alpha4 P3 determinista | 101.677 s | 32 |
| Alpha4 P5A Ethernet | 90.587 s | 24 |
| Alpha4 P6A-2 ST77xx | 84.544 s | 20 |
| Alpha4 P6B-2 GFX | 77.907 s | 16 |
| Alpha4 P6C-2 / full Adafruit | 67.322 s | 12 |
| **Alpha5 final** | **55.387 s** | **8** |

Frente al baseline local de 148.649 s:

- reducción absoluta: 93.262 s;
- reducción relativa: aproximadamente 62.74 %;
- reducción de TUs: de 102 a 8.

El resultado final no se obtiene retirando periféricos del autoload.

---

## 6. Alpha4 P8 como referencia secundaria

Alpha4 P8 obtuvo:

| Métrica | Alpha4 P8 |
|---|---:|
| Cold promedio A-B-B-A | 59.901 s |
| TUs desde fuente | 5 |

P8 fue el máximo rendimiento formal de Alpha4.

Sin embargo, fue medido principalmente en la laptop de validación cruzada,
por lo que no debe utilizarse como comparación causal directa contra
Alpha5 final medido en el PC principal.

Como referencia únicamente:

| Estado | Host | Cold | TUs |
|---|---|---:|---:|
| Alpha4 P8 | Laptop | 59.901 s | 5 |
| Alpha5 final | PC principal | 55.387 s | 8 |

Numéricamente Alpha5 final resulta 4.514 s menor, pero no se declara una
mejora porcentual causal porque los hosts son distintos.

La conclusión correcta es:

`ALPHA4_P8=REFERENCE_ONLY_DIFFERENT_HOST`

---

## 7. Compatibilidad recuperada en Alpha5

El máximo rendimiento no fue el único criterio de Alpha5.

Durante Alpha5 se auditó cada archive compartido para evitar dependencias
incorrectas respecto a símbolos internos de JWPLC.

Se adoptó únicamente el bridge GPIO limitado:

- `jwplc_pinMode`;
- `jwplc_digitalWrite`;
- `jwplc_digitalRead`.

No se añadieron:

- bridge SPI genérico;
- bridge I2C genérico;
- bridge de runtime;
- shims únicamente destinados a reducir tiempos de compilación.

Esto permite mantener compatibilidad con ESP32 Board genérico sin romper la
arquitectura de JWPLC Basic.

---

## 8. JW_MatrixButtons

La última librería recuperada en Alpha5 fue:

`JW_MatrixButtons 1.0.5`

Archive:

`JWPLC/2.1.0/libraries/JW_MatrixButtons/src/esp32/libJW_MatrixButtons.a`

SHA-256:

`55be8d7791ddad79d613dbb199c10a504de0f20cdf3330b6679a35dd64e25c81`

Commit:

`c181a27 perf(matrix-buttons): adoptar libreria precompilada ESP32`

Gates cerrados:

| Gate | Resultado |
|---|---|
| Generic ESP32 | PASS |
| JWPLC Basic | PASS |
| JWPLC Basic Core | PASS |
| Botonera física | PASS |

Resultado:

`JW_MATRIXBUTTONS_PRECOMPILED=ADOPTED`

---

## 9. Interpretación del warm build

En los warm builds tanto JWPLC Basic como JWPLC Basic Core recompilan
únicamente una TU.

Por esta razón, el tiempo warm deja de estar dominado por la compilación de
las librerías y pasa a depender principalmente de:

- library discovery;
- preprocesamiento;
- resolución de dependencias;
- enlace;
- overhead de Arduino CLI.

Por ello no debe interpretarse un warm ligeramente menor de Basic Core como
una ventaja arquitectónica frente al Basic precompilado.

La diferencia estructural aparece claramente en cold:

`Basic = 8 compilaciones`

`Basic Core = 71 compilaciones`

---

## 10. Conclusión Alpha4 vs Alpha5

Alpha4 demostró que era posible reducir agresivamente el número de TUs y
alcanzar tiempos cold cercanos a un minuto.

Alpha5 conserva ese objetivo, pero corrige el problema fundamental de
compatibilidad de archives compartidos entre:

- JWPLC Basic;
- JWPLC Basic Core;
- ESP32 Board genérico.

En el mismo PC principal:

| Versión | Cold comparable | TUs |
|---|---:|---:|
| Alpha4 P6 | 67.322 s | 12 |
| **Alpha5 final** | **55.387 s** | **8** |

Alpha5 final:

- reduce 11.935 s frente a Alpha4 P6;
- mejora el cold aproximadamente 17.73 %;
- reduce 4 TUs adicionales;
- mantiene Arduino IDE;
- mantiene los periféricos del autoload;
- mantiene las APIs ya validadas;
- conserva source fallback cuando precompilar no es arquitectónicamente seguro.

Resultado final:

`ALPHA5_BUILD_SPEED_FINAL=PASS`

`ALPHA5_VS_ALPHA4_SAME_HOST=IMPROVED`

`PRECOMPILED_RECOVERED_TUS=21/24`

`SOURCE_FALLBACK_TUS=3/24`
