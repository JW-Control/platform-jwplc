# Alpha11 — A11-3B VALUE Field

Fecha: 2026-09-05

## Objetivo

Incorporar `VALUE` al JWPLC HMI Designer usando exclusivamente la API pública existente de `JWPLC_Display`, sin crear un runtime HMI paralelo ni introducir llamadas directas `tft.*`.

```text
FIELD_TYPE=VALUE
PUBLIC_HELPER=JWPLC_UIValueField
FORMAT=JWPLC_UIValueFormat
STYLE=JWPLC_UIValueStyle
RUNTIME_SETTER=JWPLC_Display.setValue
SECOND_RUNTIME=NO
DIRECT_TFT_CALLS=NO
```

## Contrato público usado

La definición generada sigue el orden real de la API:

```cpp
JWPLC_UIValueField(
    id,
    JWPLC_UIRect(x, y),
    JWPLC_UIText(label, unit),
    JWPLC_UIValueFormat(
        integerDigits,
        decimalDigits,
        signedValue,
        leadingZeros),
    JWPLC_UIValueStyle(
        valueTextSize,
        labelTextSize,
        frame,
        layout,
        align),
    page,
    JWPLC_UIColors(
        labelColor,
        valueColor,
        backgroundColor,
        frameColor))
```

El Designer declara la variable HMI:

```cpp
float temperatura = 0.0f;
```

pero no genera `jwplcUIUpdate()`.

El usuario conserva la responsabilidad de alimentar la variable y llamar, dentro de su lógica:

```cpp
JWPLC_Display.setValue(FIELD_TEMP, temperatura);
```

## Inspector VALUE

Se reutiliza el Inspector contextual de la UX Foundation.

Secciones comunes con TEXT:

- Identidad;
- Dato HMI;
- Geometría;
- Tipografía;
- Apariencia;
- Contrato C++.

VALUE agrega `Formato numérico`:

```text
integerDigits
 decimalDigits
signedValue
leadingZeros
```

La capacidad `char[]` desaparece para VALUE y el tipo C++ visible pasa a `float`.

## Paridad de geometría

La región VALUE no se calcula usando el preview actual. Se reserva el peor caso del formato, igual que el runtime.

Ejemplo:

```text
integerDigits=3
decimalDigits=1
signedValue=false
sample=888.8
```

Con signo:

```text
integerDigits=3
decimalDigits=1
signedValue=true
sample=-888.8
```

La geometría conserva:

```text
TEXT_BODY=5x7_NOMINAL
GFX_RASTER=6x8_CLASSIC
FIELD_PADDING=3
EFFECTIVE_PADDING=max(3,labelTextSize,valueTextSize)
FIELD_GAP=4
```

## Paridad de formato

El preview replica las reglas funcionales actuales de `formatNumeric()`:

- cantidad fija de decimales;
- signo negativo sólo si `signedValue=true`;
- `leadingZeros` rellena la parte entera hasta `integerDigits`;
- si el número requiere más dígitos enteros que los reservados, se muestra overflow con `#`;
- un valor negativo en un field no signed también produce overflow.

Esto permite detectar visualmente configuraciones imposibles antes de generar el sketch.

## Integración con UX-4

VALUE entra directamente en el modelo de objetos existente:

```text
OBJECT_LIST=TEXT_PLUS_VALUE
HIT_TEST=TYPE_AGNOSTIC
DRAG=YES
BLANK_CANVAS_CLICK_MOVES_FIELD=NO
KEYBOARD_NUDGE=YES
UNDO_REDO=YES
DUPLICATE_DELETE=YES
```

Al hacer clic en una zona vacía del canvas se deselecciona; no se reposiciona ningún field.

## Codegen

El codegen ahora mezcla TEXT y VALUE en el mismo:

```cpp
enum HMIFieldId : uint8_t
static const JWPLC_UIField HMI_FIELDS[]
```

Los colores se generan como valores RGB565 explícitos (`0xFFFF`, `0x07FF`, etc.) para no depender de símbolos de color inexistentes en la API pública.

## Gate visual/funcional

Después de `git pull --ff-only` y `Ctrl+F5`:

```text
[ ] 1. VALUE está habilitado en Componentes.
[ ] 2. Seleccionar VALUE abre `Inspector · VALUE field`.
[ ] 3. El tipo C++ mostrado es `float`.
[ ] 4. Aparece la sección `Formato numérico`.
[ ] 5. Cambiar integerDigits modifica inmediatamente el ancho AUTO.
[ ] 6. Cambiar decimalDigits modifica sample, preview y ancho AUTO.
[ ] 7. signedValue=true reserva el signo en la geometría.
[ ] 8. Un valor negativo con signedValue=false muestra `#`.
[ ] 9. leadingZeros=true muestra ceros a la izquierda.
[ ] 10. INLINE / STACKED funcionan.
[ ] 11. LEFT / CENTER / RIGHT funcionan sobre la región VALUE reservada.
[ ] 12. Drag sólo funciona iniciando sobre el field.
[ ] 13. Click en canvas vacío deselecciona y no mueve el field.
[ ] 14. Ctrl+Z / Ctrl+Y funcionan con VALUE.
[ ] 15. Ctrl+D duplica VALUE con ID y variable únicos.
[ ] 16. TEXT y VALUE pueden coexistir en el mismo canvas.
[ ] 17. `Generar C++` contiene `JWPLC_UIValueField(...)`.
[ ] 18. El codegen contiene `JWPLC_UIValueFormat(...)` y `JWPLC_UIValueStyle(...)`.
[ ] 19. El codegen declara `float` para VALUE y `char[]` para TEXT.
[ ] 20. El codegen no contiene `tft.` ni genera `jwplcUIUpdate()`.
```

## Criterio de salida

```text
A11_3B_VALUE_FIELD=PASS
NEXT=A11_3C_BOOL_FIELD
```

La validación física de la composición completa permanece para los gates posteriores de Alpha11.
