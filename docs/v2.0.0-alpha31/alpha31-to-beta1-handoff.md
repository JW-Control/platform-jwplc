# Handoff - Alpha31 hacia beta1

Branch actual:

```txt
develop/alpha31-release-readiness
```

Siguiente branch sugerido:

```txt
develop/beta1-package-validation
```

Versión validada:

```txt
JW Control ESP32 Boards 2.0.0-alpha.31
```

---

## 1. Estado final de alpha31

Alpha31 queda aprobada técnicamente.

Resultado:

```txt
Package instalado desde cero, compilado, subido y validado en hardware real.
```

Se validó también instalación y manejo desde otra PC.

---

## 2. Qué quedó validado

### Instalación

- Boards Manager instala `JW Control ESP32 Boards 2.0.0-alpha.31` correctamente.
- El package descarga herramientas requeridas.
- La carpeta local previa fue eliminada antes de reinstalar.
- Las librerías JW se resuelven desde `Arduino15/packages/jwplc/.../2.0.0-alpha.31/libraries`.

### Placas

- `ESP32 Board`: compila.
- `JWPLC Basic`: compila, sube y ejecuta.
- `JWPLC Basic Core`: compila.

### Configuración

- CPU 240 MHz.
- Flash 4 MB.
- Flash frequency 40 MHz.
- Flash mode DIO.
- Bootloader base QIO.
- Partition `huge_app`.
- Upload 921600.
- Maximum program size 3145728 bytes.

### Librerías JW

- `JW_MatrixButtons` 1.0.4.
- `JW_FRAM` 1.0.2.
- `JW_RTC` 1.0.2.
- `JW_SD` 1.0.2.
- `JWPLC_Display` 1.0.0.
- `JWPLC_GlobalPeripherals` 1.0.0.
- `JWPLC_Ethernet` 1.0.0.
- `JWPLC_RS485` 1.0.0.
- `JWPLC_ModbusRTU` 1.0.0.

### Periféricos

- I/O por TCA6424A.
- Display TFT.
- Botonera.
- RTC.
- FRAM.
- microSD.
- Ethernet W5500.
- RS-485.
- Modbus RTU.

### Integración SPI

Validado:

```txt
Ethernet + SD + FRAM + Display
```

Con resultado observado:

```txt
ETH: OK | Link: UP | IP: 192.168.0.31 | SD: OK | FRAM: OK | Boot: 102 | Logs: 15
```

---

## 3. Pruebas ejecutadas

| Área | Prueba | Resultado |
|---|---|---|
| Instalación | Boards Manager desde cero | OK |
| Serial mínimo | `JWPLC alpha31 OK` | OK |
| Placas | ESP32 Board / JWPLC Basic / JWPLC Basic Core | OK |
| I/O | `DigitalIO_BlockRead` | OK |
| I/O | `DigitalIO_BlockMirror` | OK |
| RS-485 | `RS485_USB_Bridge` | OK |
| Modbus | `ModbusRTU_CRC_Test` | OK |
| Modbus | Slave v2.0.0 + `Master_Extern` | OK |
| Modbus | Master read v2.0.0 + `Slave_Extern` | OK |
| Modbus | Master write v2.0.0 + `Slave_Extern` | OK |
| RTC | `JW_RTC BasicRead` | OK |
| FRAM | `JW_FRAM Basic_RW` | OK |
| SD | `JW_SD CardInfo` | OK |
| SD | `JW_SD BasicReadWrite` | OK |
| Display | `Display_DotAPI_Minimal` | OK |
| Display | `Display_UserUI_Callbacks` | OK |
| Display | `Display_Idle_Return_Modes` | OK |
| Ethernet | `Ethernet_Auto_DHCP_Status` | OK |
| Ethernet | `Ethernet_Display_Status` | OK |
| SPI coexistence | `Ethernet_SPI_Coexistence` | OK |

---

## 4. Observaciones técnicas

### Ethernet

Durante reconexión Ethernet se observó un evento transitorio:

```txt
SPI lock timeout
```

No quedó permanente. El sistema recuperó:

```txt
HW: present
Link: UP
Status: OK
IP: 192.168.0.31
```

Decisión:

```txt
No bloqueante para alpha31.
```

### Arduino CLI

No se reportó log de Arduino CLI en esta tanda post-publicación.

Decisión:

```txt
Pendiente explícito si se desea cierre 100% estricto.
```

Recomendado para beta1:

```bat
arduino-cli core update-index
arduino-cli core list
arduino-cli board listall jwplc
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic .
arduino-cli compile --fqbn jwplc:esp32:jwplcbasiccore .
arduino-cli compile --fqbn jwplc:esp32:esp32 .
```

---

## 5. Decisiones que siguen vigentes

No asumir:

- OpenPLC integrado.
- OTA definido.
- FlashFreq 80 MHz.
- Publicación de `bootloader.bin` definitivo.
- App-only como solución principal de compilación.
- Eliminación de periféricos del autoload normal.

---

## 6. Recomendación para beta1

Beta1 debe enfocarse en:

1. Validación de instalación limpia final.
2. Revisión final de README principal.
3. Revisión final de links de librerías.
4. Arduino CLI post-publicación.
5. Confirmar package index estable.
6. Confirmar ejemplos visibles y ordenados en Arduino IDE.
7. Preparar release notes beta1.
8. No abrir features grandes salvo corrección de bloqueantes.

---

## 7. Decisión final

```txt
Alpha31 aprobada para avanzar a beta1-package-validation.
```
