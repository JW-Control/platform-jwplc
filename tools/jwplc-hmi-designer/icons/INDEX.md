# Biblioteca de iconos con estado (Alpha16)

Fuente de los iconos del panel «Iconos» del Designer (objeto ICON).

| Archivo | Qué es |
|---|---|
| `library.cjs` | Categorías e iconos: dibujo en una cuadrícula de 100 × 100 con roles de una letra y el color de cada rol por estado. **Se edita aquí.** |
| `raster.cjs` | Primitivas (círculo, anillo, arco, rectángulo, polígono, línea) muestreadas en el centro de cada píxel, sin antialias, como la TFT. |
| `build-catalog.cjs` | Genera `poc/designer-icon-catalog.js` (4 bpp en base64 + paletas RGB565) y la hoja `catalogo.png`. `--check` falla si el catálogo no está al día. |
| `catalogo.png` | Todos los iconos en todos sus estados (48 px) y en 24 px. |

Reglas:

- 2 a 8 estados por icono; tamaños de 8 a 64 px (hoy 24/32/48 y 64 en los equipos grandes).
- Máximo 15 roles (colores) por icono; el índice 0 es transparente y toma el fondo del campo.
- Si un estado sólo cambia colores, comparte la imagen: en flash se guarda una vez.
- Tras editar: `node tools/jwplc-hmi-designer/icons/build-catalog.cjs` y revisar `catalogo.png`.
