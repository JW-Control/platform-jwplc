# Alpha11 · A11-7A — Inspector derecho exclusivo

Fecha: 2026-09-06

## Problema observado

Durante la validación de A11-7, el panel derecho mezclaba secciones de distintos
modos de edición. Se observaron dos casos principales:

- con un FIELD seleccionado seguía visible `Inspector · Texto GFX RAW`;
- con un PIXEL seleccionado quedaban controles RAW/FIELD por encima del
  `Inspector · PIXEL`, además de una segunda sección global de color.

La causa es que varias secciones dependen del atributo `hidden`, mientras reglas
de estilo del panel pueden mantener elementos con `display` explícito. Además,
A11-7 agregaba un inspector PIXEL sin un único coordinador de estado del panel.

## Decisión

El panel derecho pasa a tener modos mutuamente exclusivos:

```text
FIELD -> sólo inspector TEXT / VALUE / BOOL / BAR
PIXEL -> sólo Inspector · PIXEL
RAW   -> sólo Inspector · Texto GFX RAW
NONE  -> ningún inspector semántico
```

Se añade `designer-inspector-state.js` como coordinador único de visibilidad.

El módulo:

- fuerza `display:none !important` para elementos del inspector marcados como
  `hidden`;
- sincroniza FIELD / PIXEL / RAW en cada `jwplc:editor-refresh`;
- sincroniza cambios de PixelMap y carga de proyecto;
- oculta la sección global heredada `pixel-controls` en A11-7, porque el color
  PIXEL ya vive en `Inspector · PIXEL` y los colores de fields viven en
  `Apariencia`.

## Estado

```text
A11_7A_INSPECTOR_EXCLUSIVE=IMPLEMENTED_PENDING_USER_GATE
A11_7A_FIELD_ONLY=IMPLEMENTED_PENDING_USER_GATE
A11_7A_PIXEL_ONLY=IMPLEMENTED_PENDING_USER_GATE
A11_7A_RAW_ONLY=IMPLEMENTED_PENDING_USER_GATE
A11_7A_LEGACY_GLOBAL_COLOR_HIDDEN=IMPLEMENTED_PENDING_USER_GATE
```

No marcar PASS hasta validar visualmente en la aplicación instalada.

## Gate del usuario

1. Seleccionar TEXT: no debe aparecer RAW ni PIXEL debajo.
2. Seleccionar PIXEL: debe comenzar directamente en `Inspector · PIXEL` y no
   mostrar controles FIELD/RAW por encima ni `Color RGB565 activo` duplicado.
3. Seleccionar `Texto GFX RAW`: sólo debe mostrarse su inspector técnico.
4. Volver a TEXT/PIXEL varias veces y comprobar que no quedan secciones pegadas.
