# Alpha15 · A15-4 — Compilación ESP32 real y primitivas nativas en JWPLC_TFT 0.2.0

Fecha: `2026-10-17`

```text
A15_4_ESP32_COMPILE=PASS (arduino-cli, jwplc_local:esp32:jwplcbasic) [ejecutado]
JWPLC_TFT=0.2.0 (fillEllipse, drawEllipse, fillTriangle, drawTriangle)
LIBJWPLC_TFT_A_REGENERATED=YES
ABI_COMPAT_VS_0_1_0=PASS (0 símbolos cambiados/eliminados, 4 añadidos)
RAM_DELTA=0 B
FLASH_DELTA_SKETCH_SIN_NUEVAS_FUNCIONES=+64 B (tolerancia alpha12 |Δ|<=64)
PHYSICAL_UPLOAD=NO [no verificado]
```

## Entorno (nube, Linux)

```text
arduino-cli 1.5.2-rc.1
tools: jwplc esp-x32 2601 (xtensa-esp-elf 14.2.0_20260121), esp32-libs 3.3.8
plataforma: repo JWPLC/2.1.0 vía namespace jwplc_local (symlink)
TFT_eSPI 2.5.43 (sólo para regenerar el .a; el usuario no la instala)
script: tools/alpha15/regenerate_libJWPLC_TFT.sh setup | build [--install]
```

Receta idéntica a `tools/alpha12/gates/alpha12_tft_precompiled_requalify.ps1`: compilar
`JWPLC_TFT` desde fuente con TFT_eSPI 2.5.43 sobre el sketch
`a14_h3e4a1_jwplc_tft_shapes_probe` con `-DJWPLC_TFT_ESPI_DIAGNOSTIC_NO_DISPLAY_AUTOLOAD=1`, y
empaquetar `JWPLC_TFT.cpp.o` + `TFT_eSPI.cpp.o` con `xtensa-esp32-elf-gcc-ar`.

## 1. Validación del pipeline (sin cambios de fuente)

Reconstruir el `.a` con el fuente 0.1.0 de `main`:

```text
SYMBOLS (nombre, tipo, tamaño) vs .a publicado: 318 = 318, DIFF=0
BYTES: 1090074 (publicado 1091098; diferencia por rutas de build embebidas Windows vs Linux)
```

## 2. Cambio en JWPLC_TFT 0.2.0

| Archivo | Cambio |
|---|---|
| `src/JWPLC_TFT.h` | 4 métodos públicos nuevos; **sin miembros de datos nuevos** (layout de `JWPLC_TFTClass` intacto). `JWPLC_TFT_VERSION_MINOR` 1 → 2 |
| `src/JWPLC_TFT.cpp` | Implementación con el patrón existente (`acquireForOperation` → `g_backend.*` → `releaseAfterOperation`) |
| `src/esp32/libJWPLC_TFT.a` | Regenerado (1099960 B) |
| `library.properties` | `version=0.2.0` |
| `README.md` | Sección "Elipses y triángulos" |

Símbolos del `.a` nuevo contra el publicado:

```text
SYMBOLS_REMOVED_OR_CHANGED=0
SYMBOLS_ADDED=4
  + JWPLC_TFTClass::fillEllipse(short, short, short, short, unsigned short, unsigned long)
  + JWPLC_TFTClass::drawEllipse(...)
  + JWPLC_TFTClass::fillTriangle(...)
  + JWPLC_TFTClass::drawTriangle(...)
```

`libJWPLC_Display.a` (compilado contra el header 0.1.0) sigue enlazando: sólo se añadieron
métodos no virtuales.

## 3. Regresión de tamaño (arduino-cli, ESP32)

| Sketch | `.a` 0.1.0 publicado | `.a` 0.2.0 | Δ flash | Δ RAM |
|---|---|---|---|---|
| `01_empty` (autoload) | 411045 / 29324 | 411109 / 29324 | +64 | 0 |
| `04.Display_TFT_Direct` | 425369 / 29340 | 425433 / 29340 | +64 | 0 |
| `a14_h3e4a1_jwplc_tft_shapes_probe` | 413509 / 29324 | 413573 / 29324 | +64 | 0 |
| Gate `A15_1_3_Codegen_Static` | 443973 / 34620 | 444037 / 34620 | +64 | 0 |

Atribución: reconstruir sin cambios da −32 B (rutas); el código nuevo aporta +96 B. Todos sin
warnings ni errores.

## 4. Codegen del Designer

Los helpers `jwplcHMIFillEllipse`, `jwplcHMIDrawEllipse` y `jwplcHMIFillTriangle` eligen en
compilación:

```cpp
#if defined(JWPLC_TFT_VERSION_MAJOR) && \
    ((JWPLC_TFT_VERSION_MAJOR > 0) || (JWPLC_TFT_VERSION_MINOR >= 2))
#define JWPLC_HMI_TFT_NATIVE_SHAPES 1   // tft.fillEllipse(...) nativo
#else
#define JWPLC_HMI_TFT_NATIVE_SHAPES 0   // respaldo con drawFastHLine/drawPixel
#endif
```

Así un header generado compila con el package publicado (0.1.0) y con el nuevo (0.2.0).

| Gate `A15_1_3_Codegen_Static` | Librería seleccionada | Resultado |
|---|---|---|
| Ruta nativa | `JWPLC_TFT` 0.2.0 del repo | PASS, 444649 B flash |
| Ruta de respaldo | `JWPLC_TFT` 0.1.0 publicado forzado con `--library` | PASS, 443973 B flash |

Los headers del smoke test (`full`, `afterDelete`, `shapesOnly`) también compilan y enlazan
para ESP32 sin diagnósticos.

## Pendiente

```text
FUENTES (setFont/drawString)=PENDIENTE: requieren LOAD_FONT2/4/GFXFF en tft_setup.h; medir
  flash antes de publicar y guardar el estado de fuente sin añadir miembros a la clase.
PUBLICACION=el .a 0.2.0 llega al usuario con el próximo package (Alpha13).
PARIDAD_VISUAL=elipse/triángulo de TFT_eSPI vs preview del Designer -> A15-9 en la TFT.
```
