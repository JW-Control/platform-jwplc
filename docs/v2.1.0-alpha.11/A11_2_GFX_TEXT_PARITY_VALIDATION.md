# Alpha11 — Validación A11-2 de texto

Fecha: 2026-09-05

## Objetivo

Separar dos validaciones que no deben confundirse:

1. **paridad RAW de fuente Adafruit GFX**;
2. **comportamiento final mediante la API pública declarativa de `JWPLC_Display`**.

La primera sirve para comprobar que el Designer conoce los mismos glifos/píxeles que usa la TFT. La segunda es la que define el comportamiento que verá el usuario final y el código que deberá generar el Designer.

---

## A11-2A — GFX RAW font parity

Muestra canónica:

```text
Texto       : TEMP: 25.6 C
X           : 20
Y           : 20
textSize    : 2
Foreground  : RED / 0xF800
Background  : WHITE / 0xFFFF
Bounds GFX  : 144 x 16 px
```

Se compararon:

1. captura del Designer RAW;
2. fotografía del JWPLC Basic físico ejecutando el gate RAW.

Resultado del subgate:

```text
A11_2A_GFX_CLASSIC_FONT=PASS
A11_2A_TEXT_SIZE_2X=PASS
A11_2A_CELL_GEOMETRY_144X16=PASS
A11_2A_PHYSICAL_VISUAL_MATCH=PASS
A11_2A_RAW_FONT_PARITY=PASS
```

Este PASS **no aprueba el borde/fondo final de la HMI**. El modo RAW conserva de forma intencional la celda clásica GFX 6x8, cuya separación visual no es simétrica respecto al glifo.

Además, el gate RAW usa dibujo directo (`JWPLC_Display.tft()` / `tft.*`) y por tanto no representa el codegen final del Designer.

---

## Observación pendiente de borde/fondo

La evidencia física confirmó que el modo RAW sigue mostrando la asimetría observada:

```text
arriba / izquierda -> sin margen visual equivalente
abajo / derecha    -> espacio nativo de la celda GFX
```

Por tanto, no se considera correcto cerrar A11-2 completo sólo con el gate RAW.

Regla:

```text
RAW_FONT_PARITY_PASS_DOES_NOT_IMPLY_HMI_TEXT_BOX_PASS=YES
```

---

## A11-2B — Public API Text Field

Se añade un segundo gate que **no usa `JWPLC_Display.tft()` ni llamadas `tft.*`**.

Ruta:

```text
tools/jwplc-hmi-designer/gates/A11_2B_Public_API_Text_Field/
```

El gate usa únicamente API pública:

```cpp
JWPLC_UITextField(...)
JWPLC_Display.setFields(...)
JWPLC_Display.setText(...)
JWPLC_Display.setUserRefreshMode(...)
JWPLC_Display.setUserPage(...)
JWPLC_Display.enterUserUI()
```

Muestra:

```text
TEMP: 25.6 C
field x=20
y=20
textSize=2
RED sobre WHITE
capacity=12
```

Con la geometría pública actual:

```text
GFX cell      = 144 x 16 px
FIELD_PADDING = 3 px
AUTO field    = 150 x 22 px
```

Este gate comprobará si el field declarativo actual ya ofrece un borde/fondo visual aceptable para el Designer.

### Criterio

Si el borde/fondo sigue viéndose asimétrico respecto al glifo, **no se maquillará sólo en el Designer**. Se abrirá una ampliación aditiva de la API pública y del runtime antes de A11-3/A11-4, de modo que el resultado visual y el código generado sigan representando el mismo contrato.

```text
DESIGNER_ONLY_VISUAL_FIX=FORBIDDEN
PUBLIC_API_CHANGE_IF_REQUIRED=YES
```

---

## Estado actual

```text
A11_2A_RAW_FONT_PARITY=PASS
A11_2B_PUBLIC_API_TEXT_FIELD=IN_PROGRESS
A11_2_TEXT=IN_PROGRESS
```

A11-3 no debe darse por cerrado ni avanzar a codegen final hasta decidir explícitamente el comportamiento de borde/fondo mediante API pública.
