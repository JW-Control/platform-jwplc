# JWPLC HMI Designer — Ampliación industrial: diagnóstico, arquitectura y plan

Fecha: `2026-10-18` · rama `v2.1.0-alpha.15-HmiDesigner` · Estado: **APROBADO con ajustes (ver §G)**

```text
INDUSTRIAL_ARCH_REVIEW=DONE
INDUSTRIAL_ARCH_APPROVED=2026-10-18 (decisiones en §G; §G manda sobre el resto del documento)
ALPHA15_CLOSE_SCOPE=I1,I2,I3 + Apariencia A1 + JWPLC_Buzzer
ALPHA16_SCOPE=I4..I15 + ideas aprobadas (escalado, umbrales, nivel de acceso, confirmación)
JWPLC_BUZZER=PASS_ESP32_COMPILE (JWPLC_GlobalPeripherals, 2026-10-18)
```

Responde al encargo `JWPLC HMI Designer — Alpha15: Ampliación de capacidades industriales`.

Notación:

- `[visto]`: comprobado en el código del HEAD;
- `[doc]`: decisión documentada;
- **PROPUESTA**: API o comportamiento nuevo, todavía no existe;
- `[no verificado]`: requiere hardware.

---

## A. Auditoría de compatibilidad

### A.1 Lo que ya existe y se reutiliza

| Capacidad | Estado actual | Uso en esta ampliación |
|---|---|---|
| TEXT, VALUE, BOOL, BAR | `JWPLC_UI.h` `[visto]`; 32 slots (`JWPLC_UI_MAX_FIELDS = 32`) | VALUE recibe el formato de tiempo y el separador de miles |
| Formato de VALUE | `JWPLC_UIValueFormat(enteros, decimales, signo, ceros)`; `setValue<T>` → `setNumericValue(double)` `[visto]` | `double` representa enteros exactos hasta 2^53, sin desbordamiento para duraciones ni contadores |
| BOOL con texto/fondo por estado | `JWPLC_UIBoolText` de 6 argumentos `[visto]` | Base de la representación «piloto» |
| MENU | 4 menús, 64 nodos, profundidad 3, nodos SUBMENU/VIEW/EDIT_NUM/EDIT_BOOL/GO_IDLE `[visto]` | El motor de filas sirve para las listas de RECIPE y EVENT_LOG. GO_IDLE es el primer nodo de acción, base del futuro `ACTION` |
| Páginas y NN/TT | Selector/contenido, `setPageIndicator` (hasta 99 páginas en runtime, 16 en el Designer) `[visto]` | Las plantillas son páginas |
| Cadena de ESC | MENU → selector → IDLE; `jwplcUIRuntimeServiceInput` atiende primero el MENU y luego las páginas `[visto]` | Base del gestor de foco |
| IDLE | Indicadores RUN/ERR/BUS/ETH; `setErrCode()` de 1–4 caracteres A-Z/0-9 `[visto]` | ALARM puede publicar el código activo en la IDLE |
| Modos del Display | Sólo `DISPLAY_MODE_IDLE` y `DISPLAY_MODE_USER` `[visto]` | Un aviso encima de cualquiera de los dos requiere una **capa nueva** |
| RTC DS3232 | `jwplcGetRTCState()`: `present`, `valid`, `lost_power` y fecha/hora; librería `JW_RTC` `[visto]` | Marcas de tiempo y campo fecha/hora |
| FRAM | `JW_FRAM`, 8 KB en el JWPLC Basic v2.0 (32 KB previstos) `[doc A14_3]` | Recetas y registro de eventos, a repartir (ver F.2) |
| microSD | `JWPLCDataLog`: anillo en RAM (4 KB por defecto) + commit automático a la SD por umbral o timeout, sin bloquear `[visto]`. El journal en FRAM (≤ 1 KB, A14_3) quedó diferido en A14_6 y no existe en el código `[visto]` | EVENT_LOG en SD reutiliza DataLog tal cual |
| E/S | `jwplcGetIOState()`: `di_logical_bank0` (I0_0..7) y `do_bank1` (Q0_0..7, salida **comandada**) `[visto]` | IO_DIAGNOSTIC sin firmware nuevo |
| Buzzer | No hay API de buzzer en el package `[visto: sin coincidencias]` | «SILENCIAR» sólo tiene sentido como solicitud al PLC si la máquina tiene sirena |
| Arcos | `JWPLC_TFT` no expone `drawArc`/`drawSmoothArc` `[visto]` | El BAR circular (gauge) requiere ampliar `JWPLC_TFT` |

### A.2 Lo que requiere firmware nuevo

