# Pre-Release - JWPLC Basic v2.0.0-beta.1

## Título sugerido

```txt
JWPLC Basic v2.0.0-beta.1 - Package validation
```

## Tag sugerido

```txt
v2.0.0-beta.1
```

## Asset esperado

```txt
jwplc-esp32-2.0.0-beta.1.zip
```

---

## Resumen

`v2.0.0-beta.1` es la primera versión beta del package Arduino **JW Control ESP32 Boards** para **JWPLC Basic v2.0.0**.

Esta beta parte de `v2.0.0-alpha.31`, que fue validada con instalación limpia desde Boards Manager, compilación por Arduino IDE, validación por Arduino CLI, subida real por USB y pruebas funcionales de periféricos principales.

Beta1 marca el inicio de **feature freeze** para la ruta hacia `v2.0.0`.

Objetivo:

```txt
Validar el package completo como candidato beta instalable para usuario real.
```

---

## Estado

```txt
Tipo: Beta / Pre-Release
Versión: 2.0.0-beta.1
Estado: Package validation
```

Beta1 no agrega features grandes. Su foco es:

- documentación final previa a RC;
- validación Arduino IDE;
- validación Arduino CLI;
- package index;
- separación de canal público/dev;
- instalación limpia desde Boards Manager;
- verificación de que no existan regresiones respecto a alpha31.

---

## Configuración de placa validada

Para `JWPLC Basic` y `JWPLC Basic Core`:

| Parámetro | Valor |
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

## Librerías incluidas

Esta versión mantiene snapshots validados de librerías JW dentro del package:

```txt
JWPLC/JWPLC-2.0.0/libraries/
```

| Librería | Versión |
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

## Cambios destacados

### 1. README actualizado para beta1

El README principal fue actualizado para representar la etapa `v2.0.0-beta.1`.

Incluye:

- estado de beta1;
- instalación desde Boards Manager;
- FQBN recomendado;
- configuración fija validada;
- periféricos soportados;
- librerías incluidas;
- ejemplos recomendados;
- checklist rápido;
- advertencias sobre OpenPLC, OTA, bootloader, app-only y autoload.

---

### 2. Validación Arduino CLI

Se cerró el pendiente de Arduino CLI heredado desde alpha31.

Validado con:

```txt
arduino-cli 1.0.2
```

Resultados:

| Prueba | Resultado |
|---|---|
| `core update-index` | OK |
| `core list` | OK |
| `board listall jwplc` | OK |
| Compile `jwplc:esp32:jwplcbasic` | OK |
| Compile `jwplc:esp32:jwplcbasiccore` | OK |
| Compile `jwplc:esp32:esp32` | OK |
| Upload por COM14 | OK |
| Monitor serial 115200 | OK |

Resultado observado:

```txt
JWPLC alpha31 CLI OK
JWPLC alpha31 CLI OK
JWPLC alpha31 CLI OK
```

---

### 3. Links de librerías revisados

Los links externos del README apuntan al branch `main` de los repos oficiales:

- `JW_FRAM`
- `JW_RTC`
- `JW_SD`
- `JW_MatrixButtons`
- `JW_DWIN_RS485`

Decisión:

```txt
El README público final debe apuntar a la referencia canónica donde el usuario puede revisar o descargar cada librería independiente.
```

`JW_DWIN_RS485` queda documentada como librería complementaria, no como parte del runtime base de JWPLC Basic v2.0.0.

---

### 4. Ejemplos recomendados actualizados

Se actualizó la lista de ejemplos recomendados para reflejar los nombres reales usados/revisados durante la validación.

Ejemplos principales:

```txt
DigitalIO_BlockRead
DigitalIO_BlockMirror
RS485_USB_Bridge
ModbusRTU_CRC_Test
ModbusRTU_Master_Extern
ModbusRTU_Master_ReadHoldingRegisters
ModbusRTU_Master_WriteSingleRegister
ModbusRTU_Slave_Extern
ModbusRTU_Slave_HoldingRegisters
BasicRead
Basic_RW
CardInfo
BasicReadWrite
Display_DotAPI_Minimal
Display_UserUI_Callbacks
Display_Idle_Return_Modes
Ethernet_Auto_DHCP_Status
Ethernet_Display_Status
Ethernet_SPI_Coexistence
```

