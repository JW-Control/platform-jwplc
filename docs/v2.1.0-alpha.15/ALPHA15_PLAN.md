# v2.1.0-alpha.15 — JWPLC HMI Designer

Fecha de inicio: `2026-10-16` · Rev. 2: `2026-10-16`

## Base

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner
PARENT_BRANCH=v2.0.1-alpha.15-(HmiDesigner)   (trabajo de André, se conserva intacto)
BASE_SHA=cc66d69aa1eca461b7111c856293e5f1f6e103b4
MAIN_BASE=aa0bfed170c397cc4135c72c273223558ab3bf72  (alpha12 publicada)
AUDIT=docs/v2.1.0-alpha.15/A15_0_HMI_DESIGNER_AUDIT_20261016.md
OPENPLC_DECISION=docs/v2.1.0-alpha.15/A15_OPENPLC_INTEGRATION_DECISION_20261016.md
```

## Organización de alphas (decisión del jefe, 2026-10-16)

```text
ALPHA13=correcciones y robustez del package        (jefe)
ALPHA14=mejora de OpenPLC                           (Kenyi)
ALPHA15=mejora del HMI Designer                     (André)
```

Esto reemplaza la numeración de `docs/JWPLC_V2_1_CLOSURE_ROADMAP.md` y de los
documentos del fork que asignan TFT a Alpha13.

## Objetivo

Un **único** HMI Designer, en la línea de Lopaka pero exclusivo para la TFT
ST7789 320x170 del JWPLC Basic, que permita crear páginas sin tocar la API TFT
cruda:

1. **Inspector gráfico**: las opciones se eligen viendo lo que hacen (tarjetas
   visuales), agrupadas en pestañas, no rellenando celdas. Patrón ya aplicado
   en BAR; se extiende a todos los tipos.
2. **Dos modos de proyecto, una sola interfaz**:
   - **Arduino IDE**: el usuario nombra las variables.
   - **OpenPLC**: las variables ya existen en el programa; el usuario sólo
     elige cuál vincular.
3. **Nuevo tipo MENU**: árbol de menús/submenús para **ver y configurar**
   variables (ítems de sólo lectura y de lectura/escritura) con la botonera.
4. Todo lo que el Designer ofrece **compila y se ve igual en la TFT**.

## No objetivos

- Touch, LVGL, animaciones, imágenes BMP arbitrarias.
- Reintroducir TFT_eSPI como dependencia del usuario.
- Meter OpenPLC en el autoload Arduino.
- Subir `MAX_FIELDS=32` sin un gate de memoria/tiempo.
- `JWPLC_LogicRuntime` / `JWPLC_LogicRuntime_UI` (proyecto aparte, pausado).

---

## Decisiones de arquitectura

### D1 — Formato `.jwhmi`: cambio mínimo y compatible

Se adopta el criterio del plan del fork (`openplc-editor/docs/jwplc/JWPLC_HMI_PLAN_INDUSTRIAL.md` §5.3):
el header generado **no cambia** entre modos; se añade el vínculo.

```jsonc
{
  "format": "JWPLC_HMI_PROJECT",
  "version": 2,
  "revision": 17,                         // mismo número se escribe en el header
  "link": { "mode": "arduino" | "openplc" | "none" },
  "fields": [
    { "type": "VALUE", "id": "FIELD_TEMP", "variable": "temperatura",
      "plcBinding": { "var": "TEMP_TANQUE", "type": "REAL", "access": "R" } }
  ],
  "menus": [ ... ]                        // ver §MENU
}
```

- Modo Arduino: `variable` la escribe el usuario; no hay `plcBinding`.
- Modo OpenPLC: el usuario elige `plcBinding.var` de la lista; `variable` se
  genera automáticamente (oculta en la UI) a partir del nombre PLC.
- `version 1` abre sin cambios (`link.mode="arduino"`).
- `revision` compartida `.jwhmi` ↔ header detecta headers desactualizados.

### D2 — Detección del modo

| La carpeta vinculada contiene | Modo |
|---|---|
| `*.ino` | Arduino |
| `project.json` + `devices/configuration.json` (estructura del proyecto OpenPLC Editor) | OpenPLC |
| ambos o ninguno | preguntar |

Fuente de variables en modo OpenPLC:

- **Abierto desde OpenPLC** (anfitrión `jwplc-hmi-host/1`): `listVariables()`
  del editor (globales y variables de programa, con tipo y ubicación).
- **Designer de escritorio vinculado a la carpeta**: sólo lectura de
  `project.json → configuration.resource.globalVariables` (nombre, tipo,
  `location`). Las variables locales de POUs no se listan en este modo.

### D3 — Codegen como función pura

`generate(model, targetProfile) -> { "JWPLC_HMI_Generated.h": text }`

- Un solo módulo, sin `.replace()` sobre texto de otras capas ni lectura del DOM.
- Validación previa contra el perfil de firmware.
- Ejecutable con `node` (tests de golden files) y por OpenPLC al compilar.

### D4 — Lado OpenPLC (responsabilidad de Alpha14)

Lo detalla `A15_OPENPLC_INTEGRATION_DECISION_20261016.md`. Resumen: OpenPLC
aloja el Designer, entrega variables, guarda en `Proyecto/hmi/` y genera los
bindings al compilar con el mapa de depuración de STruC++.

### D5 — Gráfica estática fuera de `jwplcUIUpdate()`

Figuras, RAW_TEXT y NAV_BAR se dibujan al entrar a la página
(`jwplcUIPageEnter(page)` o un objeto estático del motor), nunca en cada tick
(`docs/v2.1.0-alpha.14/A14_3_HMI_DIRTY_ON_DEMAND_STRONG_PASS_20260913.md`).

---

## Inspector gráfico (patrón Propuesta 2)

Referencia: `tools/jwplc-hmi-designer/JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/`.

```text
[Diseño] [Datos] [Técnico]
  Diseño -> Forma | Contenido | Apariencia | Ubicación
  Datos  -> vínculo (variable C++ o selector de variable PLC) + simulación local
  Técnico-> nombre, ID C++, contrato copiable, diagnóstico
