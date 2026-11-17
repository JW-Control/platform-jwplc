# Ejemplos que quedan en JWPLC Sim (línea alpha.17)

Package: `v2.1.0-alpha.16` (81e7e7b). Fecha: 02/11/2026.

## Qué cambia en el Sim

La lista **Ejemplos…** muestra solo las librerías propias del JWPLC Basic (`JWPLC_*` y `JW_*`).
Salen de la lista (no se borran del package):

- **Benchmark del package** (29 sketches de `tools/build-speed-benchmark`: `01_empty`, `04_p7_sd_gate`, `06_alpha4_local_physical_gate`…). Son pruebas de velocidad de compilación de alphas viejas. Las pruebas del Sim que los usan los siguen abriendo por ruta.
- **Core ESP32 y terceros**: Adafruit_BusIO, ArduinoOTA, AsyncUDP, BLE, BluetoothSerial, ESP32, Matter, WiFi, Zigbee, USB y 40 librerías más (unos 450 ejemplos). Casi ninguno aplica a la placa: no hay WiFi/BLE en uso, ni USB OTG, ni Matter.

Orden: primero `JWPLC_GlobalPeripherals` (plantilla y curso), luego las `JWPLC_*` y al final las `JW_*`.
El curso se ve con su ruta, por ejemplo «Curso_JWPLC_Basic / 1.Primeros_pasos / 01.Hola_JWPLC».

**Quedan 138 ejemplos.** Abajo va la lista con mi propuesta. Marca cada uno con **Queda**, **Corregir** o **Borrar** (en el package; se haría en alpha.17).

Leyenda: ✅ queda tal cual · 🔧 revisar o corregir · 🗑️ candidato a borrar del package

## JWPLC_GlobalPeripherals (23)

- ✅ 00.Plantilla_Proyecto_JWPLC
- ✅ 01.DigitalIO_Basic · 02.Buttons_Basic · 03.RTC_Basic · 04.FRAM_Basic · 05.microSD_Basic · 07.Buzzer_Basic
- 🔧 06.Runtime_Cache_Status: el nombre habla de «Runtime» pero es de la caché de E/S; confunde con el Runtime de panel nuevo. Renombrar.
- ✅ Curso_JWPLC_Basic: 01.Hola_JWPLC … 11.Maquina_de_estados (11 ejemplos)
- 🗑️ DigitalIO_BlockMirror · DigitalIO_BlockRead · DigitalIO_BlockWrite · JWPLC_IO_BlockMirror: cuatro variantes de lo mismo, sin número. Propongo dejar uno solo como `08.DigitalIO_Bloques`.

## JWPLC_Display (17)

- ✅ 01.Display_IDLE_Status … 09.HMI_Registro_Eventos (9). El 06 ya corre en el Sim de Windows con el arreglo.
- ✅ Display_Fuentes_Grandes · Display_Graficos_Sprite · Display_UserUI_Callbacks
- 🔧 Display_DotAPI_Minimal · Display_Efficient_Redraw · Display_Idle_Return_Modes: sin número y solapan con 01–04. Numerar o fusionar.
- ❓ Display_FlappyBird · Display_Tetris: demos de juegos. Sirven para lucir la pantalla, pero no son de un PLC. Tú decides.

## JWPLC_Ethernet (9)

- ✅ 01.Ethernet_DHCP_Basic · 02.Ethernet_StaticIP_Basic · 03.Ethernet_Diagnostics
- 🗑️ Ethernet_Auto_DHCP_Status · Ethernet_Auto_StaticIP_Status · Ethernet_Display_Status: repiten 01–03 con pantalla.
- 🗑️ Ethernet_Continuous_Stress_TFT · Ethernet_SPI_Coexistence · Ethernet_HTTP_TFT_Diagnostics: pruebas de validación de alphas viejas, no ejemplos de usuario.

## JWPLC_RS485 (9)

- ✅ 01.RS485_Send · 02.RS485_Echo · 03.RS485_Status
- 🗑️ RS485_Basic_Send · RS485_Basic_Echo · RS485_Status: duplican 01–03.
- 🔧 RS485_Bridge · RS485_USB_Bridge: útiles (puente USB↔RS-485); numerar como 04/05 y dejar uno si hacen lo mismo.
- 🗑️ RS485_Native: prueba interna del backend nativo.

## JWPLC_ModbusRTU (15)

- ✅ 01.ModbusRTU_Slave_Holding · 02.ModbusRTU_Master_Read · 03.ModbusRTU_Master_Write
- 🔧 JWPLC_RemoteIO_Slave_RTU · JWPLC_RemoteIO_Master_RTU_Test: E/S remota entre dos JWPLC; ¿lo reemplaza JWPLC_Expansion? Si sí, borrar.
- 🗑️ ModbusRTU_CRC_Test · ModbusRTU_Tool_Test · ModbusRTU_Master_Cooperative_Validation: pruebas.
- 🗑️ ModbusRTU_Master_Native · ModbusRTU_Native · ModbusRTU_Master_Extern · ModbusRTU_Slave_Extern: variantes internas de backend.
- 🗑️ ModbusRTU_Master_ReadHoldingRegisters · ModbusRTU_Master_WriteSingleRegister · ModbusRTU_Slave_HoldingRegisters: duplican 01–03.

## JWPLC_ModbusTCP (2) · JWPLC_Expansion (1) · JWPLC_Runtime (2) · JWPLC_UI_Widgets (4) · JWPLC_WebScreen (2)

- ✅ todos: 01.ModbusTCP_Server, 02.ModbusTCP_Client, Expansion_Esclavo_Basic, Runtime_HMI, Runtime_Panel, Widgets_Parametros, Widgets_QR, Widgets_Teclado, Widgets_Tendencia, 01.Pantalla_Web, 02.Runtime_En_Vivo.

## JWPLC_LogicRuntime (27) y JWPLC_LogicRuntime_UI (6)

- 🗑️ Los 33. Son del motor v1/v2 anterior (pruebas de almacenamiento A/B, adaptadores V1→V2, «_RAM», «_Validation»). El Runtime de panel nuevo (`JWPLC_Runtime`) los reemplaza y quedó decidido que v1 se puede eliminar. Si prefieres, los saco solo de la lista del Sim y la librería se borra cuando se valide el Runtime en placa.

## Librerías JW_* (21)

Son las librerías genéricas de JW Control (también sirven fuera del JWPLC).

- ✅ JW_FRAM: Basic_RW · Block_Config · Strings · Struct_Config
- ✅ JW_RTC: BasicRead · NvramDemo · SetFromBuildTime
- ✅ JW_SD: BasicReadWrite · CardInfo
- ✅ JW_MatrixButtons: 01_BasicPress · 02_AxisWrapSerial · 03_EventQueue
- ✅ JW_DWIN: Basic_RS485 · Simular_USB
- 🔧 JW_RS485: Basic_Send · DE_RE_Pin · Status, y JW_ModbusRTU: Master_Read_Async · Master_Sync · Master_Write_Async · Slave_Holding. Solapan con JWPLC_RS485/JWPLC_ModbusRTU. Propongo quitarlas de la lista del Sim (el usuario del JWPLC usa las JWPLC_*), o mostrarlas al final en un grupo «Librerías genéricas».

## Resumen de la propuesta

| | Ejemplos |
|---|---|
| Quedan tal cual | ~75 |
| Corregir/renombrar | ~12 |
| Borrar del package | ~70 (33 de LogicRuntime) |
