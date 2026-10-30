<!-- Generado por `node tools/jwplc-hmi-designer/cli/jwhmi.cjs schema` desde cli/lib/spec.cjs. No editar a mano. -->
# Referencia del formato .jwhmi

Proyecto del JWPLC HMI Designer (`JWPLC_HMI_PROJECT` v1) para la TFT ST7789 de 320×170 px (RGB565) del JWPLC Basic.
Cómo usarlo paso a paso: [GUIA_AGENTES.md](GUIA_AGENTES.md). Esquema JSON: [`schema/jwhmi.schema.json`](../schema/jwhmi.schema.json).

Las propiedades internas que el Designer guarda pero que no hace falta escribir no aparecen aquí; la CLI las completa.

## Límites

- Pantalla: 320×170 px, origen arriba a la izquierda.
- Páginas: 1 a 16. Objetos: 32 (contando la NAV_BAR). Capas PIXEL: 16.
- MENU: uno por página, 4 en el proyecto, 64 nodos y 3 niveles por MENU.

## Colores

Número RGB565 (`63488`), texto hexadecimal (`"0xF800"`), `"#RRGGBB"` / `"#RGB"` (se convierte a RGB565) o un nombre:

`BLACK` = `0x0000` · `WHITE` = `0xFFFF` · `RED` = `0xF800` · `GREEN` = `0x07E0` · `BLUE` = `0x001F` · `CYAN` = `0x07FF` · `YELLOW` = `0xFFE0` · `ORANGE` = `0xFD20` · `NAVY` = `0x000F` · `DARKGREEN` = `0x03E0` · `DARKCYAN` = `0x03EF` · `MAROON` = `0x7800` · `PURPLE` = `0x780F` · `OLIVE` = `0x7BE0` · `LIGHTGREY` = `0xD69A` · `DARKGREY` = `0x7BEF` · `GREY` = `0x8410` · `MAGENTA` = `0xF81F` · `PINK` = `0xFE19` · `GREENYELLOW` = `0xB7E0` · `BROWN` = `0x9A60` · `GOLD` = `0xFEA0`

## Proyecto

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `format` | texto | `"JWPLC_HMI_PROJECT"` | Siempre JWPLC_HMI_PROJECT. |
| `version` | entero | `1` | Siempre 1. |
| `projectName` | texto | `"HMI_ST7789"` | Nombre del proyecto (48 caracteres). |
| `display` | objeto |  | Pantalla IDLE y arranque (ver «display»). |
| `pages` **(obligatoria)** | arreglo |  | Páginas (1 a 16). |
| `fields` | arreglo | `[]` | Objetos (máximo 32 contando la NAV_BAR). |
| `pixelMaps` | arreglo | `[]` | Capas de píxeles (máximo 16). |

## Páginas (`pages[]`)

Cada página puede ser un texto (`"Principal"`) o un objeto:

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `id` | entero 0..15 |  | Índice 0..N-1 (se asigna por posición si falta). |
| `name` **(obligatoria)** | texto |  | Nombre de la página (también da el símbolo C++ PAGE_<NOMBRE>). |
| `shortName` | texto |  | Nombre corto para las pestañas de la barra de navegación. |
| `color` | color |  | Color de la pestaña de la página. |
| `fields` | arreglo |  | Atajo: objetos de esta página (la normalización los pasa a `fields` con page = id). |

## Pantalla IDLE y arranque (`display`)

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `start` | `"IDLE"` · `"HMI"` | `"IDLE"` | Al encender: pantalla de inicio JWPLC (IDLE) o directo al HMI. |
| `wake` | `"ANY"` · `"BUTTON"` · `"NONE"` | `"ANY"` | Cómo se entra al HMI desde IDLE: cualquier tecla, una tecla (wakeButton) o nunca. |
| `wakeButton` | `"BTN_LEFT"` · `"BTN_UP"` · `"BTN_RIGHT"` · `"BTN_ESC"` · `"BTN_OK"` · `"BTN_DOWN"` | `"BTN_OK"` | Tecla de entrada si wake = BUTTON. |
| `back` | `"ESC"` · `"TIMEOUT"` · `"NONE"` | `"ESC"` | Cómo se vuelve a IDLE: ESC, por inactividad (timeoutS) o nunca. |
| `timeoutS` | entero 1..3600 | `15` | Segundos sin teclas para volver a IDLE (back = TIMEOUT). |

## Objetos (`fields[]`)

Cada objeto tiene `type` y las propiedades de su tipo. Las coordenadas son de la esquina superior izquierda.

