# Ejemplos del package: qué funciones usan y si cumplen la regla del «.»

Package `v2.1.0-alpha.16` (81e7e7b). Fecha: 02/11/2026. Alcance: los 138 ejemplos de `JWPLC_*` y `JW_*`.

**Regla** (CLAUDE.md §3.9, 01/11): en el sketch el usuario no escribe `::`, `*` ni `&`. Todo se usa como `objeto.funcion()`, las opciones son constantes globales y los textos van en `String` o `const char nombre[]`.

**Cómo se revisó.** Un script recorre cada `.ino` sin comentarios ni textos y busca `::`, `->`, declaraciones con `*`, referencias `Tipo &x`, direcciones `&x`, casts, `auto` y `JWPLC_Display.tft()`. Después revisé cada hallazgo a mano y descarté las multiplicaciones que el script confundía con punteros (Widgets_QR, Widgets_Tendencia y el 06). `JWPLC_HMI_Generated.h` no se cuenta porque lo genera el Designer.

Leyenda: ✅ cumple · 🔧 corregir el ejemplo · 🧩 hace falta un cambio chico en la librería · 🏷️ renombrar · 🗑️ borrar

---

## 1. Resumen: qué choca con la regla y cómo se arregla

Estos 10 patrones explican casi todos los hallazgos.

| # | Patrón en el ejemplo | Dónde aparece | Arreglo propuesto |
|---|---|---|---|
| P1 | `auto &tft = JWPLC_Display.tft();` y luego `tft.xxx()` | Display 04, Efficient_Redraw, Idle_Return_Modes, UserUI_Callbacks, FlappyBird, Tetris | 🔧 Usar `JWPLC_TFT.xxx()`, que ya existe. Además, `tft()` deja de documentarse para el usuario. |
| P2 | `JWPLC_Display.menuChanged(MENU, &nodo)` | Display 05 y 06 | 🧩 Agregar `menuChanged(MENU)` + `JWPLC_Display.menuNode()`, y que los ejemplos usen eso. |
| P3 | `const JWPLCModbusRTUStats &s = JWPLC_ModbusRTU.stats();` | ModbusRTU 01, Slave_HoldingRegisters, RemoteIO, Cooperative; JW_ModbusRTU Slave_Holding | 🧩 Getters directos (`JWPLC_ModbusRTU.okCount()`, `.crcErrors()`, `.timeouts()`…), o como mínimo copiar sin `&`. |
| P4 | `JWPLC_ModbusRTU.setCoils(&coilMap, 8)` | RemoteIO_Slave_RTU | 🔧 Usar un arreglo `uint8_t bobinas[1]` y pasar `bobinas`, sin `&`. |
| P5 | `JW_RTC::errorToString(rtc.lastError())`, `JW_RTC::fromBuildTime(…, dt)` | JW_RTC (3 ejemplos) | 🧩 `rtc.lastErrorString()` y `rtc.setFromBuildTime()`. |
| P6 | `JW_MatrixButtons::EV_PRESS`, `JW_MatrixButtons::BtnEvent e` | JW_MatrixButtons/03_EventQueue | 🧩 Constantes globales `BTN_EV_PRESS/RELEASE/REPEAT` y un tipo `JWPLC_ButtonEvent`. |
| P7 | Funciones del sketch con `const char *texto`, `char *destino` | Display 06, Plantilla (E/G/J), 02.Buttons_Basic, Curso 11, JW_FRAM/Strings, JW_MatrixButtons/01, pruebas de Ethernet/Modbus | 🔧 `String texto` o `const char texto[]`. Las tablas de textos quedan como `const char NOMBRES[][12]`. |
| P8 | Parámetros por referencia en funciones del sketch: `Temporizador &t`, `bool &antes`, `const Receta &r` | Curso 09 y 10, Display 06 | 🔧 Usar el estilo de la Plantilla: un `struct` con método (`tArranque.ejecutar(...)`, `flancoMarcha.subida(...)`). Así se enseña también en el curso. |
| P9 | `memcmp(&a, &b, sizeof(a))` | Plantilla (I_Persistencia), Ethernet_HTTP | 🧩 Que `JWPLC_FRAM` compare por sí misma (`JWPLC_FRAM.changed(dir, datos)` o que `writeBlock` solo escriba si cambió). |
| P10 | Objetos que crea el usuario con `Serial2` y pines (`JW_RS485 rs485(Serial2, 16, 17)`, `JW_FRAM fram(13)`, `JW_SD sd(5)`, `JW_RTC rtc`) | Las 6 librerías JW_* | En el JWPLC ya existen los globales `JWPLC_RS485`, `JWPLC_FRAM`, `JWPLC_SD` y `JWPLC_Time/JWPLC_RTC`. Ver §3. |

