# Alpha31 - Checklist de validación

Branch:

```txt
develop/alpha31-release-readiness
```

Objetivo de alpha31:

```txt
Validar package completo instalado desde cero y preparar beta1.
```

Este documento sirve para registrar la validación real del package **JWPLC Basic v2.0.0** antes de pasar a beta1.

---

## 1. Alcance de alpha31

Alpha31 es una etapa de **release readiness**.

Debe validar:

- instalación limpia desde Boards Manager;
- Arduino IDE;
- Arduino CLI;
- placas principales;
- ejemplos principales;
- periféricos principales;
- documentación base;
- decisión de avance a beta1.

Alpha31 no debe abrir features grandes.

---

## 2. Configuración final esperada

Para `JWPLC Basic` y `JWPLC Basic Core`:

| Parámetro | Valor esperado |
|---|---|
| CPU | 240 MHz |
| Flash size | 4 MB |
| Flash frequency | 40 MHz |
| Flash mode | DIO |
| Bootloader base | QIO |
| Partition scheme | `huge_app` |
| Upload speed | 921600 |
| LoopCore | default del core |
| EventsCore | default del core |
| App maximum size | 3145728 bytes |

FQBN recomendado:

```txt
jwplc:esp32:jwplcbasic
```

---

## 3. Decisiones que no se reabren en alpha31

- OpenPLC no está integrado todavía.
- OTA no está definido todavía.
- No publicar `bootloader.bin` precompilado como definitivo.
- No asumir FlashFreq 80 MHz.
- No asumir que app-only resuelve compilación.
- No quitar periféricos del autoload normal por velocidad.
- No implementar precarga global de caché.
- No precompilar librerías internas.
- No tocar `platform.txt` salvo bloqueante real.

---

## 4. Preparación de entorno limpio

### 4.1 Registro del entorno

| Campo | Valor |
|---|---|
| Fecha de prueba |  |
| PC / usuario |  |
| Sistema operativo | Windows 10 / Windows 11 / otro |
| Arduino IDE |  |
| Arduino CLI |  |
| Package JWPLC instalado |  |
| URL package index |  |
| Puerto COM usado |  |
| Hardware usado | JWPLC Basic / JWPLC Basic Core / ESP32 Board |

### 4.2 Limpieza recomendada

Antes de validar instalación limpia:

- Cerrar Arduino IDE.
- Renombrar o limpiar instalación previa de `jwplc` en Arduino15.
- Confirmar que no queda una versión local vieja interfiriendo.
- Abrir Arduino IDE.
- Instalar desde Boards Manager usando el `package_jwplc_index.json` oficial.

Ruta típica en Windows:

```txt
C:\Users\<usuario>\AppData\Local\Arduino15\packages\jwplc
```

---

## 5. Validación de instalación desde Boards Manager

| Prueba | Resultado | Observación |
|---|---|---|
| URL agregada en preferencias | Pendiente |  |
| Package visible en Boards Manager | Pendiente |  |
| Instalación completa sin error | Pendiente |  |
| Aparece `ESP32 Board` | Pendiente |  |
| Aparece `JWPLC Basic` | Pendiente |  |
| Aparece `JWPLC Basic Core` | Pendiente |  |
| No aparecen familias ESP32 no soportadas como placas visibles principales | Pendiente |  |
| Herramientas descargadas correctamente | Pendiente |  |
| Reinicio de IDE no rompe instalación | Pendiente |  |

Criterio de aceptación:

```txt
El package debe instalar desde Boards Manager sin copiar archivos manualmente.
```

---

## 6. Validación de archivos de configuración instalados

Luego de instalar desde Boards Manager, revisar en la carpeta instalada:

| Archivo | Validación | Resultado | Observación |
|---|---|---|---|
| `boards.txt` | Existe | Pendiente |  |
| `boards.txt` | `JWPLC Basic` usa configuración fija | Pendiente |  |
| `boards.txt` | `JWPLC Basic Core` usa configuración fija | Pendiente |  |
| `boards.txt` | `jwplcbasic.upload.maximum_size=3145728` | Pendiente |  |
| `boards.txt` | `jwplcbasic.build.partitions=huge_app` | Pendiente |  |
| `boards.txt` | `jwplcbasic.build.flash_freq=40m` | Pendiente |  |
| `boards.txt` | `jwplcbasic.build.flash_mode=dio` | Pendiente |  |
| `boards.txt` | `jwplcbasic.build.boot=qio` | Pendiente |  |
| `platform.txt` | Existe | Pendiente |  |
| `platform.txt` | No tiene cambios alpha31 no planificados | Pendiente |  |
| `pins_is.h` | Existe en variante `jwplcbasic` | Pendiente |  |
| `pins_is.h` | Define `I0_0...I0_7` | Pendiente |  |
| `pins_is.h` | Define `Q0_0...Q0_7` | Pendiente |  |
| `pins_is.h` | Define `I0_X` y `Q0_X` | Pendiente |  |
| `pins_is.h` | Define `EN_IO=27` | Pendiente |  |

---

## 7. Validación Arduino IDE

### 7.1 Sketch vacío

Sketch:

```cpp
void setup()
{
}

void loop()
{
}
```

| Placa | FQBN esperado | Resultado | Tiempo | Observación |
|---|---|---|---:|---|
| ESP32 Board | `jwplc:esp32:esp32` | Pendiente |  |  |
| JWPLC Basic | `jwplc:esp32:jwplcbasic` | Pendiente |  |  |
| JWPLC Basic Core | `jwplc:esp32:jwplcbasiccore` | Pendiente |  |  |

### 7.2 Sketch Serial mínimo

Sketch:

```cpp
void setup()
{
    Serial.begin(115200);
}

void loop()
{
    Serial.println("JWPLC alpha31 OK");
    delay(1000);
}
```

| Placa | Resultado compilación | Resultado upload | Monitor serial | Observación |
|---|---|---|---|---|
| ESP32 Board | Pendiente | Pendiente | Pendiente |  |
| JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| JWPLC Basic Core | Pendiente | Pendiente | Pendiente |  |

Criterio de aceptación:

```txt
Las tres placas deben compilar. JWPLC Basic debe subir por USB y ejecutar Serial.
```

---

## 8. Validación Arduino CLI

### 8.1 Información del core

Comandos sugeridos:

```bat
arduino-cli core update-index
arduino-cli core search jwplc
arduino-cli core list
arduino-cli board listall jwplc
```

Resultado esperado:

```txt
JW Control ESP32 Boards instalado y placas JWPLC visibles.
```

### 8.2 Compilación sketch vacío

Comando base:

```bat
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic .
```

Si se quiere ver más detalle:

```bat
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic --verbose .
```

| Prueba | Comando | Resultado | Tiempo | Observación |
|---|---|---|---:|---|
| CLI sketch vacío JWPLC Basic | `arduino-cli compile --fqbn jwplc:esp32:jwplcbasic .` | Pendiente |  |  |
| CLI sketch vacío JWPLC Basic Core | `arduino-cli compile --fqbn jwplc:esp32:jwplcbasiccore .` | Pendiente |  |  |
| CLI sketch vacío ESP32 Board | `arduino-cli compile --fqbn jwplc:esp32:esp32 .` | Pendiente |  |  |

Validar en salida:

```txt
Sketch uses ... bytes (...%) of program storage space.
Maximum is 3145728 bytes.
```

---

## 9. Validación de ejemplos principales

### 9.1 I/O industrial

| Ejemplo | Placa | IDE | CLI | Resultado hardware | Observación |
|---|---|---|---|---|---|
| `DigitalIO_InputMonitor` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `DigitalIO_OutputSequence` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `DigitalIO_InputToOutput` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `DigitalIO_AllStatus_Serial` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `DigitalIO_BlockRead` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `DigitalIO_BlockMirror` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |

Validar:

- `pinMode(I0_x, INPUT)`.
- `pinMode(Q0_x, OUTPUT)`.
- `digitalRead(I0_x)`.
- `digitalWrite(Q0_x, value)`.
- `digitalReadBlock(I0_X)`.
- `digitalWriteBlock(Q0_X, bitmap)`.
- `JWPLC_readInputs()`.
- `JWPLC_readOutputs()`.
- `JWPLC_writeOutputs(bitmap)`.