| Capacidad | Firmware | Designer | OpenPLC |
|---|---|---|---|
| VALUE duración y miles | `JWPLC_UIValueFormat` + formateo | Tarjetas de formato | Usa INT/DINT/UDINT tal cual |
| Fecha/hora (RTC) | Formato de VALUE/TEXT o setter dedicado | Opción de formato | No hace falta: lo lee la HMI |
| ALARM / INTERLOCK | Servicio de avisos + capa sobre IDLE/USER + árbitro de teclas | Tabla de avisos y vista previa | Bits/códigos ubicados |
| SEQUENCE | Tipo de campo nuevo (1 slot) | Inspector + pintor | INT + mapa valor → etapa |
| RECIPE | Plantilla de página + gestor de datos en FRAM | Editor de parámetros y vistas | Contrato de estado y solicitudes |
| EVENT_LOG | Servicio de registro (RAM/FRAM/SD) + plantilla de página | Tabla de eventos | Eventos desde bits/códigos |
| IO_DIAGNOSTIC | — (reutiliza BOOL + `jwplcGetIOState`) | Plantilla generada | — |
| Tendencia | Tipo de campo nuevo (historial en RAM) | Inspector + pintor | Variable numérica |
| Gauge (BAR circular) | `drawArc` en `JWPLC_TFT` + variante de BAR | Habilitar la tarjeta ya diseñada | — |

### A.3 Descartado o diferido (de acuerdo)

- **TOTALIZER**: es VALUE con separador de miles.
- **MAINTENANCE**: se arma con VALUE/BAR/TEXT/MENU y ALARM.
- **COMMAND**: diferido; GO_IDLE es la base para un nodo `ACTION` de MENU.
- **MULTI-ESTADO**: lo cubre SEQUENCE.
- **LED**: como **representación de BOOL** («Texto» o «Piloto» ●), no como objeto nuevo.

---

## B. Arquitectura

### B.1 Capas y responsabilidades

```text
Programa PLC (Arduino / OpenPLC)
  Verdad del proceso: etapas, temporizadores, contadores, recetas activas, alarmas,
  enclavamientos, estado seguro. Atiende las SOLICITUDES de la HMI (handshake).
        │  variables vinculadas (lectura) · bits de solicitud (escritura) · eventos
        ▼
Runtime HMI (JWPLC_Display / JWPLC_UI)        ← nuevo: avisos, foco, registro, plantillas
  Vistas, foco de teclas, avisos, registro de eventos, persistencia de recetas.
  Nunca decide la seguridad; no temporiza el proceso.
        │  dirty flags
        ▼
Renderizador (JWPLC_TFT, bus SPI con mutex)
  Primitivas; estática al entrar; dinámico por campo; capa de aviso.
        ▲
Designer: configura, simula con el mismo pintor y genera JWPLC_HMI_Generated.h
```

### B.2 Árbitro de teclas (foco único) — PROPUESTA

El orden actual `[visto]` es:

1. `handleIdleWakeAndTimeout` (despertar o volver a la IDLE);
2. MENU de la página;
3. selector de páginas.

Se propone un único despachador en `jwplcUIRuntimeServiceInput`, con dueños en orden de prioridad:

1. **Aviso crítico visible.** Toma todas las teclas. Antes del despertar de la IDLE: una tecla sobre el aviso no despierta el HMI.
2. **Aviso menor modal** (si exige reconocimiento). OK reconoce y ESC lo minimiza a la cola.
3. **Componente con foco en el contenido**: la plantilla RECIPE o EVENT_LOG de la página.
4. **MENU de la página.**
5. **Selector de páginas**, y luego la IDLE.

Reglas:

- Una tecla la consume un solo dueño (`clearPendingInput`).
- Una tecla que cierra un aviso no pasa a la página.
- La vuelta por inactividad **no cierra un aviso crítico**.

### B.3 ALARM + INTERLOCK: un servicio de avisos — PROPUESTA

**Modelo.** Una tabla generada por el Designer (en flash) con, por cada aviso:

- código: hasta 4 caracteres (encaja con `setErrCode`);
- texto;
- categoría: `ALARM` o `INTERLOCK`;
- severidad: `INFO`, `MENOR` o `CRITICA`;
- fuente: una variable bool o el código que entrega el PLC;
- política: `AUTO` (desaparece al cesar), `ACK` (requiere OK) o `LATCH` (persiste hasta cesar **y** reconocer).

El runtime guarda por aviso: activo, reconocido y marca de tiempo (RTC si es válido; si no, el uptime con una marca).

**Ciclo.**

- El servicio muestrea las fuentes en el tick del Display y detecta flancos.
- Cada flanco va al EVENT_LOG (activó, reconocido, cesó). No registra el estado en cada scan.
- **Reconocer ≠ resetear.** OK sólo reconoce. El reset de la falla es del PLC: su bit de reset es una solicitud aparte, como la acción RESET en la vista crítica si se configura.

**Prioridad y cola.**

