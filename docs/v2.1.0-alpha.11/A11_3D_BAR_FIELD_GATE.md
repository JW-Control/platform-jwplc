# Alpha11 — A11-3D BAR Field

Fecha: 2026-09-05

## Objetivo

Incorporar `BAR` al JWPLC HMI Designer reutilizando exclusivamente la API pública declarativa existente.

```text
FIELD_TYPE=BAR
PUBLIC_HELPER=JWPLC_UIBarField
RANGE=JWPLC_UIRange
STYLE=JWPLC_UIBarStyle
RUNTIME_SETTER=JWPLC_Display.setBar
SECOND_RUNTIME=NO
DIRECT_TFT_CALLS=NO
```

## Semántica

BAR representa una magnitud `float` dentro de un rango configurable.

```text
minValue <= value <= maxValue
```

El runtime normaliza:

```text
normalized=(value-min)/(max-min)
normalized=clamp(normalized,0,1)
fillW=round(normalized*valueW)
```

Si `max<=min`, el runtime usa internamente `max=min+1` para evitar división inválida.

Valores fuera del rango no producen overflow textual: la barra se satura visualmente a 0 % o 100 %.

## Defaults propuestos del Designer

```text
Nombre objeto    BAR N
ID C++           FIELD_BAR_N
Variable         nivelN
Tipo C++         float
Etiqueta visible Nivel
Unidad           %
Mínimo           0
Máximo           100
Valor de prueba  50
Layout           STACKED
Ancho total      AUTO inicialmente
Altura BAR       12 px fija por runtime
Color fill       CYAN
Fondo            BLACK
```

## Geometría del runtime

Con `JWPLC_UIRect(x,y)` en AUTO:

```text
DEFAULT_BAR_VALUE_WIDTH=80_px
DEFAULT_BAR_VALUE_HEIGHT=12_px
```

El field conserva el mismo padding y gap:

```text
FIELD_PADDING=3
EFFECTIVE_PADDING=max(3,labelTextSize)
FIELD_GAP=4
```

Para `STACKED`:

```text
fieldW=2*pad + max(labelW, barW + optionalUnit)
fieldH=2*pad + labelH + optionalGap + max(12,unitH)
```

Para `INLINE`:

```text
fieldW=2*pad + labelW + optionalGap + barW + optionalUnit
fieldH=2*pad + max(labelH,12,unitH)
```

Si el usuario define `rect.width`, el runtime usa ese ancho total y calcula el ancho disponible de la región BAR descontando padding y, para INLINE, etiqueta/unidad.

La altura visible de la barra permanece en 12 px; `rect.height` puede permanecer AUTO en A11-3D.

## Inspector BAR

Secciones comunes:

- Identidad;
- Vinculación de datos;
- Contenido;
- Geometría;
- Apariencia;
- Contrato C++.

Se agrega:

```text
Rango BAR
  Mínimo
  Máximo
  Valor de prueba
  Porcentaje normalizado
```

Para BAR:

```text
Tipo C++ = float
Capacidad = oculto
Formato numérico = oculto
Texto booleano = oculto
Tamaño valor = no aplica
Alineación = fija LEFT / no editable
```

`Tamaño etiqueta` sí permanece activo.

## Colores

La semántica se conserva exactamente como el runtime:

```text
labelColor      -> etiqueta/unidad
valueColor      -> relleno de la barra
backgroundColor -> parte vacía + fondo del field
frameColor      -> borde del field cuando frame=true
```

No se introduce un color BAR adicional en Alpha11.

## Codegen esperado

Variable:

```cpp
float nivel4 = 0.0f;
```

Field:

```cpp
JWPLC_UIBarField(
    FIELD_BAR_4,
    JWPLC_UIRect(x, y),
    JWPLC_UIText("Nivel", "%"),
    JWPLC_UIRange(0.0f, 100.0f),
    JWPLC_UIBarStyle(
        1,
        false,
        JWPLC_UI_LAYOUT_STACKED),
    0,
    JWPLC_UIColors(...))
```

Setter documentado:

```cpp
// JWPLC_Display.setBar(FIELD_BAR_4, nivel4);
```

El Designer no genera `jwplcUIUpdate()`.

## Live Preview

El transporte Web Serial está congelado y no requiere cambios para BAR:

```text
BAR_RENDERED_IN_DESIGNER_FRAMEBUFFER=YES
LIVE_DIRTY_REGION_JWH2=REUSED
BRIDGE_CHANGES=NO_EXPECTED
```

## Gate visual/funcional

Validar:

```text
[ ] 1. BAR aparece habilitado.
[ ] 2. Se crea BAR N / FIELD_BAR_N / nivelN.
[ ] 3. Tipo C++ float.
[ ] 4. Inspector muestra rango min/max/value.
[ ] 5. 0 % deja BAR vacía.
[ ] 6. 50 % ocupa aproximadamente la mitad.
[ ] 7. 100 % ocupa todo el ancho.
[ ] 8. Valor menor a min satura en 0 %.
[ ] 9. Valor mayor a max satura en 100 %.
[ ] 10. max<=min reproduce la regla segura del runtime.
[ ] 11. INLINE / STACKED funcionan.
[ ] 12. Label, unidad, borde y colores funcionan.
[ ] 13. Drag / flechas / Undo / Redo funcionan.
[ ] 14. Ctrl+D duplica BAR con ID/variable únicos.
[ ] 15. TEXT + VALUE + BOOL + BAR coexisten.
[ ] 16. LIVE actualiza BAR por REGION sin corrupción.
[ ] 17. Codegen usa JWPLC_UIBarField.
[ ] 18. Codegen usa JWPLC_UIRange y JWPLC_UIBarStyle.
[ ] 19. Variable float + comentario setBar.
[ ] 20. Sin tft.* y sin generar jwplcUIUpdate().
```

## Criterio de salida

```text
A11_3D_BAR_FIELD=PASS
NEXT=A11_3E_MULTI_FIELD_PAGES
```
