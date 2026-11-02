# v2.1.0-alpha.16 — JWPLC Sim: estado vivo

Actualizado: `2026-11-02` (feria: pantalla web, página HMI del Runtime y editor 4.2.8-jwplc.9)

## 1. Ramas y sincronización

| Repo | Rol | Rama | SHA / estado |
|---|---|---|---|
| platform-jwplc | Base de la línea (`…/main`) | `v2.1.0-alpha.16/JWPLC-Sim/main` | `da628bb` (#216) |
| openplc-editor | Base de la línea (`…/main`) | `v2.1.0-alpha.16/JWPLC-Sim/main` | `2ef069b` (#11), prerelease `4.2.8-jwplc.9` |
| openplc-editor | Designer / VPP empaquetados | `PLATFORM_DESIGNER_SHA` = `PLATFORM_VPP_SHA` | platform `94edd7d` (Designer y VPP 2.1.0-alpha.16) |
| jwplc-sim | Simulador | `claude/jwplc-basic-simulator-8dgcen` | PR #9 (`a749024`); `package.lock` → `…/main` `da628bb` |

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
| #210 | `…/feature/ejemplos-display-andre` | `74d65ed` | ejemplos 08/09 del Display (medidor, historial) |
| #211 | `…/feature/runtime-eventlog` | `12be056` | eventos del Runtime en `JWPLC_UI_EventLog` |
| #212 | `…/feature/pantalla-web` | `de2a8dc` | `JWPLC_WebScreen` 0.1.0: la TFT en el navegador por Ethernet |
| #213 | `…/feature/eventlog-indicador` | `3e9b7ab` | contador y SD del historial fuera del indicador NN/TT |
| #214 | `…/feature/runtime-hmi` | `852cbd8` | página HMI en el Runtime + proyecto «Runtime» del Designer |
| #215 | `…/feature/pantalla-web-latencia` | `94edd7d` | espera larga, tecla en el pedido, trozos de 1 KB (Windows) |
| #216 | `…/feature/pantalla-web-optimizaciones` | `da628bb` | tramos de fila (`JWS2`), página con ETag; keep-alive desactivado |

Editor (02/11): #11 `…/feature/editor-jwplc9` → `2ef069b`, con el panel lateral HMI de Kenyi (`3531f31`).

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
  `[visto]` en placa (propietario, 02/11) con #215 y #216: cambio de página rápido.
- Página HMI del Runtime (► / ◄ desde RUN): [A16_RUNTIME_HMI_20261101.md](A16_RUNTIME_HMI_20261101.md).
- Editor 4.2.8-jwplc.9 publicado: Designer 2.1.0-alpha.16 dentro de OpenPLC. Con el package sin publicar,
  compilar con *Package Source › Local Development* (`jwplc_local`).

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

1. Validación en placa del MVP, la página HMI del Runtime y la expansión con dos JWPLC Basic (propietario).
2. Retirar `JWPLC_LogicRuntime`/`_UI` y actualizar `CLAUDE.md` tras el punto 1.
3. Registrar la línea en `RAMAS_Y_TAGS.md` (hilo «Revisar orden de ramas alpha 16», espera OK).
4. WiFi y vinculación del celular (F9), al final.
5. Cierre: regenerar los `.a` de §3, matriz completa de jwplc-sim, benchmark en la PC del propietario.

`NEXT_GATE=RT_PLACA_MVP` · `HARDWARE=PENDIENTE_PROPIETARIO`

## 8. Para retomar

- Validación en el simulador: solo los ejemplos de lo que se toca; la matriz completa, al cierre.
- Cada ejemplo nuevo del package lleva su `.jwtest.json` en jwplc-sim.
- El proxy de la nube no deja borrar ramas remotas ni compilar ESP32: lo hace el propietario.
- El proxy tampoco deja crear tags: el de `jwplc-editor-v4.2.8-jwplc.N` lo crea el propietario.
- Ramas fusionadas que el propietario borra: features #210–#216 de platform, `…/feature/editor-jwplc9` del
  editor y las de jwplc-sim #6–#9.
- `JWPLC_WebScreen`: `KEEP_ALIVE` queda en `false` (más lento en placa con Windows); no reactivar sin probar en placa.
