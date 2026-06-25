# Alpha31 - Checklist de validación

Branch:

```txt
develop/alpha31-release-readiness
```

Package validado:

```txt
JW Control ESP32 Boards 2.0.0-alpha.31
```

Objetivo de alpha31:

```txt
Validar package completo instalado desde cero y preparar beta1.
```

---

## 1. Resultado general

```txt
Alpha31 aprobada técnicamente para avanzar a beta1-package-validation.
```

La validación se realizó después de:

- publicar `2.0.0-alpha.31` como Pre-Release;
- borrar la instalación local previa de `jwplc` en Arduino IDE;
- instalar nuevamente el package desde Boards Manager;
- verificar el uso de la versión instalada desde `Arduino15`;
- validar compilación, subida, ejecución y periféricos en hardware real;
- verificar instalación y manejo también desde otra PC.

---

## 2. Configuración validada

| Parámetro | Valor esperado | Resultado |
|---|---|---|
| CPU | 240 MHz | OK |
| Flash size | 4 MB | OK |
| Flash frequency | 40 MHz | OK |
| Flash mode | DIO | OK |
| Bootloader base | QIO | OK |
| Partition scheme | `huge_app` | OK |
| Upload speed | 921600 | OK |
| LoopCore | default del core | OK |
| EventsCore | default del core | OK |
| App maximum size | 3145728 bytes | OK |

FQBN principal validado:

```txt
jwplc:esp32:jwplcbasic
```

---

## 3. Decisiones respetadas

| Decisión | Resultado |
|---|---|
| OpenPLC no está integrado todavía | OK |
| OTA no está definido todavía | OK |
| No publicar `bootloader.bin` precompilado como definitivo | OK |
| No asumir FlashFreq 80 MHz | OK |
| No asumir que app-only resuelve compilación | OK |
| No quitar periféricos del autoload normal por velocidad | OK |
| No implementar precarga global de caché | OK |
| No precompilar librerías internas | OK |
| No tocar `platform.txt` en alpha31 | OK |

---

## 4. Instalación limpia desde Boards Manager

| Prueba | Resultado | Observación |
|---|---|---|
| Instalación previa local eliminada | OK | Carpeta Arduino IDE `jwplc` eliminada antes de reinstalar. |
| Package visible en Boards Manager | OK | `JW Control ESP32 Boards`. |
| Instalación completa sin error | OK | IDE reportó instalación correcta. |
| Versión instalada | OK | `2.0.0-alpha.31`. |
| Herramientas descargadas correctamente | OK | `esp-x32`, `esptool_py`, `mkspiffs`, `mklittlefs`, `esp32-libs`. |
| `ESP32 Board` visible | OK | Compila. |
| `JWPLC Basic` visible | OK | Compila/sube/ejecuta. |
| `JWPLC Basic Core` visible | OK | Compila. |
| Validación desde otra PC | OK | Instalación y manejo verificados también desde otra PC. |

Criterio:

```txt
El package instala correctamente desde Boards Manager sin copiar archivos manualmente.
```

Resultado:

```txt
OK
```

---

## 5. Librerías incluidas y seleccionadas desde el package

Durante la compilación limpia se confirmó que las librerías JW principales fueron tomadas desde:

```txt
C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc\hardware\esp32\2.0.0-alpha.31\libraries\
```

| Librería | Versión | Resultado |
|---|---:|---|
| `JWPLC_Display` | 1.0.0 | OK |
| `JWPLC_GlobalPeripherals` | 1.0.0 | OK |
| `JW_RTC` | 1.0.2 | OK |
| `JW_FRAM` | 1.0.2 | OK |
| `JW_SD` | 1.0.2 | OK |
| `JW_MatrixButtons` | 1.0.4 | OK |
| `JWPLC_Ethernet` | 1.0.0 | OK |
| `JWPLC_RS485` | 1.0.0 | OK |
| `JWPLC_ModbusRTU` | 1.0.0 | OK |

Nota:

```txt
Dependencias externas como Adafruit BusIO, Adafruit GFX, Adafruit ST7735/ST7789 y Ethernet pueden resolverse desde el sketchbook del usuario si ya están instaladas. Esto no bloqueó alpha31.
```

---

## 6. Validación Arduino IDE