Otros puntos, fuera de la regla pero del mismo espíritu («llamados raros»):

- `JWPLC_readInputs()` / `JWPLC_writeOutputs(bits)` son funciones sueltas, no `objeto.funcion()`. Ya existen `JWPLC_IO.inputs()` y `JWPLC_IO.outputs()`. Usar esas en 01.DigitalIO_Basic, la Plantilla (C_Entradas, F_Salidas) y JWPLC_IO_BlockMirror.
- `Serial2.begin/read/write` directo aparece en RS485_Bridge y en los ejemplos `*_Extern`, que puentean el driver del package.
- `jwplcSPI_acquire()` / `jwplcSPI_release()` en Ethernet_Continuous_Stress_TFT son internos del bus y no deberían verse en un ejemplo.
- Tanto `JWPLC_Time` como `JWPLC_RTC` leen la hora. Los ejemplos mezclan los dos (Ethernet usa `JWPLC_RTC.now()`; RTC_Basic y Display usan `JWPLC_Time`). Conviene elegir uno para los ejemplos. Propongo `JWPLC_Time`.

---

## 2. Por librería

### JWPLC_GlobalPeripherals (23)

**Funciones que usan:** `JWPLC_IO.input()/output()/inputs()/outputs()/ready()/lastScanMs()`, `JWPLC_Buttons.pressed()/released()/isDown()/clearPendingInput()`, `JWPLC_Time.valid()/present()/lostPower()/day()…second()/lastUpdateMs()`, `JWPLC_RTC.readTemperatureC()`, `JWPLC_FRAM.size()/readBlock()/writeBlock()/readCString()/writeCString()`, `JWPLC_SD.isCardPresent()/isReady()/open()/lastErrorString()` + `file.print()/close()`, `JWPLC_Buzzer.beep()/play()/stop()/setVolume()/volume()`, `JWPLC_Display.setRunLed()/setErrCode()/setIdleWakeMode()/setBusLedAuto()/setEthLedAuto()`, `JWPLC_ModbusRTU.*` y `JWPLC_ModbusTCP.*` (Plantilla), y además `JWPLC_readInputs()/JWPLC_writeOutputs()`.

| Ejemplo | Estado | Nota |
|---|---|---|
| 00.Plantilla_Proyecto_JWPLC | 🔧 | P7 (textos de alarma `const char *`), P9 (`memcmp(&…)` en persistencia) y funciones sueltas de E/S. |
| 01.DigitalIO_Basic | 🔧 | Usa `JWPLC_readInputs()/JWPLC_writeOutputs()` → `JWPLC_IO`. |
| 02.Buttons_Basic | 🔧 | P7: `static const char *BUTTON_NAMES[]` → `const char NOMBRES[][8]`. |
| 03.RTC_Basic · 04.FRAM_Basic · 05.microSD_Basic · 07.Buzzer_Basic | ✅ | |
| 06.Runtime_Cache_Status | 🏷️ | Cumple, pero el nombre confunde con el Runtime de panel. Propongo `06.IO_Estado_Escaneo`. |
| Curso 01–08 | ✅ | |
| Curso 09.Temporizadores | 🔧 | P8: `retardoConexion(Temporizador &t, …)` → `struct` con método `t.conexion(entrada, ms)`. |
| Curso 10.Contadores | 🔧 | P8: `flancoSubida(bool ahora, bool &antes)` → `struct Flanco { bool subida(bool); }`, igual que la Plantilla. |
| Curso 11.Maquina_de_estados | 🔧 | P7: `cambiarA(Estado, const char *nombre)` → `const char nombre[]` o `String`. |
| DigitalIO_BlockMirror · BlockRead · BlockWrite · JWPLC_IO_BlockMirror | 🗑️ | Cuatro variantes de lo mismo. Cumplen la regla, pero sobran: dejar uno solo como `08.DigitalIO_Bloques`, con `JWPLC_IO`. |

### JWPLC_Display (17)