---

### 9.2 Display

| Ejemplo | Placa | IDE | CLI | Resultado hardware | Observación |
|---|---|---|---|---|---|
| `Display_DotAPI_Minimal` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `Display_UserUI_Callbacks` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `Display_Efficient_Redraw` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `Display_Idle_Return_Modes` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |

Validar:

- arranque de pantalla IDLE;
- entrada a USER;
- retorno por timeout;
- retorno por ESC;
- indicadores RUN/ERR/BUS/ETH;
- ausencia de flicker grave;
- ausencia de texto sobreescrito.

---

### 9.3 Ethernet

| Ejemplo | Placa | IDE | CLI | Resultado hardware | Observación |
|---|---|---|---|---|---|
| `Ethernet_Auto_DHCP_Status` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `Ethernet_Auto_StaticIP_Status` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `Ethernet_Display_Status` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `Ethernet_SPI_Coexistence` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |

Validar:

- arranque con RJ45 conectado;
- arranque sin RJ45;
- conexión posterior de RJ45;
- desconexión/reconexión;
- DHCP automático;
- IP estática configurada en `setup()`;
- estado `Ethernet disabled` esperado en `JWPLC Basic Core`;
- coexistencia con TFT, SD y FRAM.

---

### 9.4 FRAM

| Ejemplo | Placa | IDE | CLI | Resultado hardware | Observación |
|---|---|---|---|---|---|
| `FRAM_BootCounter` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `FRAM_ReadWrite` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `FRAM_ConfigStorage` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |

Validar:

- `begin()` automático o global disponible;
- lectura/escritura;
- persistencia tras reset;
- comportamiento disabled esperado en `JWPLC Basic Core`.

---

### 9.5 microSD

| Ejemplo | Placa | IDE | CLI | Resultado hardware | Observación |
|---|---|---|---|---|---|
| `SD_FileList` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `SD_ReadWrite` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `SD_DisplayBrowser` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |

Validar:

- tarjeta detectada;
- tarjeta ausente no bloquea sistema;
- FAT32;
- MBR en tarjetas grandes;
- lectura;
- escritura;
- listado;
- coexistencia SPI.

---

### 9.6 RTC

| Ejemplo | Placa | IDE | CLI | Resultado hardware | Observación |
|---|---|---|---|---|---|
| `RTC_ReadTime` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `RTC_SetTime` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `RTC_DisplayClock` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |

Validar:

- RTC detectado;
- lectura fecha/hora;
- seteo fecha/hora;
- temperatura si aplica;
- lost power flag si aplica;
- visualización en display si aplica.

---

### 9.7 RS-485

| Ejemplo | Placa | IDE | CLI | Resultado hardware | Observación |
|---|---|---|---|---|---|
| `RS485_Status` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `RS485_Basic_Send` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `RS485_Basic_Echo` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `RS485_USB_Bridge` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |

Validar:

- `JWPLC_RS485.begin()`;
- RX2 = IO16;
- TX2 = IO17;
- baudrate default 115200;
- `SERIAL_8N1` default;
- MAX13487 sin DE/RE manual;
- comunicación con adaptador externo.

---

### 9.8 Modbus RTU

| Ejemplo | Placa | IDE | CLI | Resultado hardware | Observación |
|---|---|---|---|---|---|
| `ModbusRTU_CRC_Test` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `ModbusRTU_Slave_HoldingRegisters` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `ModbusRTU_Master_ReadHoldingRegisters` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |
| `ModbusRTU_Master_WriteSingleRegister` | JWPLC Basic | Pendiente | Pendiente | Pendiente |  |

Validar:

- CRC16 con vector conocido;
- slave ID default 1;
- baudrate default 19200;
- `SERIAL_8E1` default;
- holding registers;
- lectura master;
- escritura master;
- comunicación física por RS-485 si hay equipo externo.

Nota:

```txt
OpenPLC no está integrado todavía.
```

---

## 10. Validación de periféricos principales en hardware