Arduino IDE usado:

```txt
Arduino IDE 2.3.4
```

| Prueba | Resultado | Observación |
|---|---|---|
| Sketch Serial mínimo en `JWPLC Basic` | OK | Compila, sube y ejecuta. |
| Monitor serial | OK | Imprime `JWPLC alpha31 OK`. |
| `ESP32 Board` | OK | Compila. |
| `JWPLC Basic` | OK | Compila/sube/ejecuta. |
| `JWPLC Basic Core` | OK | Compila. |
| Upload speed | OK | 921600. |
| Maximum program size | OK | 3145728 bytes. |

Resultado observado en sketch mínimo:

```txt
El Sketch usa 404497 bytes (12%) del espacio de almacenamiento de programa.
El máximo es 3145728 bytes.
Las variables globales usan 27620 bytes (8%) de RAM dinámica.
```

---

## 7. Validación Arduino CLI

| Prueba | Resultado | Observación |
|---|---|---|
| Arduino CLI post-publicación | Pendiente explícito | No se reportó log CLI en esta tanda. |

Nota:

```txt
La validación principal de alpha31 se realizó con Arduino IDE 2.3.4 tras instalación limpia desde Boards Manager. Si se desea cierre estricto del punto CLI, ejecutar una pasada rápida con arduino-cli antes de cerrar definitivamente el alpha.
```

Comandos sugeridos:

```bat
arduino-cli core update-index
arduino-cli core list
arduino-cli board listall jwplc
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic .
arduino-cli compile --fqbn jwplc:esp32:jwplcbasiccore .
arduino-cli compile --fqbn jwplc:esp32:esp32 .
```

---

## 8. Validación de ejemplos y periféricos

### 8.1 I/O industrial

| Prueba | Resultado | Observación |
|---|---|---|
| `DigitalIO_BlockRead` | OK | Compila y opera. |
| `DigitalIO_BlockMirror` | OK | Compila y opera. |
| TCA6424A / I/O | OK | Lectura/escritura por bloques validada. |

---

### 8.2 RS-485

| Prueba | Resultado | Observación |
|---|---|---|
| `RS485_USB_Bridge` en JWPLC Basic | OK | Subido y probado en hardware real. |
| Comunicación con ESP32 Board / JWPLC Basic v1.4 | OK | Mensajes bidireccionales entre ambos monitores seriales. |
| RX2/TX2 | OK | IO16/IO17. |
| MAX13487 auto-dirección | OK | Sin DE/RE manual. |

---

### 8.3 Modbus RTU

| Prueba | Resultado | Observación |
|---|---|---|
| `ModbusRTU_CRC_Test` | OK | CRC calculado `0xCDC5`, `CRC OK`. |
| `ModbusRTU_Slave_HoldingRegisters` + `Master_Extern` | OK | v2.0.0 como slave, v1.4/ESP32 Board como master externo. |
| `ModbusRTU_Master_ReadHoldingRegisters` + `Slave_Extern` | OK | v2.0.0 como master lector, v1.4/ESP32 Board como slave externo. |
| `ModbusRTU_Master_WriteSingleRegister` + `Slave_Extern` | OK | Escritura de HR1 validada. |
| RX/TX frames | OK | Contadores aumentan sin CRC errors. |
| CRC errors | OK | 0 durante pruebas reportadas. |

Conclusión:

```txt
Modbus RTU validado como master, slave, lectura, escritura y CRC sobre enlace físico RS-485.
```

---

### 8.4 RTC

| Prueba | Resultado | Observación |
|---|---|---|
| `JW_RTC BasicRead` | OK | `Valid: 1`, fecha/hora correcta. |
| Temperatura RTC | OK | Lectura aproximada 34.50-34.75 °C. |

---

### 8.5 FRAM

| Prueba | Resultado | Observación |
|---|---|---|
| `JW_FRAM Basic_RW` | OK | Lectura/escritura correcta. |
| Tamaño forzado FRAM | OK | 8192 bytes. |
| `put/get/update` | OK | `Contador: 101`, `Ganancia: 1.2345`, `Habilitado: true`. |

---

### 8.6 microSD

