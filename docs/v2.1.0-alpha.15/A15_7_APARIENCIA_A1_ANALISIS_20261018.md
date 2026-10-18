# Alpha15 · Apariencia: análisis de la propuesta A1 (flecha lateral + resaltado)

Fecha: `2026-10-18` · Estado: **implementada** (ver §6; aprobada como parte del cierre de Alpha15, plan §G.1)

```text
A1_PROMPT_REVIEW=DONE
A1_IMPLEMENTATION=PASS (TEXT/VALUE/BOOL y MENU; BAR en A15-7E)
```

## 1. Qué está bien

El objetivo es correcto: la propiedad activa, la zona del mini-modelo y el cuadro de color deben leerse de un vistazo.

Las líneas largas actuales del canal de flechas agregan ruido; reemplazarlas por un indicador local en la zona es mejor.

## 2. Correcciones al prompt (verificadas en el código)

1. **La unidad no es del «Valor».**
   - El prompt pide destacar en VALUE «la región del valor numérico, incluyendo su unidad».
   - En el firmware la unidad se dibuja con el color de la **etiqueta** (`JWPLC_UI.cpp:drawFieldStatic`, `colors.label`).
   - Si el contorno de «Valor» envolviera la unidad, señalaría una zona que ese color no cambia.
   - Correcto: Valor = sólo el número; Etiqueta = etiqueta + unidad (dos contornos).

