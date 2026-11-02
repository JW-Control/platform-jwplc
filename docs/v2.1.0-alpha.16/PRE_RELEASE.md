# v2.1.0-alpha.16 — Runtime de panel, pantalla en el navegador y JWPLC Sim

**Canal:** PreRelease de desarrollo (índice dev). No sustituye a `v2.0.0` estable.
**Origen:** línea `JWPLC-Sim`, rama `v2.1.0-alpha.16/JWPLC-Sim/main` en platform-jwplc y en openplc-editor.
Se publica sobre la 2.1.0-alpha.15.

## Descargas

| Qué | Dónde |
|---|---|
| **OpenPLC Editor – JWPLC Edition 4.2.8-jwplc.9** (Windows x64, con el HMI Designer adentro) | [`OpenPLC-Editor.exe`](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.2.8-jwplc.9/OpenPLC-Editor.exe) · [notas y SHA-256](https://github.com/JW-Control/openplc-editor/releases/tag/jwplc-editor-v4.2.8-jwplc.9) |
| **Paquete de placas JWPLC Basic (VPP) 2.1.0-alpha.16**, firmado (`jwcontrol-2026`) | [`JWPLC-Basic.vpp`](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.2.8-jwplc.9/JWPLC-Basic.vpp) |
| **JWPLC HMI Designer 2.1.0-alpha.16** para Arduino IDE | adjuntos a esta Release: `HMI-Designer-Setup.exe`, `HMI-Designer-Portable.exe` y `HMI-Designer-SHA256.txt` |
| **Package Arduino** `jwplc:esp32@2.1.0-alpha.16` | índice dev (ver README) |

Los instaladores no tienen firma de código: si Windows SmartScreen avisa, **Más información → Ejecutar de todas
formas**.

## Qué trae

- **Runtime de panel estilo LOGO! 8** (`JWPLC_Runtime` 0.2.0, ejemplos `Runtime_Panel` y `Runtime_HMI`): el equipo
  se programa desde la pantalla y la botonera, sin escribir lógica en el sketch. Editor FBD en la TFT, RUN/STOP,
  parámetros, programa guardado en FRAM y copia en microSD, red y diagnóstico. Página HMI propia (► / ◄ desde RUN)
  diseñada con el proyecto «Runtime» del HMI Designer, y eventos del programa en el historial de la pantalla.
- **Módulos de expansión por RS-485** (`JWPLC_Expansion` 0.1.0, ejemplo `Expansion_Esclavo_Basic`): otro JWPLC
  Basic hace de esclavo de E/S; el Runtime lo da de alta desde la pantalla y usa sus entradas y salidas (`1.I1`,
  `1.Q1`…) como maestro Modbus RTU.
- **La pantalla en el navegador** (`JWPLC_WebScreen` 0.1.0, ejemplos `01.Pantalla_Web` y `02.Runtime_En_Vivo`):
  con `JWPLC_WebScreen.begin()` el equipo sirve por Ethernet una página que repite la TFT en vivo con la botonera
  virtual (o flechas, Enter y Esc del teclado). Sólo envía lo que cambió de cada fila.
- **API gráfica de `JWPLC_TFT` 0.5.0:** sprites, fuentes 16/26 y 7 segmentos de 48 px, `sprite.scroll`, y
  TFT_eSPI 2.5.43 incluida dentro de la librería (no hay que instalarla ni configurarla).
- **Widgets de pantalla** (`JWPLC_UI_Widgets` 0.5.0, ejemplos `Widgets_*`): lista de parámetros editables,
  tendencia, teclado en pantalla y código QR.
- **`JWPLC_Display` 1.1.0:** medidor circular, iconos animados e industriales, registro de eventos en FRAM y SD
  (ejemplos `08.HMI_Medidores_Animados` y `09.HMI_Registro_Eventos`).
- **`JW_MatrixButtons` 1.0.6:** teclas virtuales, para que la pantalla web y el simulador pulsen la botonera.
- **HMI Designer 2.1.0-alpha.16:** iconos animados (baliza por pisos, piloto de 2 a 8 estados, motores, pistones),
  medidor circular, plantilla de historial de eventos, modo claro y proyecto «Runtime».
  Su instalador agrega la extensión del Arduino IDE 2 (`jwplc-hmi-launcher` 0.1.8): el botón de arriba a la
  derecha del editor usa ahora el ícono del Designer en lugar del cuadro naranja «JW».
- **OpenPLC Editor 4.2.8-jwplc.9:** Designer 2.1.0-alpha.16 incluido y panel lateral HMI a la derecha del área de
  trabajo. VPP 2.1.0-alpha.16 sin la pantalla «Pantalla HMI» en *Configuration*.
- **JWPLC Sim** (simulador del JWPLC Basic, `JW-Control/jwplc-sim`): un proyecto de OpenPLC compila con la cadena
  real del editor y corre simulado; guardar en el editor recarga la simulación. La publicación del package pasa
  antes por su gate `SIM_REGRESSION` (todos los ejemplos con sus pruebas).
- `JWPLC_TFT`, `JWPLC_Display` y `JW_MatrixButtons` vuelven a distribuirse **precompiladas** con los `.a`
  regenerados en esta alpha ([A16_CIERRE_PRECOMPILADAS_20261102.md](A16_CIERRE_PRECOMPILADAS_20261102.md)). Las
  cuatro librerías nuevas van en fuente.

## Instalación

1. Package: en Arduino IDE agregar el índice dev e instalar **JW Control ESP32 Boards 2.1.0-alpha.16** (ver README).
2. OpenPLC Editor: ejecutar `OpenPLC-Editor.exe` (4.2.8-jwplc.9; actualiza una JWPLC Edition anterior). En
   *Configuration*, **Package Source** vuelve a la versión publicada.
3. En el editor, importar `JWPLC-Basic.vpp` (2.1.0-alpha.16) y elegir la placa **JWPLC BASIC**.

## Verificación

- `[ejecutado]` JWPLC Sim, matriz completa sobre este árbol: 138 ejemplos, 138 compilan, corren y pasan su prueba.
- `[ejecutado]` compilación ESP32 real (`jwplc_local:esp32:jwplcbasic`, arduino-esp32 3.3.8) de los ejemplos
  `JWPLC_*` y `JW_*` del package con los `.a` nuevos: 138/138 compilan sin advertencias.
- `[ejecutado]` `.a` regenerados sin advertencias, ABI compatible con alpha.15 (0 símbolos eliminados).
- `[ejecutado]` HMI Designer: `tools/jwplc-hmi-designer/tests/run-all.cjs` 18/18.
- `[visto]` en placa por el propietario (02/11/2026): pantalla web con cambio de página rápido en Windows.
- `[no verificado]` en placa: Runtime de panel, página HMI del Runtime y expansión con dos JWPLC Basic.

## Limitaciones conocidas

- Runtime y expansión: validados en el simulador, pendientes de validación en placa.
- `JWPLC_LogicRuntime` y `JWPLC_LogicRuntime_UI` (prototipo anterior) siguen en el package; se retiran cuando el
  Runtime nuevo se valide en placa.
- Pantalla web: sin contraseña (usarla sólo en redes de confianza) y sólo por Ethernet; WiFi queda para más adelante.
- El botón «Simular» dentro de OpenPLC Editor (`JWSIM_G6_SIMULATE_BUTTON`) queda para la siguiente alpha: el
  simulador se abre aparte.
- El corte de imagen (tearing) de la TFT se acepta: el pin TE no está libre en la placa v2.0.
- Sólo Windows x64 para los instaladores.

## Versiones que forman esta Release

| Componente | Versión | Commit |
|---|---|---|
| Package `jwplc:esp32` | 2.1.0-alpha.16 | `release/v2.1.x` al publicar |
| HMI Designer | 2.1.0-alpha.16 | instaladores: árbol de la Release (extensión 0.1.8); dentro del editor: platform-jwplc `94edd7d` |
| VPP JWPLC Basic | 2.1.0-alpha.16 | platform-jwplc `94edd7d` |
| OpenPLC Editor – JWPLC Edition | 4.2.8-jwplc.9 | openplc-editor `2ef069b` (tag `jwplc-editor-v4.2.8-jwplc.9`) |
| JWPLC Sim (gate `SIM_REGRESSION`) | — | jwplc-sim `a81f302` |
