# v2.1.0-alpha.17 — Ejemplos limpios con API de «.», OpenPLC Editor 4.3.3 y HMI con temas

**Canal:** PreRelease de desarrollo (índice dev). No sustituye a `v2.0.0` estable.
**Origen:** línea `JWPLC-Sim`, rama `v2.1.0-alpha.17/JWPLC-Sim/main` en platform-jwplc, con la línea
`v2.1.0-alpha.17/Editor-Upstream` (VPP 4.3) fusionada. Se publica sobre la 2.1.0-alpha.16.

## Descargas

| Qué | Dónde |
|---|---|
| **OpenPLC Editor – JWPLC Edition 4.3.3-jwplc.2** (Windows x64, con el HMI Designer adentro) | [`OpenPLC-Editor.exe`](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.3.3-jwplc.2/OpenPLC-Editor.exe) · [notas y SHA-256](https://github.com/JW-Control/openplc-editor/releases/tag/jwplc-editor-v4.3.3-jwplc.2) |
| **Paquete de placas JWPLC Basic (VPP) 2.1.0-alpha.17**, firmado (`jwcontrol-2026`) | [`JWPLC-Basic.vpp`](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.3.3-jwplc.2/JWPLC-Basic.vpp) |
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
- **HMI Designer 2.1.0-alpha.17** (#231, #232, #239): temas claro y oscuro con tokens de diseño, indicadores de
  nivel, fuentes universales y la vista de enlaces de OpenPLC sin recortes y con su tema. Nuevos widgets **TREND**
  (hasta 3 canales y 2 umbrales, registro en MicroSD) y **`QR`** estático/dinámico (`JWPLC_UIQR`, presets Wi-Fi/URL),
  proyectos de **varias páginas** con **subvistas internas** (`views`, hasta 8 por página), **ventanas emergentes
  modales** (`popups[]`, I4–I6 con presets `INFO`/`WARN`/`CRIT`/`CONFIRM`, escala `1×–3×`, navegación al cerrar y selector
  de variables `bool` del proyecto), menú **CARDS**, inspectores de iconos, EVENT_LOG y MENU, enlaces OpenPLC en
  `MENU`, `TREND` y Baliza independiente, y hasta **64 campos** por proyecto en `JWPLC_Display`. Biblioteca de iconos
  precompilada `JWPLC_Icons` (`JWPLC_Icons.motor(48)`): el header del Designer la usa y sólo se enlaza el tamaño elegido.
- **Display TFT**: `JWPLC_TFT` 0.5.0 soporta `beginBatch()`/`endBatch()` anidados por profundidad; `JWPLC_Display` 1.1.0
  elimina destellos negros en tarjetas de `MENU` y evita el parpadeo de 1 Hz del reloj mientras hay un modal abierto.
  Árbitro único de teclas: el OK que sale del selector de páginas sólo entra a la página (antes también actuaba sobre
  el EVENT_LOG o el MENU de esa página); el segundo OK ya va al widget.
- **Ledcito** (#240): ejemplo `JWPLC_Display/17.Display_Ledcito`, la mascota de JW Control animada en la TFT (sprite de
  4 bits a pantalla completa, OK reinicia y ESC vuelve a IDLE).
- **Modbus TCP** (#238): el Server de `JWPLC_ModbusTCP` atiende varios clientes a la vez, cierra las conexiones
  inactivas y responde también a Unit ID 0 y 255 (`setStrictUnitId(true)` vuelve a la comparación exacta).
  En `JW_ModbusRTU`, una tabla sin mapa responde la excepción 02.
- **mBlock 5** (#236): extensión `jwplc_basic` 0.12.0, bridge 0.6.0 e instalador en `tools/mblock-poc/` (prueba de
  concepto, pendiente de validar en otra máquina).
- **OpenPLC Editor 4.3.3-jwplc.2** (openplc-editor #14–#17; jwplc.2 trae el HMI Designer final de esta alpha): basado en OpenPLC Editor 4.3.3 de Autonomy, interfaz
  en español y VPP 2.1.0-alpha.17 con las pantallas *Serial* y *Network* nuevas para Modbus (#227).
  Los proyectos de alpha.16 se migran solos a la configuración Modbus nueva.
- **JW Libraries** (#226): `JW_FRAM` 1.0.4, `JW_MatrixButtons` 1.0.7, `JW_RTC` 1.0.3 y `JW_SD` 1.0.3 desde
  JW-Libraries.
- **Arranque de E/S y restricciones de programación** (#237): si el expansor de E/S no responde al arrancar, el
  JWPLC lo avisa por el Serial y lo reintenta cada segundo hasta habilitar los relés (`JWPLC_IO.fault()`). Ya no
  compilan `Serial1`, `digitalWrite(I0_x)`, `pinMode(I0_x, OUTPUT)`, `pinMode(Q0_x, INPUT)` ni el uso del GPIO27.
- `JWPLC_Display`, `JWPLC_TFT`, `JWPLC_ModbusTCP`, `JW_FRAM` y `JW_MatrixButtons` vuelven a distribuirse **precompiladas**
  (`precompiled=full`) con sus `.a` regenerados en esta alpha
  ([A17_CIERRE_PRECOMPILADAS_20261104.md](A17_CIERRE_PRECOMPILADAS_20261104.md)).

## Instalación

1. Package: en Arduino IDE agregar el índice dev e instalar **JW Control ESP32 Boards 2.1.0-alpha.17** (ver README).
2. OpenPLC Editor: ejecutar `OpenPLC-Editor.exe` (4.3.3-jwplc.2; actualiza una JWPLC Edition anterior).
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
| OpenPLC Editor – JWPLC Edition | 4.3.3-jwplc.2 | openplc-editor #17 (tag `jwplc-editor-v4.3.3-jwplc.2`); Designer platform `3f29398`, VPP `0647a84` |
| JWPLC Sim (gate `SIM_REGRESSION`) | — | jwplc-sim `c807af1` |
