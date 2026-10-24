# v2.1.0-alpha.14 — JWPLC HMI Designer: MENU, formatos industriales y Live Bridge en un clic

**Canal:** PreRelease de desarrollo (índice dev). No sustituye a `v2.0.0` estable.
**Origen:** línea de trabajo `v2.1.0-alpha.15-HmiDesigner` (documentación en `docs/v2.1.0-alpha.15/`),
integrada sobre la 2.1.0-alpha.13 publicada en la rama `v2.1.0-alpha.14/HmiDesigner/main` (PR #157, antes #141).
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

Referencia adoptada: arduino-cli 1.5.1.

**Alpha.14 frente a alpha.13** (mismo equipo, mismos sketches, mediana de 3; detalle en
[A14_BENCHMARK_COMPILACION_20261024.md](A14_BENCHMARK_COMPILACION_20261024.md)):

| Medición | Placa | Frío a13 → a14 | Caliente a13 → a14 |
|---|---|---|---|
| Nube Linux, arduino-cli 1.5.2-rc.1 | JWPLC Basic | 30,3 s → 38,9 s | 4,9 s → 4,4 s |
| Nube Linux, arduino-cli 1.5.2-rc.1 | JWPLC Basic Core | 38,4 s → 45,9 s | 4,3 s → 5,1 s |
| PC Windows (12 núcleos), arduino-cli 1.5.2-rc.1 | JWPLC Basic | 40,4 s → 48,3 s | 11,0 s → 10,0 s |
| PC Windows (12 núcleos), arduino-cli 1.5.2-rc.1 | JWPLC Basic Core | 49,5 s → 60,4 s | 9,3 s → 10,0 s |

Sketch `01_empty`. La recompilación (caliente) no cambia. La primera compilación de cada placa tarda entre 7 y 12 s
más porque `JWPLC_ModbusRTU`, ahora envoltorio de `JW_ModbusRTU`/`JW_RS485`, se compila desde la fuente en esta
versión; se volverá a precompilar en una alpha posterior.

## Novedades

### Pantalla (`JWPLC_Display` 1.1.0)

- **MENU:** árbol de menús y submenús (hasta 3 niveles) dentro de una página para ver y ajustar variables
  con la botonera. Nodos de vista, edición numérica, edición ON/OFF e «Ir al inicio». La variable se escribe
  solo al confirmar con OK; ESC descarta. El sketch se entera con `JWPLC_Display.menuChanged()`.
- **Formatos de VALUE:** separador de miles, duración (`MM:SS`, `HH:MM:SS`, compacto, cuenta regresiva) y
  fecha/hora leída del RTC sin variable ni `setValue()`.
- **BOOL:** representación «Piloto» y texto y fondo por estado en todo el campo.
- **Tildes, ñ y °** en la TFT: `setText()` convierte el UTF-8 del sketch a la fuente de la pantalla.
- **ICON:** iconos con estado (hasta 8 estados por variable BOOL o entera, 4 bpp con paleta, hasta 64 px),
  con fondo o transparentes sobre la gráfica estática. El Designer trae una biblioteca de 24 iconos en 8 categorías.
- **BAR incremental:** al cambiar el valor solo se pinta o borra el tramo que cambia.
- **Barra de pestañas:** la pestaña de la página actual distingue «posicionada» y «seleccionada»; color
  editable por página. Hook nuevo `jwplcUIPageModeChanged(page, selecting)`.
- **Correcciones:** la tecla que despierta la IDLE ya no entra al contenido ni al MENU; el retorno por
  inactividad también cuenta dentro del contenido; el marco ya no se recorta en escala 3× y 4×.

### Dibujo directo (`JWPLC_TFT` 0.3.0)

- Elipses y triángulos, FreeFonts con `setFont()`/`drawString()` y `pushImage()` para bloques de píxeles.
- Conserva el arranque sin destellos de Alpha13 (DISPON diferido del ST7789).

### Periféricos

- **Pines con nombre:** librerías y ejemplos usan las constantes `JWPLC_*` del Basic (botonera, RS-485, FRAM,
  microSD, buzzer). Se corrigieron ejemplos de `JW_MatrixButtons` y `JW_RS485` que usaban pines de otra placa
  y se retiraron dos que no tenían hardware en el Basic (79 ejemplos compilan en Basic y Basic Core).

- **`JWPLC_Buzzer`** (en `JWPLC_GlobalPeripherals` 1.1.0): pitidos y melodías no bloqueantes con volumen;
  ejemplo `07.Buzzer_Basic`.
- **`JWPLC_RS485` y `JWPLC_ModbusRTU` 1.1.0** pasan a ser envoltorios de `JW_RS485` y `JW_ModbusRTU` 0.1.1,
  con la misma API y los mismos objetos globales.

### JWPLC HMI Designer

- **Plantillas listas para usar** en *Archivo → Ejemplos → JWPLC_Display*: `05.HMI_Plantilla_Caldera` (cinco
  páginas con todos los tipos de objeto y MENU), `06.HMI_Botonera_Interactiva` (teclas del sketch por página:
  horno, juego de reflejos y motor con rampa) y `07.HMI_Iconos_Planta` (15 iconos con estado). Cada una trae su
  proyecto `.jwhmi` para abrirla y modificarla en el Designer.
- Inspector gráfico rehecho para TEXT, VALUE, BOOL, BAR, figuras, textos libres, barra de páginas y MENU,
  con editor de color fijo y vista previa idéntica a la TFT.
- Herramientas PIXEL: pincel, borrador, mover, relleno y cuentagotas.
- Trabajo con el sketch: «Escribir en sketch» deja `JWPLC_HMI_Generated.h`, el `.jwhmi` y una guía
  `JWPLC_HMI_LEEME.md`, sin tocar nunca el `.ino`. Crear un sketch nuevo, abrirlo en el Arduino IDE y abrir
  el Designer desde el Arduino IDE con el sketch ya vinculado.
- **Live Bridge en un clic:** «⚡ Instalar Firmware Live» graba el firmware de vista previa desde la app,
  con selector de puertos propio. LIVE más fluido con `pushImage` y protocolo v2.
- Objeto ICON con biblioteca de iconos en la barra del lienzo.
- Proyectos recientes, aviso de cambios sin guardar y CLI `jwhmi` para crear proyectos sin la interfaz
  (también para agentes de IA; ejemplos en `tools/jwplc-hmi-designer/pruebas-ia/`).

## Validación

### Compilación y CI `[ejecutado]`

- CI del PR #141 (hoy #157) en `9a27b13` (24/10): tests del HMI Designer y smoke del package en verde.
- Tests del HMI Designer en la nube sobre la rama final: **16/16 PASS** (`run-all.cjs`: codegen puro y golden,
  inspector, MENU e ICON con paridad lienzo/firmware, firmware real en host, CLI para agentes, flasheador del
  Live Bridge y `codegen-smoke`, que compila los headers y verifica los símbolos en los `.a`).
- `libJWPLC_TFT.a` regenerado desde la fuente integrada: `ABI_COMPAT=PASS`, `ST7789_DISPON_DEFERRED=YES`,
  1 733 562 B, sha256 `f4108833…16919eaa` (commit `161e406`).
- `libJWPLC_Display.a` regenerado desde la fuente final: `ABI_COMPAT=PASS` (0 símbolos públicos quitados, 27
  añadidos), 1 370 778 B, sha256 `20d23abe…a0055745`; `JWPLC_Display` vuelve a `precompiled=full`.
- Revisión de cierre (24/10): reconstruir ambos `.a` desde la fuente da `ABI_COMPAT=PASS` con 0 símbolos
  públicos añadidos o quitados frente a los publicados (la TFT con `ST7789_DISPON_DEFERRED=YES`); ningún
  cambio de fuente después de su regeneración. `JWPLC_Display`, `JWPLC_TFT`, `JWPLC_Ethernet`,
  `JWPLC_ModbusTCP`, `JW_FRAM`, `JW_MatrixButtons` y `JW_SD` van `precompiled=full`; `JWPLC_ModbusRTU`, desde
  la fuente (se retiró su `libJWPLC_ModbusRTU.a` de Alpha13, que ya no correspondía al envoltorio).
- Compilación ESP32 de **todos los ejemplos del package (76) y los 7 gates del Designer** (MENU, MENU desde el
  Designer, formatos, reloj, tildes, codegen estático y OpenPLC F0) en **JWPLC Basic y JWPLC Basic Core**:
  83/83 en cada placa, sin warnings (arduino-cli, package de la rama, sketchbook vacío). Flash: Basic
  436 477–473 589 B; Core 394 195–433 517 B. Las 3 plantillas HMI de `JWPLC_Display` (05–07), añadidas
  después, compilan también en las dos placas (6/6, sin warnings): 79 ejemplos en total.
- Live Bridge del Designer recompilado desde el package congelado (440 752 B).

### Pruebas en placa

- `[ejecutado 19/10, línea HMI antes de integrar]` MENU, reloj, `°`, BOOL piloto, formatos, buzzer, tecla
  de despertar, pestañas y color de pestaña.
- `[ejecutado, Alpha13 y PR #138]` Modbus RTU con los envoltorios nuevos: COM4 Master ↔ COM14 Slave, FC03
  30/30 y FC06 15/15, CRC 0.
- `[ejecutado 24/10]` package integrado, todo precompilado salvo `JWPLC_ModbusRTU`, en dos JWPLC Basic unidos por
  RS-485 (Master COM6 con pantalla y Ethernet, Slave COM3), con el sketch y el checklist de
  `tools/jwplc-hmi-designer/gates/A14_F2_Combinado/`:
  - pantalla: arranque sin destellos, tecla de despertar, pestañas, reloj del RTC, `°C`, BAR, ICON, MENU con
    submenú, buzzer y vuelta a IDLE: PASS;
  - Ethernet DHCP y Modbus TCP (lectura FC03 de HR0..HR3 y mando de la bomba por HR10 desde la PC): PASS;
  - Modbus RTU: FC03 cada 500 ms sin errores y FC06 desde el MENU (`HR1=250` confirmado por el Slave): PASS;
  - 10 minutos con Modbus TCP y RTU a la vez, sin reinicios ni errores RTU: PASS;
  - Live Bridge instalado desde la app con «⚡ Instalar Firmware Live», LIVE conectado y el sketch vuelto a
    subir después: PASS, repetido con el Live Bridge recompilado desde el package congelado (440 752 B, el que
    se publica). El LIVE sigue los cambios del lienzo; el operador notó algo más de latencia que con el
    binario anterior, dentro de lo aceptable.

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
