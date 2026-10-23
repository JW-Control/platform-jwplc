# Gate A15 I3 · Tildes, ñ y ° en la TFT

```text
A15_I3_HOST_PARITY=PASS        (tests/value-format-parity.cjs: lienzo == literal del codegen == setText)
A15_I3_CODEGEN=PASS            (golden i3Accents.h)
A15_I3_ESP32_COMPILE=PASS      (arduino-cli, jwplc_local:esp32:jwplcbasic)
A15_I3_PHYSICAL=[no verificado]
```

## Qué mirar en la TFT

| Elemento | Se espera |
|---|---|
| Etiqueta VALUE | `Presión` y la unidad `°C` |
| Etiqueta BOOL | `Válvula`, valores `NO` / `SI` (la `Í` mayúscula no existe en la fuente: se ve sin tilde) |
| RAW_TEXT | `Línea 2 · ¿Listo?` |
| TEXT «Señal» (cada 2 s, texto UTF-8 desde el sketch) | `Listo ñ` → `Presión OK` → `25°C ±0,5` → `¿Listo? ¡Sí!` → `AÉIOU áéíóú` |

Lo importante es el `°`: es un glifo por encima de 0xB0, el que podía desfasarse según
el modo cp437 de TFT_eSPI. El codegen envía 0xF7 para verlo, porque `JWPLC_TFT` deja
`cp437(false)`. Si en la placa sale `≈` en lugar de `°`, el modo es otro: anotarlo y
avisar a Alpha15.

Criterio PASS: todo como en la tabla; ningún `?` salvo donde la tabla lo indica.
