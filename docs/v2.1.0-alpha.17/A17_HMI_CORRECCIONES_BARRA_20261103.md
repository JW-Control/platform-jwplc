# A17 · Correcciones de la barra del HMI Designer

Fecha: 03/11/2026. Rama: `v2.1.0-alpha.17/JWPLC-Sim/feature/hmi-designer`, sobre `b8841ed4`.

```text
A17_HMI_BAR_HEADER=PASS
A17_HMI_BAR_PARIDAD_HOST=PASS
A17_HMI_BAR_ESP32_BUILD=PENDING
A17_HMI_BAR_PLACA=PENDING
```

## Qué se corrigió

| Tema | Antes | Ahora |
|---|---|---|
| Header con BAR | Cada BAR salía duplicado, con texto suelto (`0xFFFF)) - Radio: 0`), y no compilaba | `replaceHelperCall` busca el `(` desde la llamada, no desde el comentario |
| Negro | El negro final salía como `0x07E0`; el negro de inicio o sólido se perdía al tocar otro control | 0 es un color válido. En degradado, el negro final se emite como `0x0001`, porque `colorEnd = 0` significa sólido |
| Controles ocultos | `updateBarFromControls` releía colores del panel oculto y pisaba el modelo | Ya no lee colores de ese panel |
| Degradado circular | El Designer lo ofrecía y la TFT lo dibujaba sólido | Se quita del Designer: `circleColorMode` se normaliza a `SOLID` y se oculta la tarjeta |
| ⇄ y paletas rápidas | Se perdieron en `b8841ed4` | Vuelven en una fila compacta, visible sólo con Degradado |
| Grosor horizontal | El lienzo lo respetaba y el firmware dibujaba 12 px | `JWPLC_UIBarConfig.thickness` |
| Barra vertical | El firmware restaba los márgenes: 14 px salían como 8 | `thickness` y `length`; el firmware calcula la caja del campo |
| API simple con `height` | La barra pasaba de 12 px a `height - 6` | Vuelve a 12 px, salvo que se pida `thickness` |
| Borde | El nivel se calculaba sobre el ancho exterior y se llenaba antes del 100 % | Firmware y lienzo reparten el nivel en el interior del borde |
| Esquinas | El firmware rellenaba en rectángulo | El relleno se recorta línea a línea con la misma fórmula del lienzo |
| Bloques | El firmware usaba paso 4 en barras cortas y otro conteo | Paso 5 y `(largo + 1) / 5` bloques, como el lienzo |
| Barra vertical con texto | Difería en alineación y la unidad salía a la derecha en firmware | Paridad total: la barra y la etiqueta se centran respecto al ancho máximo, y la unidad se ubica al pie |
| Panel legacy | `barAppLegacyCard` permanecía oculto con listeners muertos | Retirado por completo de `designer-bar.js` |
| Pruebas | Los stubs no tenían `setFont` ni rectángulos redondeados | Stubs ampliados y `tests/bar-host-sim.cjs` nuevo |

También se quitó el código muerto del panel de degradado y `SCREEN_H` pasó de 240 a 170.

## API

`JWPLC_UIBarConfig` suma dos argumentos al final, con valor 0 por omisión:
`thickness` (grosor en px) y `length` (largo de la barra vertical en px).
Los sketches existentes compilan igual.

## Validación

`[ejecutado]` con Chrome y g++ de Strawberry en Windows, sobre una copia con finales de línea LF:

- 17 de 21 pruebas pasan, incluidas `codegen-golden` y `bar-host-sim` (18 verificaciones contra el `JWPLC_UI.cpp` real).
- Las 4 que fallan también fallan en la base `8019197e`. Ver abajo.

| Prueba | Causa |
|---|---|
| `value-format-parity` | `[inferido]` Las cadenas impresas son iguales; parece el `\r` de la salida en Windows |
| `inspector-smoke` | Sólo «el BOOL nuevo se dibuja encima del TEXT anterior». `[no verificado]` |
| `agent-cli`, `codegen-smoke` | `[inferido]` Sólo falla la comprobación con `nm`: el `nm` de Strawberry no lee los `.a` de ESP32. Hay que repetirla con el `nm` de xtensa |

## Pendiente

- `[no verificado]` Compilación real para ESP32 y prueba en placa: grosor, esquinas, borde y bloques.
- `[no verificado]` Tiempo de scan con degradado o esquinas: se pinta línea a línea dentro de un `beginBatch`.
- `JWPLC_Display` está en modo fuente; su `.a` debe regenerarse antes del cierre (`CLAUDE.md`, regla 4).
- Medidor circular con degradado: requiere soporte en firmware.
