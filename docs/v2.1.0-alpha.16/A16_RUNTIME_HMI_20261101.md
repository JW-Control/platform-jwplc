# A16 · Página HMI del Designer dentro del Runtime (01/11/2026)

Rama: `v2.1.0-alpha.16/JWPLC-Sim/feature/runtime-hmi` (desde `…/main` en 12be056).
Base de diseño: `DISENO_RUNTIME_ESTILO_LOGO8.md` §4.2 (página HMI en las páginas RUN) y §7.4
(«exclusivos como dueños, compatibles como páginas»).

```text
A16_RUNTIME_HMI_FIRMWARE=PASS_SIM        ejemplo Runtime_HMI en JWPLC Sim (jwplc-sim, prueba Runtime_HMI)
A16_RUNTIME_HMI_DESIGNER=PASS            runtime-core.test.cjs + runtime-smoke.cjs; run-all completo
A16_RUNTIME_HMI_ESP32=PENDING            compilar y probar en placa (taller)
```

## Qué hace

Un proyecto del HMI Designer de tipo **Runtime** genera un `JWPLC_HMI_Generated.h` cuyas páginas se
muestran dentro de `JWPLC_Runtime`, con los valores del programa del panel (I, Q, M y bloques). El sketch
queda en:

```cpp
#include <JWPLC_Runtime.h>
#include "JWPLC_HMI_Generated.h"

void setup() { JWPLC_Runtime.begin(); }
void loop() {}
```

Navegación (el usuario no ve dos «inicios»): en la pantalla RUN, ► abre la primera página HMI y ◄ la
última; dentro, ◄ ► pasan de página y desde los extremos vuelven a RUN; ESC abre el MENU del Runtime.
Mientras una página HMI está en pantalla no hay vuelta automática a RUN por inactividad.

## Firmware (`JWPLC_Runtime`, modo fuente)

- Lectura nueva, API solo con «.»: `input(n)` I1..I8, `block(n)` salida de Bnn, `blockSeconds(n)`
  tiempo transcurrido de un temporizador en s, `counter(n)` cuenta de un contador (más `output` y `marker`).
- `setHMI(fields[], count, pages)` y `hasHMI()`: los llama el header generado, no el sketch.
- Hooks `extern "C"` (weak en la librería, declarados en `JWPLC_Runtime.h`):
  `jwplcRuntimeHMISetup()` al arrancar, `jwplcRuntimeHMIPageEnter(page)` gráfica estática con la
  pantalla recién borrada y `jwplcRuntimeHMIUpdate(page)` cada ciclo, solo setters. Tienen nombres
  propios porque `jwplcUIEnter/jwplcUIUpdate` ya son del Runtime.
- `ui/RtUIHMI.cpp`: la pantalla HMI de la pila de la UI. Al entrar pone los campos en `JWPLC_Display`
  (refresco periódico para que la UI siga atendiendo teclas) y dibuja dentro del batch; al salir los quita
  y consume el redibujo pendiente, para que el menú no los vea. La barra de estado y el pie del Runtime no
  se dibujan encima de la página.
- Una página puede tener solo gráfica estática (`setHMI(nullptr, 0, N)`).
- Ejemplo `JWPLC_Runtime/examples/Runtime_HMI` (2 páginas: Tablero con Q1, Q2, retardo B02 y piezas
  B05; Entradas con I1, I2, I3 e I5) con el programa de ejemplo.

## Designer

- Proyecto · Pantalla y arranque: tarjeta **Tipo de proyecto** (Arduino IDE / Runtime). En Runtime se
  ocultan las tarjetas de la IDLE (la RUN es la del Runtime). Se guarda en el `.jwhmi` como `projectType`.
  Dentro de OpenPLC no aparece.
- Datos: selector **Variable del Runtime** en BOOL, VALUE, BAR e ICON. El enlace vive en el nombre de la
  variable: `rt_i<n>`, `rt_q<n>`, `rt_m<n>`, `rt_b<nn>`, `rt_b<nn>_t` (tiempo), `rt_b<nn>_c` (cuenta), con
  sufijo `_2`… para repetir una variable. Así no cambian la unicidad, el contrato ni el guardado.
- Código: `designer-runtime-core.js` (puro, también en node) convierte el header Arduino en el del
  Runtime: incluye `JWPLC_Runtime.h`, quita las variables propias, la configuración de IDLE y páginas, y
  cambia los hooks. TEXT no se enlaza (el Runtime no tiene textos): queda comentado con un AVISO al
  inicio. MENU y EVENT_LOG dan `// ERROR RUNTIME` (el Runtime tiene los suyos). La conversión es
  idempotente.

## Evidencia

- `[ejecutado]` jwplc-sim: `Runtime_HMI` PASS (5 capturas: tablero con retardo, tablero con 3 piezas,
  entradas, MENU, RUN) y `Runtime_Panel` sin cambios; paridad PASS.
- `[ejecutado]` `runtime-core.test.cjs`: 4 headers golden convertidos compilan contra `JWPLC_Runtime.h`
  (g++ -fsyntax-only); `runtime-smoke.cjs` en Chromium: tipo de proyecto, selector, header, guardado y
  vuelta a Arduino. `run-all` del Designer: todo PASS.
- `[no verificado]` ESP32 real.

## Pendiente

- Leer en el Runtime parámetros de bloques (presets) y entradas analógicas cuando existan; escribir
  (pulsadores en la página) pasaría por la cola de órdenes del Runtime.
- Guía del sketch («Vincular sketch») para el tipo Runtime y opción en la CLI de agentes.