### `TEXT`

Texto dinámico. Declara `char variable[2·capacity+1]`; el sketch lo llena y el runtime lo redibuja si cambia.

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` **(obligatoria)** | texto |  | Tipo de objeto. |
| `name` | texto |  | Nombre visible en la lista de objetos del Designer (no llega al C++). |
| `id` | identificador C++ |  | Identificador C++ del objeto (único en el proyecto). Si falta se genera FIELD_<TIPO>_<n>. |
| `page` | entero 0..15 | `0` | Página (id) donde se dibuja. |
| `layerOrder` | entero 0..9999 |  | Orden de capa: menor = más al frente. Si falta en todos, el orden del arreglo manda (el último queda encima). |
| `x` **(obligatoria)** | entero 0..319 |  | Esquina superior izquierda, X (px). |
| `y` **(obligatoria)** | entero 0..169 |  | Esquina superior izquierda, Y (px). |
| `variable` | identificador C++ |  | Variable C++ que el header declara y tu sketch asigna. Si falta se genera. |
| `label` | texto | `""` | Etiqueta. Vacía = sin etiqueta. |
| `unit` | texto | `""` | Unidad tras el valor. Vacía = sin unidad. |
| `labelSize` | entero 1..4 | `1` | Escala de la etiqueta (celda GLCD 6×8 px × escala). |
| `valueSize` | entero 1..4 | `2` | Escala del valor. |
| `frame` | bool | `false` | Marco alrededor del campo. |
| `layout` | `"INLINE"` · `"STACKED"` | `"INLINE"` | INLINE: etiqueta y valor en una fila. STACKED: etiqueta arriba, valor abajo. |
| `align` | `"LEFT"` · `"CENTER"` · `"RIGHT"` | `"LEFT"` | Alineación del valor dentro del campo. |
| `labelColor` | color | `0xFFFF` | Color de la etiqueta. |
| `valueColor` | color | `0x07FF` | Color del valor. |
| `backgroundColor` | color | `0x0000` | Fondo del campo. |
| `frameColor` | color | `0xFFFF` | Color del marco. |
| `preview` | texto |  | Texto de muestra en el lienzo del Designer (no llega al C++). |
| `capacity` | entero 1..39 | `12` | Caracteres reservados en pantalla (el ancho del campo sale de aquí). |

### `VALUE`

Número con formato (o duración, o fecha/hora del RTC). Declara `float` (o `int32_t`/`uint32_t` en duraciones y contadores con miles).

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` **(obligatoria)** | texto |  | Tipo de objeto. |
| `name` | texto |  | Nombre visible en la lista de objetos del Designer (no llega al C++). |
| `id` | identificador C++ |  | Identificador C++ del objeto (único en el proyecto). Si falta se genera FIELD_<TIPO>_<n>. |
| `page` | entero 0..15 | `0` | Página (id) donde se dibuja. |
| `layerOrder` | entero 0..9999 |  | Orden de capa: menor = más al frente. Si falta en todos, el orden del arreglo manda (el último queda encima). |
| `x` **(obligatoria)** | entero 0..319 |  | Esquina superior izquierda, X (px). |
| `y` **(obligatoria)** | entero 0..169 |  | Esquina superior izquierda, Y (px). |
| `variable` | identificador C++ |  | Variable C++ que el header declara y tu sketch asigna. Si falta se genera. |
| `label` | texto | `""` | Etiqueta. Vacía = sin etiqueta. |
| `unit` | texto | `""` | Unidad tras el valor. Vacía = sin unidad. |
| `labelSize` | entero 1..4 | `1` | Escala de la etiqueta (celda GLCD 6×8 px × escala). |
| `valueSize` | entero 1..4 | `2` | Escala del valor. |
| `frame` | bool | `false` | Marco alrededor del campo. |
| `layout` | `"INLINE"` · `"STACKED"` | `"INLINE"` | INLINE: etiqueta y valor en una fila. STACKED: etiqueta arriba, valor abajo. |
| `align` | `"LEFT"` · `"CENTER"` · `"RIGHT"` | `"RIGHT"` | Alineación del valor dentro del campo. |
| `labelColor` | color | `0xFFFF` | Color de la etiqueta. |
| `valueColor` | color | `0x07FF` | Color del valor. |
| `backgroundColor` | color | `0x0000` | Fondo del campo. |
| `frameColor` | color | `0xFFFF` | Color del marco. |
| `preview` | texto |  | Texto de muestra en el lienzo del Designer (no llega al C++). |
| `valueKind` | `"NUMBER"` · `"DURATION"` · `"DATETIME"` | `"NUMBER"` | NUMBER: número. DURATION: segundos o ms mostrados como tiempo. DATETIME: fecha/hora del RTC, sin variable ni setter. |
| `integerDigits` | entero 1..9 | `3` | Dígitos enteros reservados. |
| `decimalDigits` | entero 0..6 | `1` | Decimales (NUMBER). |
| `signedValue` | bool | `false` | Reserva el signo menos. |
| `leadingZeros` | bool | `false` | Rellena con ceros a la izquierda. |
| `thousandsSep` | `""` · `","` · `"."` · `" "` | `""` | Separador de miles (NUMBER). Sin decimales declara un entero de 32 bits. |
| `durationPattern` | `"SS"` · `"MM_SS"` · `"HH_MM_SS"` · `"COMPACT"` | `"MM_SS"` | Formato de la duración (DURATION). |
| `durationUnit` | `"S"` · `"MS"` | `"S"` | Unidad de la variable de duración (DURATION). |
| `dateTimePattern` | `"HH_MM"` · `"HH_MM_SS"` · `"DD_MM"` · `"DD_MM_YYYY"` · `"DD_MM_YYYY_HH_MM"` | `"HH_MM"` | Formato de fecha/hora (DATETIME). |

