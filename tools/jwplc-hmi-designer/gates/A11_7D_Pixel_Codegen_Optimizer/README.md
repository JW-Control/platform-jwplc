# Alpha11 · A11-7D — PixelMap Codegen Optimizer

Fecha: 2026-09-07

## Objetivo

Reducir el tamaño del C++ generado, la memoria estática asociada a PixelMaps y
el trabajo del compilador Arduino, sin cambiar el resultado pixel-perfect ni la
API de visibilidad ya validada.

## Compatibilidad

El formato anterior se conserva intacto:

```cpp
JWPLC_Display.setPixelMaps(...);
```

Se añade un registro compacto separado:

```cpp
JWPLC_Display.setPackedPixelMaps(...);
```

No se sobrecarga `setPixelMaps()` para evitar ambigüedad en código existente que
pueda usar `setPixelMaps(nullptr, 0)`.

Los controles de runtime continúan siendo comunes a ambos formatos:

```cpp
JWPLC_Display.clearPixelMaps();
JWPLC_Display.pixelMapCount();
JWPLC_Display.setPixelMapVisible(index, visible);
JWPLC_Display.isPixelMapVisible(index);
```

El sketch no necesita conocer qué representación eligió el Designer para
mostrar/ocultar un frame.

## Formato PACKED_SPAN16

Cada span ocupa un `uint32_t`:

```text
bits  0.. 8 : X        0..319
bits  9..16 : Y        0..169
bits 17..25 : LEN-1    0..319 => 1..320 px
bits 26..29 : COLOR    índice 0..15
bit      30 : DIR      0=horizontal, 1=vertical
bit      31 : reservado, siempre 0
```

Cada PixelMap compacto dispone de una paleta RGB565 local de hasta 16 colores.

## Selección automática

El usuario no selecciona manualmente el formato.

Para cada PixelMap el Designer:

1. cuenta sus colores RGB565;
2. genera runs horizontales legacy;
3. para cada color genera spans horizontales y verticales;
4. conserva para ese color la orientación que produce menos spans;
5. calcula el costo estático estimado.

Costos usados para la decisión en ESP32 de 32 bits:

```text
RGB565_RUN:
  JWPLC_UIPixelRun = 8 B
  JWPLC_UIPixelMap ~= 12 B

PACKED_SPAN16:
  span             = 4 B
  color paleta     = 2 B
  packed map       ~= 20 B
```

El proyecto usa `PACKED_SPAN16` sólo cuando:

- todos los PixelMaps no vacíos tienen <= 16 colores; y
- el costo total estimado es menor que RGB565_RUN.

Si cualquiera de estas condiciones falla, se mantiene automáticamente el
formato `RGB565_RUN` ya validado.

Los dos formatos son mutuamente exclusivos dentro de un registro activo.

## Codegen

Para `PACKED_SPAN16`, el Designer genera:

```cpp
static const uint16_t HMI_PM_1_PAL[] = { ... };
static const uint32_t HMI_PM_1_DATA[] = { ... };

static const JWPLC_UIPixelPackedMap HMI_PIXEL_MAPS[] =
{
    ...
};
```

Los datos empaquetados se imprimen a razón de 8 valores por línea para evitar
headers de cientos de líneas de constructores C++.

El bloque conserva el marcador canónico:

```text
// PixelMaps estáticos RGB565 · JWPLC HMI Designer
```

para mantener idempotencia con el postprocesador de codegen ya existente.

## Inspector

`Inspector · PIXEL` incorpora `Optimización C++` con:

- formato elegido para el proyecto;
- píxeles del objeto;
- colores del objeto;
- runs legacy -> spans compactos;
- tamaño estimado del objeto;
- ahorro estimado del proyecto.

Esto permite medir casos reales antes de compilar.

## Runtime

`JWPLC_UI_PixelMap` soporta ahora dos representaciones:

- `JWPLC_UIPixelMap`: runs horizontales RGB565;
- `JWPLC_UIPixelPackedMap`: paleta + spans H/V de 32 bits.

El renderer compacto decodifica cada span y usa:

```cpp
tft.drawFastHLine(...);
tft.drawFastVLine(...);
```

según el bit `DIR`.

Los fields declarativos siguen dibujándose por encima del PixelMap.

## Estado

```text
A11_7C_PIXEL_WORKBENCH=IMPLEMENTED_PENDING_USER_GATE
A11_7D_PACKED_STRUCT=IMPLEMENTED_PENDING_COMPILE_GATE
A11_7D_PACKED_RUNTIME=IMPLEMENTED_PENDING_PHYSICAL_GATE
A11_7D_AUTO_FORMAT_SELECTION=IMPLEMENTED_PENDING_USER_GATE
A11_7D_HV_PER_COLOR=IMPLEMENTED_PENDING_USER_GATE
A11_7D_FALLBACK_RGB565_RUN=IMPLEMENTED_PENDING_USER_GATE
A11_7D_COMPACT_HEADER_8_PER_LINE=IMPLEMENTED_PENDING_USER_GATE
A11_7D_OPTIMIZATION_STATS=IMPLEMENTED_PENDING_USER_GATE
A11_7D_CODEGEN_IDEMPOTENT=IMPLEMENTED_PENDING_USER_GATE
A11_7D_PIXEL_VISIBILITY_COMPATIBLE=IMPLEMENTED_PENDING_PHYSICAL_GATE
```

## Gate del usuario

1. Actualizar/reinstalar el Designer.
2. Abrir el PixelMap de prueba actual (~2522 px).
3. Revisar `Inspector · PIXEL > Optimización C++` y registrar:
   - formato;
   - runs -> spans;
   - bytes estimados;
   - porcentaje de ahorro.
4. Pulsar `Generar C++` varias veces y confirmar que el bloque PixelMap no se
   duplica.
5. Si el formato elegido es `PACKED_SPAN16`, confirmar que el header contiene:
   - `JWPLC_UIPixelPackedMap`;
   - `uint16_t HMI_PM_*_PAL[]`;
   - `uint32_t HMI_PM_*_DATA[]`;
   - `JWPLC_Display.setPackedPixelMaps(...)`.
6. Compilar el sketch en Arduino IDE.
7. Subir al JWPLC Basic y confirmar resultado pixel-perfect.
8. Validar `setPixelMapVisible(PIXEL_..., false/true)` con formato compacto.
9. Comparar tamaño de código/header frente al formato anterior.

No marcar A11-7D como PASS hasta cerrar compilación y validación física.
