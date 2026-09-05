# JWPLC HMI Designer

Herramienta visual en desarrollo para diseñar la TFT/HMI del **JWPLC Basic** y generar código compatible con `JWPLC_Display`.

Estado actual:

```text
Alpha11
Gate en trabajo: A11-1 Pixel Canvas
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

## PoC A11-1 disponible

Ruta:

```text
tools/jwplc-hmi-designer/poc/
```

El PoC actual implementa:

- framebuffer RGB565 de 320 × 170;
- zoom 2× / 3× / 4× / 6× / 8×;
- escalado sin interpolación;
- grid de píxel opcional;
- coordenadas X/Y exactas;
- lectura del valor RGB565 bajo el cursor;
- dibujo y borrado por píxel;
- paleta RGB565 básica;
- preview simultáneo 1:1;
- demo geométrica para probar zoom/grid.

Todavía no reclama A11-1 como `PASS`; requiere ejecución local y revisión visual.

### Ejecutar sin dependencias

Desde la raíz del repositorio:

```powershell
cd tools\jwplc-hmi-designer\poc
py -m http.server 8080
```

Luego abrir:

```text
http://localhost:8080
```

También puede abrirse `index.html` directamente, pero el servidor local se recomienda porque los siguientes gates incorporarán recursos cargados desde el proyecto.

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

## Siguiente gate: A11-2

La siguiente incorporación será la reproducción de la fuente clásica usada por Adafruit GFX a partir del `glcdfont.c` bundled del mismo package JWPLC.

Objetivo:

```text
Texto escrito en Designer
        ==
mismos píxeles del texto renderizado por JWPLC_Display
```

Se validará primero con caracteres ASCII y `textSize` 1×/2×/3× antes de avanzar a los fields declarativos.

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