### `BOOL`

Estado ON/OFF. Declara `bool`. Muestra un texto por estado o un piloto (círculo).

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` **(obligatoria)** | texto |  | Tipo de objeto. |
| `name` | texto |  | Nombre visible en la lista de objetos del Designer (no llega al C++). |
| `id` | identificador C++ |  | Identificador C++ del objeto (único en el proyecto). Si falta se genera FIELD_<TIPO>_<n>. |
| `page` | entero 0..15 | `0` | Página (id) donde se dibuja. |
| `layerOrder` | entero 0..9999 |  | Orden de capa: menor = más al frente. Si falta en todos, el orden del arreglo manda (el último queda encima). |
| `x` **(obligatoria)** | entero 0..319 |  | Esquina superior izquierda, X (px). |
| `y` **(obligatoria)** | entero 0..169 |  | Esquina superior izquierda, Y (px). |
| `variable` | identificador C++ |  | Variable C++ que el header declara y tu sketch asigna. Si falta se genera. |
| `label` | texto | `""` | Etiqueta. Vacía = sin etiqueta. |
| `unit` | texto | `""` | Unidad tras el valor. Vacía = sin unidad. |
| `labelSize` | entero 1..4 | `1` | Escala de la etiqueta (celda GLCD 6×8 px × escala). |
| `valueSize` | entero 1..4 | `2` | Escala del valor. |
| `frame` | bool | `false` | Marco alrededor del campo. |
| `layout` | `"INLINE"` · `"STACKED"` | `"INLINE"` | INLINE: etiqueta y valor en una fila. STACKED: etiqueta arriba, valor abajo. |
| `align` | `"LEFT"` · `"CENTER"` · `"RIGHT"` | `"CENTER"` | Alineación del valor dentro del campo. |
| `labelColor` | color | `0xFFFF` | Color de la etiqueta. |
| `valueColor` | color | `0x07FF` | Color del valor. |
| `backgroundColor` | color | `0x0000` | Fondo del campo. |
| `frameColor` | color | `0xFFFF` | Color del marco. |
| `preview` | texto |  | Texto de muestra en el lienzo del Designer (no llega al C++). |
| `falseText` | texto | `"OFF"` | Texto para false. |
| `trueText` | texto | `"ON"` | Texto para true. |
| `boolStyle` | `"TEXT"` · `"PILOT"` | `"TEXT"` | TEXT: texto por estado. PILOT: círculo de color (7 px × valueSize). |
| `stateColors` | bool | `false` | TEXT: colores distintos por estado (falseColor/trueColor y fondos). |
| `falseColor` | color |  | Color del texto o del piloto en false. |
| `trueColor` | color |  | Color del texto o del piloto en true. |
| `falseBackground` | color |  | Fondo del campo en false. |
| `trueBackground` | color |  | Fondo del campo en true. |
| `boolValue` | bool | `false` | Estado de muestra en el lienzo. |

### `BAR`

Barra de nivel horizontal (JWPLC_UIBarField). Declara `float`. El firmware dibuja sólo la barra lineal sólida.

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` **(obligatoria)** | texto |  | Tipo de objeto. |
| `name` | texto |  | Nombre visible en la lista de objetos del Designer (no llega al C++). |
| `id` | identificador C++ |  | Identificador C++ del objeto (único en el proyecto). Si falta se genera FIELD_<TIPO>_<n>. |
| `page` | entero 0..15 | `0` | Página (id) donde se dibuja. |
| `layerOrder` | entero 0..9999 |  | Orden de capa: menor = más al frente. Si falta en todos, el orden del arreglo manda (el último queda encima). |
| `x` **(obligatoria)** | entero 0..319 |  | Esquina superior izquierda, X (px). |
| `y` **(obligatoria)** | entero 0..169 |  | Esquina superior izquierda, Y (px). |
| `variable` | identificador C++ |  | Variable C++ que el header declara y tu sketch asigna. Si falta se genera. |
| `label` | texto | `""` | Etiqueta. Vacía = sin etiqueta. |
| `unit` | texto | `""` | Unidad tras el valor. Vacía = sin unidad. |
| `labelSize` | entero 1..4 | `1` | Escala de la etiqueta (celda GLCD 6×8 px × escala). |
| `valueSize` | entero 1..4 | `1` | Escala del valor. |
| `frame` | bool | `false` | Marco alrededor del campo. |
| `layout` | `"INLINE"` · `"STACKED"` | `"STACKED"` | INLINE: etiqueta y valor en una fila. STACKED: etiqueta arriba, valor abajo. |
| `align` | `"LEFT"` · `"CENTER"` · `"RIGHT"` | `"LEFT"` | Alineación del valor dentro del campo. |
| `labelColor` | color | `0xFFFF` | Color de la etiqueta. |
| `valueColor` | color | `0x07FF` | Color del relleno. |
| `backgroundColor` | color | `0x0000` | Fondo del campo. |
| `frameColor` | color | `0xFFFF` | Color del marco. |
| `preview` | texto |  | Texto de muestra en el lienzo del Designer (no llega al C++). |
| `showLabel` | bool | `true` | Muestra barLabel. |
| `barLabel` | texto | `"Nivel"` | Etiqueta de la barra. |
| `barUnit` | texto | `"%"` | Unidad tras el porcentaje. |
| `barMin` | número | `0` | Valor de la variable que dibuja la barra vacía. |
| `barMax` | número | `100` | Valor de la variable que dibuja la barra llena. |
| `barValue` | número | `50` | Valor de muestra en el lienzo. |
| `barWidth` | entero 20..320 | `110` | Ancho total del campo (px). |
| `trackColor` | color | `0x0000` | Fondo de la pista (parte vacía). |

