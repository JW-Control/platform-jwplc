# Alpha15 · Revisión de Lopaka y propuestas para el JWPLC HMI Designer

Fecha: `2026-10-18`

Fuentes:

- `github.com/sbrin/lopaka`, commit 4fcd60d del 2026-10-14 (editor local);
- documentación de `lopaka.app/pages/docs/*`: editor, fuentes, imágenes, animaciones, generación de código.

La nube de Lopaka (proyectos, galería, animaciones) tiene partes de pago.

```text
LOPAKA_REVIEW=DONE
PROPUESTAS=DECIDIDAS (§4)
P1_1_TILDES=PASS_HOST_AND_ESP32_COMPILE (A15_I3_TILDES_CP437_20261018.md)
P2_7_LED=PASS (BOOL «Piloto», A15_I1) · P2_7_RELOJ=PASS (A15_I2)
```

## 1. Qué tiene Lopaka (resumen)

| Área | Lopaka |
|---|---|
| Capas | dot, line, rect, circle, ellipse, triangle, polygon, arc (sólo TFT_eSPI), text, icon, paint (bitmap), grupos; widgets sólo en LVGL (botón, checkbox, switch, slider, arc, panel) |
| Propiedades | posición/tamaño, color, relleno, radio, invertido; en bitmaps: overlay de referencia, voltear, rotar 90°, invertir, recortar, *Merge to Bitmap*; bloquear y ocultar |
| Fuentes | GLCD 5×7 + 52 FreeFonts GFX; 41 BDF (U8g2); 7 TTF (LVGL); fuentes propias BDF/TTF → GFX (de pago); rotación de texto sólo en U8g2/LVGL |
| Íconos/imágenes | packs mono Gaai (266) y Flipper (257), más de 1500 de pago; importación de PNG/BMP/GIF/video con recorte, redimensionado, brillo/contraste, dithering y salida mono o RGB565, con el tamaño en bytes en vivo |
| Animaciones | biblioteca de 32/48/64 px, 7–28 frames; genera `updateAnimations()` y `drawAnimations()`; sin timeline |
| Variables | un interruptor por propiedad (x, y, texto, color) la convierte en variable del código generado; sin widgets declarativos en C++ |
| Código | 12 plataformas; opciones: función envolvente, incluir fuentes, declarar imágenes y variables, PROGMEM; imágenes deduplicadas; prefijo/sufijo editable con `BEGIN/END`; mapa línea de código ↔ capa |
| Edición | varias pantallas, alinear (6 botones), grilla 1/10 px sin snapping, Ctrl+flechas, agrupar |
| Otros | galería pública, fork, compartir; WebMCP (un agente de IA edita capas); vista previa en hardware sólo para Flipper |

**Lo que nosotros ya tenemos y Lopaka no** (no copiar, es nuestra ventaja):

- widgets declarativos en C++ (TEXT, VALUE, BOOL, BAR, MENU, NAV);
- snapping;
- refresco parcial (dirty refresh), que cuida el bus SPI compartido;
- integración con OpenPLC;
- botonera de 6 teclas;
- vista previa en la placa por WebSerial.

## 2. Propuestas por prioridad

### P1 — mucho valor, poco costo

1. **Tildes y ñ en la TFT sin fuentes nuevas.**
   - La fuente GLCD del firmware (`TFT_eSPI/Fonts/glcdfont.c`) trae los **256** caracteres cp437, entre ellos á é í ó ú ñ Ñ (0xA0–0xA5, 0x82) y ° (0xF8) `[visto]`.
   - El Designer sólo tiene ASCII (dibuja «?») y avisa «caracteres fuera de ASCII».
   - Propuesta:
     - completar la tabla de glifos del Designer con los 256 de `glcdfont.c`;
     - en el codegen, traducir UTF-8 → cp437 (`"Presión"` → `"Presi\xA2n"`);
     - quitar esos avisos.
   - Antes, verificar en la TFT el posible desfase de un carácter por encima de 0xB0 (en Adafruit GFX depende de `cp437()`).
2. **Pack de íconos industriales** (mono 16 y 24 px, coloreables):
   - temas: motor, bomba, válvula, alarma, ventilador, nivel, temperatura, red/Ethernet, RS-485, WiFi, batería, candado, engranaje, flechas;
   - objeto ICON estático: se dibuja una vez por página con `drawBitmap`, que no cuesta en cada tick.