1. Crítica sin reconocer.
2. Crítica reconocida aún activa.
3. Menor sin reconocer.
4. Información.

- Una menor **nunca** tapa una crítica.
- Si hay varias, se ve la de mayor prioridad con un contador `1/3`; ▲ ▼ recorren la cola.

**Vistas.**

- **Menor**: ventana centrada de unos 240×72 sobre la vista actual (IDLE o USER), con icono ⚠, código, texto y pie «OK reconocer · ESC ocultar». Al cerrarla se repinta la vista de abajo una sola vez (`forceFullRedraw` de la IDLE o el `prepareEnter` de la página).
- **Crítica**: pantalla completa con fondo rojo, «E03 — SOBRE TEMPERATURA», severidad, hora, acciones permitidas y la nota «Estado seguro aplicado por el control». Persiste mientras esté activa.
- **IDLE**: con una crítica activa, `setErrCode(código)` y ERR encendido. Se reutiliza lo que ya existe.

**MENU en edición.** Si llega un aviso modal, la edición **se cancela sin escribir**. Es lo más seguro: nunca se confirma un valor a medias. La página y el nivel del menú se conservan y, al cerrar el aviso, se vuelve al mismo lugar.

**INTERLOCK.** Usa el mismo servicio y el mismo dibujo, pero:

- no se enclava ni se reconoce;
- se agrupa por operación, por ejemplo «INICIO»;
- se muestra como **lista de condiciones** con ✓ y ✗.

Se dispara así:

- cuando el PLC (o el sketch) indica «intento rechazado», por ejemplo con el bit `inicioRechazado` o con `JWPLC_Display.showInterlock(GRUPO_INICIO)` (PROPUESTA);
- a pedido, con un nodo de MENU «Ver condiciones» (nodo `ACTION` futuro).

Si el PLC clasifica una condición como falla, la publica como ALARM.

**Lo que la HMI no hace.**

- No desenergiza nada.
- No depende de que la TFT refresque para llegar al estado seguro.
- Muestra el resultado de la protección del control.

**Firmware.**

- Una tercera capa de dibujo «AVISO» que se pinta por encima y se atiende antes que IDLE y USER. Los modos IDLE y USER no se tocan.
- Costo SPI: un dibujo al aparecer y una restauración al cerrar. Sin repintado periódico, salvo el contador de la cola.

### B.4 SEQUENCE (componente nuevo, 1 slot) — PROPUESTA

**Datos.**

- etapa actual (int32);
- estado opcional: `RUN`, `PAUSA`, `ERROR` o `COMPLETA`;
- mapa ordenado `valor → etiqueta` de hasta 8 etapas.

**Variantes.**

- **Compacta**: «2/3 ENJUAGUE» con puntos ●●○.
- **Extendida**: nodos unidos con flechas en horizontal o vertical. Las anteriores van «completadas», la actual «activa» y las siguientes «pendientes».

**Dibujo.**

- Nodos y conectores son estáticos y se dibujan al entrar.
- Al cambiar de etapa sólo se repintan el nodo anterior y el nuevo.

**Valor inválido** (no está en el mapa): todos los nodos «pendientes», el rótulo «—» y el color de error. No se rompe la página.

**ENUM.**

- **Arduino**: el Designer genera `enum class EtapaProceso : int32_t { LAVADO = 0, ENJUAGUE = 1, CENTRIFUGADO = 2 };` o se vincula a uno existente por nombre (se usa con `static_cast<int32_t>`).
- **OpenPLC**: una variable INT/DINT y el **mapa explícito** de valores guardado en el `.jwhmi`. Los valores no tienen que ser contiguos, así que no hace falta un `enum class` de C++.

El mapa se guarda en el `.jwhmi`, y el codegen emite una tabla `{valor, "texto"}` en flash.

**Teclas.** No toma foco: sólo muestra.

### B.5 VALUE: tiempo y miles (ampliación) — PROPUESTA

`JWPLC_UIValueFormat` agrega:

- `kind = NUMERO | DURACION`;
- unidad de la variable: `ms` o `s`;
- patrón: `SS`, `MM:SS`, `HH:MM:SS` o compacto `1h05m`;
- `separadorMiles` para NUMERO.

Comportamiento:

- La reserva de ancho sale del patrón.
- Si el valor no cabe se muestra `--:--`.
- Con signo admite «restante» negativo.

Compatibilidad: el constructor actual se conserva; un constructor nuevo va bajo `#ifdef JWPLC_UI_HAS_VALUE_DURATION`.

El tiempo transcurrido o restante lo calcula el PLC. VALUE sólo formatea.

**Fecha/hora (RTC).** Formato «RTC» de VALUE o TEXT, sin variable, leído de `jwplcGetRTCState()`:

