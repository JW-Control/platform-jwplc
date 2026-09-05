# Alpha11 — Decisión de safe area para texto y fields

Fecha: 2026-09-05

## Contexto

Durante la validación visual de A11-2 se observó que el texto GFX colocado en `Y=0` puede quedar pegado al borde superior de la TFT.

Esto no es un offset faltante de `JWPLC_Display`: el modo GFX crudo respeta literalmente `setCursor(x, y)` y no agrega margen superior.

La HMI declarativa `JWPLC_UIField`, en cambio, ya usa internamente:

```text
FIELD_PADDING=3
FIELD_GAP=4
```

Por tanto no se debe alterar globalmente la coordenada Y de `JWPLC_Display` ni introducir un `+1` implícito.

## Decisión

```text
A11_GFX_RAW_CURSOR_IS_EXACT=YES
A11_GFX_AUTO_TOP_OFFSET=NO
JWPLC_UI_FIELD_PADDING=3
JWPLC_UI_FIELD_PADDING_CHANGE=NO
DESIGNER_SAFE_AREA=3PX_RECOMMENDED
DESIGNER_SAFE_AREA_HARD_LIMIT=NO
NEW_OBJECT_DEFAULT_X=3
NEW_OBJECT_DEFAULT_Y=3
```

## Comportamiento esperado

### Texto GFX crudo

- `X=0/Y=0` sigue siendo válido.
- El Designer debe mostrar exactamente los píxeles que producirá GFX en esa coordenada.
- La guía de 3 px no modifica el framebuffer ni el codegen.

### Objetos nuevos del Designer

El origen recomendado inicial será:

```text
X=3
Y=3
```

Esto mejora la composición visual sin ocultar las coordenadas físicas reales.

### JWPLC_UIField

Los fields seguirán reproduciendo el padding actual del runtime. Alpha11 no modifica `FIELD_PADDING` para resolver un problema que pertenece únicamente al posicionamiento explícito de GFX.

## UI del Designer

A11-2 incorpora:

- toggle `Guía 3 px`;
- rectángulo visual de safe area;
- estado `Dentro de guía` / `Aviso: fuera de guía`;
- libertad de usar todo el rango `0..319 / 0..169`;
- default de texto nuevo en `3,3`.

## Gate asociado

El gate físico A11-2 valida ambas posiciones con la misma muestra:

```text
TEMP: 25.6 C
size=2
RED sobre WHITE
```

Páginas:

```text
0 -> x=0, y=0
1 -> x=3, y=3
```

La validación debe demostrar que la guía es una recomendación del Designer y no un desplazamiento oculto del runtime.
