# Alpha31 - Resultados de validación limpia

Branch:

```txt
develop/alpha31-release-readiness
```

Versión validada:

```txt
JW Control ESP32 Boards 2.0.0-alpha.31
```

Fecha de validación:

```txt
2026-05-22
```

---

## 1. Resumen ejecutivo

Alpha31 fue validada como package instalable desde cero.

La prueba incluyó:

- borrado de instalación previa local de `jwplc`;
- instalación limpia desde Boards Manager;
- validación desde Arduino IDE 2.3.4;
- verificación del uso de librerías JW desde `Arduino15/packages/jwplc`;
- compilación y subida en `JWPLC Basic`;
- compilación de `ESP32 Board`, `JWPLC Basic` y `JWPLC Basic Core`;
- pruebas físicas de I/O, RS-485, Modbus RTU, RTC, FRAM, microSD, display, botonera y Ethernet;
- validación de coexistencia SPI Ethernet + SD + FRAM + Display;
- verificación de instalación y manejo desde otra PC.

Resultado:

```txt
Alpha31 aprobada técnicamente para avanzar a beta1-package-validation.
```

---

## 2. Instalación limpia

| Prueba | Resultado | Evidencia / observación |
|---|---|---|
| Instalación local previa eliminada | OK | Carpeta local Arduino IDE `jwplc` eliminada antes de reinstalar. |
| Instalación desde Boards Manager | OK | `JW Control ESP32 Boards 2.0.0-alpha.31` instalada correctamente. |
| Herramientas descargadas | OK | `esp-x32`, `esptool_py`, `mkspiffs`, `mklittlefs`, `esp32-libs`. |
| Otra PC | OK | Instalación y manejo verificados también desde otra PC. |

---

## 3. Configuración validada

| Parámetro | Valor |
|---|---|
| CPU | 240 MHz |
| Flash size | 4 MB |
| Flash frequency | 40 MHz |
| Flash mode | DIO |
| Bootloader base | QIO |
| Partition scheme | `huge_app` |
| Upload speed | 921600 |
| App maximum size | 3145728 bytes |

FQBN principal:

```txt
jwplc:esp32:jwplcbasic
```

Resultado de compilación observado:

```txt
El Sketch usa 404497 bytes (12%) del espacio de almacenamiento de programa.
El máximo es 3145728 bytes.
Las variables globales usan 27620 bytes (8%) de RAM dinámica.
```

---

## 4. Librerías bundled confirmadas

Durante la compilación limpia se confirmó que las librerías JW fueron tomadas desde:

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

---

## 5. Placas principales

| Placa | Resultado | Observación |
|---|---|---|
| `ESP32 Board` | OK | Compila. |
| `JWPLC Basic` | OK | Compila, sube y ejecuta. |
| `JWPLC Basic Core` | OK | Compila. |

---

## 6. Pruebas funcionales

### I/O industrial

| Prueba | Resultado |
|---|---|
| `DigitalIO_BlockRead` | OK |
| `DigitalIO_BlockMirror` | OK |

---

### RS-485

| Prueba | Resultado | Observación |
|---|---|---|
| `RS485_USB_Bridge` | OK | Comunicación bidireccional con ESP32 Board / JWPLC Basic v1.4. |

---

### Modbus RTU

| Prueba | Resultado | Observación |
|---|---|---|
| `ModbusRTU_CRC_Test` | OK | CRC `0xCDC5`, `CRC OK`. |
| `ModbusRTU_Slave_HoldingRegisters` + `Master_Extern` | OK | JWPLC v2.0.0 como slave, ESP32 Board/v1.4 como master externo. |
| `ModbusRTU_Master_ReadHoldingRegisters` + `Slave_Extern` | OK | Lectura de HR0..HR3 validada. |
| `ModbusRTU_Master_WriteSingleRegister` + `Slave_Extern` | OK | Escritura incremental de HR1 validada. |

Conclusión:

```txt
Modbus RTU validado como master, slave, lectura, escritura, CRC y comunicación física RS-485.
```

---

### RTC

| Prueba | Resultado | Observación |
|---|---|---|
| `JW_RTC BasicRead` | OK | `Valid: 1`, fecha/hora correcta, temperatura leída. |

---

### FRAM

| Prueba | Resultado | Observación |
|---|---|---|
| `JW_FRAM Basic_RW` | OK | `Contador: 101`, `Ganancia: 1.2345`, `Habilitado: true`. |

---

### microSD

| Prueba | Resultado | Observación |
|---|---|---|
| `JW_SD CardInfo` | OK | SD ready, card type 3, size aprox. 29512 MB. |
| `JW_SD BasicReadWrite` | OK | Escritura/lectura de `Hola desde JW_SD`. |

---

### Display y botonera

| Prueba | Resultado | Observación |
|---|---|---|
| `Display_DotAPI_Minimal` | OK | IDLE/USER operativo. |
| `Display_UserUI_Callbacks` | OK | Callbacks y cambio USER/IDLE operativos. |
| `Display_Idle_Return_Modes` | OK | Retorno a IDLE validado. |
| Pantalla IDLE física | OK | PWR/RUN/ETH, fecha/hora, IN/OUT visibles. |
| Botonera | OK | Manejo de pantallas validado. |

---

### Ethernet

| Prueba | Resultado | Observación |
|---|---|---|
| `Ethernet_Auto_DHCP_Status` | OK | DHCP asigna IP `192.168.0.31`. |
| `Ethernet_Display_Status` | OK | Estado Ethernet visible por Serial/display. |
| `Ethernet_SPI_Coexistence` | OK | Ethernet + SD + FRAM + Display funcionando juntos. |
| Desconexión/reconexión Ethernet | OK | Detecta Link DOWN/UP y recupera OK. |
| Desconexión/reconexión microSD en coexistencia | OK | Reconoce estado SD/logs según lógica del ejemplo. |

Nota:

```txt
Se observó un `SPI lock timeout` transitorio durante reconexión Ethernet. El sistema se recuperó automáticamente y volvió a `HW: present`, `Link: UP`, `Status: OK`, IP válida. No se considera bloqueante.
```

---

## 7. Prueba integrada

Prueba integrada observada:

```txt
ETH: OK | Link: UP | IP: 192.168.0.31 | SD: OK | FRAM: OK | Boot: 102 | Logs: 15
```

Resultado:

```txt
OK
```

La integración SPI compartida entre Ethernet, microSD, FRAM y display quedó validada.

---

## 8. Pendientes explícitos

| Pendiente | Estado | Comentario |
|---|---|---|
| Arduino CLI post-publicación | Pendiente explícito | No se reportó log CLI en esta tanda. Si se desea cierre estricto 100%, ejecutar pasada rápida con `arduino-cli`. |
| OpenPLC | Fuera de alpha31 | No integrado todavía. |
| OTA | Fuera de alpha31 | No definido todavía. |
| `bootloader.bin` precompilado | Fuera de alpha31 | No se publica como definitivo. |

---

## 9. Decisión

```txt
[x] Alpha31 aprobada técnicamente para beta1-package-validation.
[ ] Alpha31 requiere correcciones antes de beta1.
```

Siguiente rama sugerida:

```txt
develop/beta1-package-validation
```
