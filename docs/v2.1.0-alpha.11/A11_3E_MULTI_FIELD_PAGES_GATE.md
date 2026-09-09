# Alpha11 — A11-3E Multi-field + Pages

Fecha: 2026-09-05

## Objetivo

Cerrar la composición declarativa multi-field incorporando páginas reales al JWPLC HMI Designer y una navegación física compacta en el JWPLC Basic, sin introducir un segundo runtime y sin modificar el transporte LIVE ya congelado.

```text
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
PAGES=PASS
SECOND_RUNTIME=NO
LIVE_TRANSPORT=FROZEN
```

## Modelo de página

La página sigue siendo una propiedad declarativa del `JWPLC_UIField`:

```text
field.meta.page = pageId
JWPLC_Display.setUserPage(pageId)
```

Los IDs internos son 0-based. La presentación visible al operador es 1-based.

```text
PAGE_0_ID=0
PAGE_0_NAME=Principal
VISIBLE_PAGE_0=01
MAX_PAGES_DESIGNER_ALPHA11=16
MAX_PAGES_RUNTIME=99
```

Los nombres de página pertenecen al proyecto/Designer. El indicador físico no intenta imprimir el nombre; usa un formato numérico compacto.

## Indicador físico compacto

Se adopta un overlay global en la esquina superior derecha:

```text
FORMAT=NN/TT
POSITION=TOP_RIGHT
X=282
Y=3
WIDTH=36
HEIGHT=12
PAGE_INDICATOR_IS_FIELD=NO
PAGE_INDICATOR_GLOBAL=YES
```

Ejemplos:

```text
01/03
02/03
03/03
```

Si existe una sola página, el indicador no se dibuja.

La zona es un overlay reservado. Los fields pueden conservar coordenadas 0..319 / 0..169, pero cualquier contenido colocado debajo del indicador será cubierto por él; el Designer lo muestra de la misma forma para que la colisión sea visible durante diseño.

## Estados de navegación

### PAGE_SELECT

Es el estado inicial al entrar a USER cuando hay más de una página.

Visual:

```text
fondo indicador = BLACK
texto indicador = WHITE
borde           = WHITE
```

Botonera:

```text
LEFT   -> página anterior
RIGHT  -> página siguiente
OK     -> entrar a PAGE_CONTENT
UP     -> consumido / sin acción
DOWN   -> consumido / sin acción
```

En el primer y último pageId, intentar salir del rango no cambia de página.

### PAGE_CONTENT

Visual:

```text
fondo indicador = WHITE
texto indicador = BLACK
borde           = WHITE
```

Botonera:

```text
LEFT   -> usuario
RIGHT  -> usuario
UP     -> usuario
DOWN   -> usuario
OK     -> usuario
ESC    -> volver a PAGE_SELECT
```

ESC es consumido por el sistema HMI en este estado y no llega a la lógica de página.

## API pública agregada

```cpp
JWPLC_Display.setUserPageCount(count);
JWPLC_Display.userPageCount();
JWPLC_Display.isUserPageSelection();
```

El Designer genera en `jwplcHMISetup()`:

```cpp
JWPLC_Display.setFields(...);
JWPLC_Display.setUserPageCount(N);
JWPLC_Display.setUserPage(0);
```

No genera lógica dentro de `jwplcUIUpdate()`.

## Designer

El core mantiene:

```text
hmiPages[]
activePage
field.page
```

A11-3E incluye:

- crear páginas;
- renombrar páginas;
- cambiar página activa desde panel izquierdo y tabs superiores;
- mostrar en canvas/Preview sólo los fields de la página activa;
- mostrar en `Objetos` sólo los fields de la página activa;
- crear nuevos fields directamente en la página activa;
- mover un field a otra página desde el Inspector;
- conservar IDs C++ y variables globalmente únicos entre páginas;
- conservar Undo/Redo con estado de página;
- conservar TEXT/VALUE/BOOL/BAR en cualquier página;
- mantener LIVE sincronizado con la página activa mediante el framebuffer existente;
- mostrar el indicador `NN/TT` en canvas y Preview;
- previsualizar `Selector` / `Dentro` para comprobar la inversión de colores.

La lista muestra nombres humanos:

```text
01 · Principal
02 · Proceso
03 · Diagnóstico
```

El contador visual de objetos junto al nombre de página fue eliminado para mantener el panel limpio.

## Semántica de composición

Para una página activa `P`:

```text
CANVAS_FIELDS  = fields where field.page == P
OBJECT_LIST    = fields where field.page == P
CODEGEN_FIELDS = todos los fields de todas las páginas
```

Cambiar de página no elimina ni reconstruye fields; sólo cambia el subconjunto visible/editable.

## TEXT / VALUE / BOOL / BAR entre páginas

