# A16 TFT1 — API gráfica `JWPLC_TFT` 0.4.0 (base del Runtime)

Rama: `v2.1.0-alpha.16/JWPLC-Sim/feature/api-tft-graficos`. Por decisión del propietario (01/11), todo el trabajo de alpha.16 va dentro de la línea JWPLC-Sim.
Diseño de referencia: `DISENO_RUNTIME_ESTILO_LOGO8.md` §7.2, en la carpeta del proyecto.

```text
A16_TFT1_API_HEADER=PASS                 [ejecutado] g++ -fsyntax-only contra stub de TFT_eSPI 2.5.43
A16_TFT1_LIB_A_REGENERADA=PENDING        [no verificado] requiere tools/alpha15/regenerate_libJWPLC_TFT.sh build --install en la PC del equipo
A16_TFT1_ABI_COMPAT=PENDING              lo verifica el mismo script (sin cambios de layout en JWPLC_TFTClass)
A16_TFT1_EJEMPLO_COMPILA=PENDING         Display_Graficos_Sprite con jwplc_local:esp32:jwplcbasic
A16_TFT1_FISICO=PENDING                  ver checklist físico
A16_TFT1_SIM_REGRESSION=PENDING          CI; el stub de JWPLC_TFT en jwplc-sim puede necesitar la API nueva
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

## Pendiente en la PC del equipo (antes de fusionar)

```bash
tools/alpha15/regenerate_libJWPLC_TFT.sh setup     # si no está hecho
tools/alpha15/regenerate_libJWPLC_TFT.sh build --install
tools/alpha15/regenerate_libJWPLC_TFT.sh compile JWPLC/2.1.0/libraries/JWPLC_Display/examples/Display_Graficos_Sprite
```

Mientras el `.a` no se regenere, el header y el binario de la rama están **desalineados**. No se fusiona antes de ese paso.

## Checklist físico

1. Abrir `Display_Graficos_Sprite`, pulsar OK y moverse con ◄ ►: la selección cambia sin parpadeo ni barrido negro.
2. El marco de 2 px se ve continuo y con bordes suaves.
3. El sprite no pisa el marco (recorte).
4. Al volver a IDLE y entrar otra vez, el mapa reaparece igual (la RAM del sprite se conserva).
5. La IDLE y las páginas del Designer se ven igual que en alpha.15 (no hay recorte residual).