| Periférico | Prueba mínima | Resultado | Observación |
|---|---|---|---|
| TCA6424A / I/O | Leer entradas y accionar salidas | Pendiente |  |
| Display TFT | IDLE visible y USER operativo | Pendiente |  |
| Botonera | Navegación y eventos limpios | Pendiente |  |
| RTC | Leer fecha/hora | Pendiente |  |
| FRAM | Boot counter persistente | Pendiente |  |
| microSD | Leer/escribir/listar | Pendiente |  |
| Ethernet W5500 | Link + DHCP | Pendiente |  |
| RS-485 | Bridge o echo | Pendiente |  |
| Modbus RTU | CRC + slave/master | Pendiente |  |

---

## 11. Validación de subida USB

| Prueba | Placa | Upload speed | Resultado | Observación |
|---|---|---:|---|---|
| Upload sketch Serial mínimo | JWPLC Basic | 921600 | Pendiente |  |
| Upload DigitalIO | JWPLC Basic | 921600 | Pendiente |  |
| Upload Ethernet/Display | JWPLC Basic | 921600 | Pendiente |  |
| Reset posterior a upload | JWPLC Basic | 921600 | Pendiente |  |
| Monitor Serial luego de upload | JWPLC Basic | 115200 | Pendiente |  |

---

## 12. Tabla de tiempos alpha31

Registrar tiempos reales observados en la instalación limpia.

| Prueba | Tiempo alpha30 referencia | Tiempo alpha31 medido | Resultado | Observación |
|---|---:|---:|---|---|
| Build limpio con configuración fija | 02:01.014 |  | Pendiente |  |
| Build incremental del mismo sketch | 00:31.861 |  | Pendiente |  |
| App-only aislado | 00:06.352 |  | Pendiente |  |
| Segundo sketch usando cache de core | 01:58.993 |  | Pendiente |  |
| Build incremental del segundo sketch | 00:31.296 |  | Pendiente |  |

Criterio:

- No se exige mejorar tiempos en alpha31.
- Se busca confirmar que no empeoraron de forma grave.
- Cualquier desviación fuerte debe registrarse.

---

## 13. Errores encontrados

| ID | Fecha | Prueba | Error | Bloqueante | Archivo sospechoso | Estado |
|---|---|---|---|---|---|---|
| A31-001 |  |  |  | Sí/No |  | Abierto/Cerrado |
| A31-002 |  |  |  | Sí/No |  | Abierto/Cerrado |
| A31-003 |  |  |  | Sí/No |  | Abierto/Cerrado |

Formato recomendado para logs:

```txt
Primer error real:
Archivo:
Línea:
Mensaje:
Used library:
Used platform:
FQBN:
IDE/CLI:
```

---

## 14. Criterio para pasar a beta1

Alpha31 puede cerrarse y pasar a beta1 si se cumple:

| Criterio | Estado |
|---|---|
| Instalación limpia desde Boards Manager OK | Pendiente |
| Arduino IDE compila placas principales | Pendiente |
| Arduino CLI compila placas principales | Pendiente |
| `JWPLC Basic` sube por USB | Pendiente |
| Sketch Serial mínimo corre | Pendiente |
| I/O industrial validado | Pendiente |
| Display validado | Pendiente |
| Botonera validada | Pendiente |
| Ethernet validado | Pendiente |
| FRAM validada | Pendiente |
| microSD validada | Pendiente |
| RTC validado | Pendiente |
| RS-485 validado | Pendiente |
| Modbus RTU validado | Pendiente |
| README principal revisado | Pendiente |
| READMEs de librerías revisados | Pendiente |
| Sin bloqueantes abiertos | Pendiente |

---

## 15. Decisión final alpha31

Resultado:

```txt
[ ] Alpha31 aprobada para beta1
[ ] Alpha31 requiere correcciones antes de beta1
```

Resumen de decisión:

```txt
Pendiente.
```

Siguiente branch sugerido si alpha31 queda aprobada:

```txt
develop/beta1-package-validation
```

---

## 16. Notas finales

- Alpha31 valida el package como producto instalable.
- No se asume OpenPLC integrado.
- No se asume OTA definido.
- No se cambia FlashFreq a 80 MHz.
- No se publica `bootloader.bin` definitivo.
- No se eliminan periféricos del autoload normal.
- Cualquier pendiente debe quedar explícito antes de cerrar alpha31.
