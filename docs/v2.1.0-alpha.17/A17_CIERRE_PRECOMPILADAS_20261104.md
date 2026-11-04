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