### `ICON`

Icono con estado (motor, piloto, válvula…; JWPLC_UIIconField): una imagen por estado, sin animación. Declara `bool` (2 estados) o `uint8_t` (0..N-1, hasta 8). Catálogo: «Iconos del catálogo» o `jwhmi icons`.

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` **(obligatoria)** | texto |  | Tipo de objeto. |
| `name` | texto |  | Nombre visible en la lista de objetos del Designer (no llega al C++). |
| `id` | identificador C++ |  | Identificador C++ del objeto (único en el proyecto). Si falta se genera FIELD_<TIPO>_<n>. |
| `page` | entero 0..15 | `0` | Página (id) donde se dibuja. |
| `layerOrder` | entero 0..9999 |  | Orden de capa: menor = más al frente. Si falta en todos, el orden del arreglo manda (el último queda encima). |
| `x` **(obligatoria)** | entero 0..319 |  | Esquina superior izquierda, X (px). |
| `y` **(obligatoria)** | entero 0..169 |  | Esquina superior izquierda, Y (px). |
| `variable` | identificador C++ |  | Variable C++ que el header declara y tu sketch asigna. Si falta se genera. |
| `label` | texto | `""` | Etiqueta opcional (arriba con STACKED, a la izquierda con INLINE). Vacía = sin etiqueta. |
| `labelSize` | entero 1..4 | `1` | Escala de la etiqueta (celda GLCD 6×8 px × escala). |
| `frame` | bool | `false` | Marco alrededor del campo. |
| `layout` | `"INLINE"` · `"STACKED"` | `"STACKED"` | INLINE: etiqueta y valor en una fila. STACKED: etiqueta arriba, valor abajo. |
| `labelColor` | color | `0xFFFF` | Color de la etiqueta. |
| `backgroundColor` | color | `0x0000` | Fondo del campo; también el de los píxeles transparentes del icono. |
| `frameColor` | color | `0xFFFF` | Color del marco. |
| `iconId` **(obligatoria)** | texto |  | Icono del catálogo, `categoria/nombre` (p. ej. `motores/motor`). |
| `iconSize` | `24` · `32` · `48` · `64` | `32` | Lado del icono (px). Sólo los tamaños que tiene ese icono (64 en algunos). |
| `iconVarType` | `"bool"` · `"uint8"` | `"bool"` | Tipo de la variable: bool usa los estados 0 y 1; uint8 todos (0..N-1). Por defecto bool con 2 estados y uint8 con más. |
| `iconState` | entero 0..7 | `0` | Estado de muestra en el lienzo. |
| `transparentBackground` | bool | `false` | Sin fondo: no pinta el recuadro ni detrás de la etiqueta, y lo transparente en todos los estados deja ver la gráfica estática (RECT, RAW_TEXT…). Lo que cambia entre estados usa backgroundColor. |

#### Iconos del catálogo (`iconId`)

El estado `k` de la variable muestra la imagen `k` (bool: 0 = false, 1 = true). También: `jwhmi icons [categoría]`.

| `iconId` | Nombre | Tamaños (px) | Estados (0..N-1) |
|---|---|---|---|
| `motores/motor` | Motor | 24, 32, 48, 64 | Parado · En marcha · Falla |
| `motores/motor_dos_sentidos` | Motor dos sentidos | 24, 32, 48 | Parado · Adelante · Atrás |
| `bombas/bomba` | Bomba | 24, 32, 48, 64 | Parado · En marcha · Falla |
| `bombas/ventilador` | Ventilador | 24, 32, 48, 64 | Parado · En marcha · Falla |
| `bombas/compresor` | Compresor | 24, 32, 48 | Parado · En marcha · Falla |
| `pilotos/piloto` | Piloto (4 estados) | 24, 32, 48 | Apagado · Verde · Amarillo · Rojo |
| `pilotos/piloto_verde` | Piloto verde (bool) | 24, 32, 48 | Apagado · Encendido |
| `pilotos/piloto_rojo` | Piloto rojo (bool) | 24, 32, 48 | Apagado · Encendido |
| `pilotos/piloto_amarillo` | Piloto amarillo (bool) | 24, 32, 48 | Apagado · Encendido |
| `pilotos/piloto_azul` | Piloto azul (bool) | 24, 32, 48 | Apagado · Encendido |
| `pilotos/piloto_cuadrado` | Piloto cuadrado verde | 24, 32, 48 | Apagado · Encendido |
| `pilotos/piloto_cuadrado_rojo` | Piloto cuadrado rojo | 24, 32, 48 | Apagado · Encendido |
| `pilotos/piloto_bisel` | Piloto industrial biselado | 24, 32, 48 | Apagado · Encendido |
| `pilotos/baliza` | Torre de señalización | 24, 32, 48 | Apagada · Verde · Amarilla · Roja |
| `valvulas/valvula` | Válvula | 24, 32, 48, 64 | Cerrada · Abierta · Falla |
| `valvulas/valvula_motorizada` | Válvula motorizada | 24, 32, 48 | Cerrada · Abierta · Falla |
| `valvulas/valvula_solenoide` | Electroválvula | 24, 32, 48 | Cerrada · Abierta · Falla |
| `equipos/tanque` | Tanque (nivel) | 24, 32, 48, 64 | Vacío · Bajo · Medio · Lleno |
| `equipos/silo` | Silo / tolva | 24, 32, 48, 64 | Vacío · Con material · Falla |
| `equipos/resistencia` | Resistencia / calefactor | 24, 32, 48 | Apagada · Encendida |
| `equipos/agitador` | Agitador / mezclador | 24, 32, 48, 64 | Parado · En marcha · Falla |
| `equipos/transportador` | Cinta transportadora | 24, 32, 48, 64 | Parado · En marcha · Falla |
| `sensores/proximidad` | Sensor de proximidad | 24, 32, 48 | Sin detección · Detección |
| `sensores/termometro` | Temperatura | 24, 32, 48 | Baja · Normal · Alta |
| `sensores/manometro` | Presión | 24, 32, 48 | Baja · Normal · Alta |
| `alarmas/alarma` | Alarma | 24, 32, 48 | Normal · Activa |
| `alarmas/campana` | Campana / bocina | 24, 32, 48 | Silencio · Sonando |
| `alarmas/sirena` | Baliza giratoria | 24, 32, 48 | Apagada · Encendida |
| `botones/pulsador` | Pulsador | 24, 32, 48 | Suelto · Presionado |
| `botones/paro_emergencia` | Paro de emergencia | 24, 32, 48 | Normal · Activado |
| `botones/selector` | Selector 0-1 | 24, 32, 48 | Posición 0 · Posición 1 |
| `botones/contacto` | Contacto / interruptor | 24, 32, 48 | Abierto · Cerrado |

### `MENU`

Árbol de menús para ver y editar variables con la botonera. Uno por página, hasta 4 en el proyecto, 64 nodos y 3 niveles.

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` **(obligatoria)** | texto |  | Tipo de objeto. |
| `name` | texto |  | Nombre visible en la lista de objetos del Designer (no llega al C++). |
| `id` | identificador C++ |  | Identificador C++ del objeto (único en el proyecto). Si falta se genera FIELD_<TIPO>_<n>. |
| `page` | entero 0..15 | `0` | Página (id) donde se dibuja. |
| `layerOrder` | entero 0..9999 |  | Orden de capa: menor = más al frente. Si falta en todos, el orden del arreglo manda (el último queda encima). |
| `x` **(obligatoria)** | entero 0..319 |  | Esquina superior izquierda, X (px). |
| `y` **(obligatoria)** | entero 0..169 |  | Esquina superior izquierda, Y (px). |
| `title` | texto | `"Ajustes"` | Título de la cabecera. |
| `width` | entero 40..320 | `300` | Ancho (px). |
| `height` | entero 30..170 | `124` | Alto (px). Filas de 12 px (textSize 1) o 20 px (textSize 2) bajo la cabecera. |
| `textSize` | `1` · `2` | `1` | Escala del texto. |
| `frame` | bool | `true` | Marco exterior. |
| `colors` | objeto |  | Colores: text, background, value, selectText, selectBackground, editText, editBackground, title, titleBackground, frame. Los que falten toman su valor por defecto. |
| `nodes` **(obligatoria)** | arreglo |  | Nodos del árbol (ver «Nodos de MENU»). Se pueden anidar con `children` en lugar de `parent`. |