```

Reglas:

- Opción enumerable → tarjeta visual con mini-preview, no `<select>`.
- Colores → muestra + paleta RGB565; hex sólo como dato secundario.
- Mostrar sólo controles de la variante activa.
- Ninguna opción sin soporte en el firmware.
- La simulación en "Datos" nunca escribe al PLC.

Prioridad (pedido del jefe): **terminar BAR y rehacer TEXT, VALUE y BOOL** primero.
Especificación: `A15_7_INSPECTOR_REDESIGN_SPEC_20261016.md`.
Después: RAW_TEXT → figuras (completar) → NAV_BAR → PixelMap → MENU (nuevo).

---

## Navegación existente (se conserva)

Validado en Alpha8/Alpha11 (`tools/jwplc-hmi-designer/README.md`,
`docs/v2.1.0-alpha.11/A11_BUTTON_ROBUSTNESS_GATE.md`):

```text
Modo USER: indicador NN/TT arriba a la derecha
PAGE_SELECT : LEFT/RIGHT cambia página (el indicador se desplaza)
              OK entra -> el indicador queda sombreado
PAGE_CONTENT: LEFT/RIGHT/UP/DOWN/OK -> contenido de la página
              ESC vuelve a PAGE_SELECT y limpia eventos pendientes
```

El MENU no reemplaza esta navegación: es un objeto **dentro** de una página
y recibe las teclas mientras la página está en PAGE_CONTENT.

---

## Nuevo tipo MENU

Propósito: **configurar** variables, no sólo mostrarlas.

```text
MENU (objeto de página)
 └─ árbol de nodos
     ├─ SUBMENU   { label, hijos[] }
     ├─ VIEW      { label, tag, formato }            sólo lectura
     ├─ EDIT_NUM  { label, tag, min, max, paso, decimales, unidad }   lectura/escritura
     ├─ EDIT_BOOL { label, tag, texto ON/OFF }       lectura/escritura (toggle)
     ├─ EDIT_ENUM { label, tag, opciones[] }         entero -> lista de textos
     ├─ ACTION    { label, tag BOOL, pulso|set|reset } p. ej. "Reset contador"
     └─ GOTO_PAGE { label, página }
