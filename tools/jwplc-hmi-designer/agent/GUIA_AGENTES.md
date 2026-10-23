# Guía para agentes: crear proyectos del JWPLC HMI Designer sin la UI

Esta guía es para cualquier agente (Claude, GPT/Codex u otro) o persona que quiera montar una
interfaz completa para la TFT del JWPLC Basic escribiendo un JSON, sin abrir el Designer.
El resultado es el mismo que con la UI: `JWPLC_HMI_Generated.h` generado por el propio Designer
(en Chromium headless), el `.jwhmi` que la UI abre y edita, y la guía `JWPLC_HMI_LEEME.md`.

- Referencia de cada propiedad: [REFERENCIA_JWHMI.md](REFERENCIA_JWHMI.md) (o `jwhmi describe <TIPO>`).
- Esquema JSON para el editor: [`../schema/jwhmi.schema.json`](../schema/jwhmi.schema.json).
- Ejemplos que validan y compilan: [`../examples/`](../examples/).

## 1. Requisitos

- Node 18 o superior.
- Playwright con Chromium (genera el header y las vistas previas):
  `npm install -g playwright && npx playwright install chromium`. En la nube ya está instalado.
- Opcional: `g++` para `check` (compila el header en host contra los headers reales del package).

Todos los comandos se corren desde la raíz del repo:

```bash
J="node tools/jwplc-hmi-designer/cli/jwhmi.cjs"
```

## 2. Flujo de trabajo

```bash
$J new mi_hmi.json --name Bombeo     # 1. proyecto de partida
# 2. edita mi_hmi.json (forma corta, sección 3)
$J validate mi_hmi.json              # 3. errores y avisos con la corrección sugerida
$J preview mi_hmi.json -o preview/   # 4. un PNG por página: MÍRALOS antes de seguir
$J check mi_hmi.json                 # 5. genera el header y compila el sketch base (g++)
$J build mi_hmi.json --sketch ruta/al/Sketch --create-ino   # 6. escribe en el sketch
```

Repite 2 a 4 hasta que `validate` no dé errores ni avisos y las vistas previas se vean bien.
`--json` en cualquier comando da una salida para máquinas (`{ ok, errors, warnings, files }`);
`--strict` cuenta los avisos como errores. Código de salida: 0 bien, 1 errores, 2 uso incorrecto.

| Comando | Qué hace |
|---|---|
| `new <archivo> [--name N]` | Crea un proyecto de partida que valida. |
| `describe [TIPO]` | Lista los tipos o las propiedades de uno (`describe VALUE`, `describe MENU_NODE`). |
| `icons [categoría\|texto]` | Catálogo de iconos para `ICON`: `iconId`, tamaños y nombre de cada estado (`icons pilotos`, `icons valvula`). |
| `validate <p> [--fast]` | Valida. Sin `--fast` también carga el proyecto en el Designer y avisa de campos que salen de la pantalla o se pisan. |
| `preview <p> [-o dir] [--scale N]` | PNG de cada página tal como lo dibuja el lienzo (que replica el firmware). |
| `generate <p> [-o archivo.h]` | Sólo el header. |
| `check <p>` | `validate` + header + compilación en host del sketch base. |
| `normalize <p> [-o salida.jwhmi]` | Forma corta → `.jwhmi` completo (como lo guarda el Designer). |
| `build <p> --sketch <dir> [--create-ino] [--preview]` | Escribe `JWPLC_HMI_Generated.h`, `<sketch>.jwhmi` y `JWPLC_HMI_LEEME.md` en el sketch, igual que «Escribir en sketch». Con `--create-ino` crea el `.ino` con el sketch base **sólo si no existe**: el `.ino` del usuario nunca se escribe. |

## 3. La forma corta

La CLI acepta un JSON mínimo y completa el resto con los mismos valores por defecto que el Designer:

```json
{
  "$schema": "../schema/jwhmi.schema.json",
  "projectName": "Bombeo",
  "display": { "start": "HMI" },
  "pages": [
    { "name": "Proceso", "fields": [
      { "type": "RAW_TEXT", "text": "Estación 1", "x": 4, "y": 4, "font": "SANS_BOLD_9" },
      { "type": "VALUE", "id": "FIELD_CAUDAL", "variable": "caudal", "label": "Caudal", "unit": "L/m", "x": 4, "y": 30, "integerDigits": 4, "decimalDigits": 1 },
      { "type": "BOOL", "id": "FIELD_BOMBA", "variable": "bomba", "label": "Bomba", "x": 4, "y": 60, "boolStyle": "PILOT", "trueColor": "GREEN" }
    ] },
    "Alarmas"
  ],
  "fields": [ { "type": "NAV_BAR", "preset": "industrial-color" } ]
}
```

