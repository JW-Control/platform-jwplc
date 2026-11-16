# A17 · Cierre: `.a` regenerados (JWPLC_Display, JW_FRAM, JW_MatrixButtons)

Fecha: 2026-11-04. Rama: `v2.1.0-alpha.17/JWPLC-Sim/feature/cierre-alpha17`.

```text
A17_PRECOMPILADAS_REGENERADAS=PASS
A17_ABI_IMPORTS_OTRAS_LIBS=PASS (0 símbolos faltantes)
A17_ABI_SIMBOLOS_ELIMINADOS=8 en JWPLC_Display (cambio de firma intencional, ver §3)
```

## 1. Qué se regeneró y por qué

Las tres librerías pasaron a modo fuente durante la alpha (regla de CLAUDE.md §3.4) porque cambió su código:

| Librería | Versión | Cambios en alpha.17 |
|---|---|---|
| `JWPLC_Display` | 1.1.0 | API con «.» (#224), indicadores de nivel y fuentes universales (#232) |
| `JW_FRAM` | 1.0.4 | `updateBlock()`: escribe solo si el bloque cambió (sincronización de JW-Libraries, #226) |
| `JW_MatrixButtons` | 1.0.7 | nombres globales `BTN_EV_*`, `JW_ButtonEvent`, `JW_ButtonMapItem` (sincronización de JW-Libraries, #226) |

## 2. Cómo

`[ejecutado]` en la nube con la receta de `A16_CIERRE_PRECOMPILADAS_20261102.md` §5 (arduino-cli, esp-x32 2601,
arduino-esp32 3.3.8, ctags 5.8-arduino11):

```bash
CTAGS_DIR=$HOME/jwplc-build/ctags tools/alpha16/regenerate_precompiled.sh build JWPLC_Display JW_FRAM JW_MatrixButtons
```

El sketch sonda ahora incluye la cabecera principal de cada librería pedida (antes solo Display y MatrixButtons,
así que no servía para `JW_FRAM`), y la comprobación de importaciones cubre también `JW_FRAM`.

| `.a` | Bytes | SHA-256 | Símbolos eliminados / añadidos |
|---|---|---|---|
| `libJWPLC_Display.a` | 1 736 352 | `93068857732a59b912c93baa0c2eb3e97438f00b59590b55b63281aa387a0faf` | 8 / 21 |
| `libJW_FRAM.a` | 124 218 | `bb5cb710814fe47d33ea6b7c99d0fc27935a50e1e789ac2d9bfd3ec49bdd6375` | 0 / 0 |
| `libJW_MatrixButtons.a` | 133 844 | `1c8b487551464063c92ca643ae82b6f08451c53ef4858234a187d2c61618b49a` | 0 / 2 |

Compilación de la sonda: 0 advertencias. Ningún otro `.a` del package importa un símbolo que falte.

## 3. Los 8 símbolos de `JWPLC_Display`

El script marca `ABI_COMPAT=FAIL` porque desaparecen 8 firmas del `.a` de alpha.16. Es intencional: en #232 los
constructores de estilo ganaron dos parámetros de fuente con valor por defecto
(`const JWPLC_TFTFont *labelFont = nullptr, const JWPLC_TFTFont *valueFont = nullptr`):

- `JWPLC_UIStyle::JWPLC_UIStyle(…)` (dos variantes C1/C2), `JWPLC_UIBoolStyle`, `JWPLC_UIValueStyle`,
  `JWPLC_UITextFieldStyle`, `JWPLC_UIBarField` (dos sobrecargas) y `JWPLC_UIMenuField`.

El código fuente que llama con los argumentos de antes sigue compilando (los nuevos tienen valor por defecto). Lo
único que se rompería es un binario compilado contra el `.a` de alpha.16, y eso no existe en el package: los
sketches se compilan siempre contra la cabecera y el `.a` de la misma versión. Por eso los `.a` se instalaron a
mano desde el directorio de salida del script en vez de usar `--install`, que se detiene ante el FAIL.

## 4. Verificación

- `[ejecutado]` compilación ESP32 real de los 79 ejemplos del package con estos `.a`: 79/79 (detalle en `ALPHA17_STATUS.md` §3).
- `[ejecutado]` HMI Designer `tests/run-all.cjs` 21/21: los headers generados compilan y sus llamadas existen en los `.a`.
- `[pendiente]` benchmark de compilación en la PC del propietario (Arduino IDE, sketch limpio y recompilación).

## 5. Segunda regeneración (16/11): #238 y #239

Después del 04/11 entraron a `…/main` dos PR que cambian fuentes de librerías precompiladas:

| Librería | Cambio | Resultado |
|---|---|---|
| `JWPLC_Display` 1.1.0 | #239 (André): TREND, 64 campos en Flash, máscaras por página, menú CARDS, `JWPLC_Icons` | `.a` regenerado, sigue `precompiled=full` |
| `JWPLC_ModbusTCP` 0.1.0 | #238: varios clientes, cierre por inactividad, Unit ID 0/255 | `.a` regenerado y **vuelve a `precompiled=full`** (decisión del propietario, 16/11) |

`[ejecutado]` en la nube, misma receta (§2):

```bash
CTAGS_DIR=$HOME/jwplc-build/ctags tools/alpha16/regenerate_precompiled.sh build JWPLC_Display JWPLC_ModbusTCP
```

| `.a` | Bytes | SHA-256 | Símbolos eliminados / añadidos |
|---|---|---|---|
| `libJWPLC_Display.a` | 2 737 588 | `b1f2a6b8d0bcef441b79f3cb773aa0b7350b15217b238db2ea3c0f85a10b027d` | 46 / 41 (reemplazado por el de §6) |
| `libJWPLC_ModbusTCP.a` | 479 116 | `849af1163c6ffb51fde1239b3b17ff7c205d8500c4cf6e4857cbcfcb2cb92271` | 0 / 15 |

- Ningún otro `.a` del package importa un símbolo que falte (`IMPORTS_FROM_OTHER_LIBS_MISSING=0`).
- Los 46 símbolos que salen de `JWPLC_Display` son constructores y ayudantes de `JWPLC_UI.h` (`JWPLC_UIRect`,
  `JWPLC_UIText`, `JWPLC_UIColors`, `JWPLC_UIBoolText`, `JWPLC_UIValueFormat`, `JWPLC_UIBarField`, …) que #239 pasó a
  `constexpr`/`inline` en la cabecera (`JWPLC_UI_CONSTEXPR14`) para que las tablas de 64 campos queden en Flash. Ya no
  hacen falta en el `.a`; como en §3, nada compilado contra el `.a` anterior se mezcla con este.
- El `.a` de `JWPLC_ModbusTCP` anterior no tenía información de depuración; el nuevo sí (como `JWPLC_Display`), por eso
  pesa más. El código que entra al firmware pasa de 16 277 a 18 101 B (`size -t`).
- `[ejecutado]` compilación ESP32 real de los 34 sketches afectados con estos `.a` (16 de `JWPLC_Display`, 2 de
  `JWPLC_GlobalPeripherals`, 1 de `JWPLC_Runtime`, 4 de `JWPLC_UI_Widgets`, 2 de `JWPLC_ModbusTCP`, 4 de
  `JW_ModbusRTU` y los 5 proyectos de `tools/jwplc-hmi-designer/pruebas-ia/`): **34/34, 0 advertencias**. Los 2 de
  Modbus TCP usan el `.a` (`JWPLC_ModbusTCP has been declared precompiled`).
- `[ejecutado]` HMI Designer `tests/run-all.cjs`: 34/34 (ver `ALPHA17_STATUS.md` §3).

## 6. `JWPLC_Icons`: sólo el tamaño usado (16/11)

La biblioteca nativa de iconos (#239) ya iba dentro de `libJWPLC_Display.a`, pero cada método elegía el tamaño
dentro de la función: `JWPLC_Icons.motor(48)` arrastraba los 4 tamaños del motor y `get()` los 28 iconos (~280 KB).
Por eso el Designer seguía copiando el mapa de bits al header.

- `icons/export-cpp-library.cjs` genera ahora un `const JWPLC_UIIcon JWPLC_ICON_<ICONO>_<TAMAÑO>` externo por tamaño y
  los métodos de `JWPLC_IconsClass` como `inline` en `JWPLC_Icons.h`. Con el tamaño fijo el compilador deja una sola
  referencia y `--gc-sections` sólo enlaza ese tamaño. `get()` sigue en el `.cpp` y avisa que enlaza todo.
- `[ejecutado]` sketch sonda con `JWPLC_Icons.motor(48)`: **+4 760 B** de flash (antes **+16 632 B**).
- El Designer emite `JWPLC_Icons.<icono>(tamaño)` para los iconos del catálogo sin colores propios; los que tienen
  colores propios (o no están en el catálogo) siguen con su mapa de bits en el header.
- `.a` regenerado: 2 717 940 B, SHA-256 `bc7afc9729be95095a5459d815f79f843556470478fba4befbb7af2cb424a3e6`. Salen 27
  símbolos (los métodos, ahora `inline`) y entran 111 (un `JWPLC_UIIcon` por icono y tamaño). Ningún otro `.a` importa
  un símbolo que falte.
- `[ejecutado]` compilación ESP32 de los 16 ejemplos de `JWPLC_Display` y los 5 de `pruebas-ia`: 21/21, 0 advertencias.
  HMI Designer `tests/run-all.cjs`: 34/34 (`icon-smoke` vuelve a exigir la llamada nativa y que no haya mapas de bits
  del catálogo en el header).
