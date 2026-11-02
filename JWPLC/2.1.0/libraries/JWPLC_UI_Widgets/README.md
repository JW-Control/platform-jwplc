# JWPLC_UI_Widgets

Widgets de pantalla para la TFT del JWPLC Basic, con dirty refresh (solo se redibuja lo que cambió) y manejo de
la botonera. Son la base de las pantallas del Runtime. Versión **0.5.0** (package 2.1.0-alpha.16).

| Widget | Para qué |
|---|---|
| `JWPLC_UIList` | lista con scroll (▲ ▼) y dos columnas por fila (`JWPLC_UIRow`: `left()` / `right()`) |
| `JWPLC_UIDigitEditor` | edición de un número dígito a dígito (◄ ► dígito, ▲ ▼ valor), con rango |
| `JWPLC_UIDialog` | confirmación Sí/No o aviso con OK |
| `JWPLC_UIToast` | aviso temporal en el pie |
| `JWPLC_UITrend` | tendencia de hasta 3 canales con umbrales y escala |
| `JWPLC_UIKeyboard` | teclado en pantalla para escribir textos con la botonera |
| `JWPLC_UIQR` | código QR (p. ej. un enlace `http://` al equipo) |

Colores con `JWPLC_UITheme` (`JWPLC_UI_THEME_DARK` por defecto). Se dibujan dentro de los callbacks USER de
`JWPLC_Display`, que ya corren con el bus SPI tomado.

Ejemplos: `Widgets_Parametros` (lista + editor + diálogo + aviso), `Widgets_Tendencia`, `Widgets_Teclado` y
`Widgets_QR` (vinculación con código que expira y confirmación con OK).

Se distribuye en código fuente (sin `.a`). Detalle: `docs/v2.1.0-alpha.16/A16_UI1_WIDGETS_RUNTIME_20261101.md` y
`A16_F0_GATES_RESTANTES_20261101.md`.
