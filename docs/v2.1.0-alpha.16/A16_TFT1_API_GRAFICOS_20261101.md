# A16 TFT1 — API gráfica `JWPLC_TFT` 0.4.0 (base del Runtime)

Rama: `v2.1.0-alpha.16/JWPLC-Sim/feature/api-tft-graficos`. Por decisión del propietario (01/11), todo el trabajo de alpha.16 va dentro de la línea JWPLC-Sim.
Diseño de referencia: `DISENO_RUNTIME_ESTILO_LOGO8.md` §7.2, en la carpeta del proyecto.

```text
A16_TFT1_API_HEADER=PASS                 [ejecutado] g++ -fsyntax-only contra stub de TFT_eSPI 2.5.43
A16_TFT1_MODO_FUENTE=IN_PROGRESS         JWPLC_TFT sin precompiled=full; TFT_eSPI 2.5.43 reducida en src/ (ver src/TFT_eSPI_VENDOR.md)
A16_TFT1_EJEMPLO_COMPILA=PASS            [ejecutado por el propietario, 01/11] Arduino IDE, jwplc_local:esp32:jwplcbasic: 438653 B flash (10 %), 29660 B RAM global
A16_TFT1_LIB_A_REGENERADA=DEFERRED       al cierre del alpha, desde la copia de src/ (ajustar el script)
A16_TFT1_ABI_COMPAT=DEFERRED             al cierre, con el mismo script (sin cambios de layout en JWPLC_TFTClass)
A16_TFT1_FISICO=IN_PROGRESS              [ejecutado por el propietario, 01/11] render correcto (foto); fallo: ◄ ► pulsados en IDLE movían la selección al entrar. Corregido en el ejemplo, falta repetir 1–5
A16_TFT1_SIM_REGRESSION=PASS             [ejecutado] jwplc-sim (Linux, native) con package.lock local en esta rama: Display_Graficos_Sprite enlaza y corre; JWSIM_G4_TFT_HMI da las mismas capturas que alpha.15 (la página RTC difiere 170 px de la referencia también con alpha.15: preexistente)
```

## Qué se añade

| API | Para qué (Runtime) | Backend |
|---|---|---|
| `setClipRect(x,y,w,h)` / `resetClip()` / `clipActive()` | Dibujar el mapa FBD, listas y tendencias sin pisar la barra de estado ni el pie | `TFT_eSPI::setViewport(..., vpDatum=false)` |
| `drawWideLine(x0,y0,x1,y1,width,color,background)` | Cables de 2 px con estado (verde/gris) | `TFT_eSPI::drawWideLine` con fondo explícito |
| `drawPolyline(xy, points, width, color, background)` | Cables con codos y marcos, con un solo lock | `drawLine`/`drawWideLine` |
| `JWPLC_TFTSprite` (16/8/4/1 bits) | Redibujar zonas sin parpadeo: mapa FBD, cursor, listas con scroll | `TFT_eSprite` (va dentro de `TFT_eSPI.cpp.o`) |

Los defines `JWPLC_TFT_HAS_CLIP`, `JWPLC_TFT_HAS_WIDE_LINE` y `JWPLC_TFT_HAS_SPRITE` permiten que el codegen y el Runtime detecten la API. La versión pasa a 0.4.0.

## Reglas de diseño

- **ABI:** `JWPLC_TFTClass` no cambia de layout. El estado del recorte vive en el `.cpp` y solo se añadió un `friend`. `JWPLC_TFTSprite` guarda un puntero opaco, así que el sketch no incluye TFT_eSPI.
- **Recorte acotado al batch:** `setClipRect` solo se acepta dentro de un batch de la misma tarea, y `endBatch()` lo quita siempre. Los callbacks USER ya corren dentro del batch de `JWPLC_Display`, así que ahí basta con llamar `setClipRect` … `resetClip`.
- **Fondo explícito en `drawWideLine`:** el valor por defecto de TFT_eSPI (`0x00FFFFFF`) lee la GRAM, y en el bus compartido no es fiable.
- **Sprites de 4 y 1 bit:** los colores son índices de la paleta y `drawWideLine` dibuja un trazo sólido sin mezcla.
- **`push()` del sprite:** usa el mismo lock que las primitivas, participa en el batch abierto de la tarea y respeta el recorte.

