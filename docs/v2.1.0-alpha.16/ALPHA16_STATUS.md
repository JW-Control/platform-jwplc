# v2.1.0-alpha.16 — JWPLC Sim: estado vivo

Actualizado: `2026-11-01` (integración de Runtime, expansión, widgets y HMI Designer de André y Kenyi)

## 1. Ramas y sincronización

| Repo | Rol | Rama | SHA / estado |
|---|---|---|---|
| platform-jwplc | Base de la línea (`…/main`) | `v2.1.0-alpha.16/JWPLC-Sim/main` | `1a33951` (#209) |
| platform-jwplc | Feature en curso | `v2.1.0-alpha.16/JWPLC-Sim/feature/ejemplos-display-andre` | ejemplos 08/09, arreglos de Display y este estado |
| openplc-editor | Base de la línea | (lo lleva el hilo «Revisar orden de ramas alpha 16») | — |
| jwplc-sim | Simulador | `claude/jwplc-basic-simulator-8dgcen` | `066d1d9` (PR #2), `package.lock` en `…/main` @ `a0900c0` |

Ramas de Kenyi con el nombre viejo, contenidas en `…/main` (se borran cuando él confirme):
`v2.1.0-alpha.16/OpenPLC-HMI/feature/modo-claro-designer` @ `1817838` y
`v2.1.0-alpha.16/OpenPLC-HMI/feature/vpp-version-alpha16` @ `55286cd`.

## 2. Qué entró en `…/main` (01/11/2026)

| PR | Rama | Merge | Contenido |
|---|---|---|---|
| #199 | `…/feature/api-tft-graficos` | `b31cf56` | API gráfica de `JWPLC_TFT` 0.5.0 (modo fuente, TFT_eSPI en `src/`) |
| #200 | `…/feature/ui-widgets-runtime` | `86c0626` | `JWPLC_UI_Widgets` 0.5.0 |
| #201 | `…/feature/ui-tendencia` | `3af6770` | tendencia `JWPLC_UITrend` |
| #202 | `…/feature/ui-f0-resto` | `8629664` | teclado, QR, fuentes 26/7SEG_48, `sprite.scroll` |
| #203 | `…/feature/runtime-mvp` | `55aaf5d` | `JWPLC_Runtime` 0.1.0 (panel estilo LOGO! 8) |
| #204 | `…/feature/runtime-expansion` | `686b617` | `JWPLC_Expansion` 0.1.0 (esclavo) + Runtime 0.2.0 (maestro RTU) |
| #198 | `…/feature/apertura-linea` | `0f1b74f` | plan, estado y `CLAUDE.md` de la línea |
| #205 | `…/feature/hmi-designer` (André) | `3ef27ba` | iconos animados e industriales, baliza, piloto 2..8, medidor circular, registro de eventos |
| #206 | `…/feature/modo-claro-designer` (Kenyi) | `23cef32` | modo claro del Designer |
| #207 | `…/feature/vpp-version` (Kenyi) | `75283fb` | versión del VPP |
| #208 | `…/feature/integracion-designer` | `a0900c0` | arreglos de integración: EventLog compila, listener del modo claro, `JWPLC_Display` en modo fuente |
| #209 | `…/feature/designer-pruebas-andre` | `1a33951` | spec ICON, copia de iconos fuera del catálogo, pruebas del Designer 18/18 |

## 3. Librerías en modo fuente (regenerar `.a` al cierre)

`JWPLC_TFT` 0.5.0 (incluye TFT_eSPI), `JWPLC_Display` 1.1.0, `JWPLC_UI_Widgets` 0.5.0, `JWPLC_Runtime` 0.2.0,
`JWPLC_Expansion` 0.1.0, `JWPLC_WebScreen` 0.1.0 y `JW_MatrixButtons` 1.0.6 (teclas virtuales). Al cierre: regenerar sus `.a` y benchmark de compilación en la PC del propietario.

## 4. Qué funciona en la máquina

`[ejecutado]` en JWPLC Sim (nativo); `[no verificado]` en placa:

- Runtime de panel (MVP): edición FBD, RUN/STOP, pantalla RUN sobre la IDLE.
- Expansión RS-485: alta desde la pantalla, `1.Q1 ← 1.I1` en RUN, ERR `X1` sin esclavo, búsqueda y vínculo
  de un esclavo bloqueado por sketch (gate `JWSIM_EXPANSION_RTU`). `XU` no se puede probar (misma MAC en el sim).
- Widgets, tendencia, teclado y QR.
- HMI Designer: medidor circular (ejemplo 08), iconos animados (08) y registro de eventos (09).
- Pantalla en el navegador por Ethernet (`JWPLC_WebScreen`, gate `JWSIM_WEBSCREEN`):
  [A16_PANTALLA_WEB_20261101.md](A16_PANTALLA_WEB_20261101.md), con el guion de la demo del 03/11.

## 5. Qué existe aislado o detrás de un flag

`PENDIENTE_DE_PROMOCIÓN:` `JWPLC_LogicRuntime` y `JWPLC_LogicRuntime_UI` siguen en el package; se retiran
cuando el propietario valide el MVP del Runtime en placa.

## 6. Evidencia vigente

- Matriz completa de jwplc-sim sobre `ea4f349` (= árbol de `a0900c0`): 133/133 compilan, corren y pasan.
- `JWSIM_PARITY` PASS sobre `…/main` y sobre esta feature.
- jwplc-sim `tests/examples/JWPLC_Display`: 17/17 con los ejemplos 08 y 09 (se retiró la prueba de
  `Display_Alpha8_HMI_Gate`, cuyo ejemplo ya no existe).
- Suite del HMI Designer (`tests/run-all.cjs`): 18/18.
- Pin TE de la TFT: no está libre en la placa v2.0; el corte de imagen (tearing) se acepta.

## 7. Plan de hitos y NEXT

H1–H4 en [ALPHA16_PLAN.md](ALPHA16_PLAN.md). Runtime:
[A16_RUNTIME_MVP_20261101.md](A16_RUNTIME_MVP_20261101.md) y
[A16_RT2_EXPANSION_DISENO_20261101.md](A16_RT2_EXPANSION_DISENO_20261101.md).

1. Runtime: registro de eventos en `JWPLC_UI_EventLog` (`…/feature/runtime-eventlog`, PASS_SIM).
2. Validación en placa del MVP y de la expansión con dos JWPLC Basic (propietario).
3. Retirar `JWPLC_LogicRuntime`/`_UI` y actualizar `CLAUDE.md` tras el punto 2.
4. Enlazar páginas del HMI Designer con variables del Runtime.
5. Servidor web y vinculación del celular (F9), al final.

`NEXT_GATE=RT_PLACA_MVP` · `HARDWARE=PENDIENTE_PROPIETARIO`

## 8. Para retomar

- Validación en el simulador: solo los ejemplos de lo que se toca; la matriz completa, al cierre.
- Cada ejemplo nuevo del package lleva su `.jwtest.json` en jwplc-sim.
- El proxy de la nube no deja borrar ramas remotas ni compilar ESP32: lo hace el propietario.
- Pendiente menor del registro de eventos: el indicador de página del Display (`02/02`) se dibuja encima de
  «SD:OK» en la cabecera del historial mientras está visible.