#### Nodos de MENU (`nodes[]`)

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `key` | texto |  | Clave única del nodo dentro del MENU (se genera si falta). |
| `parent` | texto |  | Clave del nodo padre (SUBMENU). null = raíz. Con `children` no hace falta. |
| `type` **(obligatoria)** | `"SUBMENU"` · `"VIEW"` · `"EDIT_NUM"` · `"EDIT_BOOL"` · `"GO_IDLE"` |  | SUBMENU: abre hijos. VIEW: muestra una variable. EDIT_NUM/EDIT_BOOL: el operador la edita. GO_IDLE: vuelve a la pantalla de inicio. |
| `label` **(obligatoria)** | texto |  | Texto de la fila. |
| `variable` | identificador C++ | `""` | Variable C++ (VIEW, EDIT_NUM, EDIT_BOOL). Puede ser la misma que la de un campo si el tipo coincide. |
| `valueType` | `"float"` · `"int32"` · `"bool"` · `""` |  | VIEW: float\|int32\|bool. EDIT_NUM: float\|int32. EDIT_BOOL: bool. SUBMENU/GO_IDLE: vacío. |
| `initial` | número | `0` | Valor inicial de la variable. |
| `min` | número | `0` | Mínimo (EDIT_NUM). |
| `max` | número | `100` | Máximo (EDIT_NUM). |
| `step` | número | `1` | Paso de ▲▼ (EDIT_NUM). int32: entero. |
| `decimals` | entero 0..6 | `1` | Decimales mostrados (float). |
| `unit` | texto | `""` | Unidad. |
| `falseText` | texto | `"OFF"` | Texto para false (bool). |
| `trueText` | texto | `"ON"` | Texto para true (bool). |
| `children` | arreglo |  | Atajo: hijos anidados de un SUBMENU (la normalización los aplana con `parent`). |

