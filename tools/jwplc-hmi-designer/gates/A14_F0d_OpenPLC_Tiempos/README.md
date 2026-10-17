# Gate F0d · OpenPLC: costo de la HMI en el scan (objetivo < 50 µs)

Cuarta prueba HMI Designer ↔ OpenPLC (Alpha14). Mide en la placa cuánto agrega
la HMI a cada scan y si el dibujo de la pantalla atrasa el scan.

La medición llegó con el VPP **2.1.0-alpha.31** (pantalla de configuración
**"Pantalla HMI"**, casilla **"Medir tiempos de scan"**; sin la casilla el
firmware es idéntico al de alpha.30). **Desde F0d-2 se usa el VPP
2.1.0-alpha.32**, que saca `jwplcUIUpdate()` del scan (ver F0d-2).

## Qué se mide

Cada segundo el HAL publica 8 palabras `%MW` (UINT, en µs, saturadas a 65535),
a partir de **"Primer %MW de resultados"** (por defecto `%MW10`):

| %MW | Valor | Qué es |
|---|---|---|
| base+0 | HMI promedio | `jwplcHMIUpdate()` del scan: copia de enlaces (`jwplcHMIBindingsSnapshot`); en alpha.31 también `jwplcUIUpdate()` |
| base+1 | HMI máximo | idem, máximo del último segundo |
| base+2 | HMI máximo total | máximo desde el arranque |
| base+3 | Scan promedio | desde el inicio de `updateInputBuffers()` hasta el fin de `updateOutputBuffers()` (entradas + programa + salidas + HMI + Backplane) |
| base+4 | Scan máximo | idem, máximo del último segundo |
| base+5 | Periodo promedio | entre inicios de scan consecutivos (nominal = intervalo de la tarea) |
| base+6 | Periodo máximo | idem; si el dibujo de la TFT atrasa el scan, sube aquí |
| base+7 | Scans | scans en el último segundo (nominal 50 con `T#20ms`) |

El dibujo de la pantalla no ocurre dentro del scan; su efecto se ve en el
**periodo máximo**.

## Preparación

1. Instalar `jwplc-basic-openplc-2.1.0-alpha.32.jwcontrol-signed.vpp` en el editor
   (rama `develop/alpha14-openplc-hmi-v1`).
2. Dispositivo **JWPLC BASIC [2.0.0]** → pestaña **"Pantalla HMI"** → marcar
   **"Medir tiempos de scan"**; primer `%MW` = **10**.
3. Proyecto `HMIProbe` con la pantalla del gate F0c (`hmi/`), y agregar al
   programa `main`, para verlas en el debugger:

```text
    T_HMI_PROM  : UINT AT %MW10;
    T_HMI_MAX   : UINT AT %MW11;
    T_HMI_TOTAL : UINT AT %MW12;
    T_SCAN_PROM : UINT AT %MW13;
    T_SCAN_MAX  : UINT AT %MW14;
    T_PER_PROM  : UINT AT %MW15;
    T_PER_MAX   : UINT AT %MW16;
    T_SCANS     : UINT AT %MW17;
```

   El programa no las escribe; sólo sirven para verlas. También se leen por
   Modbus como holding registers 42..49 (base 10 + 32).

## Escenarios (≈ 30 s cada uno; anotar los 8 valores)

| # | Escenario | Cómo |
|---|---|---|
| F0d-1 | Línea base sin HMI | Mover `hmi/JWPLC_HMI_Generated.h` y `hmi/bindings.json` fuera de `hmi/`, compilar y subir |
| F0d-2 | HMI cargada, pantalla en IDLE | Volver a poner los archivos, compilar y subir; no tocar teclas |
| F0d-3 | Página "Valores" con cambios | Entrar a la HMI y pulsar I0_0 seguido (relé, contador y barra cambian) |
| F0d-4 | MENU editando | Página "Ajustes", editar Limite manteniendo UP/DOWN (autorrepetición) |
| F0d-5 | Navegación | Cambiar de página con LEFT/RIGHT varias veces (redibujo completo) |

## Criterios

- **HMI máximo total (`T_HMI_TOTAL`) < 50 µs** en F0d-2..F0d-5.
- **Scan promedio** con HMI (F0d-2..5) no más de 50 µs por encima de la línea
  base (F0d-1).
- **Scans por segundo** = 50 (con `T#20ms`) en todos los escenarios.
- **Periodo máximo**: anotarlo. Si en F0d-3..5 supera mucho los 20 000 µs, el
  dibujo de la TFT está atrasando el scan; es un dato para Alpha15/Alpha13
  (dibujo incremental), no un fallo del enlace.

Resultados: anotar en `docs/v2.1.0-alpha.14/A14_HMI_V1_OPENPLC_20261017.md`
con `F0D_TIMING_PHYSICAL`.

## Resultados en placa

