# A16 F0' — gates restantes de la base gráfica (teclado, QR de vinculación, fuentes grandes y scroll de región)

Rama: `v2.1.0-alpha.16/JWPLC-Sim/feature/ui-f0-resto`, que sale de `v2.1.0-alpha.16/JWPLC-Sim/feature/ui-tendencia` (PR #201) porque usa la API 0.2.0 de los widgets.
Diseño de referencia: `DISENO_RUNTIME_ESTILO_LOGO8.md` §7.2, puntos 4, 9, 10 y 11 (carpeta del proyecto). Todo el trabajo se hizo de una pasada la noche del 01/11 para que el propietario lo revise en la mañana.

Revisión del propietario (01/11, misma noche):

- **Vinculación:** debe conservar la propuesta de la maqueta: código que expira, barra de vigencia y confirmación física con OK. El ejemplo `Widgets_QR` se rehízo así.
- **Registro de eventos y barra:** ya existen en la línea del HMI Designer (`…/feature/hmi-designer`, commit `1007dc6`). El registro está en `JWPLC_UI_EventLog`, con `JWPLC_Display.logEvent()`, FRAM en 0x1E00, SD y plantilla en el Designer; la barra es el campo BAR, con `JWPLC_UIBarField` y `JWPLC_Display.setBar()`. Se retiraron `JWPLC_UIBar`, `JWPLC_UILog` y sus ejemplos, que los duplicaban. El Runtime usará los del Designer. El color por umbral de la barra ya está aprobado para el BAR del Designer en esta alpha (`A15_INDUSTRIAL_ARQUITECTURA_PLAN_20261018.md`, G.9: «umbrales de color en VALUE y BAR») y todavía no está implementado.
- **Vinculación, ajuste:** el recuadro de mensajes se recortaba arriba porque lo pisaba el texto de vigencia; se subieron el código, la barra y el texto. El QR lleva un enlace `http://` completo para que la cámara del celular abra la página del equipo sola.

```text
A16_UI2_TECLADO=PASS_SIM            [ejecutado] Widgets_Teclado en JWPLC Sim: borra, escribe «Bomba 2», clave «a1» oculta, resumen
A16_UI3_QR=PASS_SIM                 [ejecutado] 396 códigos verificados fuera del equipo; vinculación con expiración y OK físico simulada; el QR del simulador se lee con un lector de QR
A16_TFT2_FUENTES_GRANDES=PASS_SIM   [ejecutado] Display_Fuentes_Grandes en JWPLC Sim (7 segmentos 48 px y 26 px)
A16_TFT3_SCROLL_REGION=PASS_SIM     [ejecutado] solo la primitiva JWPLC_TFTSprite.scroll(); el registro de eventos es el del Designer
A16_UI4_BARRA=DEFERRED              la barra es el campo BAR del HMI Designer (JWPLC_Display.setBar)
EJEMPLOS_COMPILA_ESP32=PENDING      [no verificado] desde la nube no hay acceso a las descargas de Arduino
FISICO=PENDING                      ver checklist físico
```

## Qué se agregó

`JWPLC_TFT` 0.5.0 (modo fuente):

| Elemento | Uso |
|---|---|
| `JWPLC_FONT_26` | Font 4 de TFT_eSPI (26 px, misma familia que `JWPLC_FONT_16`) convertida a GFXfont: valores grandes y títulos. ~2,8 KB |
| `JWPLC_FONT_7SEG_48` | Font 7 de TFT_eSPI (7 segmentos, 48 px), solo « -.0123456789:»: reloj y valores de proceso. ~2 KB |
| `sprite.scroll(dx, dy, color)` | Desplaza el contenido de un `JWPLC_TFTSprite` y rellena el hueco: base del scroll de región (disponible para el registro de eventos del Designer si se quiere animar la entrada de líneas) |
| `DATUM_TOP_LEFT` … | Constantes sin «::» (regla de API solo con «.») |

Las fuentes solo se enlazan si el sketch las usa. Las dos se generaron desde los archivos RLE de TFT_eSPI 2.5.43 (`Fonts/Font32rle.c` y `Fonts/Font7srle.c`) con un conversor propio. Esas fuentes ya venían en la copia de TFT_eSPI, así que no se agregan dependencias.

`JWPLC_UI_Widgets` 0.3.0:

| Clase | Uso | Teclas |
|---|---|---|
| `JWPLC_UIKeyboard` | Teclado en pantalla: campo con título, 4 filas de 10 teclas (minúsculas, mayúsculas y símbolos) y fila de funciones (ABC/abc, #+=, espacio, borrar, OK). Modo contraseña con «*» | ◄ ► ▲ ▼ OK ESC |
| `JWPLC_UIQR` | Código QR propio (modo byte, versiones 1 a 10, niveles L/M/Q/H). `setText()` lo calcula una vez y `draw(x, y, escala)` lo dibuja con su margen blanco | — |

El teclado devuelve las mismas constantes que el editor numérico (`UI_EDIT_CHANGED`, `UI_EDIT_ACCEPTED`, `UI_EDIT_CANCELLED`).

## Cómo se validó el QR

El codificador está escrito en la librería: no usa código de terceros ni red. Se compiló en el PC con un programa de prueba y se generaron 396 códigos: 150 textos al azar de 1 a 280 caracteres, más los dos del ejemplo, en los 4 niveles. Un verificador independiente en Python comprobó, para cada código:

- **Patrones fijos:** buscadores, alineación, temporización, formato y versión idénticos a los de la librería de referencia `segno` con la misma versión y máscara.
- **Corrección de errores:** el Reed-Solomon de cada bloque coincide con uno calculado aparte.
- **Datos:** al leer los datos se recupera el texto exacto.

Resultado: 396/396 correctos. Además, las capturas del simulador de `Widgets_QR` se leyeron con el lector de QR de OpenCV y devolvieron el texto exacto: `http://192.168.1.50/v?c=` más el código de 6 dígitos de cada momento.

Ese lector falla en algunos códigos grandes generados al azar, pero también falla con los de `segno`. Por eso la prueba de corrección es la verificación estructural de arriba.

Límites: modo byte solamente (sin modos numérico ni alfanumérico) y versión máxima 10. Eso alcanza para 213 caracteres en nivel M, de sobra para un enlace de vinculación.

## Ejemplos

| Ejemplo | Librería | Qué muestra |
|---|---|---|
| `Widgets_Teclado` | `JWPLC_UI_Widgets` | Pide nombre y clave; imprime `NOMBRE=…` y `CLAVE=…` por Serial |
| `Widgets_QR` | `JWPLC_UI_Widgets` | Vinculación como en la maqueta: QR con la dirección y un código de 6 dígitos que expira a los 60 s (barra y «Expira en N s»), rol Operador/Admin con ◄ ►, y confirmación física: cuando el celular pide acceso (simulado con ▼), OK acepta y ESC rechaza y renueva el código |
| `Display_Fuentes_Grandes` | `JWPLC_Display` | Reloj en 7 segmentos, valor en 26 px, comparación con 16 px y GLCD |

Todos usan la API solo con «.» (regla 9 del `CLAUDE.md`). Las pruebas están en `JW-Control/jwplc-sim`, rama `feature/alpha16-api-tft-widgets`, con capturas de referencia.

## Pendiente

- **Teclado:** no hay sugerencias de palabras (§7.2 punto 9) ni acentos. La GLCD tiene á é í ó ú ñ en otra codificación; se agregan si el propietario lo pide.
- **Scroll por hardware:** el scroll por hardware del ST7789 no se usa. Con la pantalla girada, el ST7789 solo desplaza en horizontal y toda la franja, así que el scroll de región va por sprite.
- **Al cierre de la alpha:** regenerar `libJWPLC_TFT.a` y hacer el chequeo ABI.

## Checklist físico

1. **`Widgets_Teclado`:**
   - La marca se mueve sin parpadeo.
   - ABC y #+= cambian todas las teclas de una vez.
   - El campo muestra el texto y el cursor «_»; en la clave muestra «*».
2. **`Widgets_QR`:**
   - El celular lee el QR (la dirección `http://192.168.1.50/v?c=…`).
   - La barra de vigencia baja sin parpadeo y el código se renueva a los 60 s.
   - ▼ simula el pedido: OK muestra «Celular vinculado» y ESC muestra «Solicitud rechazada» con código nuevo.
3. **`Display_Fuentes_Grandes`:**
   - El reloj cambia cada segundo sin parpadeo visible.
   - Los 7 segmentos y la fuente de 26 px se ven nítidos.
