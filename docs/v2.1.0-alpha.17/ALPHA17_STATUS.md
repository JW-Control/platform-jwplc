# v2.1.0-alpha.17 — JWPLC Sim: estado vivo

Actualizado: `2026-11-16` (cierre: entran #238 y #239, `.a` de `JWPLC_Display` y `JWPLC_ModbusTCP` regenerados)

## 1. Ramas y sincronización

| Repo | Rol | Rama | SHA / estado |
|---|---|---|---|
| platform-jwplc | Base de la línea (`…/main`) | `v2.1.0-alpha.17/JWPLC-Sim/main` | `168c574` (#239) |
| platform-jwplc | Línea del editor | `v2.1.0-alpha.17/Editor-Upstream/main` | `0647a84` (VPP 4.3, #227); fusionada en `…/JWPLC-Sim/main` con #234; rama borrada el 14/11 |
| platform-jwplc | Feature de cierre | `v2.1.0-alpha.17/JWPLC-Sim/feature/cierre-alpha17` | PR hacia `…/main`, luego `…/main` → `release/v2.1.x` |
| openplc-editor | Línea cerrada | `v2.1.0-alpha.17/Editor-Upstream/main` | `2bf0b0c`, tag `jwplc-editor-v4.3.3-jwplc.1`; en `release/v2.1.x` @ `6886eed` (#16) |
| openplc-editor | Designer / VPP empaquetados | `PLATFORM_DESIGNER_SHA` / `PLATFORM_VPP_SHA` | platform `4cb23c5` / `0647a84` |
| jwplc-sim | Simulador | `main` | `5021377` (#32: prueba `Simular_USB` corregida; incluye #30 y #33); fijado en `sim-regression` de los dos workflows |

Ramas borradas el 04/11 (todas fusionadas): features de alpha.16 y alpha.17 en los tres repos. Sin PR y con su
contenido ya dentro: `v2.1.0-alpha.16/OpenPLC-HMI/feature/modo-claro-designer` @ `1817838`,
`…/feature/vpp-version-alpha16` @ `55286cd` (platform) y `…/feature/panel-lateral-hmi` @ `3531f31` (editor).

Ramas borradas el 14/11 (platform, todas contenidas en `…/main`): `v2.1.0-alpha.17/JWPLC-Sim/feature/mblock-poc` @
`852a415e`, `…/feature/integrar-editor-upstream` @ `333b4140` y `v2.1.0-alpha.17/Editor-Upstream/main` @ `0647a84c`.

Para borrar el 16/11 (platform, todas contenidas en `…/main`): `ccr-4e8cc1e7-lygr9t` @ `8f3caa25` (#238),
`v2.1.0-alpha.17/JWPLC-Sim/feature/hmi-designer` @ `65128335` (#239) y `…/feature/arranque-io-enable` @ `cd238fdb`
(#237, squash; su árbol es igual al de `f0e5edc`). jwplc-sim: `feature/alpha17-editor-componentes` @ `25d14800` (#33) y
`fix/alpha17-prueba-simular-usb` @ `ea11be89` (#32).

## 2. Qué entró en `…/main`

| PR | Rama | Merge | Contenido |
|---|---|---|---|
| #223 | `…/feature/apertura-linea` | `c651576` | plan, estado y `SIM_REGRESSION` una sola vez por árbol |
| #224 | `…/feature/api-punto` | `54a275a` | API con «.» y ejemplos `JWPLC_*` sin `::`, `*` ni `&` |
| #225 | `…/feature/limpieza-ejemplos` | `f2c0aab` | 138 → 79 ejemplos numerados; fuera `JWPLC_LogicRuntime(_UI)` |
| #226 | `…/feature/sync-jw-libraries` | `8019197` | JW-Libraries: FRAM 1.0.4, MatrixButtons 1.0.7, RTC 1.0.3, SD 1.0.3 |
| #228 | `…/feature/runtime-ejemplo-ton` | `2c96699` | `Runtime_Intermitente` (dos TON) |
| #229 | `…/feature/runtime-intermitente-arranque` | `bb7cc9a` | `loadDemo()`: reemplaza el programa de la FRAM si es otro |
| #230 | `…/feature/runtime-ejemplo-escalonado` | `e2d967c` | `Runtime_Escalonado` (I1 + dos TON) |
| #231 | `…/feature/hmi-designer-openplc` | `4cb23c5` | vista de enlaces de OpenPLC sin recorte y con su tema |
| #232 | `…/feature/hmi-designer` | `b6c8508` | temas claro/oscuro, indicadores de nivel, fuentes universales |
| #233 | `…/feature/ejemplos-digital-block` | `56950b3` | ejemplos con `digitalRead/Write` y `digitalRead/WriteBlock` |
| #227 | `…/Editor-Upstream/feature/vpp-4.3` | `0647a84` | VPP 2.1.0-alpha.17 (Serial/Network) en la línea Editor-Upstream |
| #234 | `…/feature/integrar-editor-upstream` | `852a415` | trae `Editor-Upstream/main` a esta línea |
| #237 | `…/feature/arranque-io-enable` | `f0e5edc` | aviso y reintento si el TCA no arranca (EN_IO=GPIO27); errores de compilación para `Serial1`, `digitalWrite(I0_x)`, `pinMode` cruzado y GPIO27; `core.a` regenerado |
| #236 | `…/feature/mblock` | `c756170` | prueba de concepto de mBlock en `tools/mblock-poc/` |
| #238 | `ccr-4e8cc1e7-lygr9t` | `005677a` | Modbus TCP: varios clientes, cierre por inactividad, Unit ID 0/255; `JW_ModbusRTU` excepción 02 |
| #239 | `…/feature/hmi-designer` | `168c574` | HMI Designer (André): TREND, multipágina, menú CARDS, 64 campos en Flash, inspectores nuevos |

## 3. Librerías precompiladas y evidencia del cierre

Regeneradas y de vuelta en `precompiled=full`: `JWPLC_Display` 1.1.0 (otra vez el 16/11, con #239), `JWPLC_ModbusTCP`
0.1.0 (16/11, con #238; decisión del propietario), `JW_FRAM` 1.0.4 y `JW_MatrixButtons` 1.0.7
([A17_CIERRE_PRECOMPILADAS_20261104.md](A17_CIERRE_PRECOMPILADAS_20261104.md), §5 para el 16/11). Siguen
precompiladas sin cambios: `JWPLC_TFT`, `JWPLC_Ethernet`, `JW_SD`. En fuente: `JWPLC_Runtime`, `JWPLC_Expansion`,
`JWPLC_UI_Widgets`, `JWPLC_WebScreen` y el resto de librerías sin `.a`.

Evidencia del 16/11 (con #238 y #239 dentro):

- `[ejecutado]` compilación ESP32 real de los 34 sketches afectados por #238/#239 con los `.a` nuevos: 34/34, 0 advertencias.
- `[ejecutado]` HMI Designer `tests/run-all.cjs`: 34/34. Al traer #239 fallaban 7; se corrigieron en el cierre:
  - Errores reales del Designer: el selector tipográfico del inspector llamaba a `syncInspector()`, que no existe en
    ese archivo (error de JS y el inspector no se refrescaba al cambiar fuente/tamaño); y un clic en una categoría del
    menú Iconos lo cerraba (el cierre de menús de `app.js` usaba `contains(e.target)` sobre botones ya redibujados).
  - Pruebas desactualizadas por cambios intencionales de #239: stubs de host sin `fillRoundRect`/`drawRoundRect`,
    `drawLine`, `JW_SD.h` ni `jwplcUIRuntimeEnsureLinked()`; el ICON ahora envía 64x64 en 2 bloques (búfer compartido
    320x8) en vez de 8; el tamaño de fuente y la posición de la etiqueta del ICON se eligen con controles nuevos; la
    prueba de la barra vertical STACKED tenía coordenadas mal calculadas y se pisaba con otras barras.
  - `JWPLC_Icons`: los métodos pasan a `inline` con un dato por tamaño (`JWPLC_Icons.motor(48)` enlaza 4,8 KB en vez de
    16,6 KB) y el Designer los usa para los iconos del catálogo sin colores propios (§6 del documento de precompiladas).

Evidencia del 04/11, sobre el árbol de la feature de cierre de entonces:

- `[ejecutado]` compilación ESP32 real (`jwplc_local:esp32:jwplcbasic`, arduino-esp32 3.3.8) de los 79 ejemplos
  `JWPLC_*`/`JW_*` con los `.a` nuevos: **79/79 compilan**. 74 sin advertencias; los 5 que usan `JWPLC_Runtime` dan una
  (`-Wvolatile` en `JWPLC_Runtime.cpp:594`, `++g_postSeq`, viene de alpha.16).
- `[ejecutado]` HMI Designer `tests/run-all.cjs`: 21/21 (incluye `codegen-smoke`: los headers generados compilan y sus llamadas existen en los `.a` nuevos).
- `[ejecutado]` jwplc-sim `9622dca` contra `56950b3`: paridad 0 diferencias; los ejemplos tocados en el
  cierre de la línea (Runtime ×4, 00.Plantilla, 01.DigitalIO_Basic, 03.Display_HMI_Pages) 7/7.
- `SIM_REGRESSION` (matriz completa del Sim) con jwplc-sim `5021377`: la corre el CI del PR de cierre (#235).
- `[pendiente]` benchmark de compilación en la PC del propietario.

## 4. Qué funciona en la máquina

- `[ejecutado]` en JWPLC Sim: los 79 ejemplos con su prueba; Runtime_Intermitente y Runtime_Escalonado.
- `[no verificado]` en placa: `Runtime_Intermitente` y `Runtime_Escalonado` con la «Demo feria» en la FRAM.
- `[ejecutado]` OpenPLC Editor 4.3.3-jwplc.1 compila un proyecto JWPLC Basic de punta a punta (en la nube).
- `[no verificado]` en placa: firmware de OpenPLC 4.3 con las pantallas Serial/Network nuevas.

## 5. Qué queda para la siguiente alpha

- H5 Wiki: 68 de 79 ejemplos sin la cabecera «qué conectar / qué mirar».
- Runtime: columna de marcas M1..M8 en el mapa del editor (hoy M no se ve).
- Instalador del Sim: «Reintentar» no sigue y «Cancelar» sí termina la instalación.
- Numerar los ejemplos `Runtime_*`, `Widgets_*`, `Expansion_*` y `JW_*` (pregunta abierta al propietario).
- `JWPLC_Runtime.cpp:594`: `++` sobre `volatile` (advertencia `-Wvolatile` en los 5 ejemplos con Runtime).
- Deuda del editor: `validate:arch` con 4 avisos (ports importan `backend/shared/compile/jwplc-hmi*`).

## 6. Para retomar

1. Fusionar la feature de cierre en `…/main`; luego PR `…/main` → `release/v2.1.x` (publica con el
   cambio de `JWPLC_RELEASE_VERSION` del README). Requiere OK explícito del propietario.
2. Tras publicar: sincronizar `release/v2.1.x` → `main` con merge commit, PR de índices y fast-forward de
   `release/v2.1.x`; en jwplc-sim, `package.lock` → tag `v2.1.0-alpha.17` y Release del Sim.
3. Borrar `v2.1.0-alpha.17/JWPLC-Sim/main` y `v2.1.0-alpha.17/Editor-Upstream/main` (anotar sus SHAs aquí) y
   actualizar `RAMAS_Y_TAGS.md` en jwplc-dev-mcp.
4. André sigue desde `v2.1.0-alpha.17/JWPLC-Sim/main` (o la línea siguiente) en una feature nueva. Si cambia el
   catálogo de iconos: `node icons/export-cpp-library.cjs` y regenerar el `.a` de `JWPLC_Display`.

`NEXT_GATE=A17_PUBLICACION`
