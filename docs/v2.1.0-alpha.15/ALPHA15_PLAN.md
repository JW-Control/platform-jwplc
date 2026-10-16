# v2.1.0-alpha.15 — JWPLC HMI Designer

Fecha de inicio: `2026-10-16`

## Base

```text
BRANCH=v2.1.0-alpha.15-HmiDesigner
PARENT_BRANCH=v2.0.1-alpha.15-(HmiDesigner)   (trabajo del practicante, se conserva intacto)
BASE_SHA=cc66d69aa1eca461b7111c856293e5f1f6e103b4
MAIN_BASE=aa0bfed170c397cc4135c72c273223558ab3bf72  (alpha12 publicada)
HMI_DESIGNER_BASELINE=Alpha11 CLOSED_PUBLISHED + trabajo del practicante (sep-oct 2026)
AUDIT=docs/v2.1.0-alpha.15/A15_0_HMI_DESIGNER_AUDIT_20261016.md
```

> Nota de numeración: `docs/JWPLC_V2_1_CLOSURE_ROADMAP.md` asignaba Alpha13 a
> TFT/Display y Alpha15 a Retain/FRAM. El propietario decidió trabajar el HMI
> Designer como alpha.15. El roadmap debe actualizarse antes del cierre.

## Objetivo

Un **único** HMI Designer, en la línea de Lopaka pero exclusivo para la
TFT ST7789 320x170 del JWPLC Basic, que permita crear páginas sin tocar la
API TFT cruda:

1. **Inspector gráfico**: las opciones se eligen viendo lo que hacen
   (tarjetas visuales), agrupadas en pestañas, no rellenando celdas.
2. **Dos modos de proyecto, una sola interfaz**:
   - **Arduino IDE**: el usuario nombra las variables.
   - **OpenPLC**: las variables ya existen (las definió el programa
     OpenPLC); el usuario sólo elige cuál vincular.
3. **Sistema de menús** integrado con la botonera de 6 teclas.
4. Todo lo que el Designer ofrece **compila y se ve igual en la TFT**.

## No objetivos

- Touch, LVGL, animaciones, imágenes BMP arbitrarias.
- Reintroducir TFT_eSPI como dependencia del usuario.
- Meter OpenPLC en el autoload Arduino.
- Subir `MAX_FIELDS=32` sin un gate de memoria/tiempo.

---

## Decisiones de arquitectura

### D1 — Modelo de proyecto v2 (`.jwhmi` version 2)

```jsonc
{
  "format": "JWPLC_HMI_PROJECT",
  "version": 2,
  "target": { "profile": "jwplc-basic-st7789-320x170-rot3" },
  "link": {
    "mode": "arduino" | "openplc",
    "path": "<carpeta vinculada>",
    "arduino": { "sketch": "MiProyecto.ino" },
    "openplc": { "project": "<archivo de proyecto OpenPLC>", "importedAt": "..." }
  },
  "tags": [
    { "id": "t1", "name": "temperatura", "type": "REAL",
      "source": { "kind": "cpp" } },
    { "id": "t2", "name": "MOTOR_ON", "type": "BOOL",
      "source": { "kind": "openplc", "location": "%QX0.0" } }
  ],
  "pages": [...],
  "widgets": [ { "type": "VALUE", "tag": "t1", ... } ],
  "menus": [...]
}
```

- Los widgets referencian `tag` (id), no un nombre C++ suelto.
- `version 1` se migra al abrir: cada `field.variable` crea un tag
  `source.kind=cpp` con el tipo deducido del widget (como hoy).
- El `target.profile` describe resolución, rotación, fuentes y widgets que el
  firmware soporta; la UI deshabilita lo que el perfil no soporta.

### D2 — Detección del modo

Al vincular una carpeta:

```text
contiene *.ino                       -> mode=arduino
contiene proyecto OpenPLC            -> mode=openplc   (marcador exacto: A CONFIRMAR con el fork)
ambos / ninguno                      -> preguntar al usuario
```

Diferencias visibles en la UI:

| | Arduino | OpenPLC |
|---|---|---|
| Panel "Datos" del inspector | campo de texto "Variable C++" + tipo | selector de variables disponibles filtrado por tipo compatible |
| Crear tag | sí | no (sólo vincular) |
| Renombrar tag | sí (actualiza codegen) | no (lo manda OpenPLC); se muestra nombre + ubicación |
| Salida | `JWPLC_HMI_Generated.h` (actual) | bridge generado para el runtime OpenPLC (D4) |
| Tag huérfano (variable borrada en OpenPLC) | n/a | widget marcado en rojo en "Problemas" |

### D3 — Codegen como función pura

`generate(model, targetProfile, backend) -> { files: { name: text } }`

- Un solo módulo, sin `.replace()` sobre texto de otras capas ni lectura del DOM.
- Backends: `arduino` (actual) y `openplc` (D4).
- Validación previa contra el perfil: cada llamada emitida debe existir en la API real.
- Tests de golden files ejecutables con `node` (sin navegador).

### D4 — Backend OpenPLC

Generar un archivo que, dentro del hook de UI, lea las variables localizadas
del runtime OpenPLC y llame a los setters de `JWPLC_Display`
(`setValue/setBool/setText/setBar`), siguiendo la frontera definida en
`docs/v2.1.0-alpha.8/ALPHA8_TO_ALPHA9_OPENPLC_HANDOFF.md`:

```text
OpenPLC / Ladder -> bridge -> JWPLC_Display.setXxx(...)
botonera / menú  -> bridge -> variables OpenPLC (sólo si el menú escribe)
```

Insumos necesarios del fork (pendientes):

