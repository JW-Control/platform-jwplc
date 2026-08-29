# JWPLC v2.1.0-alpha.5 — Validación de iteración de desarrollo en Arduino IDE

Fecha: 2026-08-25

## Objetivo

Registrar el comportamiento real de compilación del package publicado
`jwplc:esp32@2.1.0-alpha.5` durante un flujo de trabajo típico en Arduino IDE:

1. primera compilación;
2. modificaciones pequeñas y sucesivas del mismo sketch;
3. reemplazos completos por sketches progresivamente mayores;
4. recompilación sin cambios.

Estas mediciones son observacionales y manuales. No sustituyen el benchmark
automatizado de Alpha5; complementan dicho benchmark con una medición de
experiencia de desarrollo real.

## Entorno

- Arduino IDE: 2.3.10.
- Board: `JWPLC Basic`.
- Package instalado desde Boards Manager: `jwplc:esp32@2.1.0-alpha.5`.
- Namespace utilizado por Arduino IDE: `jwplc`.
- No se utilizó `jwplc_local` para estas pruebas.
- No había hardware JWPLC conectado durante estas compilaciones.

La instalación desde Boards Manager reemplazó correctamente
`jwplc:esp32@2.1.0-alpha.4` por `jwplc:esp32@2.1.0-alpha.5`.

Resultado:

`ALPHA5_PUBLIC_BOARDS_MANAGER_INSTALL=PASS`

---

## 1. Compilación inicial desde package publicado

Se verificó primero un sketch vacío desde Arduino IDE utilizando el package
publicado.

Resultados observados:

| Caso | Tiempo | APP | RAM global |
|---|---:|---:|---:|
| Primera compilación observada | 76.79 s | 394317 bytes | 27596 bytes |
| Recompilación sin cambios | 11.18 s | 394317 bytes | 27596 bytes |

Arduino IDE finalizó con `Compilación completada`.

Resultado:

`ALPHA5_PUBLIC_ARDUINO_IDE_COMPILE=PASS`

---

## 2. Gate integral desde package publicado

Se compiló desde Arduino IDE el sketch integral heredado de Alpha4:

`tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate`

Tiempo manual observado:

`56.68 s`

El sketch compiló utilizando `JWPLC Basic` y el package público Alpha5.

Resultado:

`ALPHA5_PUBLISHED_PACKAGE_INTEGRAL_COMPILE=PASS`

---

## 3. Serie A — crecimiento incremental mediante DEV_STAGE

### Metodología

Se utilizó un único sketch con bloques condicionales controlados por:

```cpp
#define DEV_STAGE N
```

Cada incremento de `DEV_STAGE` hacía que el preprocesador incorporara más
código C++ a la misma unidad de aplicación. La prueba aproxima el flujo de
"editar el mismo sketch, añadir lógica y volver a compilar" manteniendo las
mismas dependencias del package.

Se mantuvo Arduino IDE abierto durante toda la serie.

### Resultados

| Stage | Contenido añadido | Tiempo | Resultado |
|---:|---|---:|---|
| 1 | base inicial | 53.85 s | PASS |
| 2 | E/S digital | 13.78 s | PASS |
| 3 | timers y funciones | 13.07 s | PASS |
| 4 | arrays y procesamiento | 13.38 s | PASS |
| 5 | máquina de estados | 13.88 s | PASS |
| 6 | lógica de proceso | 13.82 s | PASS |
| 7 | recetas y diagnóstico | 13.21 s | PASS |

Promedio de las recompilaciones incrementales Stage 2 a Stage 7:

`13.52 s`

### Incidencias del sketch de prueba

Durante la construcción del ensayo aparecieron dos errores que se corrigieron
antes de considerar válidas las mediciones:

1. Los identificadores `I0_x` y `Q0_x` fueron declarados inicialmente en
   arrays `uint8_t`. En JWPLC Basic son identificadores extendidos
   `uint16_t`, por lo que el compilador detectó correctamente un narrowing.
2. En Stage 5 Arduino generó un prototipo automático antes de conocer el tipo
   `MachineState`. Se añadió un prototipo explícito.

Los tiempos de compilaciones que terminaron en esos errores no se incluyen en
la tabla y no representan fallos del package Alpha5.

### Conclusión de la serie A

Después de la primera compilación, modificaciones pequeñas y medianas del
mismo sketch se mantuvieron alrededor de 13–14 s aunque la lógica incorporada
fuera creciendo.

