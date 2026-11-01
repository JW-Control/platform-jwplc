# Copia de TFT_eSPI dentro de JWPLC_TFT (modo fuente alpha.16)

`JWPLC_TFT` cambia durante alpha.16 (API 0.4.0), así que se compila desde fuente y no desde `src/esp32/libJWPLC_TFT.a`. El backend es una copia **reducida** de TFT_eSPI 2.5.43 (Bodmer, licencia en `TFT_eSPI_LICENSE.txt`), de modo que el usuario no instala nada.

Cambios respecto al original (los mismos que aplica `tools/alpha15/regenerate_libJWPLC_TFT.sh`, más el empaquetado):

| Archivo | Cambio |
|---|---|
| `TFT_Drivers/ST7789_Init.h` | Los dos `DISPON` bajo `#ifndef JWPLC_TFT_DEFER_DISPON` (encendido diferido, Alpha13) |
| `TFT_eSPI.cpp` | `#error` si `tft_setup.h` no llegó; incluye `Processors/TFT_eSPI_ESP32.inc` y `Extensions/{Button,Sprite}.inc` |
| `Processors/TFT_eSPI_ESP32.c`, `Extensions/*.cpp`, `Fonts/glcdfont.c` | Renombrados a `.inc`: Arduino compila todo `.c/.cpp` de `src/`, y estos solo deben entrar vía `#include` |
| `TFT_eSPI.h` | `glcdfont.inc`; sin `User_Setups/User_Custom_Fonts.h` |
| `Fonts/GFXFF/gfxfont.h` | Solo las 24 FreeFonts que exporta `JWPLC_TFT` |
| `TFT_eSPI.cpp` (JWSIM) | Con `JWSIM` quita `ESP32` y usa el procesador Generic: así corre en JWPLC Sim |
| `Processors/TFT_eSPI_Generic.{h,inc}` | Copiados para JWPLC Sim (el `.c` renombrado a `.inc`) |

Solo se copian el procesador ESP32 (y la cabecera Generic para el simulador) y el driver ST7789; los `#include` de otras plataformas quedan en ramas `#if` que no se toman.

**Al cierre del alpha:**
1. Regenerar `libJWPLC_TFT.a` desde esta copia: `JWPLC_TFT.cpp.o` + `TFT_eSPI.cpp.o`, con el chequeo ABI. Hay que ajustar el script, porque hoy copia la TFT_eSPI instalada.
2. Volver a `precompiled=full`.
3. Decidir si la copia se queda en `src/` (la ignora el modo precompilado) o se retira.
