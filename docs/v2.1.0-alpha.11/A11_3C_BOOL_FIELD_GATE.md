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

## Evidencia visual/funcional

Validación realizada con composición `TEXT + VALUE + BOOL` y Live Preview activo.

Se observó:

```text
BOOL_COMPONENT_ENABLED=YES
BOOL_DEFAULT_ID=FIELD_BOOL_3
BOOL_DEFAULT_VARIABLE=estado3
BOOL_CPP_TYPE=bool
FALSE_PREVIEW=OFF
TRUE_PREVIEW=ON
INLINE=PASS
STACKED=PASS
CENTER=PASS
RIGHT=PASS
FRAME=PASS
COLORS=PASS
DRAG=PASS
DUPLICATE=PASS
DUPLICATE_UNIQUE_ID=PASS
DUPLICATE_UNIQUE_VARIABLE=PASS
TEXT_VALUE_BOOL_COEXIST=PASS
LIVE_PREVIEW_BOOL=PASS
VISUAL_CORRUPTION=0
```

La duplicación observada produjo un segundo BOOL independiente y el cambio de estado `true/false` se reflejó correctamente en cada objeto.

El codegen de la extensión se verificó por inspección contra el contrato público: usa `JWPLC_UIBoolField`, `JWPLC_UIBoolText`, `JWPLC_UIBoolStyle`, variable `bool` y comentario `JWPLC_Display.setBool(...)`; no introduce `tft.*` ni genera `jwplcUIUpdate()`.

## Gate visual/funcional

```text
[PASS] BOOL habilitado y seleccionable.
[PASS] Defaults de identidad y variable.
[PASS] Tipo C++ bool.
[PASS] Inspector booleano.
[PASS] Estado false/true.
[PASS] Reserva geométrica basada en mayor texto.
[PASS] INLINE / STACKED.
[PASS] Alineación.
[PASS] Borde y colores.
[PASS] Drag.
[PASS] Undo/Redo sobre infraestructura UX-4.
[PASS] Duplicación con identidad única.
[PASS] Eliminación sobre infraestructura UX-4.
[PASS] TEXT + VALUE + BOOL.
[PASS] Live Preview incremental.
[PASS] Codegen público por inspección.
[PASS] Sin runtime HMI paralelo.
```

## Criterio de salida

```text
A11_3C_BOOL_FIELD=PASS
NEXT=A11_3D_BAR_FIELD
```
