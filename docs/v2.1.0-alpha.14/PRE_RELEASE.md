# v2.1.0-alpha.14 — JWPLC HMI Designer: MENU, formatos industriales y Live Bridge en un clic

> **Borrador de notas de publicación (23/10/2026).** Las secciones marcadas `[pendiente]` se completan
> al cerrar las fases 2 y 3 del traspaso
> ([JWPLC_NEW_CHAT_START_ALPHA14_20261023.md](../v2.1.0-alpha.15/JWPLC_NEW_CHAT_START_ALPHA14_20261023.md) §4).
> Estas notas no anticipan tag, ZIP ni checksum: pertenecen al GitHub Release y al índice dev.

**Canal:** PreRelease de desarrollo (índice dev). No sustituye a `v2.0.0` estable.
**Origen:** línea de trabajo `v2.1.0-alpha.15-HmiDesigner` (documentación en `docs/v2.1.0-alpha.15/`),
integrada sobre la 2.1.0-alpha.13 publicada en la rama `v2.1.0-alpha.14/feature/hmi-designer` (PR #141).
La numeración sigue el orden de publicación: el trabajo de OpenPLC se publicará como 2.1.0-alpha.15.

## Requisito recomendado: Arduino IDE 2.3.10 o superior

El package compila con cualquier Arduino IDE 2.x, pero la versión del `arduino-cli` interno cambia mucho la
compilación en caliente. Medición en la PC del jefe (`Run-JWPLCBuildBenchmark.ps1`, sketches `01_empty` y
`02_io_basic`, JWPLC Basic, alpha.13 instalada):

| arduino-cli | Frío | Caliente |
|---|---|---|
| 1.0.2 (Alpha9–12) | 45,5 s | 18,7 s |
| 1.1.1 (Arduino IDE 2.3.4) | 45,6 s | 18,2 s |
| 1.4.1 | 43,6 s | 14,3 s |
| 1.5.1 (Arduino IDE 2.3.10) | 41,5 s | **9,0 s** |

Referencia adoptada: arduino-cli 1.5.1. Benchmark de la alpha.14 con todo precompilado: `[pendiente]`.

## Novedades

### Pantalla (`JWPLC_Display` 1.1.0)

- **MENU:** árbol de menús y submenús (hasta 3 niveles) dentro de una página para ver y ajustar variables
  con la botonera. Nodos de vista, edición numérica, edición ON/OFF e «Ir al inicio». La variable se escribe
  solo al confirmar con OK; ESC descarta. El sketch se entera con `JWPLC_Display.menuChanged()`.
- **Formatos de VALUE:** separador de miles, duración (`MM:SS`, `HH:MM:SS`, compacto, cuenta regresiva) y
  fecha/hora leída del RTC sin variable ni `setValue()`.
- **BOOL:** representación «Piloto» y texto y fondo por estado en todo el campo.
- **Tildes, ñ y °** en la TFT: `setText()` convierte el UTF-8 del sketch a la fuente de la pantalla.
- **Barra de pestañas:** la pestaña de la página actual distingue «posicionada» y «seleccionada»; color
  editable por página. Hook nuevo `jwplcUIPageModeChanged(page, selecting)`.
- **Correcciones:** la tecla que despierta la IDLE ya no entra al contenido ni al MENU; el retorno por
  inactividad también cuenta dentro del contenido; el marco ya no se recorta en escala 3× y 4×.

### Dibujo directo (`JWPLC_TFT` 0.3.0)

- Elipses y triángulos, FreeFonts con `setFont()`/`drawString()` y `pushImage()` para bloques de píxeles.
- Conserva el arranque sin destellos de Alpha13 (DISPON diferido del ST7789).

### Periféricos

- **`JWPLC_Buzzer`** (en `JWPLC_GlobalPeripherals` 1.1.0): pitidos y melodías no bloqueantes con volumen;
  ejemplo `07.Buzzer_Basic`.
- **`JWPLC_RS485` y `JWPLC_ModbusRTU` 1.1.0** pasan a ser envoltorios de `JW_RS485` y `JW_ModbusRTU` 0.1.1,
  con la misma API y los mismos objetos globales.

### JWPLC HMI Designer

- Inspector gráfico rehecho para TEXT, VALUE, BOOL, BAR, figuras, textos libres, barra de páginas y MENU,
  con editor de color fijo y vista previa idéntica a la TFT.
- Herramientas PIXEL: pincel, borrador, mover, relleno y cuentagotas.
- Trabajo con el sketch: «Escribir en sketch» deja `JWPLC_HMI_Generated.h`, el `.jwhmi` y una guía
  `JWPLC_HMI_LEEME.md`, sin tocar nunca el `.ino`. Crear un sketch nuevo, abrirlo en el Arduino IDE y abrir
  el Designer desde el Arduino IDE con el sketch ya vinculado.
- **Live Bridge en un clic:** «⚡ Instalar Firmware Live» graba el firmware de vista previa desde la app,
  con selector de puertos propio. LIVE más fluido con `pushImage` y protocolo v2.
- Proyectos recientes, aviso de cambios sin guardar y CLI `jwhmi` para crear proyectos sin la interfaz.

## Validación

### Compilación y CI `[ejecutado]`

- CI del PR #141 en `d7afe353`: tests del HMI Designer 10/10 (incluye `codegen-smoke`, que verifica los
  símbolos en los `.a`, y `menu-host-sim`, el firmware real del MENU en host) y smoke del package. Sobre el
  SHA final: `[pendiente]`.