## RAM de referencia (área 320×140)

| Profundidad | RAM |
|---|---|
| 16 bits | ~87.5 KB (solo para zonas pequeñas) |
| 8 bits | ~43.8 KB |
| 4 bits | ~21.9 KB (recomendada para el mapa FBD) |
| 1 bit | ~5.5 KB |

## Modo fuente durante alpha.16

Como `JWPLC_TFT` cambia en este alpha, se compila desde fuente (regla del proyecto, recordada por el propietario el 01/11):

- `library.properties` ya no tiene `precompiled=full`. El `.a` de alpha.15 queda en `src/esp32/` y se ignora.
- El backend es una copia reducida de TFT_eSPI 2.5.43 dentro de `src/`, con el parche DISPON y el guard de `tft_setup.h` del script de regeneración. Así el usuario sigue sin instalar TFT_eSPI. Detalle en `src/TFT_eSPI_VENDOR.md`.
- **Probar en la PC del equipo:** abrir `Display_Graficos_Sprite` con la placa local (JWPLC 2.1.0 apuntando a este árbol) y compilar. La primera compilación de `JWPLC_TFT` tarda más, porque ya no viene precompilada.
- **Al cierre:** regenerar `libJWPLC_TFT.a` desde esta copia, con el chequeo ABI y el benchmark de compilación, y volver a `precompiled=full`.

## JWPLC Sim

`jwplc-sim` compila todo `src/` de cada librería del package (ignora `precompiled`) y, hasta alpha.15, enlazaba aparte su propia TFT_eSPI 2.5.43 con el procesador Generic porque `JWPLC_TFT.cpp` hacía `#include <TFT_eSPI.h>`. Con la copia en `src/`, `JWPLC_TFT.cpp` encuentra el header local, el simulador ya no enlaza la suya y la copia de `src/` entraba con el procesador ESP32 (registros físicos). Arreglo en el package, sin tocar el simulador:

- Con `JWSIM` definido, `TFT_eSPI.cpp` de `src/` quita `ESP32`/`ARDUINO_ARCH_ESP32` y se compila con el procesador Generic (`SPI.transfer` + `digitalWrite`), como la copia del simulador.
- Se añaden `Processors/TFT_eSPI_Generic.h` y `Processors/TFT_eSPI_Generic.inc` (el `.c` original renombrado).

Prueba en el simulador (01/11), con `package.lock` apuntando a esta rama sólo en el clon local:

- `Display_Graficos_Sprite` con ◄ ◄ en IDLE y luego OK: sin el arreglo de teclas entra con «SR» seleccionado; con el arreglo entra con «&» y ◄ ► mueven la selección.
- `JWSIM_G4_TFT_HMI`: capturas idénticas a las de alpha.15 en reposo, página 0 y vuelta a reposo. La página RTC difiere de la referencia en 170 px también con alpha.15, así que no viene de este cambio.

## Teclas pendientes al entrar desde IDLE

`JWPLC_Display` no consume los latches `pressed()/released()` de la aplicación (regla Alpha8; ver el comentario de `enterUserUI()`), así que una tecla pulsada en IDLE sigue pendiente al entrar a USER. El ejemplo llama `JWPLC_Display.clearPendingInput()` al inicio de `jwplcUIEnter()`. El modo Runtime hará lo mismo al entrar; una opción del Display para hacerlo siempre queda para cuando `JWPLC_Display` pase a modo fuente.

## Checklist físico

1. Abrir `Display_Graficos_Sprite`, pulsar OK y moverse con ◄ ►: la selección cambia sin parpadeo ni barrido negro.
2. El marco de 2 px se ve continuo y con bordes suaves.
3. El sprite no pisa el marco (recorte).
4. Al volver a IDLE y entrar otra vez, el mapa reaparece igual (la RAM del sprite se conserva).
5. La IDLE y las páginas del Designer se ven igual que en alpha.15 (no hay recorte residual).