| Prueba | Resultado | Observación |
|---|---|---|
| `JW_SD CardInfo` | OK | SD ready. |
| Tipo/tamaño de tarjeta | OK | Card type 3, size aprox. 29512 MB. |
| `JW_SD BasicReadWrite` | OK | Escritura/lectura de `Hola desde JW_SD`. |
| Coexistencia SPI con Ethernet/FRAM/Display | OK | Validada en prueba integrada. |

---

### 8.7 Display y botonera

| Prueba | Resultado | Observación |
|---|---|---|
| `Display_DotAPI_Minimal` | OK | IDLE/USER operativo. |
| `Display_UserUI_Callbacks` | OK | Entrada/salida USER validada. |
| `Display_Idle_Return_Modes` | OK | Retorno a IDLE validado. |
| Pantalla IDLE | OK | Visualiza PWR/RUN/ERR/BUS/ETH, fecha/hora, IN/OUT. |
| Botonera | OK | Navegación y retorno USER/IDLE operativos. |

---

### 8.8 Ethernet

| Prueba | Resultado | Observación |
|---|---|---|
| `Ethernet_Auto_DHCP_Status` | OK | DHCP asigna IP. |
| `Ethernet_Display_Status` | OK | Estado ETH/IP visible en monitor/display. |
| `Ethernet_SPI_Coexistence` | OK | Ethernet + SD + FRAM + Display operativos. |
| Link down/up | OK | Detecta desconexión y reconexión. |
| DHCP/IP | OK | IP observada: `192.168.0.31`. |
| Recuperación tras evento SPI transitorio | OK | Se observó `SPI lock timeout` transitorio, luego recuperación automática. |

Nota:

```txt
El `SPI lock timeout` observado durante reconexión no quedó permanente. El sistema recuperó HW present, Link UP, Status OK e IP válida. No se considera bloqueante para alpha31.
```

---

## 9. Prueba integrada

| Integración | Resultado | Observación |
|---|---|---|
| Ethernet + SD + FRAM + Display | OK | `ETH: OK`, `SD: OK`, `FRAM: OK`, `Boot: 102`, logs incrementan. |
| Display IDLE con Ethernet activo | OK | ETH en verde, IP válida. |
| Desconexión/reconexión Ethernet | OK | Link cambia DOWN/UP y recupera OK. |
| microSD durante coexistencia SPI | OK | Logs SD OK y skipped OK según lógica del ejemplo. |

---

## 10. Errores encontrados

| ID | Prueba | Error | Bloqueante | Estado |
|---|---|---|---|---|
| A31-001 | Ethernet reconexión | `SPI lock timeout` transitorio | No | Recuperado automáticamente |

No se registran bloqueantes abiertos.

---

## 11. Criterio para pasar a beta1

| Criterio | Estado |
|---|---|
| Instalación limpia desde Boards Manager OK | OK |
| Arduino IDE compila placas principales | OK |
| `JWPLC Basic` sube por USB | OK |
| Sketch Serial mínimo corre | OK |
| I/O industrial validado | OK |
| Display validado | OK |
| Botonera validada | OK |
| Ethernet validado | OK |
| FRAM validada | OK |
| microSD validada | OK |
| RTC validado | OK |
| RS-485 validado | OK |
| Modbus RTU validado | OK |
| README/documentación alpha31 agregada | OK |
| READMEs/librerías snapshot sincronizado | OK |
| Sin bloqueantes abiertos | OK |
| Arduino CLI post-publicación | Pendiente explícito |

---

## 12. Decisión final alpha31

Resultado:

```txt
[x] Alpha31 aprobada técnicamente para beta1
[ ] Alpha31 requiere correcciones antes de beta1
```

Resumen:

```txt
Alpha31 valida correctamente el package instalado desde cero desde Boards Manager, incluyendo compilación, subida, selección de librerías bundled, periféricos principales e integración SPI/Ethernet/SD/FRAM/Display.
```

Siguiente branch sugerido:

```txt
develop/beta1-package-validation
```

---

## 13. Notas finales

- No se asume OpenPLC integrado.
- No se asume OTA definido.
- No se cambia FlashFreq a 80 MHz.
- No se publica `bootloader.bin` definitivo.
- No se eliminan periféricos del autoload normal.
- Arduino CLI queda como pendiente explícito si se desea cierre 100% estricto de alpha31.