2. **Naranja contra naranja.**
   - El naranja de acento del Designer (#ff9a43) es casi el color de edición por defecto del MENU (`editBackground = 0xFD20`) y uno de los 8 de la paleta.
   - En el Caso 2 (Editando) del MENU, un contorno naranja sobre una fila naranja no se ve o se confunde con la edición real. El prompt lo advierte pero no lo resuelve.
   - Propuesta: el indicador del editor nunca usa un color de la paleta RGB565. Usar «hormigas» (discontinuo animado) blanco y negro de 1 px, separado 1 px de la zona, o un doble trazo (negro fuera, naranja dentro).
   - Así se distingue en cualquier fondo y nunca parece parte de la HMI.

3. **«Flecha a la izquierda» del elemento.**
   - En el mini-modelo la etiqueta empieza a 3 px del borde del campo (6 px a ×2). No cabe una flecha sin pisar el marco.
   - En layout En línea, el valor tiene la etiqueta a su izquierda: una flecha ahí taparía la etiqueta, lo que el propio prompt prohíbe.
   - Propuesta: un **canal fijo** de ~10 px a la izquierda del mini-modelo, fuera de la imagen, donde la flecha ▶ se ubica a la altura (Y) de la zona activa.
   - Para el eje X, una **banda en la regla superior** (como la regla del lienzo) marca el ancho de la zona.
   - Nada se dibuja sobre los píxeles de la HMI.

4. **BAR no tiene mini-modelo todavía.**
   - Su Apariencia es la del inspector de BAR (A15-7E pendiente).
   - Aplicar A1 a BAR implica primero construir su mini-modelo con el pintor de `designer-bar.js` y respetar los colores que comparte el firmware: `JWPLC_UIBarField` usa `colors.value` para la barra, `colors.background` para la pista y `colors.label` para etiqueta y unidad.
   - Es más trabajo que «aplicar el indicador», y conviene separarlo.

5. **BOOL por estado.**
   - Etiqueta y marco son comunes: al elegirlos, el indicador aparece en **ambas** vistas (FALSE y TRUE).
   - Texto y fondo del estado aparecen sólo en la vista de su estado.
   - El prompt no lo dice y es la principal fuente de confusión en ese modo.

6. **Escala y 4K.**
   - Los mini-modelos se dibujan a ×1–×3 entero y el CSS puede reducirlos.
   - Las posiciones deben salir de `getBoundingClientRect()` del canvas (ya lo hace `designer-legend-arrows.js`), no de cálculos con la escala, para que coincidan a 125–200 % de zoom de Windows.

## 3. Prompt refinado (para aplicar cuando se apruebe)

> JWPLC HMI Designer · Alpha15 · Apariencia — Propuesta A1 refinada
>
> Reemplaza las flechas de canal largas (`poc/designer-legend-arrows.js`) por un indicador
> local en TEXT/VALUE/BOOL (`designer-field-inspector.js`) y MENU (`designer-menu.js`).
> BAR queda fuera hasta que tenga mini-modelo (A15-7E).
>
> Indicador de la propiedad activa (`activeColorKey`):
>
> 1. **Contorno** alrededor de la zona: doble trazo de 1 px (negro exterior y naranja de acento
>    interior) o «hormigas» blanco/negro, a 1 px de la zona. Nunca un color de la paleta
>    RGB565 de la HMI.
> 2. **Marcador ▶** en un canal fijo de 10 px a la izquierda del mini-modelo, a la altura de la
>    zona, más una banda en una mini-regla superior con el ancho de la zona. Nada se dibuja
>    sobre los píxeles del widget.
> 3. **Fila de la leyenda activa** con fondo de acento tenue y el cuadro con borde naranja (ya
>    existe). Sin líneas que crucen el inspector.
>
> Zonas (de la geometría real del firmware/lienzo):
>
> - Etiqueta: etiqueta **y unidad** (dos rectángulos).
> - Valor: sólo el texto del valor.
> - Fondo: interior del campo menos el texto, con el contorno en el margen.
> - Marco: el borde.
> - BOOL por estado: lo común se marca en ambas vistas; texto y fondo, sólo en la vista de su estado.
> - MENU: la fila o el texto según la propiedad (título, ítem, valor, selección, edición, fondo).
>   En el Caso 2 el contorno no debe confundirse con la fila naranja de edición.
>
> Comportamiento: actualizar con `syncAppearance()` sin reconstruir la tarjeta; sin desplazar el
> layout; recalcular con `ResizeObserver`; posiciones con `getBoundingClientRect()`.
>
> Validación: agregar al `tests/inspector-smoke.cjs` y al `tests/menu-smoke.cjs` comprobaciones
> de que el contorno coincide con la zona (coordenadas) y que los píxeles del mini-modelo son
> idénticos con y sin indicador. Entregar capturas de VALUE, BOOL (FALSE y TRUE) y MENU
> (navegando y editando).

## 4. Cómo habilitar una mirada de diseño

No hace falta habilitar nada para que el agente analice un prompt como este; este documento es ese análisis. Si se quiere un criterio de diseño adicional y reutilizable:

- **Plugins del catálogo** (Anthropic): «Design» (skills `design-critique`, `accessibility-review`, `ux-copy`) y «frontend-design».
  - Se activan desde la tarjeta que aparece en el chat o en claude.ai → Plugins.
  - Después se pide, por ejemplo, «usa design-critique sobre las capturas de Apariencia».
- **Skill propio en el repo** (sirve a Claude, GPT/Codex u otros):
  - `.claude/skills/hmi-designer-ux/SKILL.md` con las reglas de la casa: Propuesta 2 de BAR, tarjetas y no selects, «no ofrecer lo que el firmware no dibuja», paleta RGB565, indicadores fuera de la imagen, captura obligatoria.
  - Se carga solo cuando un agente trabaja en la Apariencia.

## 5. Crítica de diseño (skill `design:design-critique`)

Hecha sobre las capturas actuales de VALUE, BOOL por estado y MENU (2026-10-18).

### Impresión general

La relación color ↔ zona ya existe, pero hoy se lee **después** de la leyenda. Las flechas largas atraen la vista tanto como el mini-modelo, cuando el mini-modelo debería mandar. A1 va en la buena dirección si el indicador queda fuera de los píxeles de la HMI y no usa un color que la HMI pueda tener.

### Usabilidad

| Hallazgo | Severidad | Recomendación |
|---|---|---|
| VALUE con marco amarillo: el contorno naranja discontinuo de «Marco» se superpone al marco amarillo real y apenas se distingue | 🔴 | Separar el contorno 2 px **fuera** del borde y usar doble trazo (negro + acento) |
| MENU Caso 2: la fila de edición es naranja (`0xFD20`), igual que el acento | 🔴 | Indicador neutro (doble trazo o hormigas) y nunca relleno; el marcador ▶ en el canal externo |
| Mini-modelos de campos anchos a ×1 (VALUE de ~130 px): la zona activa mide 2–3 px de alto | 🟡 | Mini-modelo a ×2 como mínimo con la leyenda debajo (ya pasa en BOOL), o ampliación de la zona activa |
| Las letras T/F/V/M exigen memoria; la nota explicativa queda lejos, bajo la tarjeta | 🟡 | Rótulo completo en la fila («Texto», «Fondo»…) o tooltip; quitar la nota |
| Tres anillos naranjas a la vez: cuadro activo, flecha y color de la paleta | 🟢 | El de la paleta en blanco/gris; el naranja reservado para «lo que se edita» |

### Jerarquía visual

- **Lo primero que se ve**: las flechas y los cuadros de color. Debería ser el mini-modelo con su zona resaltada.
- **Orden de lectura**: mini-modelo → zona resaltada → fila de la leyenda activa → paleta. A1 lo consigue si se quitan los conectores largos y la fila activa tiene un fondo de acento tenue.

### Consistencia

| Elemento | Problema | Recomendación |
|---|---|---|
| Indicador | Campos y MENU usan el mismo componente (bien), pero BAR no tiene mini-modelo | Mantener un único componente compartido y sumar BAR en A15-7E |
| BOOL por estado | Etiqueta y marco aparecen en ambos casos sin indicar que son comunes | Indicador en ambas vistas y rótulo «(común)» en esas filas |
| Unidad | Visualmente parece parte del valor, pero es color de etiqueta | Zona de «Etiqueta» = etiqueta + unidad |

### Accesibilidad

- **Cuadros de color**: 20×18 px, por debajo de 24×24. Subir a 24×24 sin cambiar el layout de las filas.
- **Nota inferior**: gris azulado sobre fondo oscuro, con contraste bajo para 11 px. Va a «Usabilidad» de todas formas (eliminar).
- **Color**: el indicador no debe depender sólo del color. El doble trazo y el marcador ▶ dan forma, además de color.

### Lo que funciona

- Mini-modelos pintados con el mismo dibujo del firmware: lo que se ve es lo que sale en la TFT.
- Edición en vivo con la paleta rápida y el selector nativo, sin cerrar nada.

### Recomendaciones por prioridad

1. **Indicador neutro, fuera de los píxeles**: doble trazo a 1–2 px de la zona más un marcador ▶ en un canal externo. Resuelve los dos 🔴.
2. **Quitar los conectores largos y marcar la fila activa**: menos ruido y orden de lectura correcto.
3. **Mini-modelo a ×2 como mínimo**: la zona activa debe medir al menos 6–8 px en pantalla.

## 6. Implementación (2026-10-18)

`poc/designer-zone-indicator.js` reemplaza a `designer-legend-arrows.js`, que se elimina. Es un único componente para campos y MENU.

| Punto del análisis | Cómo quedó |
|---|---|
| 1. Unidad = color de la etiqueta | La zona Etiqueta son dos contornos: etiqueta y unidad. En el piloto suma el aro |
| 2. Naranja contra naranja | Doble trazo de 1 px: negro del lado de la HMI, acento del lado de la zona. Se distingue sobre la fila de edición `0xFD20` (captura del caso «Editando») |
| 3. Flecha encima del widget | ▶ en un canal de 12 px a la izquierda, a la altura de la zona, y banda en una mini-regla de 8 px arriba. El canvas deja ese margen |
| 4. BAR sin mini-modelo | Fuera de alcance (A15-7E); usará el mismo `draw()` |
| 5. BOOL por estado | Lo común se marca en los dos casos y su fila dice «(común)». Texto y fondo, sólo en su caso |
| 6. Escala y zoom | Posiciones con `getBoundingClientRect()`; se recalcula con `ResizeObserver` |
| Fondo | Contorno hacia adentro, en el margen del campo |
| Fila activa | Fondo de acento tenue y rótulo en negrita |
| Paleta | La pastilla activa pasa a blanco: el naranja queda sólo para lo que se edita |

Verificación `[ejecutado]`:

- `tests/inspector-smoke.cjs` · APARIENCIA A1:
  - BOOL por estado: la etiqueta común se marca en los 2 casos y el fondo TRUE sólo en el suyo;
  - VALUE con unidad: 2 contornos;
  - los píxeles de los mini-modelos son idénticos con y sin indicador.
- `tests/menu-smoke.cjs`: un contorno doble por caso, el ▶ a la altura de la fila activa y los píxeles sin cambio.
- Capturas de VALUE, BOOL (FALSE y TRUE) y MENU (navegando y editando) revisadas.

Quedan de la crítica (§5), sin hacer:

- cuadros de 24×24: las filas del MENU miden lo que una fila pintada, y a ×2 no entran;
- quitar las letras T/F/V/M.
