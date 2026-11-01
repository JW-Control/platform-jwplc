# A16 UI1 — `JWPLC_UI_Widgets` 0.1.0 (widgets del Runtime)

Rama: `v2.1.0-alpha.16/JWPLC-Sim/feature/ui-widgets-runtime`, que sale de `v2.1.0-alpha.16/JWPLC-Sim/feature/api-tft-graficos` (PR #199): necesita la API 0.4.0 de `JWPLC_TFT`.
Diseño de referencia: `DISENO_RUNTIME_ESTILO_LOGO8.md` §7.2 (puntos 5, 6, 8 y 12), en la carpeta del proyecto.

```text
A16_UI1_API=PASS                 [ejecutado] compila en JWPLC Sim (native) contra el package de esta rama
A16_UI1_SIM_EJEMPLO=PASS         [ejecutado] Widgets_Parametros en JWPLC Sim: lista, scroll, editor, diálogo, aviso y vuelta a IDLE (capturas)
A16_UI1_SIM_REGRESSION=IN_PROGRESS  [ejecutado] jwplc-sim rama feature/alpha16-api-tft-widgets (feature/alpha16-api-tft-widgets): parity PASS y pruebas de los 2 ejemplos nuevos PASS. Pendiente: fusionarla y, al cierre, fijar sim_ref
A16_UI1_EJEMPLO_COMPILA=PENDING  [no verificado] Arduino IDE con la placa local
A16_UI1_FISICO=PENDING           ver checklist físico
```

## Por qué una librería aparte

Los widgets viven en `JWPLC_UI_Widgets` y no dentro de `JWPLC_UI` (que está en `JWPLC_Display`) por dos motivos:

- `JWPLC_Display` sigue precompilada y André trabaja en `JWPLC_UI` en `…/feature/hmi-designer`; tocarla ahora obligaría a ponerla en modo fuente y chocaría con su rama.
- Los widgets solo necesitan `JWPLC_TFT` y la botonera, así que el Runtime, el modo USER y, más adelante, el codegen del Designer pueden usarlos sin depender del Display.

Está en modo fuente desde el inicio (sin `precompiled`).

## Qué incluye

| Clase | Uso | Teclas que consume |
|---|---|---|
| `JWPLC_UITheme` / `JWPLC_UI_THEME_DARK` | Colores comunes (fondo, superficie, borde, texto, acento, ok, aviso, error) | — |
| `JWPLC_UIList` | Lista con scroll; filas entregadas por callback (texto izquierdo y valor a la derecha), sin copia en RAM | ▲ ▼ con repetición |
| `JWPLC_UIDigitEditor` | Editor por dígito como el del LOGO!: entero escalado con decimales, unidad, rango y signo; al mantener ▲ ▼ acelera | ◄ ► ▲ ▼ OK ESC |
| `JWPLC_UIDialog` | Diálogo modal Sí/No o Aceptar | ◄ ► OK ESC |
| `JWPLC_UIToast` | Aviso temporal en una franja reservada (pie) | — |

Fuentes (decisión del propietario, 01/11): títulos, botones y dígitos en GLCD 5×7 ×2, la de la IDLE; filas, mensajes y avisos en `JWPLC_FONT_16` (Font 2 de TFT_eSPI convertida a GFXfont, trazo de 1 px, ~0,7 KB, nueva en `JWPLC_TFT`), elegida frente a GLCD ×2 y Spleen 8×16 tras una comparación en el simulador. Barra de título con el gris de la IDLE (0x39E7). Todos usan dirty refresh: la lista redibuja solo las dos filas que cambian de selección (o la vista al hacer scroll), el editor solo los dígitos que cambian y el diálogo solo sus botones. Se llaman desde los callbacks USER, que ya corren dentro del batch de `JWPLC_Display`, y dejan `JWPLC_FONT_GLCD` activa al terminar.

## Ejemplo `Widgets_Parametros`

Una pantalla de parámetros al estilo LOGO! con 8 parámetros (temporizadores, contador, umbrales, temperatura con negativos). OK sale de IDLE; en la lista, OK edita y ESC vuelve a IDLE; en el editor, OK pide confirmación y ESC descarta. Al guardar imprime `GUARDADO <nombre> = <valor>` por Serial, lo que la prueba del simulador usa como verificación.

Como la pantalla usa ESC, el ejemplo pone `IDLE_RETURN_DISABLED` y llama `JWPLC_Display.goIdle()` desde `loop()`, fuera del batch.

Prueba en JWPLC Sim (01/11), estímulo: OK, ▼ ▼, OK, ▲, ◄, ▲, OK, OK (Sí), ▼ ×5, ESC:

- C1 Preset pasa de 250 a 261 pzs y la consola muestra `GUARDADO C1 Preset = 261 pzs`.
- La lista hace scroll hasta «Ciclo PWM» y la barra lateral se mueve.
- ESC vuelve a IDLE.

## JWPLC Sim

La prueba del ejemplo, la de `Display_Graficos_Sprite` (PR #199) y la actualización de paridad (`JWPLC_UI_Widgets` nueva; TFT_eSPI sale del autoload) están en `JW-Control/jwplc-sim`, rama `feature/alpha16-api-tft-widgets`, con `package.lock` en esta rama. El CI de platform solo corre `SIM_REGRESSION` en los PR hacia `release/v2.1.x`, así que `sim_ref` (`ci-jwplc-package-smoke.yml` y `release-jwplc-package.yml`, hoy `19a1a40d`) se sube al cierre de la alpha, al commit de jwplc-sim que quede fusionado.

## Pendiente (siguientes entregas de F0')

Tendencia (buffer circular, varios canales), teclado en pantalla, QR, fuente grande y scroll de región. El tema se enlazará con el HMI Designer cuando se acuerde con la línea de André.

## Checklist físico

1. Abrir `Widgets_Parametros`, pulsar OK: la lista aparece sin barrido negro.
2. ▲ ▼ y mantenerlas: la selección se mueve fila a fila y la lista hace scroll sin parpadeo.
3. OK en un parámetro: editor con el dígito de las unidades marcado; ◄ ► cambian de dígito, ▲ ▼ cambian el valor con acarreo y, al mantener, aceleran.
4. OK → diálogo; Sí guarda y muestra «Guardado»; ESC en el editor muestra «Edición descartada».
5. ESC en la lista vuelve a IDLE; al volver a entrar, los valores guardados se mantienen.
