# Alpha11 — Safe area descartada para A11-2

Fecha: 2026-09-05
Estado: **SUPERSEDED**

## Motivo

Durante A11-2 se interpretó inicialmente que la observación visual correspondía a un margen insuficiente del texto respecto al borde físico de la TFT.

La observación real era distinta: el problema señalado corresponde al **margen interno del glifo respecto al fondo de su celda GFX**.

Por tanto, la decisión previa de introducir una `safe area` de 3 px en el Designer no resuelve el problema correcto y se descarta antes de cerrar A11-2.

## Decisiones retiradas

Las siguientes decisiones ya no aplican:

```text
DESIGNER_SAFE_AREA=3PX_RECOMMENDED
DESIGNER_SAFE_AREA_HARD_LIMIT=NO
NEW_OBJECT_DEFAULT_X=3
NEW_OBJECT_DEFAULT_Y=3
```

También se retiran del PoC:

- toggle `Guía 3 px`;
- rectángulo visual de safe area;
- estado dentro/fuera de guía;
- `safe-area.js`.

## Decisiones que se mantienen

```text
A11_GFX_RAW_CURSOR_IS_EXACT=YES
A11_GFX_AUTO_TOP_OFFSET=NO
JWPLC_UI_FIELD_PADDING=3
JWPLC_UI_FIELD_PADDING_CHANGE=NO
```

El modo GFX RAW debe seguir reproduciendo literalmente `setCursor(x, y)` y el comportamiento nativo de `Adafruit_GFX::drawChar()`.

La geometría de `JWPLC_UIField` permanece separada y conserva su `FIELD_PADDING=3` actual.

## Documento vigente

La decisión que reemplaza este documento es:

```text
docs/v2.1.0-alpha.11/A11_2_GFX_CELL_BACKGROUND_DECISION.md
```