**Funciones que usan:** `JWPLC_Display.setIdleWakeMode()/setIdleWakeButton()/setIdleReturnMode()/setIdleTimeoutMs()/goIdle()/isIdleMode()/enterUserUI()/isUserPageSelection()/userPage()/setUserPage()/setUserRefreshMode()/setUserRefreshPeriodMs()/setFields()/setText()/setValue()/setBool()/setBar()/menuChanged()/setRunLed()/setErrCode()/setBusLed()/setEthLed()/forceRedraw()/setPixelMapVisible()/notifyActivity()`, los constructores de campos `JWPLC_UITextField()/JWPLC_UIValueField()/JWPLC_UIBoolField()/JWPLC_UIBarField()/JWPLC_UIRange()…`, los callbacks `jwplcUIEnter()/jwplcUIUpdate()/jwplcUIExit()`, `jwplcHMISetup()` (del Designer) y `JWPLC_TFT.*`. Siete ejemplos todavía usan `JWPLC_Display.tft()`.

| Ejemplo | Estado | Nota |
|---|---|---|
| 01.Display_IDLE_Status · 02.Display_HMI_Fields · 03.Display_HMI_Pages | ✅ | |
| 04.Display_TFT_Direct | 🔧 | P1: `auto &tft = JWPLC_Display.tft()` → `JWPLC_TFT`. Es justo el ejemplo de «dibujar directo», así que es el que más importa. |
| 05.HMI_Plantilla_Caldera | 🔧🧩 | P2 (`menuChanged(…, &nodo)`). |
| 06.HMI_Botonera_Interactiva | 🔧🧩 | P2, P7 (`ponerTexto(char *destino…)`, `const char *nombre` en el struct, `TEXTOS[]`) y P8 (`const Receta &r`). |
| 07.HMI_Iconos_Planta · 08.HMI_Medidores_Animados · 09.HMI_Registro_Eventos | ✅ | |
| Display_Fuentes_Grandes · Display_Graficos_Sprite | ✅ 🏷️ | Cumplen; numerarlos 10 y 11. |
| Display_DotAPI_Minimal | 🗑️ | Cumple, pero es una prueba de que la API con «.» compila sin el include («Valida: …»). Los LED RUN/ERR/BUS/ETH ya se ven en 01; si falta algo, pasarlo a 01. |
| Display_Efficient_Redraw · Display_Idle_Return_Modes · Display_UserUI_Callbacks | 🔧 🏷️ | P1. Corregidos, numerarlos 12–14 (o fusionar Idle_Return_Modes con 01). |
| Display_FlappyBird · Display_Tetris | 🔧 o 🗑️ | ~1500 líneas cada uno, con `tft()` crudo, punteros, referencias y `getTextBounds(&x,&y,&w,&h)` (P1, P7, P8). Pasarlos a la regla es reescribirlos. Si se quedan como demo, corregirlos con `JWPLC_TFT.textWidth()/fontHeight()`; si no, borrarlos. |

### JWPLC_UI_Widgets (4) · JWPLC_Runtime (2) · JWPLC_WebScreen (2) · JWPLC_Expansion (1)

**Funciones:** widgets con objeto propio (`g_lista.begin()/setRows()/handleKeys()/draw()/selected()`, `g_editor.*`, `g_dialogo.open()/close()`, `g_aviso.show()`, `g_teclado.open()/text()`, `g_qr.setText()/draw()`, `g_tendencia.push()/draw()`), `fila.left()/right()` y `JWPLC_TFT.*`. Además `JWPLC_Runtime.begin()/loadDemoIfEmpty()/output()`, `JWPLC_WebScreen.begin()/url()` y `JWPLC_ExpansionSlave.begin()`.

✅ Los 9 cumplen; se escribieron con la regla. Como detalle, los widgets usan `String.c_str()` para pasar textos; se podría aceptar `String` directo y quitar el `.c_str()`.

### JWPLC_Ethernet (9)

**Funciones:** `JWPLC_Ethernet.isReady()/isEnabled()/linkUp()/localIP()/gatewayIP()/dnsServerIP()/useDHCP()/setStaticIP()/statusString()/lastErrorString()/diagnosticCode()/printStatus()/hardwarePresent()/isBusy()`; en las pruebas, `Ethernet.*`, `EthernetClient`, `DNSClient` y `jwplcSPI_acquire()`.

| Ejemplo | Estado | Nota |
|---|---|---|
| 01.Ethernet_DHCP_Basic · 02.Ethernet_StaticIP_Basic · 03.Ethernet_Diagnostics | ✅ | |
| Ethernet_Auto_DHCP_Status · Ethernet_Auto_StaticIP_Status · Ethernet_Display_Status | 🗑️ | Cumplen, pero repiten 01–03 con la pantalla. Si quieres uno con pantalla, dejar `04.Ethernet_Estado_Pantalla`. |
| Ethernet_Continuous_Stress_TFT (1640 líneas) · Ethernet_HTTP_TFT_Diagnostics (728) · Ethernet_SPI_Coexistence (294) | 🗑️ | Pruebas de validación de alpha.13/14: punteros, `&`, `::`, mutex del bus y `Ethernet.` crudo. No son ejemplos de usuario. Las pruebas del Sim que los usan se pueden pasar a `tests/` del Sim. |

