# JWPLC HMI Designer

Herramienta visual en desarrollo para diseñar la TFT/HMI del **JWPLC Basic** y generar código compatible con `JWPLC_Display`.

Estado actual:

```text
Alpha11
PoC inicial
Target: ST7789 / 320 x 170 / rotation 3
```

## Objetivo

El Designer debe permitir construir interfaces viendo los píxeles reales que ocupará la TFT, configurar campos HMI y generar código sobre la API existente:

```cpp
JWPLC_UIValueField(...)
JWPLC_UITextField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)

JWPLC_Display.setFields(...)

extern "C" void jwplcUIUpdate()
{
    // bindings generados
}
```

No se crea un runtime gráfico alternativo.

## Contrato V1

```text
Canvas lógico : 320 x 170
Controlador   : ST7789
Rotación      : 3
Color         : RGB565
Fuente        : Adafruit GFX classic 5x7
Campos        : VALUE / TEXT / BOOL / BAR
Máximo actual : 32 fields
```

## Prioridad del PoC

1. framebuffer 320 × 170;
2. zoom sin interpolación;
3. grid de píxel;
4. coordenadas exactas;
5. fuente clásica Adafruit GFX;
6. campos HMI existentes;
7. inspector de propiedades;
8. proyecto `.jwhmi`;
9. codegen C++;
10. exportación segura hacia sketch.

## Regla de integración

El núcleo del Designer debe poder funcionar de forma independiente de Arduino IDE.

La futura integración con el IDE debe actuar como una capa para:

- abrir el diseñador;
- detectar el sketch actual;
- escribir/regenerar archivos controlados;
- compilar/subir cuando exista una ruta soportada.

No se debe acoplar el modelo `.jwhmi` ni el renderer a APIs privadas del IDE.

## Documentación Alpha11

Ver:

```text
docs/v2.1.0-alpha.11/ALPHA11_HMI_DESIGNER_ARCHITECTURE.md
```
