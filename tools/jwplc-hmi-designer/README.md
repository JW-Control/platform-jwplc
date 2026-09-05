# JWPLC HMI Designer

Herramienta visual en desarrollo para diseñar la TFT/HMI del **JWPLC Basic** y generar código compatible con `JWPLC_Display`.

Estado actual:

```text
Alpha11
A11-0 Arquitectura: PASS
A11-1 Pixel Canvas: PASS
Gate en trabajo: A11-2 GFX Text Parity
Target: ST7789 / 320 x 170 / rotation 3
```

## Objetivo

El Designer debe permitir construir interfaces viendo los píxeles reales que ocupará la TFT, configurar campos HMI y generar la **definición/configuración visual** sobre la API pública existente:

```cpp
JWPLC_UIValueField(...)
JWPLC_UITextField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)

JWPLC_Display.setFields(...)
```

El Designer termina antes de `jwplcUIUpdate()`.

La lógica dinámica pertenece al usuario:

```cpp
extern "C" void jwplcUIUpdate()
{
    // código manual del usuario
    // JWPLC_Display.setValue(...)
    // JWPLC_Display.setText(...)
    // JWPLC_Display.setBool(...)
    // JWPLC_Display.setBar(...)
}
```

El Designer no genera bindings ni reescribe este callback.

No se crea un runtime gráfico alternativo.

## Contrato V1

```text
Canvas lógico : 320 x 170
Controlador   : ST7789
Rotación      : 3
Color         : RGB565
Fuente        : Adafruit GFX classic
Campos        : VALUE / TEXT / BOOL / BAR
Máximo actual : 32 fields
```

## Frontera de codegen

```text
Designer genera:
- IDs simbólicos de fields
- JWPLC_UIField[]
- helpers JWPLC_UI*Field(...)
- JWPLC_Display.setFields(...)
- configuración de display elegida visualmente

Usuario genera:
- jwplcUIUpdate()
- setValue(...)
- setText(...)
- setBool(...)
- setBar(...)
- adquisición y lógica de proceso
```

Documento contractual:

```text
docs/v2.1.0-alpha.11/A11_3_PUBLIC_API_CODEGEN_CONTRACT.md
```

## PoC disponible

Ruta:

```text
tools/jwplc-hmi-designer/poc/
```

El PoC implementa actualmente:

- framebuffer RGB565 de 320 × 170;
- zoom 2× / 3× / 4× / 6× / 8×;
- escalado sin interpolación;
- grid de píxel opcional;
- coordenadas X/Y exactas;
- lectura del valor RGB565 bajo el cursor;
- dibujo y borrado por píxel;
- preview simultáneo 1:1;
- fuente clásica Adafruit GFX para ASCII 32–126;
- celda RAW de 6 × 8 px por carácter;
- `textSize` 1×..4×;
- edición en vivo de texto, X/Y, foreground y background;
- `Fondo de celda GFX` reproducido sin padding simétrico artificial.

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

## A11-2 — paridad física RAW

Muestra canónica:

```text
TEMP: 25.6 C
X=20
Y=20
size=2
RED sobre WHITE
bounds GFX=144x16
```

Gate físico:

```text
tools/jwplc-hmi-designer/gates/A11_2_GFX_Text_Parity/
```

A11-2 comprueba que el Designer reproduce el comportamiento nativo equivalente a:

```cpp
tft.setTextSize(2);
tft.setTextColor(ST77XX_RED, ST77XX_WHITE);
tft.setCursor(20, 20);
tft.print("TEMP: 25.6 C");
```

El background del modo RAW pertenece a la celda clásica GFX. No es una caja con padding simétrico.

La `safe area` de pantalla probada temporalmente durante A11-2 fue retirada porque respondía a una interpretación incorrecta de la observación visual.

## Prioridad del PoC

1. framebuffer 320 × 170 — PASS;
2. zoom sin interpolación — PASS;
3. grid de píxel — PASS;
4. coordenadas exactas — PASS;
5. fuente clásica Adafruit GFX — EN VALIDACIÓN FÍSICA;
6. campos HMI existentes;
7. inspector de propiedades;
8. proyecto `.jwhmi`;
9. codegen C++ estructural;
10. exportación segura hacia sketch.

## Siguiente gate tras A11-2

A11-3 incorporará los fields declarativos reales:

```text
VALUE
TEXT
BOOL
BAR
```

El renderer del Designer deberá reproducir exactamente la geometría actual de `JWPLC_Display`:

```text
FIELD_PADDING=3
FIELD_GAP=4
AUTO width/height
INLINE / STACKED
LEFT / CENTER / RIGHT
```

Sólo después de reproducir esa geometría se evaluará si una futura API necesita padding configurable o un objeto de texto con fondo simétrico.

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
docs/v2.1.0-alpha.11/A11_2_GFX_CELL_BACKGROUND_DECISION.md
docs/v2.1.0-alpha.11/A11_2_SAFE_AREA_DECISION.md  (SUPERSEDED)
docs/v2.1.0-alpha.11/A11_3_PUBLIC_API_CODEGEN_CONTRACT.md
```