### JWPLC_RS485 (9)

**Funciones:** `JWPLC_RS485.begin()/available()/read()/write()/print()/println()/flush()/isReady()/printStatus()/statusString()/configString()/baudRate()/lastErrorString()/hasRecentActivity()/lastRxActivityMs()/lastTxActivityMs()`.

| Ejemplo | Estado | Nota |
|---|---|---|
| 01.RS485_Send · 02.RS485_Echo · 03.RS485_Status | ✅ | |
| RS485_Basic_Send · RS485_Basic_Echo · RS485_Status | 🗑️ | Cumplen, pero duplican 01–03. |
| RS485_USB_Bridge | ✅ 🏷️ | Cumple. Numerarlo `04.RS485_Puente_USB`. |
| RS485_Bridge | 🗑️ | Hace lo mismo que USB_Bridge pero con `Serial2` directo. |
| RS485_Native | 🗑️ | Prueba interna del backend (21 líneas). |

### JWPLC_ModbusRTU (15)

**Funciones:** `JWPLC_ModbusRTU.begin()/task()/setHoldingRegisters()/setCoils()/setDiscreteInputs()/requestReadHoldingRegisters()/requestWriteSingleRegister()/requestReadCoils()/requestWriteSingleCoil()/…/masterBusy()/masterDone()/masterSucceeded()/masterResult()/clearMasterResult()/stats()/resetStats()/printStatus()/lastErrorString()/crc16()`.

| Ejemplo | Estado | Nota |
|---|---|---|
| 01.ModbusRTU_Slave_Holding | 🔧🧩 | P3 (`stats()` con `&`). |
| 02.ModbusRTU_Master_Read · 03.ModbusRTU_Master_Write | ✅ | |
| JWPLC_RemoteIO_Slave_RTU · JWPLC_RemoteIO_Master_RTU_Test | 🗑️ | Los reemplaza `JWPLC_Expansion` (alpha.16). Además tienen P3, P4, `&rxBits` y P7. |
| ModbusRTU_Master_ReadHoldingRegisters · Master_WriteSingleRegister · Slave_HoldingRegisters | 🗑️ | Duplican 01–03 (y Slave_HoldingRegisters tiene P3). |
| ModbusRTU_Master_Extern · ModbusRTU_Slave_Extern | 🗑️ | Implementan Modbus a mano con `Serial2` y punteros a tramas; enseñan lo contrario a usar la librería. |
| ModbusRTU_Native · ModbusRTU_Master_Native · ModbusRTU_CRC_Test · ModbusRTU_Tool_Test · ModbusRTU_Master_Cooperative_Validation | 🗑️ | Pruebas internas. |

### JWPLC_ModbusTCP (2)

**Funciones:** `JWPLC_ModbusTCP.setCoils()/setDiscreteInputs()/setHoldingRegisters()/setInputRegisters()/beginServer()/serverReady()/stats()/printStatus()` y `JWPLC_ModbusTCPClient.begin()/task()/requestReadHoldingRegisters()/busy()/done()/succeeded()/result()/resultString()/exceptionCode()/clearResult()`.

✅ Los 2 cumplen.

### JWPLC_LogicRuntime (27) y JWPLC_LogicRuntime_UI (6)

Son los peores con la regla: entre 2 y 95 usos de `::` por ejemplo (`LogicV2BlockType::Ton`, `JWPLCLogicStorage::…`), casts, punteros y referencias en casi todos. Son pruebas del motor v1/v2, que el Runtime nuevo reemplaza y que el CLAUDE.md marca como fuera de alcance. 🗑️ Los 33.

---

## 3. Librerías JW_* (21 ejemplos)

Son las librerías genéricas de JW Control (sirven en cualquier ESP32). Por eso **el usuario crea el objeto** y le pasa el puerto y los pines: `JW_RS485 rs485(Serial2, 16, 17)`, `JW_ModbusRTU modbus(rs485)`, `JW_FRAM fram(JWPLC_FRAM_CS)`, `JW_SD sd(SD_CS)`, `JW_RTC rtc`. En el JWPLC Basic eso ya lo hace el package con `JWPLC_RS485`, `JWPLC_ModbusRTU`, `JWPLC_FRAM`, `JWPLC_SD` y `JWPLC_Time`.