- `libJWPLC_TFT.a` regenerado desde la fuente integrada: `ABI_COMPAT=PASS`, `ST7789_DISPON_DEFERRED=YES`,
  1 733 562 B, sha256 `f4108833…16919eaa` (commit `161e406`).
- Compilación ESP32 (arduino-cli, package de la rama) de 18 sketches en **JWPLC Basic y JWPLC Basic Core**
  (18/18 en cada placa): ejemplos de Display 01–04, gates del Designer (MENU, formatos, reloj, tildes,
  codegen, OpenPLC F0), Ethernet DHCP, Modbus TCP Server/Client, Modbus RTU Slave/Master y RS485_Echo.

### Pruebas en placa

- `[ejecutado 19/10, línea HMI antes de integrar]` MENU, reloj, `°`, BOOL piloto, formatos, buzzer, tecla
  de despertar, pestañas y color de pestaña.
- `[ejecutado, Alpha13 y PR #138]` Modbus RTU con los envoltorios nuevos: COM4 Master ↔ COM14 Slave, FC03
  30/30 y FC06 15/15, CRC 0.
- `[pendiente]` package integrado en COM4 (pantalla + Ethernet) con COM14 como Slave RTU; Live Bridge
  instalado desde la app y LIVE conectado.

## Limitaciones conocidas

- El modo OpenPLC del Designer no se publica en esta versión (llega con la 2.1.0-alpha.15).
- `Á Í Ó Ú` en mayúscula no tienen glifo propio en la fuente de la TFT.
- MENU: un MENU por página, hasta 4 menús y 64 nodos por menú.
- La app de escritorio del Designer es solo para Windows; la versión de navegador no puede abrir el
  Arduino IDE ni carpetas.

## Instalar el HMI Designer (Windows)

Descarga de los assets de esta Release:

- `JWPLC-HMI-Designer-Setup-2.1.0-alpha.14-x64.exe`: instalador por usuario, sin administrador. Crea los
  accesos e instala la extensión para Arduino IDE 2. Es la opción recomendada.
- `JWPLC-HMI-Designer-Portable-2.1.0-alpha.14-x64.exe`: se abre sin instalar, pero no integra el Arduino IDE.

No hace falta Node.js ni el repositorio. El .exe no está firmado: si aparece «Windows protegió su PC», pulsa
*Más información* y luego *Ejecutar de todas formas*.

## Distribución

Versión `2.1.0-alpha.14`, carpeta raíz `2.1.0/` dentro del ZIP, canal dev de Boards Manager. El índice dev se
actualiza mediante PR hacia `main`. La instalación, compilación y subida desde el package ya distribuido son
un gate postpublicación aparte.
