# Alpha15 · I2: fecha/hora del RTC en VALUE

Fecha: `2026-10-18` · plan: `A15_INDUSTRIAL_ARQUITECTURA_PLAN_20261018.md` (§B.5 «Fecha/hora»)

```text
A15_I2_VALUE_DATETIME=PASS_HOST_AND_ESP32_COMPILE
A15_I2_FORMAT_PARITY=PASS (55 relojes firmware real == Designer)
A15_I2_PHYSICAL=[no verificado] (gate tools/jwplc-hmi-designer/gates/A15_I2_Reloj)
```

## 1. Qué es

Un VALUE con formato **Fecha/hora** muestra el reloj de la placa (RTC). **No lleva variable ni setter**: el usuario lo coloca en el lienzo y elige qué mostrar.

| Patrón | Se ve | Sin RTC válido |
|---|---|---|
| `HH_MM` | `14:35` | `--:--` |
| `HH_MM_SS` | `14:35:07` | `--:--:--` |
| `DD_MM` | `18/10` | `--/--` |
| `DD_MM_YYYY` | `18/10/2026` | `--/--/----` |
| `DD_MM_YYYY_HH_MM` | `18/10/2026 14:35` | `--/--/---- --:--` |

«Sin RTC válido» incluye `valid == false` (sin pila o sin ajustar) y cualquier campo fuera de rango (mes 13, hora 24…).

Decisión: sólo en VALUE, no en TEXT. El plan decía «VALUE o TEXT», pero un TEXT de reloj sería un VALUE con otro nombre: la reserva de ancho la da el patrón, no una capacidad en caracteres.

## 2. Firmware (`JWPLC_Display`, modo fuente)

- `JWPLC_UIDateTimeFormat(patrón)` y `JWPLC_UI_HAS_VALUE_DATETIME`. El formateo vive en `JWPLC_UI_Format.cpp`: `JWPLCUIFormat::dateTime`, con la misma muestra y el mismo desborde que el resto de VALUE.
- `JWPLCUI::pollClockFields()` corre en cada ciclo USER, dentro de `jwplcUIRuntimeServiceInput()` y antes del filtro de refresco:
  - lee `jwplcGetRTCState()` y formatea cada campo de reloj;
  - marca dirty sólo si cambió el texto: con `HH:MM`, la TFT se toca una vez por minuto;
  - si el texto no cambió no llama al setter, así que el perfil H3E1 no cuenta setters vacíos;
  - formatea los relojes de todas las páginas, para que al entrar a una página no se vea un instante la hora vieja;
  - no toma el bus: sólo prepara el texto, y el dibujo es el dirty refresh de siempre.
- Un `setValue()` del sketch sobre un campo de reloj se ignora y devuelve `false`. Si no, escribiría guiones y el campo parpadearía.

## 3. Designer

- VALUE · Contenido · Formato: tercera tarjeta **Fecha/hora**, con 5 patrones en tarjetas y el interruptor «Ver sin reloj válido».
- La vista previa del lienzo usa la hora del PC al dibujar.
- Datos: se oculta la variable y aparece la nota «Reloj de la placa».
- Codegen:
  - `JWPLC_UIDateTimeFormat(JWPLC_UI_DATETIME_…)`;
  - sin declaración de variable ni setter en `jwplcUIUpdate()`;
  - un nombre de variable repetido en dos relojes no se marca como duplicado.

## 4. Verificación `[ejecutado]`

| Prueba | Resultado |
|---|---|
| `tests/value-format-parity.cjs`: 5 patrones × 11 relojes (válidos, bordes y fuera de rango), `JWPLC_UI_Format.cpp` real con g++ vs Designer | 55/55 |
| `tests/codegen-core.test.cjs` | PASS |
| `tests/codegen-golden.cjs`: nuevo `i2Clock.h` (dos relojes + un VALUE normal) | PASS |
| `tests/codegen-smoke.cjs`: compila; los relojes sin variable ni setter; sin avisos de duplicado | PASS |
| `tests/inspector-smoke.cjs` · VALUE · FECHA/HORA | PASS |
| ESP32: `gates/A15_I2_Reloj` | 426673 B flash · 35620 B RAM |

`[no verificado]`:

- el paso de la medianoche y el RTC sin pila en la placa (pasos del README del gate);
- la lectura de `JWPLC_RTCState`, que es `volatile` sin lock: en el cambio de segundo podría tomarse un valor mezclado durante un ciclo. El siguiente ciclo lo corrige. Es el mismo criterio que usa la IDLE (`JWPLC_IdleScreen.cpp`).