Colores del MENU (`colors`), por defecto:

`text` = `0xFFFF` · `background` = `0x0000` · `value` = `0x07FF` · `selectText` = `0x0000` · `selectBackground` = `0x07FF` · `editText` = `0x0000` · `editBackground` = `0xFD20` · `title` = `0x0000` · `titleBackground` = `0xFFFF` · `frame` = `0xFFFF`

### `EVENT_LOG`

Historial y registro de eventos (Alpha16). 10 filas de altura fija con registro en RAM/FRAM/SD.

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` **(obligatoria)** | texto |  | Tipo de objeto. |
| `name` | texto |  | Nombre visible en la lista de objetos del Designer (no llega al C++). |
| `id` | identificador C++ |  | Identificador C++ del objeto (único en el proyecto). Si falta se genera FIELD_<TIPO>_<n>. |
| `page` | entero 0..15 | `0` | Página (id) donde se dibuja. |
| `layerOrder` | entero 0..9999 |  | Orden de capa: menor = más al frente. Si falta en todos, el orden del arreglo manda (el último queda encima). |
| `x` **(obligatoria)** | entero 0..319 |  | Esquina superior izquierda, X (px). |
| `y` **(obligatoria)** | entero 0..169 |  | Esquina superior izquierda, Y (px). |
| `width` | entero 40..320 | `320` | Ancho (px). |
| `height` | entero 30..170 | `170` | Alto (px). |
| `ramCapacity` | entero 16..128 | `64` | Capacidad del buffer en RAM. |
| `enableFram` | bool | `true` | Persistencia de los últimos 16 eventos en FRAM ante corte de luz. |
| `enableSd` | bool | `true` | Exportación no bloqueante a microSD. |
| `sdFileName` | texto | `"/EVENTOS.CSV"` | Nombre del archivo CSV en microSD. |
| `descriptors` | arreglo | `[]` | Descriptores de eventos conocidos del proyecto. |

### `RAW_TEXT`

Texto fijo (se dibuja una vez al entrar a la página). Admite fuentes GLCD, bitmap y FreeFonts.

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` **(obligatoria)** | texto |  | Tipo de objeto. |
| `name` | texto |  | Nombre visible en la lista de objetos del Designer (no llega al C++). |
| `id` | identificador C++ |  | Identificador C++ del objeto (único en el proyecto). Si falta se genera FIELD_<TIPO>_<n>. |
| `page` | entero 0..15 | `0` | Página (id) donde se dibuja. |
| `layerOrder` | entero 0..9999 |  | Orden de capa: menor = más al frente. Si falta en todos, el orden del arreglo manda (el último queda encima). |
| `x` **(obligatoria)** | entero 0..319 |  | Esquina superior izquierda, X (px). |
| `y` **(obligatoria)** | entero 0..169 |  | Esquina superior izquierda, Y (px). |
| `text` **(obligatoria)** | texto |  | Texto. Tildes, ñ y ° sí existen en GLCD. |
| `font` | `"GLCD"` · `"FONT2"` · `"FONT4"` · `"SANS_9"` · `"SANS_BOLD_9"` · `"SANS_12"` · `"SANS_BOLD_12"` · `"SANS_18"` · `"SANS_BOLD_18"` · `"SANS_24"` · `"SANS_BOLD_24"` · `"SERIF_9"` · `"SERIF_BOLD_9"` · `"SERIF_12"` · `"SERIF_BOLD_12"` · `"SERIF_18"` · `"SERIF_BOLD_18"` · `"SERIF_24"` · `"SERIF_BOLD_24"` · `"MONO_9"` · `"MONO_BOLD_9"` · `"MONO_12"` · `"MONO_BOLD_12"` · `"MONO_18"` · `"MONO_BOLD_18"` · `"MONO_24"` · `"MONO_BOLD_24"` | `"GLCD"` | Fuente. GLCD = clásica 6×8 escalable; FONT2/FONT4 bitmap de 16/26 px; FreeFonts SANS/SERIF/MONO [BOLD] 9/12/18/24 pt. |
| `size` | entero 1..4 | `1` | Escala (GLCD y bitmap). |
| `textColor` | color | `0xFFFF` | Color del texto. |
| `backgroundColor` | color | `0x0000` | Fondo detrás del texto (si no es transparente). |
| `transparentBackground` | bool | `true` | Sin fondo detrás del texto. |

