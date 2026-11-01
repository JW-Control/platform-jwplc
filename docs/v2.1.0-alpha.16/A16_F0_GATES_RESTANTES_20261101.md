# A16 F0' — gates restantes de la base gráfica (teclado, QR, fuentes grandes, scroll de región y barra)

Rama: `v2.1.0-alpha.16/JWPLC-Sim/feature/ui-f0-resto`, que sale de `v2.1.0-alpha.16/JWPLC-Sim/feature/ui-tendencia` (PR #201) porque usa la API 0.2.0 de los widgets.
Diseño de referencia: `DISENO_RUNTIME_ESTILO_LOGO8.md` §7.2, puntos 4, 9, 10 y 11 (carpeta del proyecto), más la barra estilo LOGO! que pidió el propietario el 01/11.
Todo el trabajo se hizo de una pasada la noche del 01/11 para que el propietario lo revise en la mañana.

```text
A16_UI2_TECLADO=PASS_SIM            [ejecutado] Widgets_Teclado en JWPLC Sim: borra, escribe «Bomba 2», clave «a1» oculta, resumen
A16_UI3_QR=PASS_SIM                 [ejecutado] 396 códigos verificados fuera del equipo; las dos capturas del simulador se leen con un lector de QR
A16_TFT2_FUENTES_GRANDES=PASS_SIM   [ejecutado] Display_Fuentes_Grandes en JWPLC Sim (7 segmentos 48 px y 26 px)
A16_TFT3_SCROLL_REGION=PASS_SIM     [ejecutado] Widgets_Registro en JWPLC Sim: las líneas suben con sprite.scroll()
A16_UI4_BARRA=PASS_SIM              [ejecutado] Widgets_Barras en JWPLC Sim: horizontales, vertical y cambio de color al pasar el umbral
EJEMPLOS_COMPILA_ESP32=PENDING      [no verificado] desde la nube no hay acceso a las descargas de Arduino
FISICO=PENDING                      ver checklist físico
```

## Qué se agregó

`JWPLC_TFT` 0.5.0 (modo fuente):

| Elemento | Uso |
|---|---|
| `JWPLC_FONT_26` | Font 4 de TFT_eSPI (26 px, misma familia que `JWPLC_FONT_16`) convertida a GFXfont: valores grandes y títulos. ~2,8 KB |
| `JWPLC_FONT_7SEG_48` | Font 7 de TFT_eSPI (7 segmentos, 48 px), solo « -.0123456789:»: reloj y valores de proceso. ~2 KB |
| `sprite.scroll(dx, dy, color)` | Desplaza el contenido de un `JWPLC_TFTSprite` y rellena el hueco: base del scroll de región |
| `DATUM_TOP_LEFT` … | Constantes sin «::» (regla de API solo con «.») |

Las fuentes solo se enlazan si el sketch las usa. Las dos se generaron desde los archivos RLE de TFT_eSPI 2.5.43 (`Fonts/Font32rle.c` y `Fonts/Font7srle.c`) con un conversor propio. Esas fuentes ya venían en la copia de TFT_eSPI, así que no se agregan dependencias.

`JWPLC_UI_Widgets` 0.3.0:

| Clase | Uso | Teclas |
|---|---|---|
| `JWPLC_UIKeyboard` | Teclado en pantalla: campo con título, 4 filas de 10 teclas (minúsculas, mayúsculas y símbolos) y fila de funciones (ABC/abc, #+=, espacio, borrar, OK). Modo contraseña con «*» | ◄ ► ▲ ▼ OK ESC |
| `JWPLC_UIQR` | Código QR propio (modo byte, versiones 1 a 10, niveles L/M/Q/H). `setText()` lo calcula una vez y `draw(x, y, escala)` lo dibuja con su margen blanco | — |
| `JWPLC_UIBar` | Barra de valor como en los textos de aviso del LOGO!: horizontal o vertical, mín./máx., umbral que cambia el color y marca del umbral. Solo redibuja el tramo que cambió | — |
| `JWPLC_UILog` | Registro de eventos con scroll de región: sprite de 4 bits (~19 KB para 316 × 119) y hasta 15 colores de texto | — |

El teclado devuelve las mismas constantes que el editor numérico (`UI_EDIT_CHANGED`, `UI_EDIT_ACCEPTED`, `UI_EDIT_CANCELLED`).

## Cómo se validó el QR

El codificador está escrito en la librería: no usa código de terceros ni red. Se compiló en el PC con un programa de prueba y se generaron 396 códigos: 150 textos al azar de 1 a 280 caracteres, más los dos del ejemplo, en los 4 niveles. Un verificador independiente en Python comprobó, para cada código:

- **Patrones fijos:** buscadores, alineación, temporización, formato y versión idénticos a los de la librería de referencia `segno` con la misma versión y máscara.
- **Corrección de errores:** el Reed-Solomon de cada bloque coincide con uno calculado aparte.
- **Datos:** al leer los datos se recupera el texto exacto.

Resultado: 396/396 correctos. Además, las dos capturas del simulador se leyeron con el lector de QR de OpenCV y devolvieron el texto exacto (`jwplc://vincular?id=JWPLC-0001&c=482913` y la red Wi-Fi).

Ese lector falla en algunos códigos grandes generados al azar, pero también falla con los de `segno`. Por eso la prueba de corrección es la verificación estructural de arriba.

Límites: modo byte solamente (sin modos numérico ni alfanumérico) y versión máxima 10. Eso alcanza para 213 caracteres en nivel M, de sobra para un enlace de vinculación.

## Ejemplos

| Ejemplo | Librería | Qué muestra |
|---|---|---|
| `Widgets_Teclado` | `JWPLC_UI_Widgets` | Pide nombre y clave; imprime `NOMBRE=…` y `CLAVE=…` por Serial |
| `Widgets_QR` | `JWPLC_UI_Widgets` | QR de vinculación (enlace `jwplc://` con equipo y código) y QR de la red Wi-Fi; ◄ ► cambian |
| `Widgets_Barras` | `JWPLC_UI_Widgets` | Nivel, presión y temperatura simulados, con valores en `JWPLC_FONT_26` |
| `Widgets_Registro` | `JWPLC_UI_Widgets` | Un evento simulado cada 1,5 s; OK borra el registro |
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
2. **`Widgets_QR`:** el celular lee los dos códigos. La cámara del sistema abre el texto `jwplc://…`, y el de Wi-Fi ofrece conectarse a la red «JWPLC-0001».
3. **`Widgets_Barras`:**
   - Las barras se mueven sin parpadeo y sin redibujar el marco.
   - Al pasar el umbral cambian de color.
   - Los números grandes no parpadean.
4. **`Widgets_Registro`:**
   - Cada 1,5 s entra una línea por abajo y, con el área llena, el resto sube sin parpadeo.
   - OK borra el registro.
   - Al salir y volver, el registro se conserva.
5. **`Display_Fuentes_Grandes`:**
   - El reloj cambia cada segundo sin parpadeo visible.
   - Los 7 segmentos y la fuente de 26 px se ven nítidos.
