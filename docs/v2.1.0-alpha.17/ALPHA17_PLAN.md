# v2.1.0-alpha.17 — JWPLC Sim: plan

Línea `JWPLC-Sim`, segunda vuelta: **limpieza, actualización y renombre de ejemplos** (propietario, 02/11/2026).
Abierta el 2026-11-02, después de publicar 2.1.0-alpha.16. Número reservado en `RAMAS_Y_TAGS.md`.

```text
LINEA=JWPLC-Sim
RAMA=v2.1.0-alpha.17/JWPLC-Sim/main (desde release/v2.1.x @ d007b44 = main = 2.1.0-alpha.16 + índices)
PUBLICA_COMO=2.1.0-alpha.17
EDITOR=sin cambios previstos (si un hito lo toca, misma rama en openplc-editor)
```

Punto de partida: [A17_ANALISIS_API_EJEMPLOS_20261102.md](A17_ANALISIS_API_EJEMPLOS_20261102.md) y
[A17_EJEMPLOS_LISTA_SIM_20261102.md](A17_EJEMPLOS_LISTA_SIM_20261102.md):
138 ejemplos `JWPLC_*`/`JW_*`, patrones P1–P10 que chocan con la regla del «.» (CLAUDE.md §3.9) y
propuesta de borrado y renombre (~55–60 ejemplos al final).

## Hitos

| Hito | Resultado | Criterio de cierre | Repos |
|---|---|---|---|
| H0 Apertura | Rama de la línea; CI de PR en las features de la alpha; `SIM_REGRESSION` una sola vez por árbol | Este PR fusionado; `A17_SIM_REGRESSION_UNA_VEZ` visto en el PR de cierre y en la Release | platform-jwplc |
| H1 JW_* desde JW-Libraries | JW-Libraries al día con el package (SD DataLog, MatrixButtons 1.0.6, FRAM) y su sincronización apuntando a esta línea | PR de sincronización `…/feature/sync-jw-libraries` fusionado | JW-Libraries, platform-jwplc |
| H2 API con «.» | Lo que faltaba para que los ejemplos no usen `::`, `*` ni `&` (solo agrega, no rompe) | Compila contra la API real; ejemplos afectados compilan (ESP32) y pasan en el Sim | platform-jwplc, JW-Libraries |
| H3 Ejemplos corregidos y nuevos | P1–P9 corregidos; ejemplo nuevo donde el viejo enseñaba `::` | Ídem H2, cada ejemplo con su `.jwtest.json` en jwplc-sim | platform-jwplc, jwplc-sim |
| H4 Limpieza y renombre | Borrar pruebas internas, duplicados y LogicRuntime; numerar el resto | OK del propietario por grupo; matriz del Sim sin huecos | platform-jwplc, jwplc-sim |
| H5 Wiki | Cada ejemplo sirve para la Wiki del JWPLC: cabecera (qué hace, qué conectar, qué mirar), README de la librería y prueba en el Sim | Lista de verificación en el gate de H5 | platform-jwplc |
| H6 Cierre | `.a` regenerados de lo que pasó a fuente, matriz completa (Sim + ESP32), pin de jwplc-sim | Release con `SIM_REGRESSION` en verde | platform-jwplc |

API de H2 (propuesta del análisis):

1. `JWPLC_Display.menuChanged(MENU)` + `JWPLC_Display.menuNode()`.
2. `JWPLC_ModbusRTU.okCount()/crcErrors()/timeouts()/exceptions()`.
3. `rtc.lastErrorString()` y `rtc.setFromBuildTime()` en `JW_RTC`.
4. `BTN_EV_PRESS/RELEASE/REPEAT` y `JWPLC_ButtonEvent` en la botonera.
5. `JWPLC_FRAM.writeIfChanged(dir, datos)`.
6. Global `JWPLC_DWIN` sobre el RS-485 del JWPLC.

## Reglas que valen aquí

- Librería que cambia → modo fuente durante la alpha (se quita `precompiled=full`); su `.a` se regenera una
  vez al cierre con `tools/alpha16/regenerate_precompiled.sh`.
- Las `JW_*` se cambian en JW-Libraries y llegan por su sincronización (no se editan a mano en el package).
- Validación: compilar (ESP32) y simular solo los ejemplos afectados; la matriz completa, al cierre.