### Figuras: `LINE`, `RECT`, `ELLIPSE`, `TRIANGLE`, `POLYGON`

- `LINE`: Línea de (x,y) a (x2,y2).
- `RECT`: Rectángulo (cornerRadius para esquinas redondeadas).
- `ELLIPSE`: Elipse inscrita en la caja.
- `TRIANGLE`: Triángulo isósceles inscrito en la caja.
- `POLYGON`: Polígono regular de `sides` lados inscrito en la caja.

Se dibujan una vez al entrar a la página (gráfica estática).

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` **(obligatoria)** | texto |  | Tipo de objeto. |
| `name` | texto |  | Nombre visible en la lista de objetos del Designer (no llega al C++). |
| `id` | identificador C++ |  | Identificador C++ del objeto (único en el proyecto). Si falta se genera FIELD_<TIPO>_<n>. |
| `page` | entero 0..15 | `0` | Página (id) donde se dibuja. |
| `layerOrder` | entero 0..9999 |  | Orden de capa: menor = más al frente. Si falta en todos, el orden del arreglo manda (el último queda encima). |
| `cornerRadius` | entero 0..85 | `0` | Radio de esquinas (RECT). |
| `x` **(obligatoria)** | entero 0..319 |  | Esquina superior izquierda, X (px). |
| `y` **(obligatoria)** | entero 0..169 |  | Esquina superior izquierda, Y (px). |
| `x2` **(obligatoria)** | entero 0..319 |  | Esquina opuesta (o extremo de la línea), X. |
| `y2` **(obligatoria)** | entero 0..169 |  | Esquina opuesta (o extremo de la línea), Y. |
| `frameColor` | color | `0xFFFF` | Color del contorno (o de la línea). |
| `fill` | bool | `false` | Relleno. |
| `fillColor` | color | `0x001F` | Color de relleno. |
| `borderEnabled` | bool | `true` | Dibuja el contorno (con relleno se puede quitar). |
| `size` | entero 1..8 | `1` | Grosor del trazo (px). |
| `rotation` | número | `0` | Rotación en grados alrededor del centro. |
| `sides` | entero 3..12 | `5` | Lados (POLYGON). |

### `NAV_BAR`

Barra de navegación entre páginas (◀ ▶ de la botonera). Global: una sola, se ve en todas las páginas.

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` **(obligatoria)** | texto |  | Tipo de objeto. |
| `name` | texto | `"Barra de Navegación"` | Nombre visible en la lista de objetos del Designer (no llega al C++). |
| `preset` | `"industrial-color"` · `"dark-pill"` · `"segmented-bar"` · `"compact-dots"` · `"edge-tabs"` · `"numeric-pills"` · `"counter"` | `"industrial-color"` | Estilo. counter: sólo el indicador NN/TT que pinta el firmware (sin barra). |
| `position` | `"bottom"` · `"top"` · `"left"` · `"right"` | `"bottom"` | Borde de la pantalla. |
| `x` | entero 0..319 |  | X (se calcula de position si falta). |
| `y` | entero 0..169 |  | Y (se calcula de position si falta). |
| `width` | entero 8..320 |  | Ancho (320 horizontal, ~24 vertical). |
| `height` | entero 8..170 | `22` | Alto (~22 horizontal, 170 vertical). |
| `tabDistribution` | `"center"` · `"start"` · `"end"` · `"stretch"` | `"center"` | Reparto de las pestañas. |
| `textAlign` | `"center"` · `"left"` · `"right"` | `"center"` | Texto dentro de la pestaña. |
| `fontSize` | `"normal"` · `"medium"` · `"micro"` | `"normal"` | Tamaño del texto. |
| `labelFormat` | `"short"` · `"number"` · `"num_short"` | `"short"` | short: nombre corto de la página; number: 1, 2, 3; num_short: ambos. |
| `bgMode` | `"solid"` · `"dark"` · `"transparent"` | `"solid"` | Fondo de la barra. |
| `borderMode` | `"none"` · `"line"` · `"frame"` | `"none"` | Borde de la barra. |
| `radius` | entero 0..11 | `3` | Radio de las pestañas. |
| `gap` | entero 0..10 | `3` | Separación entre pestañas. |

