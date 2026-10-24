# Alpha15 · I1: VALUE con duración y separador de miles, BOOL «Piloto»

Fecha: `2026-10-18` · plan: `A15_INDUSTRIAL_ARQUITECTURA_PLAN_20261018.md` (§B.5, §G.1)

```text
A15_I1_VALUE_THOUSANDS=PASS_HOST_AND_ESP32_COMPILE
A15_I1_VALUE_DURATION=PASS_HOST_AND_ESP32_COMPILE
A15_I1_BOOL_PILOT=PASS_DESIGNER_AND_ESP32_COMPILE
A15_I1_FORMAT_PARITY=PASS (19 200 casos firmware real == Designer)
A15_I1_PHYSICAL=[no verificado] (gate tools/jwplc-hmi-designer/gates/A15_I1_Formatos)
```

## 1. Qué se agregó

### Firmware (`JWPLC_Display`, modo fuente)

- `JWPLC_UI_Format.{h,cpp}` (nuevo): todo el formateo de VALUE sale de `JWPLC_UI.cpp` a un archivo que compila solo en host. Contiene la muestra (reserva de ancho), el desborde y el valor.
- `JWPLC_UIValueFormat` suma:
  - `thousandsSeparator`: quinto argumento opcional, `','`, `'.'` o `' '`. Con `'.'` el decimal se muestra con `','`.
  - `kind`, `durationPattern` y `durationUnit` (`JWPLC_UI_HAS_VALUE_DURATION`, `JWPLC_UI_HAS_VALUE_THOUSANDS`).
  - El constructor de 4 argumentos sigue igual: el header de un proyecto anterior genera el mismo binario de formato.
- `JWPLC_UIDurationFormat(patrón, unidad, dígitos, signo)`:
  - patrones `SS`, `MM:SS`, `HH:MM:SS` y compacto (`1h05m` · `5m03s` · `45s`);
  - la variable trae segundos o ms y se trunca a segundos;
  - el tercer argumento es el ancho de la primera unidad: da la reserva y el máximo; si no cabe se ve `--:--`;
  - con signo admite el restante negativo (`-01:15`); `-0,4 s` se muestra `00:00`, sin signo.
- `JWPLC_UIBoolPilot(colorF, colorV[, fondoF, fondoV])` (`JWPLC_UI_HAS_BOOL_PILOT`):
  - el valor del BOOL se dibuja como un círculo de 7 × escala px, a la altura del texto;
  - aro del color de la etiqueta y relleno del color del estado, con `fillCircle` de `JWPLC_TFT`;
  - se repinta sólo al cambiar el estado, como el texto.

### Designer

- `poc/designer-value-format.js` (nuevo, también se carga en node): gemelo de `JWPLC_UI_Format.cpp`. Lo usan el lienzo, la reserva de ancho, `designer-pixelmap-compat.js` y el inspector.
- Inspector VALUE · Contenido · **Formato**:
  - tarjetas **Número · Duración**;
  - en Número, el separador de miles: Ninguno · `1,234` · `1.234` · `1 234`;
  - en Duración: patrones en tarjetas, dígitos de la primera unidad (el título cambia: segundos, minutos u horas), «La variable trae» segundos o milisegundos y «Admite negativos»;
  - cambiar de unidad convierte la vista previa (303 s ↔ 303000 ms).
- Inspector BOOL · Contenido · **Representación**:
  - **Texto · Piloto**; con Piloto se ocultan los textos de estado y la alineación;
  - en Apariencia, «Un color / Por estado» se oculta (el piloto siempre va por estado) y la leyenda dice «Etiqueta y aro» y «Piloto».
- Codegen:
  - `JWPLC_UIValueFormat(..., ',')` sólo cuando hay separador. Sin separador sale el mismo texto que antes y los 6 golden anteriores no cambian;
  - `JWPLC_UIDurationFormat(...)` para las duraciones;
  - `JWPLC_UIBoolPilot(...)` dentro de `#ifdef JWPLC_UI_HAS_BOOL_PILOT`, con respaldo de texto para un package anterior. Los fondos sólo se emiten si difieren del fondo del campo.
- **Tipo de la variable:**
  - antes, VALUE siempre declaraba `float`, que no representa exactos los enteros mayores que 2^24: 4,6 h en ms o un contador de 16,7 millones;
  - ahora las duraciones y los contadores con miles sin decimales declaran `uint32_t`, o `int32_t` con signo;
  - el resto sigue en `float`.

## 2. Hallazgo: redondeo en empates

El Designer usaba `toFixed()`, que redondea hacia arriba un empate exacto. `printf` lo redondea al par:

| Valor | Firmware | Designer antes |
|---|---|---|
| 2.5 con 0 decimales | `2` | `3` |
| 0.125 con 2 decimales | `0.12` | `0.13` |

Ya pasaba antes de I1 en VALUE `[ejecutado]`. Ahora `designer-value-format.js` redondea igual que `printf` y la grilla de paridad incluye esos casos.

## 3. Verificación `[ejecutado]`

| Prueba | Resultado |
|---|---|
| `tests/value-format-parity.cjs`: compila `JWPLC_UI_Format.cpp` real con g++ y compara 480 formatos × 40 valores con el Designer (muestra, desborde, texto y validez) | 19 200/19 200 |
| `tests/codegen-core.test.cjs`: separador, duración y tipos de variable | PASS |
| `tests/codegen-golden.cjs`: 6 golden anteriores idénticos + nuevo `i1Formats.h` | PASS |
| `tests/codegen-smoke.cjs`: `i1Formats.h` compila contra los headers reales | PASS |
| `tests/inspector-smoke.cjs` · VALUE · FORMATO: tarjetas, reserva `-8,888,888`, `05:03`, conversión s↔ms, undo, header | PASS |
| `tests/inspector-smoke.cjs` · BOOL · PILOTO: caja 7×escala, centro del estado, aro de la etiqueta y esquinas con el fondo del estado en 1×–4×, header | PASS |
| `tests/run-all.cjs` | 7/7 PASS |
| ESP32 (`arduino-cli`, `jwplc_local:esp32:jwplcbasic`): `gates/A15_I1_Formatos` | 427217 B flash · 36108 B RAM |

`[inferido]`:

- El círculo del lienzo es la misma rutina que `TFT_eSPI::fillCircle`, portada línea por línea. No hay un test de píxeles contra la TFT porque el fuente de TFT_eSPI no está en el repo.
- El gate físico lo confirma.

## 4. Gate físico

`tools/jwplc-hmi-designer/gates/A15_I1_Formatos/`: el `.jwhmi`, el header generado y un sketch que cambia las variables cada segundo. La tabla de lo que hay que ver está en su `README.md`.

## 5. Sigue

I2 (fecha/hora del RTC en VALUE/TEXT) → I3 (tildes cp437) → Apariencia A1.