- formatos `HH:MM`, `HH:MM:SS`, `DD/MM` y `DD/MM/AAAA HH:MM`;
- con `valid == false` se muestra `--:--`;
- se repinta sólo cuando cambia el minuto o el segundo mostrado.

### B.6 RECIPE (plantilla de página + gestor) — PROPUESTA

**Datos.**

- **Arduino**: el Designer define los parámetros (nombre, tipo, unidad, mín, máx, paso, decimales) y genera
  `struct Receta { float temperatura; int32_t tiempo; int32_t rpm; }; Receta recetas[N];`.
  También genera la tabla de metadatos con el offset de cada campo, los nombres de las recetas y la **receta activa** que lee el programa.
- **Persistencia en FRAM**: bloque versionado con CRC y doble copia para tolerar un reinicio a mitad de escritura. 10 recetas × 6 parámetros × 4 B = 240 B + cabecera. Se validan los rangos antes de guardar y de aplicar.
- **OpenPLC**: las recetas viven en la HMI (FRAM). «Cargar» copia la receta elegida a memoria ubicada (`%MD`/`%MW`, o `openplc_debug_set` cuando se confirme la semántica de F0c) y activa la solicitud `CARGAR`.

**Contrato con el PLC** (solicitudes con handshake):

- La HMI pone a 1 `reqInicio`, `reqPausa`, `reqReanudar`, `reqConfirmar`, `reqSilenciar` o `reqDetener`.
- El PLC la atiende y la vuelve a 0.
- La HMI no envía otra solicitud mientras haya una pendiente.

**El estado del proceso lo da el PLC** (un entero):

```text
0 SELECCION · 1 CARGANDO · 2 EJECUTANDO · 3 PAUSADO · 4 ESPERA_CONFIRMACION
5 AVISO_SILENCIABLE · 6 DETENIENDO · 7 ERROR · 8 FINALIZADO
```

**La vista es función del estado del PLC**, no de la navegación:

- `SELECCION` → vista de selección;
- cualquier otro estado → vista de ejecución.

Por eso:

- **ESC nunca detiene ni cambia de vista**: sólo saca el foco (NN/TT negro). Al volver a la página se ve la ejecución actualizada.
- Sólo «DETENER» lleva de vuelta a la selección: muestra «DETENIENDO…» hasta que el PLC informa `SELECCION`.

**Acciones contextuales** (dos botones; ◀ ▶ elige y OK ejecuta):

| Estado (prioridad descendente) | Acción 1 | Acción 2 |
|---|---|---|
| ESPERA_CONFIRMACION | CONFIRMAR | DETENER |
| AVISO_SILENCIABLE | SILENCIAR | DETENER |
| PAUSADO | REANUDAR | DETENER |
| EJECUTANDO | PAUSAR | DETENER |
| FINALIZADO | ACEPTAR | — |

- DETENER pide confirmación («¿Detener? OK sí · ESC no») para evitar paradas accidentales.
- SILENCIAR no reconoce ni resetea alarmas.

**Fin normal (FINALIZADO).** Propuesta: la vista de ejecución muestra «FINALIZADO» y ACEPTAR envía una solicitud. El PLC vuelve a SELECCION. No se regresa sola, para que el operador vea que terminó (decisión F.3).

**Teclas.**

- Selección: ▲ ▼ eligen la receta, ▶ entra a sus parámetros (reutiliza las filas EDIT_NUM del MENU sobre un búfer de edición) y OK carga.
- Ejecución: ◀ ▶ y OK para las acciones; ESC sale del foco.

### B.7 EVENT_LOG (servicio + plantilla) — PROPUESTA

**Registro** de unos 16 B, sin texto (la descripción sale de la tabla en flash por código):

```text
{ tiempo u32 (epoch RTC o uptime + bit), código u16, categoría u8, transición u8, fuente u8, reservado }
```

**Fuentes.**

- flancos del servicio de avisos;
- eventos del PLC (inicio, fin, pausa, cambio de receta);
- `JWPLC_Display.logEvent(código, transición)` (PROPUESTA).

Sólo se registran transiciones, nunca un estado repetido en cada scan.

**Almacenamiento por niveles**:

1. **RAM**: siempre; anillo de N registros (por defecto 64 = 1 KB).
2. **FRAM**: si se habilita; anillo con índice y CRC por registro, con presupuesto a decidir (F.2).
3. **microSD**: si hay tarjeta; líneas CSV a través de `JWPLCDataLog`. No bloquea, se sirve solo y se recupera si se retira la tarjeta. El estado se muestra en la página («SD: no»).

**Página.**

- Lista de 10 filas en 170 px: hora, código y texto recortado, con una franja de color por severidad.
- ▲ ▼ desplazan, OK abre el detalle (ventana), y aparece el contador «12/128».
- ESC sale del foco.
- No es una ventana emergente.

### B.8 IO_DIAGNOSTIC (plantilla, sin firmware)