```

Teclas dentro del MENU (PAGE_CONTENT):

```text
UP/DOWN   mover selección / cambiar valor en edición
OK        entrar a submenú / empezar edición / confirmar
ESC       cancelar edición / subir un nivel / en la raíz -> PAGE_SELECT
LEFT/RIGHT cambiar dígito o paso en edición (opcional)
```

Reglas:

- Al mover la selección sólo se repintan el ítem anterior y el nuevo; el marco
  del menú se dibuja al entrar a la página.
- La escritura se aplica al confirmar con OK; ESC descarta.
- Límites `min/max` se validan en el equipo, no sólo en el Designer.
- Modo Arduino: el `tag` es una variable C++ del sketch.
- Modo OpenPLC: el `tag` es un `plcBinding` con `access:"RW"`; la escritura
  se encola y se aplica al inicio del scan siguiente, sin pisar forzados y
  bloqueada en STOP/ERR (plan del fork, P4/P5).
- Implementación en firmware: objeto declarativo en `JWPLC_UI`
  (`JWPLC_UIMenuField` + tabla de nodos en flash). Requiere regenerar
  `libJWPLC_Display.a` (coordinar con Alpha13).
- Indicador de "editando": ítem invertido o con marco de color, consistente
  con el sombreado del indicador NN/TT.

### MENU MVP (decisión del jefe, 2026-10-16)

```text
MENU_MAX_DEPTH=3            (raíz = nivel 1; submenú de submenú = nivel 3)
MENU_MVP_NODES=SUBMENU, VIEW, EDIT_NUM, EDIT_BOOL
MENU_POST_MVP=EDIT_ENUM, ACTION, GOTO_PAGE, pulsación larga, clave por rama
TECLAS=6 (BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_ESC, BTN_OK, BTN_DOWN)
```

- El Designer bloquea crear un submenú por debajo del nivel 3.
- El número de nodos totales y de ítems por nivel se mide en el gate A15-8
  (RAM/flash) y se fija después del MVP.
- Tras el MVP se evalúa en banco antes de ampliar.

---

## Gates

```text
A15-0  Auditoría de partida                                   PASS
A15-1  Codegen compila contra la API real (B1-B4)             PENDING   (= F1 del fork)
A15-2  Persistencia y páginas (B5, I1, I2)                    PENDING   (= F1)
A15-3  Gráfica estática fuera del tick (B6)                   PENDING   (= F1, O2)
A15-4  CI del Designer: Playwright + arduino-cli               PENDING   (= F1)
A15-5  Codegen puro + golden tests (D3)                       PENDING   (= F1/F6, O6)
A15-6  .jwhmi v2 + modo Arduino/OpenPLC + host adapter        PENDING   (= F2)
A15-7  Inspector gráfico: 7A BAR, 7B VALUE, 7C TEXT, 7D BOOL  PENDING   (prioridad alta)
       7E+ RAW_TEXT, figuras, NAV_BAR, PixelMap               PENDING
A15-8  Widget MENU (Designer + JWPLC_UI)                      PENDING   (= F5 setpoints/teclas)
A15-9  Paridad física en JWPLC Basic                          PENDING
A15-10 Empaquetado, versiones, docs                           PENDING
```

Dependencias con otras alphas:

- `libJWPLC_TFT.a` / `libJWPLC_Display.a` regenerados (fuentes, primitivas,
  MENU): Alpha13.
- Anfitrión en OpenPLC, `generate-hmi`, HAL, Designer dentro del VPP: Alpha14.

Orden: A15-1 → A15-2 → A15-3 → **A15-7A..7D** → A15-5 → A15-4 → A15-6 → A15-8 → A15-7E+ → A15-9 → A15-10.
