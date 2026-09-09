# Alpha11 — Validación A11-1 Pixel Canvas

Fecha: 2026-09-05

## Base

```text
BRANCH=v2.1.0-alpha.11/feature/hmi-designer
TARGET=JWPLC Basic v2
DISPLAY=ST7789
LOGICAL_SIZE=320x170
ROTATION=3
COLOR=RGB565
```

## Objetivo del gate

Validar que el primer PoC del `JWPLC HMI Designer` representa el framebuffer lógico real del JWPLC Basic y permite inspección/edición a nivel de píxel sin interpolación.

## Evidencia visual recibida

Se realizó validación manual en Edge/Chromium sobre `localhost:8080` con capturas del PoC ejecutándose.

Se verificó:

- canvas inicial vacío a `320 x 170`;
- ejecución del patrón `Demo`;
- preview `1:1` simultáneo;
- zoom `2x`;
- zoom `3x`;
- zoom `4x`;
- zoom `6x`;
- zoom `8x`;
- grid activo en distintos niveles de zoom;
- grid desactivado sin modificar la imagen lógica;
- dibujo manual por píxel/trazo continuo;
- selección de color RGB565;
- dibujo manual blanco con actualización inmediata del preview;
- herramienta `Borrar`;
- lectura de coordenadas/píxel bajo cursor.

Una captura de borrado mostró explícitamente:

```text
X=3
Y=2
Pixel=0x0000
```

confirmando que la lectura de coordenadas y el valor del framebuffer siguen el espacio lógico y no las coordenadas escaladas del navegador.

## Resultado observado

El patrón del canvas y la vista `1:1` conservaron la misma geometría al cambiar de zoom. En `4x`, `6x` y `8x` los píxeles individuales permanecieron definidos; no se observó suavizado/interpolación del framebuffer.

El scroll del viewport en zoom altos es comportamiento esperado: el canvas continúa teniendo `320 x 170` píxeles lógicos y sólo cambia su escala de inspección.

## Gate

```text
A11_0_ARCHITECTURE=PASS
A11_1_PIXEL_CANVAS=PASS
A11_1_FRAMEBUFFER_320X170=PASS
A11_1_ZOOM_PIXEL_PERFECT=PASS
A11_1_GRID_OPTIONAL=PASS
A11_1_COORDINATES=PASS
A11_1_PREVIEW_1_TO_1=PASS
A11_1_FREEHAND_DRAW=PASS
A11_1_ERASE=PASS
```

## Siguiente gate

```text
NEXT=A11-2_GFX_TEXT_PARITY
```

A11-2 debe reproducir la fuente clásica `glcdfont.c` incluida en Adafruit GFX, usando la misma celda `6x8`, el mismo bitmap de glifos y escalado entero equivalente a `setTextSize()`.

A11-2 no se considerará cerrado únicamente por inspección del navegador: debe terminar con una comparación seleccionada contra el render del JWPLC Basic físico.