- El Designer genera una página con 16 BOOL (DI0–DI7 y DO0–DO7) con etiquetas configurables, alimentados por un helper generado que lee `jwplcGetIOState()`.
- DO = **salida comandada**.
- Opcional: «realimentación» por salida vinculada a una entrada, que marca la discrepancia («Q0.3 ON · sin confirmación»).
- No asume que el actuador funciona.

### B.9 Tendencia y gauge (pedidos en esta conversación)

**Tendencia.** Gráfico de línea del historial de una variable, por ejemplo la temperatura de los últimos N minutos. PROPUESTA de campo nuevo:

- historial circular en RAM de 1 B o 2 B por muestra (por ejemplo 120 muestras cada 5 s);
- rango fijo o automático, líneas de consigna y de alarma opcionales;
- cada muestra nueva redibuja **sólo una columna** o desplaza, sin repintar el gráfico;
- el muestreo lo hace el runtime con un periodo fijo, no depende de la página visible.

**Gauge de arco.** Sí: es el BAR circular que quedó deshabilitado. Requiere agregar `drawArc` o `drawSmoothArc` a `JWPLC_TFT` (`TFT_eSPI` los tiene) y la variante de BAR en `JWPLC_UI`. El inspector de BAR ya tiene la tarjeta.

---

## C. Contratos de datos (ejemplos)

> Todo lo marcado `PROPUESTA` no existe hoy. Los nombres son tentativos.

**Arduino — alarma por bool + secuencia + duración:**

```cpp
#include "JWPLC_HMI_Generated.h"   // tablas: avisos, etapas, recetas (PROPUESTA)

bool sobreTemperatura = false;      // fuente del aviso E03 (CRITICA, LATCH)
int32_t etapa = 0;                  // SEQUENCE: EtapaProceso
uint32_t tiempoRestante_s = 0;      // VALUE formato MM:SS

void loop() {
    sobreTemperatura = temperatura > 95.0f;    // el estado seguro lo aplica el programa
    if (sobreTemperatura) apagarCalefaccion(); // NO la HMI
    etapa = static_cast<int32_t>(etapaActual);
}
// jwplcUIUpdate() (generado) llama a los setters; el servicio de avisos lee
// sobreTemperatura por puntero (tabla generada) y registra los flancos.
```

**OpenPLC — mismos datos por memoria ubicada** (HAL, igual que F0):

```text
%QX0.0  RELE              BOOL
%MW0    ETAPA             INT     -> SEQUENCE (mapa 0/1/2 en el .jwhmi)
%MD0    T_RESTANTE        UDINT   -> VALUE duración (s)
%MX?    ALM_SOBRETEMP     BOOL    -> aviso E03 (fuente)
%MW1    ESTADO_RECETA     INT     -> contrato B.6
%MW2    SOLICITUD_RECETA  INT     <- la HMI escribe el código, el PLC lo vuelve a 0
```

Nota de compatibilidad: hoy OpenPLC en ESP32 expone `int_memory[20]`, `dint_memory[20]`, `lint_memory[20]`, `bool_output` y `bool_input` `[visto openplc.h]`. No hay memoria BOOL tipo `%MX`. Los bits de aviso y de solicitud deberían empaquetarse en un `%MW` (bit n = aviso n) o ir en salidas sin uso. A acordar con Kenyi.

**Receta (Arduino, generado):**

```cpp
struct Receta { float temperatura; int32_t tiempo_min; int32_t rpm; };
extern Receta jwplcRecetas[JWPLC_HMI_RECIPE_COUNT];   // PROPUESTA, en FRAM
extern const Receta &jwplcRecetaActiva();             // la lee el programa al iniciar
```

**Evento:** `JWPLC_Display.logEvent(EVT_INICIO_PROCESO, JWPLC_EVT_ON);` (PROPUESTA). Los avisos se registran solos.

---

## D. Propuesta visual (320×170) e inspectores

```text
ALARM menor (ventana sobre IDLE/USER)        ALARM crítica (pantalla completa, fondo rojo)
┌──────────────────────────────┐            ┌──────────────────────────────────────────┐
│ ⚠ A12  FILTRO SUCIO     1/2  │            │ ⚠ E03 · CRITICA            14:32:05      │
│ Revisar filtro de aire       │            │   SOBRE TEMPERATURA                      │
│ OK reconocer   ESC ocultar   │            │   Estado seguro aplicado por el control  │
└──────────────────────────────┘            │   OK reconocer          ▲▼ otras (1/2)   │
                                            └──────────────────────────────────────────┘
INTERLOCK (lista)                            SEQUENCE extendido / compacto
┌──────────────────────────────┐            (●)──(●)──(◉)──( )──( )
│ NO PUEDE INICIAR             │            LAV  ENJ  CEN  SEC  FIN        2/5 ENJUAGUE ●●○○○
│ ✗ Puerta abierta             │
│ ✓ Variador listo             │            RECIPE ejecución
│ ✗ Presión insuficiente       │            ┌──────────────────────────────────────────┐
│ ESC cerrar                   │            │ ALGODON        EJECUTANDO      12:41 rest│
└──────────────────────────────┘            │ (●)──(◉)──( )   Temp 68.5/70 C   800 RPM │
                                            │      [ PAUSAR ]        [ DETENER ]       │
EVENT_LOG (página)                           └──────────────────────────────────────────┘
│14:32 E03 SOBRE TEMPERATURA  ON │▌rojo
│14:30 P01 INICIO ALGODON        │▌azul      … 10 filas, ▲▼ desplaza, OK detalle, 12/128
```