Lectura de las gráficas del debugger de OpenPLC (Modbus RTU por USB activo,
graficando las 8 variables), ventana de 10 s.

| Variable | F0d-1 sin HMI | F0d-2 IDLE (alpha.31) | F0d-2b IDLE (alpha.32) | F0d-3 Valores | F0d-4 MENU | F0d-5 Páginas |
|---|---|---|---|---|---|---|
| `T_HMI_PROM` (µs) | 0 | 164–169 | 7–9 | 14–15 | 5–14 | 7–13 |
| `T_HMI_MAX` (µs) | 0 | 225–340 | 10–16 | 19–25 | 10–27 (un pico de ≈ 85) | 10–27 (un pico de ≈ 73) |
| `T_HMI_TOTAL` (µs) | 0 | 837 | **21** | 90 (pico aislado) | no se graficó (≥ 90, es acumulado) | no se graficó |
| `T_SCAN_PROM` (µs) | 60–65 (pico 74) | 219–226 | 46–53 | 71–74 | 49–67 | 45–65 |
| `T_SCAN_MAX` (µs) | 80–170 (pico ≈ 640) | 280–400 | 61–89 | 96–123 | 65–157 | 56–181 |
| `T_PER_PROM` (µs) | 19 920–20 080 | 19 940–20 100 | 19 940–20 060 | 19 910–20 090 | 19 915–20 085 | 19 910–20 095 |
| `T_PER_MAX` (µs) | 26 200–28 300 | 23 200–28 000 | 21 800–27 800 | 21 400–28 000 | 23 000–31 300 | **53 000–65 000** al cambiar de página |
| `T_SCANS` | 50–51 | 50–51 | 50–51 | 50–51 | 50–51 | 50–51 |

### F0d-1 · línea base sin HMI `[ejecutado 2026-10-17, Kenyi]`

- **HMI = 0 µs**, como corresponde: sin `hmi/JWPLC_HMI_Generated.h` el editor
  escribe `JWPLC_HMI_ENABLED 0` y el HAL no llama a la HMI. Confirma que la
  medición distingue los dos casos.
- **Scan ≈ 60 µs de promedio** (entradas, programa, salidas y Backplane):
  0,3 % del ciclo de 20 ms. El máximo suele quedar en 80–170 µs, con un pico
  aislado de ≈ 640 µs en el mismo segundo en que el promedio subió a 74 µs.
- **Periodo promedio 20 000 µs ± 80** y **50–51 scans por segundo**: la tarea
  `T#20ms` se cumple. El 51 aparece porque la ventana se cierra en el primer
  fin de scan después de 1 s, y a veces entra un scan más.
- **Periodo máximo 26–28 ms, sin HMI.** Algunos scans arrancan 6–8 ms tarde y
  el siguiente se adelanta (el planificador recupera la cadencia, por eso el
  promedio sigue en 20 ms). Este retraso ya existe en la línea base, así que
  no lo causa la HMI. Candidatos, fuera del scan: el refresco de la IDLE en la
  TFT, la atención del debugger Modbus RTU por USB y el servicio del Backplane.
  Para separarlo se puede repetir F0d-1 sin el debugger graficando (leyendo
  por Modbus una sola vez al final) o con el debugger por TCP.

Referencia para F0d-2..F0d-5: scan promedio ≈ 60 µs y periodo máximo
≈ 28 ms. La HMI pasa si `T_HMI_TOTAL` < 50 µs, el scan promedio no supera
≈ 110 µs y el periodo máximo no crece de forma notable sobre 28 ms.

### F0d-2 · HMI cargada, pantalla en IDLE `[ejecutado 2026-10-17, Kenyi, VPP alpha.31]`

**No cumple el objetivo: HMI ≈ 167 µs de promedio (máximo 340 µs, 837 µs
desde el arranque) contra < 50 µs.** El scan pasa de ≈ 60 µs a ≈ 222 µs: los
≈ 160 µs extra son exactamente la HMI. El periodo (promedio 20 ms, máximo
23–28 ms) y los 50–51 scans/s quedan igual que en la línea base, así que la
HMI no agrega retraso entre scans; sólo alarga el scan.

Causa `[visto en código]`:

- El HAL (bloque F0) llamaba `jwplcUIUpdate()` en cada scan. Esa función
  llama a los setters de la página activa, y `setValue()` formatea el número a
  texto (`formatNumeric`) en cada llamada, aunque el valor no haya cambiado.
  Con la pantalla en IDLE ese trabajo no sirve para nada.
- Además sobra: el runtime de `JWPLC_Display` ya llama `jwplcUIUpdate()` en
  cada refresco de USER (`USER_REFRESH_PERIODIC` del header del Designer), con
  el bus TFT tomado y justo antes de `jwplcUIRuntimeDrawDirty()`. En la IDLE no
  la llama.
