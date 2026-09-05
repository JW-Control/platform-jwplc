# Alpha11 — A11-2C validación física de métricas balanceadas

Fecha: 2026-09-05

## Objetivo

Validar físicamente la corrección de geometría de texto de la HMI declarativa usando exclusivamente la API pública de `JWPLC_Display`, sin `JWPLC_Display.tft()` ni llamadas `tft.*` en el sketch de validación.

## Problema observado

Con la fuente clásica de Adafruit GFX, `getTextBounds()` considera la celda completa 6×8. En `textSize=2`, la fila/columna de spacing de la celda agregaba aproximadamente 2 px visuales adicionales al borde inferior/derecho del field, aun cuando `FIELD_PADDING=3`.

La observación física previa fue:

```text
arriba      = 3 px
izquierda   = 3 px
abajo       = 5 px aprox.
```

## Corrección

`JWPLC_UI.cpp` conserva la rasterización real de Adafruit GFX, pero para el layout HMI usa el cuerpo nominal 5×7:

```text
layoutWidth  = gfxBoundsWidth  - textSize
layoutHeight = gfxBoundsHeight - textSize
```

Además se usa un padding efectivo:

```text
effectivePadding = max(FIELD_PADDING, maxTextSize)
FIELD_PADDING = 3
```

La limpieza de la región dinámica mantiene una escala adicional en vertical para cubrir la posible fila 8 usada por descendentes.

## Gate utilizado

```text
tools/jwplc-hmi-designer/gates/A11_2B_Public_API_Text_Field/
```

Configuración:

```text
Texto       : TEMP: 25.6 C
X/Y         : 20 / 20
textSize    : 2
Foreground  : RED
Background  : WHITE
capacity    : 12
```

API usada:

```cpp
JWPLC_UITextField(...)
JWPLC_Display.setFields(...)
JWPLC_Display.setText(...)
JWPLC_Display.setUserRefreshMode(...)
JWPLC_Display.setUserPage(...)
JWPLC_Display.enterUserUI()
```

## Resultado físico

La prueba física posterior a la corrección mostró el margen visual balanceado alrededor del texto. El exceso inferior observado antes dejó de presentarse.

Marcadores:

```text
A11_2A_RAW_FONT_PARITY=PASS
A11_2B_PUBLIC_API_TEXT_FIELD=PASS
A11_2C_BALANCED_SOURCE=PASS
A11_2C_PUBLIC_API_ONLY=PASS
A11_2C_VISUAL_PADDING_BALANCED=PASS
```

## Política temporal de Alpha11

Durante el desarrollo de Alpha11, `JWPLC_Display` se mantendrá en fallback source retirando temporalmente del branch:

```text
JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a
```

`library.properties` conserva `precompiled=full`; por tanto el package mantiene el contrato final y Arduino compila desde source mientras el archive no esté presente.

Al cierre de Alpha11 se deberá:

1. regenerar `libJWPLC_Display.a` desde las fuentes finales;
2. verificar los cuatro objetos esperados del archive;
3. repetir gate físico source/precompiled;
4. repetir gate de build-speed;
5. reintroducir el archive final únicamente después de PASS.

```text
ALPHA11_DISPLAY_DEVELOPMENT_MODE=SOURCE
ALPHA11_FINAL_PRECOMPILED_REGEN=PENDING
```