**Inspectores**, todos con Diseño | Datos | Técnico:

- **Avisos**: no es un objeto del lienzo. Es una sección de **Proyecto** (como *Pantalla y arranque*):
  - tabla editable de avisos (código, texto, severidad, política, fuente);
  - vista previa de la ventana menor y de la pantalla crítica con el pintor real;
  - grupos INTERLOCK con sus condiciones.
- **SEQUENCE**: Forma (compacta o extendida, orientación), Contenido (etapas en tarjetas reordenables), Apariencia (colores por estado con el mini-modelo), Datos (variable o enum y el mapa de valores).
- **Plantillas**: «+ Página» ofrece «Página vacía · Recetas · Registro de eventos · Diagnóstico E/S». La plantilla ocupa la página entera y se configura en un inspector de página.
- **VALUE**: la tarjeta «Formato» suma «Número · Duración · Fecha/hora».
- **BOOL**: la tarjeta «Representación» suma «Texto · Piloto ●».

---

## E. Plan por hitos

Cada hito tiene un alcance cerrado y sus pruebas. Las capas son:

- **D**: sólo Designer;
- **F**: `JWPLC_Display`/`JWPLC_UI`;
- **O**: OpenPLC;
- **H**: host-sim;
- **E**: compilación ESP32;
- **P**: prueba física.

| Hito | Contenido | Capas | Depende de | Riesgo | Pruebas |
|---|---|---|---|---|---|
| I0 | Aprobación de este plan y de las decisiones F | — | — | — | — |
| I1 | VALUE: duración y miles · BOOL: «Piloto» | F, D | — | Bajo (formateo) | golden, host (formato), E, P |
| I2 | Fecha/hora RTC (formato de VALUE/TEXT) | F, D | I1 | Bajo | host con RTC simulado, E, P (RTC sin pila) |
| I3 | Tilde/ñ cp437 en Designer y codegen (P1 de Lopaka) | D | — | Medio (desfase cp437 > 0xB0) | golden, P |
| I4 | Árbitro de teclas único (refactor sin cambio de comportamiento) | F | — | **Alto**: toca la cadena de ESC | menu-host-sim y gates A11/A15 sin regresión, P |
| I5 | Servicio de avisos + ventana menor + IDLE `setErrCode` | F, D | I4 | Medio (restauración de la vista) | host (cola/prioridad/ACK), E, P |
| I6 | Pantalla crítica + INTERLOCK (lista) | F, D | I5 | Medio | host, E, P |
| I7 | SEQUENCE (compacta y extendida) + enum/mapa | F, D | — | Bajo | paridad lienzo/firmware (como MENU), golden, E, P |
| I8 | EVENT_LOG RAM + página | F, D | I5 | Bajo | host (anillo, flancos), E, P |
| I9 | EVENT_LOG FRAM + SD (DataLog) | F | I8, F.2 | Medio (FRAM compartida, SD) | host FRAM simulada, E, P (sacar la SD) |
| I10 | RECIPE: datos + FRAM + vista selección | F, D | I4, F.2 | Medio | host (CRC, rangos), E, P |
| I11 | RECIPE: vista ejecución + contrato con el PLC | F, D | I10 | **Alto**: semántica del proceso | host (máquina de estados con PLC simulado), E, P |
| I12 | IO_DIAGNOSTIC (plantilla) | D | — | Bajo | golden, E, P |
| I13 | Tendencia | F, D | — | Medio (RAM, SPI) | host, E, P (medición SPI) |
| I14 | Gauge de arco (BAR circular) | F (JWPLC_TFT + UI), D | — | Medio (`.a` de JWPLC_TFT) | paridad, E, P |
| I15 | OpenPLC: avisos, SEQUENCE, RECIPE por memoria ubicada | O | I5–I11 | Medio | F0b/F0c de Kenyi |

Reglas que se mantienen:

