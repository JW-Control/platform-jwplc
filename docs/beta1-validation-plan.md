# Beta1 - Plan de validación

Branch:

```txt
develop/beta1-package-validation
```

Versión objetivo:

```txt
JWPLC Basic v2.0.0-beta.1
```

Versión base validada:

```txt
JW Control ESP32 Boards 2.0.0-alpha.31
```

---

## 1. Objetivo

Beta1 debe convertir alpha31 en un package instalable, documentado y verificable para usuario real.

Objetivo principal:

```txt
Preparar y validar JWPLC Basic v2.0.0-beta.1 sin abrir features grandes.
```

---

## 2. Estado heredado desde alpha31

Alpha31 quedó aprobada técnicamente con:

- instalación limpia desde Boards Manager;
- instalación y manejo desde otra PC;
- compilación de `ESP32 Board`, `JWPLC Basic` y `JWPLC Basic Core`;
- subida y ejecución en `JWPLC Basic`;
- selección de librerías JW desde `Arduino15/packages/jwplc`;
- I/O por TCA6424A;
- display y botonera;
- RTC;
- FRAM;
- microSD;
- Ethernet W5500;
- RS-485;
- Modbus RTU;
- coexistencia SPI Ethernet + SD + FRAM + Display.

Resultado heredado:

```txt
Alpha31 aprobada técnicamente para avanzar a beta1-package-validation.
```

---

## 3. Alcance de beta1

Incluido:

- revisar README principal;
- revisar links de librerías;
- validar Arduino CLI post-publicación;
- revisar ejemplos visibles en Arduino IDE;
- generar ZIP `2.0.0-beta.1`;
- publicar Pre-Release `v2.0.0-beta.1`;
- calcular SHA-256 y size;
- actualizar `package_jwplc_index.json`;
- reinstalar desde cero por Boards Manager;
- validar IDE y CLI sobre beta1 instalada desde cero.

No incluido:

- no integrar OpenPLC;
- no definir OTA;
- no cambiar FlashFreq a 80 MHz;
- no publicar `bootloader.bin` como definitivo;
- no asumir app-only como solución principal;
- no quitar periféricos del autoload normal;
- no reestructurar `platform.txt` salvo bloqueante real;
- no abrir features grandes.

---

## 4. Configuración esperada

| Parámetro | Valor esperado |
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

---

## 5. Librerías bundled esperadas

| Librería | Versión inicial esperada |
|---|---:|
| `JW_MatrixButtons` | 1.0.4 |
| `JW_FRAM` | 1.0.2 |
| `JW_RTC` | 1.0.2 |
| `JW_SD` | 1.0.2 |
| `JWPLC_Display` | 1.0.0 |
| `JWPLC_GlobalPeripherals` | 1.0.0 |
| `JWPLC_Ethernet` | 1.0.0 |
| `JWPLC_RS485` | 1.0.0 |
| `JWPLC_ModbusRTU` | 1.0.0 |

---

## 6. Validación Arduino CLI

Este punto quedó como pendiente explícito al cerrar alpha31.

Comandos base:

```bat
arduino-cli core update-index
arduino-cli core list
arduino-cli board listall jwplc
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic .
arduino-cli compile --fqbn jwplc:esp32:jwplcbasiccore .
arduino-cli compile --fqbn jwplc:esp32:esp32 .
```

Validar:

```txt
Maximum is 3145728 bytes.
```

---

## 7. Revisión README principal

Validar que `README.md` contenga:

- descripción clara de JWPLC Basic;
- instalación desde Boards Manager;
- URL del package index;
- selección de placa `JWPLC Basic`;
- FQBN recomendado;
- configuración final de placa;
- lista de periféricos soportados;
- librerías incluidas;
- ejemplos principales;
- advertencias sobre OpenPLC/OTA;
- troubleshooting básico.

Resultado:

```txt
Pendiente.
```

---

## 8. Revisión de ejemplos

Validar que los ejemplos principales estén visibles en Arduino IDE:

- `DigitalIO_BlockRead`;
- `DigitalIO_BlockMirror`;
- `RS485_USB_Bridge`;
- `ModbusRTU_CRC_Test`;
- `ModbusRTU_Master_Extern`;
- `ModbusRTU_Master_ReadHoldingRegisters`;
- `ModbusRTU_Master_WriteSingleRegister`;
- `ModbusRTU_Slave_Extern`;
- `ModbusRTU_Slave_HoldingRegisters`;
- `JW_RTC BasicRead`;
- `JW_FRAM Basic_RW`;
- `JW_SD CardInfo`;
- `JW_SD BasicReadWrite`;
- `Display_DotAPI_Minimal`;
- `Display_UserUI_Callbacks`;
- `Display_Idle_Return_Modes`;
- `Ethernet_Auto_DHCP_Status`;
- `Ethernet_Display_Status`;
- `Ethernet_SPI_Coexistence`.

Resultado:

```txt
Pendiente.
```

---

## 9. Publicación beta1

ZIP esperado:

```txt
jwplc-esp32-2.0.0-beta.1.zip
```

Tag esperado:

```txt
v2.0.0-beta.1
```

Release title sugerido:

```txt
JWPLC Basic v2.0.0-beta.1 - Package validation
```

Calcular:

```powershell
certutil -hashfile jwplc-esp32-2.0.0-beta.1.zip SHA256
(Get-Item .\jwplc-esp32-2.0.0-beta.1.zip).Length
```

---

## 10. Instalación limpia beta1

Después de publicar beta1 y actualizar el index:

1. Cerrar Arduino IDE.
2. Borrar o renombrar `Arduino15/packages/jwplc`.
3. Abrir Arduino IDE.
4. Instalar `JW Control ESP32 Boards 2.0.0-beta.1`.
5. Confirmar carpeta instalada.
6. Confirmar librerías incluidas.
7. Compilar sketch vacío.
8. Compilar/subir sketch Serial mínimo.
9. Validar monitor serial.
10. Ejecutar validación CLI.

Resultado:

```txt
Pendiente.
```

---

## 11. Criterios de aceptación beta1

| Criterio | Estado |
|---|---|
| README principal revisado | Pendiente |
| Links de librerías revisados | Pendiente |
| Ejemplos visibles en Arduino IDE | Pendiente |
| Arduino IDE compila placas principales | Pendiente |
| Arduino CLI compila placas principales | Pendiente |
| `JWPLC Basic` sube por USB | Pendiente |
| Instalación limpia beta1 desde Boards Manager | Pendiente |
| Librerías JW bundled detectadas desde beta1 | Pendiente |
| Periféricos principales sin regresión | Pendiente |
| `package_jwplc_index.json` apunta a beta1 | Pendiente |
| Sin bloqueantes abiertos | Pendiente |

---

## 12. Decisión final beta1

```txt
[ ] Beta1 aprobada
[ ] Beta1 requiere correcciones
```

Resumen:

```txt
Pendiente.
```

---

## 13. Nota de control

Beta1 no debe convertirse en una etapa de desarrollo de nuevas funciones. Cualquier mejora que no sea bloqueante debe enviarse a una rama posterior.