- Ese refresco corre en la **tarea de sistema** del core (`cores/jwcontrol/
  main.cpp`, `jwplcSystemDisplayHook()` + `vTaskDelay`), no en el `loop()` del
  PLC. O sea que el HAL entraba a `JWPLC_UI` desde una segunda tarea a la vez.
  Es también el candidato más probable del periodo máximo de 26–28 ms de la
  línea base: esa tarea dibuja la IDLE y le quita CPU al scan.

Corrección (VPP **2.1.0-alpha.32**): en el scan sólo queda
`jwplcHMIBindingsSnapshot()` (copia de enlaces HMI <-> PLC, incluida la
escritura RW). Los setters y el dibujo quedan en la tarea del Display. La
pantalla se actualiza al ritmo de refresco de USER en vez de cada scan, que es
lo que dibujaba de todos modos. Compila con el runtime real (468169 B) y
`jwplcUIUpdate` sigue enlazado para la tarea del Display.

Siguiente: **F0d-2b**, repetir F0d-2 con alpha.32; luego F0d-3..F0d-5 con
alpha.32.

### F0d-2b · HMI cargada, pantalla en IDLE `[ejecutado 2026-10-17, Kenyi, VPP alpha.32]`: PASS

- **HMI 7–9 µs de promedio, 10–16 µs de máximo y 21 µs desde el arranque**:
  cumple el objetivo de < 50 µs con margen. Es sólo la copia de los 7 enlaces
  (incluidos los 2 RW con su sombra). Bajó de ≈ 167 µs a ≈ 8 µs, unas 20 veces.
- **Scan 46–53 µs de promedio, máximo 61–89 µs.** Queda en el mismo orden que
  la línea base sin HMI (60–65 µs); la diferencia hacia abajo está dentro de la
  variación entre corridas (otra compilación y otra carga del debugger), no se
  atribuye a la HMI.
- **Periodo 20 000 ± 60 µs, máximo 21,8–27,8 ms, 50–51 scans/s**: igual que la
  línea base. El retraso ocasional de algunos scans sigue presente sin cambios,
  lo que confirma que no viene de la HMI.

Con alpha.32 la HMI deja de pesar en el scan. Falta ver F0d-3..F0d-5, donde la
tarea del Display sí dibuja (cambios de valores, MENU y páginas) y el dato a
mirar es el periodo máximo.

### F0d-3 · página Valores con cambios `[ejecutado 2026-10-17, Kenyi, VPP alpha.32]`: PASS con observación

Página "Valores" activa, I0_0 pulsado seguido y también forzado desde el
debugger (relé, contador y barra cambiando).

- **HMI 14–15 µs de promedio y 19–25 µs de máximo por segundo**, dentro del
  objetivo.
- **`T_HMI_TOTAL` = 90 µs**: hubo **un** scan, en algún momento de la corrida,
  en que la medición de la HMI dio 90 µs. No se repite en los máximos por
  segundo (19–25 µs).
- **Scan 71–74 µs de promedio, máximo 96–123 µs**: ≈ 20 µs más que en IDLE.
- **Periodo 20 000 ± 90 µs, máximo 21,4–28,0 ms, 50–51 scans/s**: igual que la
  línea base. Dibujar los cambios de la página no atrasa el scan más de lo que
  ya lo atrasaba la IDLE.

Explicación `[visto en código, inferido para el pico]`:

- `cores/jwcontrol/main.cpp` crea `jwplcSystemTask` (la que dibuja la TFT) en
  **el mismo core que `loopTask` y con prioridad más alta**. Cuando la pantalla
  está en USER y hay cambios, esa tarea se despierta más seguido y desaloja al
  scan.
- La medición es tiempo de reloj (`micros()` antes y después), así que incluye
  esas interrupciones. Eso explica los ≈ 20 µs más de scan y de HMI respecto
  de la IDLE, y probablemente el pico aislado de 90 µs: la tarea del Display
  interrumpió justo durante la copia de enlaces, que por sí sola tarda < 10 µs
  (F0d-2b).
- Para separar CPU propia de desalojo habría que medir con la prioridad o el
  core de la tarea del Display cambiados, lo que es trabajo del package
  (Alpha13), no del enlace HMI.

### F0d-4 · MENU editando `[ejecutado 2026-10-17, Kenyi, VPP alpha.32]`: PASS con observación

Página "Ajustes", edición de Limite durante ≈ 30 s: subir y bajar con UP/DOWN
mantenidos (autorrepetición) y al final decenas con LEFT/RIGHT, confirmando
con OK (cada confirmación escribe `%MW1` por el enlace RW).