- cómo genera el editor el sketch/glue para targets Arduino y la definición de placa JWPLC;
- buffers de variables localizadas del runtime, tipos y tamaños;
- formato del archivo de proyecto OpenPLC (para listar variables con nombre, tipo y ubicación);
- mapeo Modbus del HAL JWPLC;
- estructura del `.vpp`;
- si el Designer irá embebido en el editor o como app separada que importa el proyecto.

### D5 — Gráfica estática fuera de `jwplcUIUpdate()`

Figuras, RAW_TEXT y NAV_BAR se dibujan al entrar a la página
(`jwplcUIPageEnter(page)` o un `drawStatic` declarativo en `JWPLC_UI`), nunca
en cada tick. Es la misma política de dirty refresh que ya sigue
`JWPLC_Display` (`docs/v2.1.0-alpha.14/A14_3_HMI_DIRTY_ON_DEMAND_STRONG_PASS_20260913.md`).

---

## Inspector gráfico (patrón Propuesta 2)

Referencia: `tools/jwplc-hmi-designer/JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/`.

Estructura común para todos los tipos:

```text
[Diseño] [Datos] [Técnico]
  Diseño -> Forma | Contenido | Apariencia | Ubicación
  Datos  -> vínculo a tag (según modo) + simulación local
  Técnico-> nombre, ID C++, contrato copiable, diagnóstico
```

Reglas:

- Opción enumerable → tarjeta visual con mini-preview, no `<select>`.
- Colores → muestra + paleta RGB565, no hex como primera opción.
- Mostrar sólo controles de la variante activa.
- Ninguna opción sin soporte en el perfil de firmware.
- Simulación en "Datos" nunca escribe al PLC.

Tipos a migrar (orden propuesto): VALUE → TEXT → BOOL → RAW_TEXT →
figuras (completar) → NAV_BAR → PixelMap → MENU (nuevo).

---

## Sistema de menús + botonera

### Lo que ya existe en el firmware

- Botonera de 6 teclas `BTN_LEFT/UP/RIGHT/ESC/OK/DOWN`, scanner `jwplcBtnScan`
  a 5 ms, robusto en loop cerrado sin `delay()`
  (`docs/v2.1.0-alpha.11/A11_BUTTON_ROBUSTNESS_GATE.md`, `docs/v2.1.0-alpha.8/ALPHA8_HMI_BUTTON_VALIDATION.md`).
- Navegación multipágina con dos estados:

  ```text
  PAGE_SELECT : LEFT/RIGHT cambia página, OK entra
  PAGE_CONTENT: LEFT/RIGHT/UP/DOWN/OK -> aplicación, ESC vuelve a SELECT
  ```

  Al volver a SELECT se limpian eventos pendientes (`tools/jwplc-hmi-designer/README.md`, A11).
- El Display no consume latches de la aplicación (Alpha8).
- Regla de render propuesta para menús: al mover el cursor sólo se repintan
  el ítem anterior y el nuevo; el marco del menú se dibuja al entrar.

> `JWPLC_LogicRuntime` / `JWPLC_LogicRuntime_UI` es un proyecto aparte
> (runtime estilo LOGO!, pausado) y queda fuera del alcance de alpha.15.

### Propuesta de widget MENU

```text
MENU
  items[]: { label, icon?, action }
  action:
    GOTO_PAGE(page)
    OPEN_SUBMENU(menu)
    TOGGLE(tag BOOL)
    EDIT_VALUE(tag, min, max, step)   UP/DOWN cambia, OK confirma, ESC cancela
    SET_VALUE(tag, valor)
    CALLBACK(nombre C++)              sólo modo Arduino
  keys: UP/DOWN mueven, OK ejecuta, ESC sale/vuelve (configurable)
  layout: lista vertical | grilla | barra horizontal
```

- En modo OpenPLC, `TOGGLE/EDIT/SET` escriben la variable vinculada.
- Mientras un MENU tiene foco, posee las teclas declaradas; al salir se aplica
  la misma limpieza que CONTENT → SELECT.
- Implementación preferida: widget declarativo en `JWPLC_UI` (firmware,
  requiere recompilar `.a`) y el Designer sólo genera la tabla. Alternativa
  temporal: máquina de estados generada en el header (más código por proyecto,
  riesgo de robar latches).

Preguntas abiertas:

- ¿El menú reemplaza a PAGE_SELECT o vive dentro de PAGE_CONTENT?
- ¿Se permiten teclas largas (long-press) como acción distinta?
- ¿Cuántos ítems/niveles como máximo (RAM/flash)?

---

## Gates

```text
A15-0  Auditoría de partida                                   PASS
A15-1  Codegen compila contra la API real (B1-B4)             PENDING
A15-2  Persistencia y páginas (B5, I1, I2)                    PENDING
A15-3  Gráfica estática fuera del tick (B6) + gate de bus     PENDING
A15-4  JWPLC_TFT fuentes: .a regenerado + ABI + versión (B3)  PENDING
A15-5  Codegen puro + golden tests (D3, I5)                   PENDING
A15-6  Proyecto v2 + modo Arduino/OpenPLC (D1, D2)            PENDING
A15-7  Inspector gráfico por tipo                             PENDING
A15-8  Widget MENU + botonera                                 PENDING
A15-9  Backend OpenPLC (D4)                                   BLOCKED_ON_FORK_INPUTS
A15-10 Paridad física en JWPLC Basic (B7)                     PENDING
A15-11 Empaquetado, versiones, docs, roadmap                  PENDING
```

Orden recomendado: A15-1 → A15-2 → A15-3 → A15-5 → A15-4 → A15-6 → A15-7 →
A15-8 → A15-9 → A15-10 → A15-11. A15-1..A15-3 desbloquean un designer usable
con el firmware actual; A15-5 es prerequisito para A15-6/A15-9.