- `format`, `version`, `target`, `navigationBar`: los pone la CLI.
- `pages`: textos o objetos. Los objetos de una página pueden ir en `pages[i].fields`; los globales
  (la `NAV_BAR`) o los que llevan `page` explícita, en `fields`.
- `id` y `variable`: si faltan se generan, pero **ponlos siempre**: son los nombres que usará el sketch.
  Identificadores C++ válidos (sin tildes ni espacios), únicos. Convención: `FIELD_<NOMBRE>` y `camelCase`.
- Colores: `"RED"`, `"#FF8800"`, `"0xF800"` o número RGB565 (lista en la referencia).
- `layerOrder`: si no lo pones, manda el orden del arreglo (el último queda encima).
- MENU: los nodos pueden anidarse con `children` en lugar de `parent`.

## 4. Diseñar para la TFT

- Pantalla de **320 × 170 px**, origen arriba a la izquierda, RGB565. `x`/`y` son la esquina
  superior izquierda de cada objeto.
- Texto GLCD: celda de **6 × 8 px** por escala. Un valor de 4 caracteres a `valueSize` 2 ocupa
  48 × 16 px. Los campos suman un margen de 3 px arriba/izquierda y `max(3, escala + 1)` abajo/derecha.
  Alturas de un campo INLINE: ~13 px (escala 1), ~20 px (escala 2), ~28 px (escala 3).
- Con una `NAV_BAR` abajo (alto 22) deja libre `y ≥ 147`. Arriba (`position: "top"`), `y ≤ 24`.
- **Estático vs dinámico.** Figuras y `RAW_TEXT` se dibujan una vez al entrar a la página. Los campos
  (TEXT, VALUE, BOOL, BAR) y el MENU se redibujan cuando cambian y pintan su caja con su
  `backgroundColor`: si un campo va sobre una figura de color, dale ese mismo fondo
  (ver `FIELD_HORA` en `examples/05_estacion_bombeo.json`).
- **Dos campos dinámicos no deben pisarse** (el último en redibujarse tapa al otro): `validate` lo avisa.
- Tildes, ñ y ° existen en la fuente GLCD (no las mayúsculas acentuadas como Á, Í, Ú). Las fuentes
  FreeFont (`SANS_*`, `SERIF_*`, `MONO_*`) y `FONT2`/`FONT4` sólo tienen ASCII.
- `VALUE` reserva el ancho de `integerDigits` + decimales: elige los dígitos del peor caso.
- BAR: el firmware dibuja sólo la barra horizontal lineal sólida; no hay medidor circular ni degradado.
- ICON: imagen fija por estado del catálogo (`jwhmi icons`), sin animación. Ocupa `iconSize` (24, 32, 48;
  64 en algunos) + 3 px de margen por lado, y con etiqueta STACKED suma ~9 px arriba (escala 1).
  Con 2 estados la variable es `bool`; con más, `uint8_t` y el sketch asigna 0..N-1 (el nombre de cada
  estado está en `icons`). Un estado fuera de rango deja el último dibujado. Cuenta en los 32 objetos;
  dos ICON del mismo icono y tamaño comparten el bitmap en flash (una imagen de 64 px pesa 2 KB).
  `transparentBackground: true` (sin fondo) deja ver la figura o el texto fijo de debajo fuera de la
  silueta del icono: úsalo para poner equipos sobre un `RECT` de color.
- MENU: uno por página; necesita teclas (▲▼ OK ESC) y ocupa la página, así que suele ir en una
  página propia («Ajustes»).
- Con varias páginas el firmware cambia de página con ◀ ▶ (`JWPLC_UI_Pages`); la `NAV_BAR` muestra las pestañas.

## 5. Recetas

- **Barra de título:** `RECT` de `0,0` a `319,18` con `fill` + `RAW_TEXT` blanco en `4,6` (GLCD) o
  `4,4` (`SANS_BOLD_9`), y la hora con un `VALUE` `valueKind: "DATETIME"` del mismo fondo.
- **Indicador grande:** `VALUE` con `valueSize: 3`, `layout: "STACKED"` y etiqueta a escala 1.
- **Estados de equipos:** `BOOL` con `boolStyle: "PILOT"` (círculo) o con texto y
  `stateColors: true` (`trueBackground: "RED"` para alarmas que llaman la atención).