El codegen conserva todos los tipos, incluso los que estén en páginas no visibles durante la generación.

```cpp
JWPLC_UITextField(..., page, ...)
JWPLC_UIValueField(..., page, ...)
JWPLC_UIBoolField(..., page, ...)
JWPLC_UIBarField(..., page, ...)
```

## Pixel y GFX RAW

En Alpha11 los tools `Pixel`, `Borrador` y `Texto GFX RAW` siguen siendo herramientas técnicas/globales y no quedan page-scoped.

```text
DECLARATIVE_FIELDS_PAGE_SCOPED=YES
PIXEL_LAYER_PAGE_SCOPED=NO
RAW_GFX_PAGE_SCOPED=NO
```

## LIVE Preview

No se modificó el protocolo.

Al cambiar página activa:

```text
Designer recompone framebuffer de la nueva página + indicador NN/TT
-> Dirty Region / FULL según diferencia
-> bridge existente
-> TFT física
```

El bridge continúa siendo transporte de framebuffer y no conoce páginas.

### Smoke físico LIVE aprobado

Se validaron tres páginas distintas con el bridge existente:

```text
01/03 -> PASS
02/03 -> PASS
03/03 -> PASS
LIVE_ERRORS=0
VISUAL_CORRUPTION=0
BRIDGE_CHANGE_REQUIRED=NO
```

En cambios de página se observaron transmisiones `FULL 320x170`, comportamiento esperado al cambiar de forma extensa el framebuffer visible.

## Runtime / input

La navegación física se procesa antes del filtro `dirty/forced` mediante:

```text
jwplcUIRuntimeServiceInput()
```

El overlay `NN/TT` se dibuja al final del refresh HMI para permanecer visible.

## Gate físico de botonera

Sketch:

```text
tools/jwplc-hmi-designer/gates/A11_Pages_Navigation/
  JWPLC_HMI_Pages_Gate/
    JWPLC_HMI_Pages_Gate.ino
```

Resultado:

```text
PAGE_INDICATOR_PHYSICAL=PASS
PAGE_SELECT_LEFT_RIGHT=PASS
PAGE_BOUNDARIES=PASS
OK_ENTERS_PAGE_CONTENT=PASS
PAGE_CONTENT_LEFT_RIGHT_USER=PASS
PAGE_CONTENT_UP_DOWN_USER=PASS
PAGE_CONTENT_OK_USER=PASS
ESC_RETURNS_TO_PAGE_SELECT=PASS
UNEXPECTED_PAGE_CHANGE_IN_CONTENT=0
```

## Checklist

```text
[x] 1. Página 0 Principal existe al iniciar.
[x] 2. + crea una nueva página hasta el límite del Designer.
[x] 3. Panel izquierdo y tabs muestran la misma página activa.
[x] 4. Doble click permite renombrar página.
[x] 5. Cambiar página cambia canvas y Preview 1:1.
[x] 6. La lista Objetos muestra sólo fields de la página activa.
[x] 7. Un field nuevo recibe el pageId activo.
[x] 8. TEXT / VALUE / BOOL / BAR funcionan en páginas diferentes.
[x] 9. Volver a una página conserva intactos sus objetos.
[x] 10. Inspector permite mover un objeto a otra página.
[x] 11. Mover conserva ID, variable, geometría y estilo.
[x] 12. Ctrl+D duplica dentro de la página activa.
[x] 13. Undo/Redo conserva página y composición.
[x] 14. IDs/variables siguen siendo únicos globalmente.
[x] 15. Generar C++ incluye fields de todas las páginas.
[x] 16. Cada helper contiene el pageId correcto.
[x] 17. Codegen genera setUserPageCount(N) y setUserPage(0).
[x] 18. Con >1 página aparece NN/TT arriba a la derecha.
[x] 19. PAGE_SELECT muestra negro/blanco.
[x] 20. PAGE_CONTENT muestra blanco/negro.
[x] 21. LEFT/RIGHT cambian página sólo en PAGE_SELECT.
[x] 22. OK entra a PAGE_CONTENT.
[x] 23. LEFT/RIGHT/UP/DOWN/OK llegan al usuario en PAGE_CONTENT.
[x] 24. ESC vuelve a PAGE_SELECT y no llega al usuario.
[x] 25. LIVE cambia de página sin corrupción y sin cambios al bridge.
[x] 26. Codegen no contiene tft.*.
[x] 27. Designer no genera jwplcUIUpdate().
```

## Criterio de salida

```text
A11_3E_MULTI_FIELD_PAGES=PASS
A11_3E_PAGE_INDICATOR=PASS
A11_3E_PAGE_BUTTON_ROUTING=PASS
A11_3E_LIVE_PAGE_SWITCH=PASS
NEXT=A11_4_CODEGEN
```