Resultado:

`ALPHA5_ARDUINO_IDE_PROGRESSIVE_SMALL_EDIT=PASS`

`ALPHA5_ARDUINO_IDE_SMALL_EDIT_AVG=13.52s`

---

## 4. Serie B — reemplazo completo por sketches progresivamente mayores

### Metodología

Para aproximar aún más un flujo de edición real se eliminó completamente el
contenido del mismo archivo `.ino` en cada paso y se reemplazó por un sketch
nuevo y mayor.

No se utilizaron `#if` ni `DEV_STAGE` en esta serie.

Se conservó:

- la misma carpeta;
- el mismo nombre de sketch;
- la misma sesión de Arduino IDE;
- la misma board `JWPLC Basic`;
- el mismo package `jwplc:esp32@2.1.0-alpha.5`.

Los sketches crecieron aproximadamente así:

- Test 1: 88 líneas;
- Test 2: 240 líneas;
- Test 3: 630 líneas;
- Test 4: 1014 líneas.

### Resultados

| Test | Tamaño del sketch | Tiempo | APP observada |
|---:|---:|---:|---:|
| 1 | 88 líneas | 67.52 s | 408129 bytes |
| 2 | 240 líneas | 19.00 s | 408701 bytes |
| 3 | 630 líneas | 19.71 s | 411097 bytes |
| 4 | 1014 líneas | 20.35 s | 414033 bytes |
| 4 sin modificar | 1014 líneas | 8.50 s | 414033 bytes |

En Test 3 se observaron 29876 bytes de RAM global y en Test 4,
33684 bytes.

Promedio de los tres reemplazos incrementales posteriores a la primera
compilación:

`19.69 s`

### Observación principal

El sketch creció desde 88 hasta 1014 líneas, más de 11 veces en longitud,
mientras las recompilaciones posteriores a la primera se mantuvieron entre
19.00 y 20.35 s.

La diferencia entre Test 2 y Test 4 fue de sólo 1.35 s pese al crecimiento de
240 a 1014 líneas.

La recompilación del Test 4 sin modificar ningún carácter cayó a:

`8.50 s`

Resultado:

`ALPHA5_ARDUINO_IDE_PROGRESSIVE_LARGE_EDIT=PASS`

`ALPHA5_ARDUINO_IDE_LARGE_EDIT_AVG=19.69s`

`ALPHA5_ARDUINO_IDE_WARM_NOCHANGE=8.50s`

---

## 5. Interpretación

Los resultados observados separan claramente tres comportamientos prácticos:

| Escenario | Orden de tiempo observado |
|---|---:|
| Primera compilación / cold-like | ~54–77 s |
| Edición pequeña o mediana | ~13–14 s |
| Reemplazo grande del sketch | ~19–20 s |
| Recompilación sin cambios | 8.50–11.18 s |

La conclusión relevante para experiencia de usuario es que, después de la
primera compilación, el flujo normal de desarrollo queda en el orden de
segundos y no vuelve al coste de una compilación fría por cada edición del
sketch.

Dentro del rango ensayado, el crecimiento del código de aplicación no produjo
un incremento proporcional del tiempo de compilación incremental.

Esto es consistente con el objetivo de Alpha5: reutilizar core y librerías
precompiladas/caché de forma segura mientras sólo se recompila el trabajo que
realmente cambió.

No se afirma que el tiempo sea independiente del tamaño para cualquier
aplicación posible; la conclusión se limita al rango y entorno ensayados.

---

## 6. Resultado final

`ALPHA5_PUBLIC_BOARDS_MANAGER_INSTALL=PASS`

`ALPHA5_PUBLIC_ARDUINO_IDE_COMPILE=PASS`

`ALPHA5_PUBLISHED_PACKAGE_INTEGRAL_COMPILE=PASS`

`ALPHA5_ARDUINO_IDE_PROGRESSIVE_SMALL_EDIT=PASS`

`ALPHA5_ARDUINO_IDE_PROGRESSIVE_LARGE_EDIT=PASS`

`ALPHA5_ARDUINO_IDE_WARM_NOCHANGE=8.50s`

`ALPHA5_SKETCH_GROWTH_VALIDATION=PASS`

`ALPHA5_DEVELOPER_ITERATION_EXPERIENCE=PASS`

La validación física de upload y la repetición física integral permanecen
fuera de esta sesión por ausencia de hardware conectado; no se reinterpretan
como fallos de compilación o publicación.