- `JWPLC_Display` sigue en modo fuente; el `.a` se regenera al cierre.
- No se sube `JWPLC_UI_MAX_FIELDS`.
- Cada objeto nuevo trae su prueba de paridad lienzo/firmware.
- Se conservan los gates A15 ya cerrados y no se repiten auditorías.

---

## F. Decisiones que necesitan al responsable

1. **¿En qué alpha va?**
   - Propuesta: cerrar Alpha15 con I1–I3 (y la Apariencia A1).
   - Abrir Alpha16 «HMI industrial» con I4–I15.
   - Los servicios (avisos, registro, recetas) son de otra escala que el Designer actual.
2. **Presupuesto de la FRAM de 8 KB.** Compiten:
   - el journal de DataLog (≤ 1 KB según A14_3; diferido en A14_6, hoy 0 B);
   - Retain (roadmap);
   - las recetas (~0,3–1 KB);
   - EVENT_LOG (por ejemplo 2 KB = 128 eventos).

   Hace falta un mapa de FRAM aprobado antes de I9 e I10.
3. **Fin normal de receta.**
   - Propuesta: mostrar «FINALIZADO» y esperar ACEPTAR.
   - Alternativa: volver solo a la selección tras N s.
4. **Política por defecto de los avisos.** Propuesta:
   - crítica = LATCH (cesar y reconocer);
   - menor = ACK;
   - info = AUTO.
5. **SILENCIAR.** El package no tiene buzzer. ¿Hay una sirena externa que controle el PLC? Si no, quitar la acción del contrato.
6. **Contrato con OpenPLC.** No hay `%MX`, así que habría que empaquetar los bits en `%MW`. Revisar con Kenyi (Alpha14).
7. **Slots de campos.** SEQUENCE y tendencia ocupan 1 slot de los 32 cada uno. ¿Está bien, o se reserva un presupuesto aparte?

---

## G. Decisiones aprobadas (2026-10-18)

| # | Tema | Decisión |
|---|---|---|
| G.1 | Alcance | **Alpha15** cierra con I1 (VALUE duración y miles, BOOL «Piloto»), I2 (RTC), I3 (tildes cp437), Apariencia A1 y el buzzer. **Alpha16** lleva el resto: I4–I15 y las ideas aprobadas (G.9). |
| G.2 | Capa de avisos | Va **por encima** de IDLE y USER, como capa propia del firmware. Se revisa el firmware y se actualiza en Alpha16 (I5). |
| G.3 | Teclas de las plantillas | Los elementos seleccionables de cada pantalla (botones de RECIPE, diálogos) se recorren con ◀ ▶ y se activan con OK. El árbitro de I4 sólo garantiza que una tecla tenga un único dueño. |
| G.4 | Registro de eventos | Se usa **`JWPLCDataLog`** (anillo en RAM + SD, ya optimizado para registro continuo; el journal en FRAM sigue diferido, ver G.6). No se crea otro almacenamiento de eventos. La página EVENT_LOG lee el anillo en RAM y, si hay tarjeta, el archivo. |
| G.5 | Recetas: vistas | «En proceso» y «pausado» mantienen la **vista de ejecución**; «detenido» vuelve a la **selección**. ESC nunca detiene. Fin normal o con error: **FINALIZADO + ACEPTAR**, para que el operario sepa cómo terminó. |
| G.6 | Mapa de la FRAM | El **último KB** queda para el sistema y el usuario tiene libre acceso del byte 0 al 7 KB (abajo). |
| G.7 | Avisos por defecto | Críticos: se mantienen hasta que **se resuelve la falla y el operario reconoce** (LATCH). Menores: **sólo reconocimiento** (ACK). |
| G.8 | Buzzer / SILENCIAR | El JWPLC **sí tiene buzzer** (GPIO26, usado por los ejemplos Tetris/Flappy). Se integra al package como `JWPLC_Buzzer`, sin declaración en el sketch y configurable en `setup()` (hecho, §G.11). SILENCIAR calla el buzzer de la HMI y, si existe, envía la solicitud a la sirena del PLC. |
| G.9 | Ideas aprobadas | Escalado (ganancia/offset) y umbrales de color en VALUE y BAR; nivel de acceso en el MENU; **confirmación con dos botones en pantalla** (G.10). |
| G.10 | Diálogo de confirmación | Dos botones **[ CANCELAR ] [ ACEPTAR ]**. Por defecto está seleccionado **CANCELAR**; ▶ pasa a ACEPTAR, ◀ vuelve y **OK ejecuta el botón seleccionado**; ESC equivale a CANCELAR. Se usa en los nodos críticos del MENU, en DETENER de RECIPE y en cargar o guardar receta. |
| G.11 | OpenPLC | Se agregan **bits de memoria `%MX`** (IEC 61131-3) al runtime de OpenPLC para los bits de aviso y de solicitud, en vez de empaquetarlos en `%MW`. Le toca a Alpha14 (Kenyi), en el repo `openplc-editor`. |
| G.12 | Slots | Se mantienen los 32 campos por ahora; más adelante se revisa subir a 64 o 128. |

