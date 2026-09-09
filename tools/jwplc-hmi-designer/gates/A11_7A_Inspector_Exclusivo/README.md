# Alpha11 · A11-7A — Inspector derecho exclusivo

Fecha: 2026-09-06

## Problema observado

Durante la validación de A11-7, el panel derecho mezclaba secciones de distintos
modos de edición. Se observaron dos casos principales:

- con un FIELD seleccionado seguía visible `Inspector · Texto GFX RAW`;
- con un PIXEL seleccionado quedaban controles RAW/FIELD por encima del
  `Inspector · PIXEL`, además de una segunda sección global de color.

La causa es que varias secciones dependían del atributo `hidden`, mientras reglas
de estilo del panel podían mantener elementos con `display` explícito. Además,
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

## Validación del usuario

Validado visualmente el 2026-09-07 en la aplicación Electron instalada.

Se comprobó:

- `TEXT` muestra únicamente su inspector de field;
- `PIXEL` muestra únicamente `Inspector · PIXEL`;
- `Texto GFX RAW` mostraba únicamente su inspector técnico;
- el cambio repetido entre modos no deja secciones pegadas ni controles cruzados.

## Estado

```text
A11_7A_INSPECTOR_EXCLUSIVE=PASS
A11_7A_FIELD_ONLY=PASS
A11_7A_PIXEL_ONLY=PASS
A11_7A_RAW_ONLY=PASS
A11_7A_LEGACY_GLOBAL_COLOR_HIDDEN=PASS
A11_7A_USER_GATE=PASS
```

A11-7A queda cerrado. El siguiente gate es A11-7B para edición avanzada de
PixelMaps por capas.