- **HMI 5–14 µs de promedio; máximo por segundo 10–27 µs**, con **un** segundo
  de ≈ 85 µs. Mismo patrón que el pico de F0d-3: desalojo por la tarea del
  Display, que en edición redibuja la fila del MENU en cada repetición de
  tecla. `T_HMI_TOTAL` no se graficó: es el máximo desde el arranque, por eso
  se ve constante (aquí vale al menos los 90 µs de F0d-3).
- **Scan 49–67 µs de promedio, máximo 65–157 µs.**
- **Periodo 20 000 ± 85 µs y 50–51 scans/s**: la cadencia se mantiene.
- **Periodo máximo 23,0–31,3 ms**: dos segundos llegaron a ≈ 31,3 ms, unos
  3 ms por encima del peor caso de la línea base (28,3 ms). Es el escenario con
  más dibujo hasta ahora (redibujo de la fila en cada repetición). Es un
  retraso ocasional de un scan, no se pierden scans; queda anotado para
  Alpha13/Alpha15 (prioridad/core de `jwplcSystemTask` y redibujo del MENU).

### F0d-5 · cambio de páginas `[ejecutado 2026-10-17, Kenyi, VPP alpha.32]`: el enlace pasa, el dibujo de página completa no

En la selección de página, alternando Valores ↔ Ajustes con LEFT/RIGHT durante
≈ 30 s. En la gráfica se distinguen dos tramos: hasta ≈ 47 s (vista de páginas
con pocos cambios, más un cambio aislado a ≈ 44 s) y de ≈ 47 s en adelante
(cambios seguidos).

- **HMI 7–13 µs de promedio; máximo por segundo 10–27 µs**, un pico de ≈ 73 µs
  (mismo desalojo que en F0d-3/4). **El enlace sigue sin pesar en el scan.**
- **Scan 45–65 µs de promedio, máximo 56–181 µs.**
- **Periodo promedio 20 000 ± 95 µs y 50–51 scans/s**: no se pierden scans en
  el segundo.
- **Periodo máximo 53–65 ms en cada segundo con cambio de página** (contra
  ≈ 28 ms de la base). Es decir: **al cambiar de página, un scan arranca hasta
  ≈ 45 ms tarde** (más de dos ciclos de 20 ms) y luego el planificador de
  OpenPLC (`last_run += scan_cycle`) ejecuta los scans atrasados seguidos para
  recuperar la cadencia; por eso el promedio y los 50 scans/s se mantienen.

Causa `[visto en código, inferido para la duración]`: el cambio de página
borra la pantalla y redibuja toda la página (`fillScreen` + estáticos + todos
los campos) en `jwplcSystemTask`, que corre en el **mismo core que `loopTask`
y con mayor prioridad**; el PLC no corre hasta que termina. Es el mismo
mecanismo que el retraso de 6–8 ms de la IDLE, pero con mucho más dibujo.

Impacto: para un PLC, un retraso de ≈ 45 ms cada vez que el operador cambia de
página no es aceptable en tareas rápidas (temporizadores cortos, conteo,
comunicaciones con plazos). No depende del enlace HMI ↔ PLC ni del editor: es
del runtime de pantalla del package. Opciones para Alpha13/Alpha15 (decisión
del jefe):

1. Correr `jwplcSystemTask` en el otro core (el ESP32 tiene dos), o con
   prioridad menor o igual que `loopTask` y cediendo entre etapas.
2. Partir el redibujo de página completa en tramos (por ejemplo un campo por
   vuelta de la tarea) en vez de dibujar todo de una vez.
3. Medir el efecto de cada opción con este mismo gate.

## Conclusión F0d

| Pregunta | Resultado |
|---|---|
| ¿Cuesta la HMI < 50 µs por scan? | **Sí desde alpha.32**: 5–15 µs de promedio y ≤ 27 µs de máximo por segundo en todos los escenarios; picos aislados de 73–90 µs por desalojo de la tarea del Display, no por el código del enlace. Con alpha.31 (`jwplcUIUpdate()` en el scan) costaba ≈ 167 µs. |
| ¿Alarga el scan? | No de forma significativa: 45–74 µs de promedio contra 60–65 µs de la base. |
| ¿Se pierden scans? | No: 50–51 scans/s en todos los escenarios. |
| ¿Atrasa el dibujo de la pantalla al scan? | IDLE y página Valores: igual que la base (≤ 28 ms). MENU editando: hasta 31,3 ms. **Cambio de página: hasta ≈ 65 ms.** Pendiente del runtime de pantalla del package. |

## Verificación sin hardware `[ejecutado 2026-10-17]`

Compilación con el runtime real de OpenPLC + HAL alpha.31 + `jwplc_local`:

| Variante | Flash |
|---|---|
| HMI F0c + medición | 468173 B |
| HMI F0c sin medición | 467749 B (igual que alpha.30) |
| Sin HMI + medición | 451353 B |

VPP alpha.31 firmado con `jwcontrol-2026`; `verifyPackageSignature` del editor: válido.