| Librería | Funciones que usan | Estado |
|---|---|---|
| JW_FRAM (4) | `fram.begin()/put()/get()/update()/writeBlock()/readBlock()/writeString()/readString()/writeCString()/readCString()/enableDebug()` | Strings con P7. Los 4 repiten 04.FRAM_Basic con otro objeto. |
| JW_RTC (3) | `rtc.begin()/read()/write()/isTimeValid()/readTemperatureC()/nvramReadObject()/nvramWriteObject()/lastError()` | P5 en los 3 (`JW_RTC::`) y `const JWRTCDateTime& dt` en BasicRead. |
| JW_SD (2) | `sd.begin()/open()/cardType()/cardSize()/lastErrorString()` | Cumplen (salvo un `auto file`). Repiten 05.microSD_Basic. |
| JW_RS485 (3) | `rs485.begin()/setAutoDirection()/setDirectionPin()/print()/available()/read()/printStatus()` | Sin `::`, pero con `Serial2` y pines a mano. DE_RE_Pin no aplica a la placa (el pin está fijo). |
| JW_ModbusRTU (4) | `modbus.begin()/task()/requestReadHoldingRegisters()/readHoldingRegistersSync()/writeSingleRegisterSync()/setHoldingRegisters()/stats()/…` | Slave_Holding con P3. Repiten JWPLC_ModbusRTU 01–03. |
| JW_MatrixButtons (3) | `JWPLC_Buttons.pressed()/applyAxis()/eventCount()/getEvent()/clearEventQueue()` | 01 con P7; 03_EventQueue con P6 (`JW_MatrixButtons::`). Los 3 usan `JWPLC_Buttons`, así que son del JWPLC. 🔧 Mover `02_AxisWrapSerial` y `03_EventQueue` a GlobalPeripherals como `02b`/`02c` y borrar 01 (es 02.Buttons_Basic). |
| JW_DWIN (2) | `dwin.begin()/setPage()/sendText()/sendValue()/checkKey()/isOnline()/currentPage()/enableTouch()…` | Es el único que no tiene equivalente JWPLC_. Basic_RS485 arma la cadena `JW_RS485 → JW_ModbusRTU → transporte → JW_DWIN` a mano. Simular_USB tiene `const String &cmd` y `Stream &console`. 🔧🧩 Agregar un global `JWPLC_DWIN` sobre el RS-485 del JWPLC y dejar 2 ejemplos limpios. |

**Propuesta para JW_*:** sacarlas de la lista de Ejemplos del Sim (la librería sigue en el package para quien la quiera fuera del JWPLC). Las excepciones son `JW_DWIN`, que pasa a un ejemplo con `JWPLC_DWIN`, y los 2 de botonera, que se mueven a GlobalPeripherals.

---

## 4. Qué quedaría para alpha.17

| | Cantidad |
|---|---|
| Ejemplos hoy (JWPLC_* + JW_*) | 138 |
| Borrar (incluye 33 de LogicRuntime y 20 pruebas o duplicados) | ~75 |
| Corregir el ejemplo (sin tocar la librería) | 14 |
| Necesitan un cambio chico en la librería (P2, P3, P5, P6, P9, DWIN) | 6 cambios de API |
| Renombrar o numerar | ~10 |
| **Quedan** | **~55–60**, todos con «.» |

Cambios de API sugeridos (todos agregan; no rompen lo existente):

1. `JWPLC_Display.menuChanged(MENU)` + `JWPLC_Display.menuNode()`.
2. Getters de estadísticas en `JWPLC_ModbusRTU` (`okCount()`, `crcErrors()`, `timeouts()`, `exceptions()`).
3. `rtc.lastErrorString()` y `rtc.setFromBuildTime()` en `JW_RTC`, o mejor `JWPLC_Time.setFromBuildTime()`.
4. `BTN_EV_PRESS/RELEASE/REPEAT` y `JWPLC_ButtonEvent` en la botonera.
5. `JWPLC_FRAM.writeIfChanged(dir, datos)`, o que `writeBlock` compare antes de escribir.
6. Global `JWPLC_DWIN` sobre el RS-485 del JWPLC.

Dime qué grupos apruebas (o qué cambiarías) y lo armo en la línea `v2.1.0-alpha.17/…` del package, con su prueba en el Sim para cada ejemplo que quede.
