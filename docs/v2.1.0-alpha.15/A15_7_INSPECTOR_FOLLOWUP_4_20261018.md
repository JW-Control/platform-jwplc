# Alpha15 · A15-7 seguimiento (4.ª ronda) — ESC, vuelta a la IDLE e "Ir al inicio" en el MENU

Fecha: `2026-10-18`

```text
ESC_CHAIN=CONFIRMADO [visto] (edición -> submenú -> raíz -> selector NN/TT -> IDLE)
IDLE_RETURN_OTHER_KEY=RETIRADO (Designer; proyectos viejos pasan a ESC)
IDLE_TIMEOUT_IN_PAGE_CONTENT=PASS_SOURCE (antes se suspendía al entrar al contenido)
MENU_GO_IDLE_NODE=PASS_SOURCE (JWPLC_UIMenuGoIdle + Designer + host-sim)
PHYSICAL=[no verificado]
```

## 1. Cómo funciona ESC hoy `[visto]`

Con varias páginas:

1. **Selector**: NN/TT con fondo **negro**. Teclas: ◀ ▶ cambian de página, OK entra, ESC vuelve a la IDLE.
2. **Contenido**: NN/TT con fondo **blanco**. Las 5 teclas son del contenido y ESC pertenece al sistema.
   - Sin MENU, ESC vuelve al selector.
   - Con MENU, el MENU atiende primero (`jwplcUIRuntimeServiceInput`):
     - en edición, ESC cancela;
     - en un submenú, ESC sube un nivel;
     - en la raíz no lo consume y el selector lo toma.
3. Mientras se está en el contenido, `enterContentMode` suspende la vuelta por tecla. Así **sólo** se vuelve a la IDLE con ESC desde el selector (negro).

Con una sola página no hay selector. ESC en la raíz del contenido vuelve a la IDLE.

Es exactamente la secuencia pedida: entrar al contenido, usar las teclas, retroceder con ESC hasta el NN/TT negro y un ESC más para la IDLE.

## 2. Cambios

### «Con otra tecla» retirado

La vuelta a la IDLE con otra tecla se cruzaba con ◀ ▶ OK del selector y del contenido.

- El panel *Pantalla y arranque* ahora ofrece: **ESC**, **Por inactividad** y **Nunca**.
- Un proyecto guardado con «otra tecla» abre como ESC.
- El firmware conserva `IDLE_RETURN_BUTTON_ONLY` por compatibilidad, pero el Designer ya no lo emite.

### La vuelta por inactividad funciona en cualquier sección

Antes, al entrar al contenido el modo de vuelta se cambiaba a `IDLE_RETURN_DISABLED`, incluido el modo por inactividad, así que el tiempo sólo contaba en el selector.

Ahora `JWPLC_UI_Pages.cpp:enterContentMode` sólo suspende la vuelta **por tecla** (ESC u otra tecla). Con «Por inactividad» la IDLE vuelve tras N s sin teclas, tanto en el selector como en el contenido. Cualquier tecla reinicia la cuenta.

### «Nunca» + ítem de MENU «Ir al inicio»

Con «Nunca», la salida la decide la aplicación:

- **Desde el sketch** (ya era posible): `JWPLC_Display.goIdle()`. Por ejemplo, al confirmar un BOOL del menú:
  `if (JWPLC_Display.menuChanged(MENU_AJUSTES, &n) && n == MENU_AJUSTES_SALIR && salir) { salir = false; JWPLC_Display.goIdle(); }`
- **Sin código (nuevo)**: un ítem de acción en *Árbol y datos* → **⏎ Ir al inicio**.
  - Al pulsar OK sobre él se vuelve a la IDLE.
  - No necesita variable.
  - Se dibuja con `<` a la derecha (el submenú usa `>`).

Se eligió un ítem de **acción** en vez de una variable BOOL que haya que poner en true: es un solo OK, no deja una variable "pegada" en true y no requiere código en el sketch.

Firmware:

```cpp
JWPLC_UIMenuGoIdle(JWPLC_UI_MENU_ROOT, "Ir al inicio")   // JWPLC_UI_MENU_GO_IDLE
#define JWPLC_UI_HAS_MENU_GO_IDLE 1
```

`goIdle()` se llama al atender la entrada. El refresco revisa el modo después, así que la IDLE se dibuja en ese mismo ciclo. `goIdle()` también espera a que se suelte la tecla, para que el OK no vuelva a despertar el HMI.

## 3. Verificación `[ejecutado]`

- `tests/menu-host-sim.cjs`, sección «IR AL INICIO» con el `JWPLC_UI_Menu.cpp` real en host:
  - acepta un nodo sin variable;
  - RIGHT no sale;
  - OK llama a `goIdle()` y se consume;
  - no se lee como variable al refrescar.
- `tests/menu-smoke.cjs`:
  - el botón «⏎ Ir al inicio» agrega el ítem sin variable;
  - el header emite `JWPLC_UIMenuGoIdle` y compila;
  - el símbolo existe en el fuente.
- `tests/codegen-smoke.cjs`: un proyecto con vuelta «otra tecla» genera `IDLE_RETURN_ESC_ONLY` y no `setIdleReturnButton`.
- `run-all` 6/6 PASS.
- Compilación ESP32:
  - sketch con `JWPLC_UIMenuGoIdle` + `IDLE_RETURN_DISABLED`: 425933 B de flash, 35836 B de RAM;
  - `A15_8_Menu_Designer`: 440813 B de flash, 36108 B de RAM.

Gate físico pendiente:

- con 2 páginas y un MENU, recorrer la cadena de ESC completa;
- con «Por inactividad» dentro del contenido, comprobar que vuelve a la IDLE;
- con «Nunca», comprobar que «Ir al inicio» vuelve y que el OK no re-despierta.