### G.6 Mapa de FRAM (8 KB actuales) y RAM de DataLog — propuesta de reparto

**Punto de partida `[visto]`:** `JWPLCDataLog` hoy **no usa FRAM**.

- `JW_SD.h`/`JW_SD.cpp` sólo tienen el anillo en RAM (`JW_SDDataLogConfig::bufferSize`, 4096 B por defecto, reservado en `begin()`) y el commit a la SD.
- A14_6 dejó el journal en FRAM diferido (`DATALOG_FRAM_JOURNAL=DEFERRED_NON_BLOCKING`, pipeline `RAM_RING_TO_SD`) justamente porque no había un mapa de FRAM.
- El «≤ 1 KB» de A14_3 es un **techo**, no una reserva. Así que el KB de sistema no tiene que cederle nada hoy.

**FRAM (8 KB):**

```text
0x0000 – 0x1BFF   7 KB   USUARIO (libre: sketch, Retain de OpenPLC, datos propios)
0x1C00 – 0x1FFF   1 KB   SISTEMA JWPLC
   0x1C00 – 0x1C1F    32 B   cabecera: firma «JWSY», versión del mapa, CRC
   0x1C20 – 0x1C5F    64 B   avisos: estado LATCH/ACK de los críticos (sobrevive al corte) + contadores
   0x1C60 – 0x1DFF   416 B   recetas: 8 × 52 B (nombre 12 B + 8 parámetros × 4 B + CRC 4 B + 4 B libres)
   0x1E00 – 0x1EFF   256 B   últimos eventos persistentes: 16 × 16 B (lo que se vio antes del corte)
   0x1F00 – 0x1FFF   256 B   reserva: journal de DataLog o Retain del sistema (cuando se decida)
```

**RAM (no FRAM):**

| Quién | Cuánto | Nota |
|---|---|---|
| Anillo de `JWPLCDataLog` de EVENT_LOG | 2048 B | Mitad del valor por defecto: los eventos son líneas cortas y sólo transiciones |
| Anillo de eventos de la página | 64 × 16 B = 1 KB | Lo que lista la página EVENT_LOG |
| Avisos (32 slots) | ~256 B | Estado vivo; la FRAM sólo guarda lo que debe sobrevivir |

Criterios:

- El usuario pierde **0 B** de los 7 KB acordados.
- Si mañana se habilita el journal de DataLog en FRAM, usa la reserva de 256 B. Con commits a la SD cada 512 B o 5 s, 256 B cubren ~2,5 s de eventos; para más, se espera a la FRAM de 32 KB.
- Con la FRAM de 32 KB, el sistema sigue siendo el **último KB** (0x7C00–0x7FFF) y cada zona crece sin mover la cabecera (la versión del mapa lo indica).
- Sin tarjeta SD, el registro es RAM + los 16 últimos en FRAM: el operario siempre ve lo reciente, incluso tras un corte.

Los offsets se fijan en código en I9/I10 (`JW_FRAM` todavía no define una zona de sistema) y se protegen con una comprobación: escribir del sketch por encima de 0x1BFF se documenta como prohibido.

### G.13 `JWPLC_Buzzer` (hecho en esta ronda)

- **Dónde está:** `JWPLC_GlobalPeripherals/src/JWPLC_Buzzer.{h,cpp}`, con el pin en `jwplc_hardware_config.h` (`JWPLC_HAS_BUZZER`, `JWPLC_BUZZER_PIN 26`).
- **Objeto global:** `JWPLC_Buzzer`, disponible por el autoload, sin declararlo en el sketch.
- **Funciones:**
  - `setVolume(0..255)`, con la misma escala que los ejemplos;
  - `setEnabled`;
  - `tone(Hz, ms)`;
  - `beep()`;
  - `play(notas, n, loop, velocidad)`;
  - `stop()`, `isPlaying()` e `isBusy()`.
- **No bloquea:** un `esp_timer` de una sola vez apaga la nota o pasa a la siguiente, sin `delay()` ni `service()`. Un `beep()` interrumpe la melodía y ésta sigue al terminar, como los SFX del Tetris.
- **El pin** se configura con LEDC en el primer uso. Si nada usa el buzzer, el GPIO26 queda libre.
- **Compilación ESP32 `[ejecutado]`:** `07.Buzzer_Basic` 436964 B / 29580 B. Tetris, Flappy y `A15_8_Menu_Designer` siguen compilando. Prueba física `[no verificado]`.
- **Compatibilidad:** los ejemplos Tetris/Flappy siguen con su propio manejo del pin; no mezclarlos con `JWPLC_Buzzer` en un mismo sketch. Migrarlos a la API queda opcional.