- **Equipos con símbolo:** `ICON` (`motores/motor`, `bombas/bomba`, `valvulas/valvula`…) con `label`;
  para el estado de falla usa el estado 2 de los que lo tienen. Pilotos pequeños: `pilotos/piloto` de 24 px
  con `layout: "INLINE"` (etiqueta a la izquierda). Ejemplo: `examples/06_iconos.json`.
- **Nivel de tanque:** `BAR` con `barWidth` del ancho disponible, `frame: true` y `trackColor` oscuro.
- **Ajustes:** página con un `MENU` de `EDIT_NUM` (consignas) y `EDIT_BOOL` (habilitaciones), un
  `SUBMENU` para lo avanzado y un `GO_IDLE` para volver al inicio.
- **Pantalla de inicio:** `display.start: "IDLE"` muestra la pantalla JWPLC al encender;
  `wake: "ANY"` entra al HMI con cualquier tecla y `back: "TIMEOUT"` vuelve tras `timeoutS`.

## 6. El sketch

`build` escribe en la carpeta del sketch (la carpeta y el `.ino` se llaman igual):

| Archivo | Qué es |
|---|---|
| `JWPLC_HMI_Generated.h` | Pantallas, campos, MENU e IDLE en C++. **Declara las variables**: no las vuelvas a declarar en el `.ino`. |
| `<sketch>.jwhmi` | El diseño completo: el Designer lo abre («Abrir» o al vincular el sketch). |
| `JWPLC_HMI_LEEME.md` | Sketch base, tabla de variables y cómo leer el MENU (`JWPLC_Display.menuChanged`). |

En el `.ino`: `#include "JWPLC_HMI_Generated.h"`, `jwplcHMISetup();` en `setup()` y en `loop()` sólo
asignar las variables (sin `delay()`, sin dibujar). El runtime redibuja lo que cambió.

Si el `.ino` lee la botonera: sólo cuando la página está en modo contenido
(`!JWPLC_Display.isUserPageSelection() && JWPLC_Display.userPage() == PAGE_X`), nunca en la página
del MENU; ESC es del sistema. Ejemplo completo: [`../pruebas-ia/02_botonera/`](../pruebas-ia/02_botonera/README.md).
El IDE de Arduino declara los prototipos de las funciones antes que tus `enum`/`struct`: no los uses
como tipo de parámetro (usa `uint8_t`), o compila en host pero falla en ESP32.

## 7. Verificación

1. `validate` sin errores ni avisos y las vistas previas revisadas.
2. `check`: el header compila en host y cada llamada existe en las librerías.
3. Compilación ESP32 real (red a downloads.arduino.cc): `tools/alpha15/regenerate_libJWPLC_TFT.sh compile <sketch>`.
4. Prueba en la placa: la hace el equipo en taller. Hasta entonces el resultado es `[no verificado]` en hardware.

## 8. Errores frecuentes

| Mensaje | Causa y arreglo |
|---|---|
| `valor "MIDDLE" no válido; usa "LEFT" \| "CENTER" \| "RIGHT" (¿CENTER?)` | Valor enumerado fuera de la lista: usa uno de los propuestos. |
| `propiedad desconocida «labelColour» … ¿quisiste decir «labelColor»?` | Nombre mal escrito: se ignora hasta que lo corrijas. |
| `el id FIELD_X ya lo usa fields[n]` | Ids únicos en todo el proyecto. |
| `la variable v ya la usa fields[n]` | Cada campo declara su variable. Para mostrar un valor en dos lugares usa dos variables; el MENU sí puede reutilizar la de un campo del mismo tipo. |
| `sale de la pantalla` / `se pisa con` | Mueve o achica el campo; mira la vista previa. |
| `«Í» no existe en la fuente de la TFT` | Usa minúsculas acentuadas o quita la tilde. |
| `la página ya tiene el MENU …` | Un MENU por página. |
| `icono "motor" no está en el catálogo (¿motores/motor?)` | `iconId` es `categoria/nombre`: búscalo con `jwhmi icons`. |
| `… tiene 4 estados … y con bool sólo se ven 0 y 1` | Quita `iconVarType: "bool"` (por defecto `uint8` con más de 2 estados). |

## 9. Abrir y seguir en el Designer

El `.jwhmi` que escribe `build` (o `normalize`) es el mismo formato que guarda la UI: se abre con
«Abrir proyecto» o al vincular el sketch desde el Arduino IDE, y se puede seguir editando a mano.
Al revés también: un `.jwhmi` guardado por el Designer se puede validar, regenerar y modificar con la CLI.
