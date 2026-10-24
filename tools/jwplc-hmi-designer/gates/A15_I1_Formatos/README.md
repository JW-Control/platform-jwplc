# Gate A15 I1 · Formatos de VALUE y BOOL «Piloto»

```text
A15_I1_HOST_PARITY=PASS        (tests/value-format-parity.cjs: firmware real vs Designer)
A15_I1_CODEGEN=PASS            (golden i1Formats.h + codegen-smoke)
A15_I1_ESP32_COMPILE=PASS      (arduino-cli, jwplc_local:esp32:jwplcbasic)
A15_I1_PHYSICAL=[no verificado]
```

Archivos:

- `A15_I1_Formatos.jwhmi`: el proyecto del Designer (mismo que `tests/fixtures.cjs` → `i1Formats`).
- `JWPLC_HMI_Generated.h`: header generado (igual a `tests/golden/i1Formats.h`).
- `A15_I1_Formatos.ino`: cambia las variables una vez por segundo.

## Qué mirar en la TFT

| Campo | Formato | Se espera |
|---|---|---|
| Piezas | 7 dígitos, coma de miles | `1,234`, `2,468`, … alineado a la derecha y con marco intacto |
| kWh | 6 + 1 decimal, punto de miles | `12,5` … `1.234,5` (decimal con coma) |
| Resta | Duración MM:SS con signo, 2 dígitos, ×3 | `01:15` → `00:00` → `-00:01` … |
| Marcha | Duración HH:MM:SS, ms, 3 dígitos | `00:00:01` … (las horas se rellenan a 2) |
| Ciclo | Duración compacta | `7s`, `14s` … `1m03s` … `1h00m` |
| Bomba | BOOL piloto ×2 | círculo gris ↔ verde cada segundo, aro blanco |
| Alarma | BOOL piloto ×1 con marco | con Resta < 0: círculo amarillo y **todo el campo** en rojo; el marco no cambia |

Criterio PASS:

- los textos coinciden con la vista del Designer para el mismo valor;
- sin parpadeo: sólo se repinta el valor que cambió;
- sin restos cuando el texto se acorta (por ejemplo, `1m03s` frente a `7s` en Ciclo).
