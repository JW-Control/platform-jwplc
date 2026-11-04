# v2.1.0-alpha.17 — Ejemplos limpios con API de «.», OpenPLC Editor 4.3.3 y HMI con temas

**Canal:** PreRelease de desarrollo (índice dev). No sustituye a `v2.0.0` estable.
**Origen:** línea `JWPLC-Sim`, rama `v2.1.0-alpha.17/JWPLC-Sim/main` en platform-jwplc, con la línea
`v2.1.0-alpha.17/Editor-Upstream` (VPP 4.3) fusionada. Se publica sobre la 2.1.0-alpha.16.

## Descargas

| Qué | Dónde |
|---|---|
| **OpenPLC Editor – JWPLC Edition 4.3.3-jwplc.1** (Windows x64, con el HMI Designer adentro) | [`OpenPLC-Editor.exe`](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.3.3-jwplc.1/OpenPLC-Editor.exe) · [notas y SHA-256](https://github.com/JW-Control/openplc-editor/releases/tag/jwplc-editor-v4.3.3-jwplc.1) |
| **Paquete de placas JWPLC Basic (VPP) 2.1.0-alpha.17**, firmado (`jwcontrol-2026`) | [`JWPLC-Basic.vpp`](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.3.3-jwplc.1/JWPLC-Basic.vpp) |
| **JWPLC HMI Designer 2.1.0-alpha.17** para Arduino IDE | adjuntos a esta Release: `HMI-Designer-Setup.exe`, `HMI-Designer-Portable.exe` y `HMI-Designer-SHA256.txt` |
| **Package Arduino** `jwplc:esp32@2.1.0-alpha.17` | índice dev (ver README) |

Los instaladores no tienen firma de código: si Windows SmartScreen avisa, **Más información → Ejecutar de todas
formas**.

## Qué trae

- **Ejemplos limpios y numerados** (#225): de 138 ejemplos `JWPLC_*`/`JW_*` quedan 79, numerados por librería y
  cada uno con su prueba en el JWPLC Sim. Se retiraron las pruebas internas, los duplicados y el Runtime anterior
  (`JWPLC_LogicRuntime` y `JWPLC_LogicRuntime_UI`; último SHA con ellas: `d007b44`).
- **API con «.»** (#224, #233): los ejemplos ya no usan `::`, `*` ni `&`. Novedades de API que lo permiten:
  `JWPLC_Display.menuChanged()`/`menuNode()`, `rtc.lastErrorString()` y `rtc.setFromBuildTime()`,
  `BTN_EV_PRESS/RELEASE/REPEAT` y `JW_ButtonEvent`, y `JWPLC_FRAM.updateBlock()`. Las entradas y salidas se manejan con las funciones de Arduino: `digitalRead`,
  `digitalWrite`, `digitalReadBlock(I0_X)` y `digitalWriteBlock(Q0_X, bits)`.
- **Runtime** (#228–#230): ejemplos `Runtime_Intermitente` (dos TON, solo salidas) y `Runtime_Escalonado` (entrada
  I1 y dos TON). `JWPLC_Runtime.loadDemo()` reemplaza el programa guardado en la FRAM si es otro, así el ejemplo
  carga aunque el equipo tenga la «Demo feria».
- **HMI Designer 2.1.0-alpha.17** (#231, #232): temas claro y oscuro con tokens de diseño, indicadores de nivel,
  fuentes universales y la vista de enlaces de OpenPLC sin recortes y con su tema.
- **OpenPLC Editor 4.3.3-jwplc.1** (openplc-editor #14–#16): basado en OpenPLC Editor 4.3.3 de Autonomy, interfaz
  en español y VPP 2.1.0-alpha.17 con las pantallas *Serial* y *Network* nuevas para Modbus (#227).
  Los proyectos de alpha.16 se migran solos a la configuración Modbus nueva.
- **JW Libraries** (#226): `JW_FRAM` 1.0.4, `JW_MatrixButtons` 1.0.7, `JW_RTC` 1.0.3 y `JW_SD` 1.0.3 desde
  JW-Libraries.
- `JWPLC_Display`, `JW_FRAM` y `JW_MatrixButtons` vuelven a distribuirse **precompiladas** con los `.a`
  regenerados en esta alpha ([A17_CIERRE_PRECOMPILADAS_20261104.md](A17_CIERRE_PRECOMPILADAS_20261104.md)).

## Instalación

1. Package: en Arduino IDE agregar el índice dev e instalar **JW Control ESP32 Boards 2.1.0-alpha.17** (ver README).
2. OpenPLC Editor: ejecutar `OpenPLC-Editor.exe` (4.3.3-jwplc.1; actualiza una JWPLC Edition anterior).
3. En el editor, importar `JWPLC-Basic.vpp` (2.1.0-alpha.17) y elegir la placa **JWPLC BASIC**.

## Verificación

Ver `ALPHA17_STATUS.md` §3 (resultados de la matriz ESP32, del Sim y de los tests del HMI Designer sobre este árbol).

## Limitaciones conocidas

- Con el firmware de OpenPLC 4.3 ya no se depura por RS-485: solo por USB o por red.
- Los ejemplos que quedan no tienen todos todavía la cabecera de la Wiki (qué hace, qué conectar, qué mirar):
  H5 pasa a la siguiente alpha.
- Runtime y expansión: validados en el simulador, pendientes de validación en placa.
- Sólo Windows x64 para los instaladores.

## Versiones que forman esta Release

| Componente | Versión | Commit |
|---|---|---|
| Package `jwplc:esp32` | 2.1.0-alpha.17 | `release/v2.1.x` al publicar |
| HMI Designer | 2.1.0-alpha.17 | instaladores: árbol de la Release; dentro del editor: platform-jwplc `4cb23c5` |
| VPP JWPLC Basic | 2.1.0-alpha.17 | platform-jwplc `0647a84` |
| OpenPLC Editor – JWPLC Edition | 4.3.3-jwplc.1 | openplc-editor `2bf0b0c` (tag `jwplc-editor-v4.3.3-jwplc.1`) |
| JWPLC Sim (gate `SIM_REGRESSION`) | — | jwplc-sim `9622dca` |
