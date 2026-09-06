# Alpha11 — A11-3E Multi-field + Pages

Fecha: 2026-09-05

## Objetivo

Cerrar la composición declarativa multi-field incorporando páginas reales al JWPLC HMI Designer y una navegación física compacta en el JWPLC Basic, sin introducir un segundo runtime y sin modificar el transporte LIVE ya congelado.

```text
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
PAGES=IMPLEMENTED_PENDING_GATE
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

Los eventos de LEFT/RIGHT/UP/DOWN/OK consumidos por el sistema no deben llegar a la lógica de usuario.

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

ESC es consumido por el sistema HMI en este estado y no debe llegar a la lógica de la página.

Para que el manejador histórico de `IDLE_RETURN_ESC_ONLY` no robe ESC antes del selector, PAGE_CONTENT deshabilita temporalmente el retorno IDLE por botón y restaura el modo previo al regresar a PAGE_SELECT.

En PAGE_SELECT, ESC conserva el contrato de retorno a IDLE que ya tenga configurado `JWPLC_Display`.

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

No genera lógica de navegación dentro de:

```cpp
jwplcUIUpdate()
```

El usuario mantiene la responsabilidad sobre LEFT/RIGHT/UP/DOWN/OK cuando está dentro de PAGE_CONTENT.

## Designer

El core ahora mantiene:

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

La lista muestra nombres humanos, por ejemplo:

```text
01 · Principal
02 · Proceso
03 · Diagnóstico
```

La TFT sólo muestra el número compacto `NN/TT`.

## Semántica de composición

Para una página activa `P`:

```text
CANVAS_FIELDS  = fields where field.page == P
OBJECT_LIST    = fields where field.page == P
CODEGEN_FIELDS = todos los fields de todas las páginas
```

Cambiar de página no elimina ni reconstruye fields; sólo cambia el subconjunto visible/editable.

## TEXT / VALUE / BOOL / BAR entre páginas

El codegen debe conservar todos los tipos, incluso los que estén en páginas no visibles durante la generación.

```cpp
JWPLC_UITextField(..., page, ...)
JWPLC_UIValueField(..., page, ...)
JWPLC_UIBoolField(..., page, ...)
JWPLC_UIBarField(..., page, ...)
```

Las extensiones BOOL/BAR se actualizaron para obtener el conjunto global de fields desde el core, en vez de inferirlo únicamente desde la lista DOM de la página activa.

## Pixel y GFX RAW

En Alpha11 los tools `Pixel`, `Borrador` y `Texto GFX RAW` siguen siendo herramientas técnicas/globales y no quedan page-scoped.

```text
DECLARATIVE_FIELDS_PAGE_SCOPED=YES
PIXEL_LAYER_PAGE_SCOPED=NO
RAW_GFX_PAGE_SCOPED=NO
```

No se amplía su alcance para evitar mezclar el gate declarativo con un sistema de capas por página.

## LIVE Preview

No se modifica el protocolo.

Al cambiar página activa:

```text
Designer recompone framebuffer de la nueva página + indicador NN/TT
-> Dirty Region / FULL según diferencia
-> bridge existente
-> TFT física
```

El bridge continúa siendo transporte de framebuffer y no conoce páginas.

## Runtime / input

La navegación física se procesa antes del filtro `dirty/forced` del Display mediante un hook interno dedicado:

```text
jwplcUIRuntimeServiceInput()
```

Esto evita que un refresh forzado salte el procesamiento de la botonera de sistema.

El overlay `NN/TT` se dibuja al final del refresh HMI para quedar por encima de los fields.

## Gate de compilación y físico

Sketch:

```text
tools/jwplc-hmi-designer/gates/A11_Pages_Navigation/
  JWPLC_HMI_Pages_Gate/
    JWPLC_HMI_Pages_Gate.ino
```

El gate usa tres páginas y los cuatro tipos declarativos.

## Checklist

```text
[ ] 1. Página 0 Principal existe al iniciar.
[ ] 2. + crea una nueva página hasta el límite del Designer.
[ ] 3. Panel izquierdo y tabs muestran la misma página activa.
[ ] 4. Doble click permite renombrar página.
[ ] 5. Cambiar página cambia canvas y Preview 1:1.
[ ] 6. La lista Objetos muestra sólo fields de la página activa.
[ ] 7. Un field nuevo recibe el pageId activo.
[ ] 8. TEXT / VALUE / BOOL / BAR funcionan en páginas diferentes.
[ ] 9. Volver a una página conserva intactos sus objetos.
[ ] 10. Inspector permite mover un objeto a otra página.
[ ] 11. Mover conserva ID, variable, geometría y estilo.
[ ] 12. Ctrl+D duplica dentro de la página activa.
[ ] 13. Undo/Redo conserva página y composición.
[ ] 14. IDs/variables siguen siendo únicos globalmente.
[ ] 15. Generar C++ incluye fields de todas las páginas.
[ ] 16. Cada helper contiene el pageId correcto.
[ ] 17. Codegen genera setUserPageCount(N) y setUserPage(0).
[ ] 18. Con >1 página aparece NN/TT arriba a la derecha.
[ ] 19. PAGE_SELECT muestra negro/blanco.
[ ] 20. PAGE_CONTENT muestra blanco/negro.
[ ] 21. LEFT/RIGHT cambian página sólo en PAGE_SELECT.
[ ] 22. OK entra a PAGE_CONTENT.
[ ] 23. LEFT/RIGHT/UP/DOWN/OK llegan al usuario en PAGE_CONTENT.
[ ] 24. ESC vuelve a PAGE_SELECT y no llega al usuario.
[ ] 25. LIVE cambia de página sin corrupción y sin cambios al bridge.
[ ] 26. Codegen no contiene tft.*.
[ ] 27. Designer no genera jwplcUIUpdate().
```

## Criterio de salida

```text
A11_3E_MULTI_FIELD_PAGES=PASS
A11_3E_PAGE_INDICATOR=PASS
A11_3E_PAGE_BUTTON_ROUTING=PASS
NEXT=A11_4_CODEGEN
```