## Capas de píxeles (`pixelMaps[]`)

| Propiedad | Tipo / valores | Por defecto | Descripción |
|---|---|---|---|
| `type` | texto | `"PIXELMAP"` | Siempre PIXELMAP. |
| `name` | texto |  | Nombre de la capa. |
| `page` | entero 0..15 | `0` | Página. |
| `x` | entero 0..319 | `0` | Origen X. |
| `y` | entero 0..169 | `0` | Origen Y. |
| `pixels` | arreglo | `[]` | Píxeles {x, y, color} relativos al origen. Una capa = un solo color (para otro color, otra capa). |

## Fuentes de `RAW_TEXT`

`GLCD` · `FONT2` · `FONT4` · `SANS_9` · `SANS_BOLD_9` · `SANS_12` · `SANS_BOLD_12` · `SANS_18` · `SANS_BOLD_18` · `SANS_24` · `SANS_BOLD_24` · `SERIF_9` · `SERIF_BOLD_9` · `SERIF_12` · `SERIF_BOLD_12` · `SERIF_18` · `SERIF_BOLD_18` · `SERIF_24` · `SERIF_BOLD_24` · `MONO_9` · `MONO_BOLD_9` · `MONO_12` · `MONO_BOLD_12` · `MONO_18` · `MONO_BOLD_18` · `MONO_24` · `MONO_BOLD_24`
