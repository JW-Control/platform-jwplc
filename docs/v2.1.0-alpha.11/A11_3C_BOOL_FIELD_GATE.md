# Alpha11 — A11-3C BOOL Field

Fecha: 2026-09-05

## Objetivo

Incorporar `BOOL` al JWPLC HMI Designer reutilizando la API pública existente del runtime declarativo.

```text
FIELD_TYPE=BOOL
PUBLIC_HELPER=JWPLC_UIBoolField
BOOL_TEXT=JWPLC_UIBoolText
STYLE=JWPLC_UIBoolStyle
RUNTIME_SETTER=JWPLC_Display.setBool
SECOND_RUNTIME=NO
DIRECT_TFT_CALLS=NO
```

El Live Preview Web Serial permanece congelado como transporte de desarrollo y no modifica este contrato.

## Semántica

Un BOOL vincula una variable C++ `bool` con dos representaciones visibles configurables:

```text
false -> falseText (default OFF)
true  -> trueText  (default ON)
```

Defaults del Designer:

```text
Nombre objeto    BOOL N
ID C++           FIELD_BOOL_N
Variable         estadoN
Tipo C++         bool
Etiqueta visible Estado
Texto FALSE      OFF
Texto TRUE       ON
Estado preview   false
Alineación       CENTER
```

El `Nombre del objeto`, el `ID C++`, la variable y la etiqueta visible conservan funciones distintas.

## Geometría

La región de valor reserva siempre el mayor ancho entre `falseText` y `trueText`, igual que `JWPLC_UI.cpp`:

```text
valueW=max(width(falseText),width(trueText))
valueH=max(height(falseText),height(trueText))
```

Se conserva:

```text
TEXT_BODY=5x7_NOMINAL
GFX_RASTER=6x8_CLASSIC
FIELD_PADDING=3
EFFECTIVE_PADDING=max(3,labelTextSize,valueTextSize)
FIELD_GAP=4
```

Por tanto, cambiar el estado de prueba no debe cambiar la geometría AUTO del field.

## Inspector

Para BOOL se oculta:

```text
Capacidad char[]
Valor de prueba libre TEXT/VALUE
Formato numérico
```

Y se muestra:

```text
Tipo C++ = bool
Texto FALSE
Texto TRUE
Estado de prueba = false / true
Reserva geométrica
```

Las secciones comunes se mantienen:

- Identidad;
- Vinculación de datos;
- Contenido;
- Geometría;
- Tipografía;
- Apariencia;
- Contrato C++.

## Codegen esperado

Variable:

```cpp
bool estado3 = false;
```

Field:

```cpp
JWPLC_UIBoolField(
    FIELD_BOOL_3,
    JWPLC_UIRect(x, y),
    JWPLC_UIText("Estado", nullptr),
    JWPLC_UIBoolText(
        "OFF",
        "ON"),
    JWPLC_UIBoolStyle(
        2,
        1,
        false,
        JWPLC_UI_LAYOUT_INLINE,
        JWPLC_UI_ALIGN_CENTER),
    0,
    JWPLC_UIColors(...))
```

Setter documentado, no ejecutado por el Designer:

```cpp
// JWPLC_Display.setBool(FIELD_BOOL_3, estado3);
```

`jwplcUIUpdate()` continúa siendo responsabilidad del usuario.

## Implementación Designer

Para minimizar riesgo sobre `app.js`, que ya cerró TEXT/VALUE/UX-4, A11-3C se implementa como extensión del Designer:

```text
poc/designer-bool.js
```

La extensión se carga antes de `designer-live.js` desde `ux-foundation.js`.

El core sigue siendo responsable de:

- raster clásico;
- geometría común;
- drag/hit-test;
- undo/redo;
- duplicate/delete;
- framebuffer;
- TEXT/VALUE.

La extensión BOOL aporta:

- alta del componente;
- semántica false/true;
- inspector específico;
- icono/lista;
- adaptación del codegen a `JWPLC_UIBoolField`;
- variable `bool` y setter `setBool`.

## Gate visual/funcional

Después de `git pull --ff-only` y `Ctrl+F5`:

```text
[ ] 1. BOOL aparece habilitado en Componentes.
[ ] 2. Al agregar BOOL se crea `BOOL N / FIELD_BOOL_N / estadoN`.
[ ] 3. El Inspector muestra Tipo C++ `bool`.
[ ] 4. Se muestra `Texto booleano` y no `Formato numérico`.
[ ] 5. `false` muestra falseText y `true` muestra trueText.
[ ] 6. Cambiar falseText/trueText actualiza inmediatamente preview y geometría.
[ ] 7. Cambiar sólo true/false no cambia el tamaño reservado.
[ ] 8. INLINE / STACKED funcionan.
[ ] 9. LEFT / CENTER / RIGHT funcionan.
[ ] 10. Label, unidad, frame y colores funcionan.
[ ] 11. Drag, flechas, Undo/Redo funcionan.
[ ] 12. Ctrl+D duplica BOOL conservando textos y creando ID/variable únicos.
[ ] 13. Delete elimina BOOL sin afectar otros fields.
[ ] 14. TEXT + VALUE + BOOL coexisten en la misma página.
[ ] 15. Live Preview actualiza BOOL mediante REGION sin corrupción.
[ ] 16. `Generar C++` contiene `JWPLC_UIBoolField(...)`.
[ ] 17. Codegen contiene `JWPLC_UIBoolText(...)` y `JWPLC_UIBoolStyle(...)`.
[ ] 18. Codegen declara `bool` y documenta `JWPLC_Display.setBool(...)`.
[ ] 19. Codegen no contiene `tft.*`.
[ ] 20. Codegen no genera `jwplcUIUpdate()`.
```

## Criterio de salida

```text
A11_3C_BOOL_FIELD=PASS
NEXT=A11_3D_BAR_FIELD
```