3. **Íconos o figuras con estado (BOOL → color o visibilidad).**
   - Es la «variable por propiedad» de Lopaka adaptada al refresco parcial: una lámpara o un motor que cambia de color cuando cambia una variable, y sólo entonces se repinta.
   - Cubre el caso PLC más común, el piloto de estado.
4. **Bloquear/ocultar y capa de referencia (overlay).**
   - Una foto del tablero o una captura de la HMI anterior como fondo de calco.
   - No genera código.
5. **Alinear y distribuir** respecto de la pantalla o de la selección.
   - Lopaka no tiene «distribuir»: ventaja nuestra.
6. **Clic en el código ↔ selección del objeto**, en el panel «Código generado».

### P2 — mucho valor, requiere firmware

7. **Nuevos tipos de variable para PLC:**
   - **MULTI-ESTADO**: INT → texto y color por valor. Por ejemplo, 0 PARADO, 1 MARCHA, 2 FALLA, 3 MANUAL. Es la generalización de BOOL.
   - **LED/piloto**: círculo con color por estado; es el indicador más usado en tableros.
   - **Reloj/fecha** desde el RTC DS3232 de la placa, con formato elegible.
   - **Tendencia** (sparkline): historial de N muestras de una variable. Se repinta por desplazamiento de columna, no completo.
   - **Gauge de arco**: usa `drawArc` y `drawSmoothArc` de TFT_eSPI. Requiere exponerlos en `JWPLC_TFT`; es el «BAR circular» que hoy está deshabilitado.
8. **Asistente de importación de imagen:**
   - PNG → RGB565 o mono, con recorte, dithering y tamaño en bytes visible;
   - aviso del costo en flash y del tiempo de SPI;
   - alimenta la capa Pixel o un objeto IMAGE.
9. **Animación por frames con presupuesto**:
   - 4–8 frames de 16–32 px, por ejemplo un ventilador que gira o una bomba en marcha;
   - avanza en el tick del Display, nunca con `delay()`, y sólo mientras la página está visible.

### P3 — conveniencia

10. **Rotación de texto 90°** para etiquetas verticales en los 170 px de alto. Requiere `setRotation` local o glifos rotados.
11. **Patrones de pincel** (damero, rayas) en la capa Pixel.
12. **Modo presentación**: recorrer páginas, NAV y MENU con las 6 teclas simuladas en el lienzo (← → ↑ ↓ Enter Esc), sin placa.
13. **Fuentes propias** BDF/TTF → GFX con rango latino (á é ñ °).
14. **API para agentes** (como el WebMCP de Lopaka): crear y editar widgets por un contrato validado, respetando el undo. Encaja con el trabajo multiagente del equipo (Claude, GPT/Codex).

## 3. Sugerencia de orden

```text
Alpha15 (cierre):  P1.1 tildes · P1.4 bloquear/ocultar/overlay · P1.5 alinear/distribuir
Alpha15/16:        P1.2 íconos · P1.3 íconos con estado · P2.7 MULTI-ESTADO y LED
Siguiente alpha:   P2.7 reloj/tendencia/gauge · P2.8 importar imagen · P2.9 animación · P3
```

Cada objeto nuevo con firmware sigue la regla del proyecto:

- primero `JWPLC_UI` en modo fuente;
- después el pintor del Designer igual al firmware (test de paridad como el del MENU);
- al final el codegen con golden.

## 4. Decisión del jefe (2026-10-18)

- **P1**: aprobado en su conjunto.
- **P2**:
  - MULTI-ESTADO descartado; lo cubre SEQUENCE.
  - LED/piloto pasa a ser una **representación de BOOL**, no un objeto nuevo.
  - **Reloj RTC** aprobado.
  - **Tendencia** (gráfico de línea del historial) aprobada.
  - **Gauge de arco** = el BAR circular deshabilitado.
- **P3**: interesante, sin prioridad.

Plan integrado con la ampliación industrial (ALARM, INTERLOCK, SEQUENCE, RECIPE, EVENT_LOG):
`A15_INDUSTRIAL_ARQUITECTURA_PLAN_20261018.md`.