---

### 5. Canal público y canal de desarrollo

Desde beta1 se recomienda separar los índices de Boards Manager:

#### Canal público

```txt
JWPLC/package_jwplc_index.json
```

Contenido recomendado:

```txt
Betas, RCs y releases estables.
```

#### Canal dev / interno

```txt
JWPLC/package_jwplc_index_dev.json
```

Contenido recomendado:

```txt
Alphas, betas, RCs y releases estables.
```

Objetivo:

```txt
Evitar que el usuario final vea decenas de alphas en Boards Manager.
```

---

## Validaciones heredadas desde alpha31

Alpha31 fue validada con instalación limpia desde Boards Manager y pruebas funcionales reales.

| Área | Resultado |
|---|---|
| Instalación limpia Boards Manager | OK |
| Instalación y manejo desde otra PC | OK |
| Arduino IDE | OK |
| Arduino CLI | OK |
| Placas principales | OK |
| Subida USB | OK |
| Librerías bundled desde `Arduino15/packages/jwplc` | OK |
| I/O TCA6424A | OK |
| Display | OK |
| Botonera | OK |
| RTC | OK |
| FRAM | OK |
| microSD | OK |
| Ethernet W5500 | OK |
| RS-485 | OK |
| Modbus RTU | OK |
| Coexistencia SPI Ethernet + SD + FRAM + Display | OK |

---

## Decisiones mantenidas

Esta beta mantiene las decisiones cerradas en alpha30/alpha31:

- No publicar `bootloader.bin` precompilado como definitivo.
- No implementar precarga global de caché.
- No precompilar librerías internas.
- No tocar `platform.txt` salvo bloqueante real.
- OTA queda fuera de esta etapa.
- OpenPLC no está integrado todavía.
- No quitar periféricos del autoload normal por velocidad.
- No asumir FlashFreq 80 MHz.
- `app-only` queda documentado como herramienta útil, no como solución principal.

---

## Limitaciones conocidas

### OpenPLC

OpenPLC no está integrado todavía.

El mapa Modbus/OpenPLC documentado es preliminar y queda como base para futuras pruebas.

### OTA

OTA no está definido todavía.

La configuración actual usa `huge_app` para maximizar espacio de aplicación en hardware de 4 MB.

### Dependencias externas de Arduino

Arduino puede resolver algunas dependencias externas desde el sketchbook del usuario si ya están instaladas.

Durante las validaciones se confirmó que las librerías JW críticas se toman desde la carpeta del package JWPLC.

---

## Checksums

Completar luego de generar el ZIP final:

```txt
Archivo: jwplc-esp32-2.0.0-beta.1.zip
Size: PENDIENTE
SHA-256: PENDIENTE
```

Comandos sugeridos en PowerShell:

```powershell
certutil -hashfile jwplc-esp32-2.0.0-beta.1.zip SHA256
(Get-Item .\jwplc-esp32-2.0.0-beta.1.zip).Length
```

---

## Instalación recomendada

Para usuarios finales, usar el canal público:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

Para pruebas internas con alphas, usar canal dev:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

---

## Validación recomendada después de instalar

Después de instalar `2.0.0-beta.1` desde Boards Manager:

1. Seleccionar `JWPLC Basic`.
2. Compilar sketch vacío.
3. Compilar/subir sketch Serial mínimo.
4. Confirmar monitor serial.
5. Compilar:
   - `ESP32 Board`
   - `JWPLC Basic`
   - `JWPLC Basic Core`
6. Validar:
   - `DigitalIO_BlockRead`
   - `DigitalIO_BlockMirror`
   - `RS485_USB_Bridge`
   - `ModbusRTU_CRC_Test`
   - `Ethernet_SPI_Coexistence`
7. Ejecutar validación CLI:
   - `jwplc:esp32:jwplcbasic`
   - `jwplc:esp32:jwplcbasiccore`
   - `jwplc:esp32:esp32`

---

## Estado final

```txt
Pre-Release beta.
No recomendado aún como release estable.
```

La siguiente etapa esperada, si beta1 no presenta bloqueantes, es:

```txt
develop/rc1-v2.0.0
```
