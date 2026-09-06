# Alpha11 — A11-3E Multi-field + Pages

Fecha: 2026-09-05

## Objetivo

Cerrar la composición declarativa multi-field incorporando páginas reales al JWPLC HMI Designer sin introducir un segundo runtime y sin modificar el transporte LIVE ya congelado.

```text
TEXT=PASS
VALUE=PASS
BOOL=PASS
BAR=PASS
PAGES=NEXT
SECOND_RUNTIME=NO
LIVE_TRANSPORT=FROZEN
```

## Modelo de página

La página es una propiedad declarativa del `JWPLC_UIField` y debe coincidir con el runtime existente:

```text
field.meta.page = pageId
JWPLC_Display.setUserPage(pageId)
```

El Designer manejará una página activa para edición.

Defaults:

```text
PAGE_0_ID=0
PAGE_0_NAME=Principal
MAX_PAGES_ALPHA11=8
```

Los IDs de página son estables durante la sesión. No se renumeran automáticamente por cambios de nombre.

## Alcance A11-3E

Debe incluir:

- crear páginas;
- cambiar página activa desde panel izquierdo y tabs superiores;
- mostrar en canvas/Preview sólo los fields de la página activa;
- mostrar en `Objetos` sólo los fields de la página activa;
- crear nuevos fields directamente en la página activa;
- mover un field a otra página desde el Inspector;
- conservar IDs C++ y variables globalmente únicos entre páginas;
- conservar Undo/Redo y edición normal sobre la página activa;
- conservar TEXT/VALUE/BOOL/BAR en cualquier página;
- mantener LIVE sincronizado con la página activa mediante el framebuffer existente;
- conservar el `page` correcto en cada helper generado.

## Fuera de alcance de este gate

Para reducir riesgo, A11-3E no necesita todavía:

- eliminar páginas;
- reordenar IDs numéricos de páginas;
- navegación automática por botonera;
- generar lógica de cambio de página dentro de `jwplcUIUpdate()`;
- persistencia `.jwhmi`;
- renombrado avanzado con símbolos C++ de página.

La navegación runtime sigue siendo responsabilidad del usuario mediante la API pública.

## UX propuesta

Panel izquierdo:

```text
Páginas
  0 · Principal
  1 · Página 1
  2 · Página 2
  + Nueva página
```

Tabs centrales:

```text
[Principal] [Página 1] [Página 2] [+]
```

Inspector:

```text
Apariencia
  Página  [0 · Principal v]
```

Al mover un objeto a otra página mediante el Inspector, el Designer debe cambiar a la página destino para mantener el objeto visible y seleccionado.

## Semántica de composición

Para una página activa `P`:

```text
CANVAS_FIELDS = fields where field.page == P
OBJECT_LIST   = fields where field.page == P
CODEGEN_FIELDS = todos los fields de todas las páginas
```

Por tanto, cambiar de página no elimina ni reconstruye fields; sólo cambia el subconjunto visible/editable.

## Codegen

A11-3E conserva el contrato actual de helpers:

```cpp
JWPLC_UITextField(..., page, ...)
JWPLC_UIValueField(..., page, ...)
JWPLC_UIBoolField(..., page, ...)
JWPLC_UIBarField(..., page, ...)
```

El codegen debe incluir todos los fields de todas las páginas y mantener el número de página configurado.

En A11-4 se podrá decidir si conviene añadir también un enum simbólico de páginas como mejora del código generado.

El Designer continúa sin generar:

```cpp
extern "C" void jwplcUIUpdate()
```

El usuario podrá navegar, por ejemplo, con:

```cpp
JWPLC_Display.setUserPage(1);
```

## LIVE Preview

No se modifica el protocolo.

Al cambiar la página activa:

```text
Designer recompone framebuffer de la nueva página
-> Dirty Region / FULL según diferencia
-> bridge existente
-> TFT física
```

No se añaden comandos de página al bridge porque LIVE sigue siendo un transporte de framebuffer y no un runtime HMI paralelo.

## Gate visual/funcional

```text
[ ] 1. Página 0 Principal existe al iniciar.
[ ] 2. El botón + crea Página 1.
[ ] 3. Panel izquierdo y tabs muestran la misma página activa.
[ ] 4. Cambiar de página cambia canvas y Preview 1:1.
[ ] 5. La lista Objetos muestra sólo fields de la página activa.
[ ] 6. Un field creado en Página 1 recibe page=1.
[ ] 7. TEXT funciona en Página 1.
[ ] 8. VALUE funciona en Página 1.
[ ] 9. BOOL funciona en Página 1.
[ ] 10. BAR funciona en Página 1.
[ ] 11. Volver a Principal conserva intactos sus objetos.
[ ] 12. Inspector permite mover un objeto de página.
[ ] 13. Mover un objeto conserva ID, variable, geometría y estilo.
[ ] 14. Ctrl+D duplica dentro de la página activa.
[ ] 15. Undo/Redo no mezcla visualmente páginas.
[ ] 16. IDs/variables siguen siendo únicos globalmente.
[ ] 17. Generar C++ incluye fields de todas las páginas.
[ ] 18. Cada helper contiene el pageId correcto.
[ ] 19. LIVE cambia de página sin corrupción.
[ ] 20. Sin cambios al bridge y sin `tft.*` en codegen.
```

## Criterio de salida

```text
A11_3E_MULTI_FIELD_PAGES=PASS
NEXT=A11_4_CODEGEN
```
