# Release Notes - JWPLC Basic v2.0.0

## Título sugerido

```txt
JWPLC Basic v2.0.0 - Release estable inicial
```

## Tag sugerido

```txt
v2.0.0
```

## Asset esperado

```txt
jwplc-esp32-2.0.0.zip
```

---

## Resumen

`JWPLC Basic v2.0.0` es la primera release estable del package Arduino **JW Control ESP32 Boards** para programar el **JWPLC Basic** desde Arduino IDE.

Esta versión permite trabajar con un PLC industrial basado en ESP32 manteniendo una experiencia de programación tipo Arduino, pero con periféricos industriales integrados al runtime del package.

---

## Qué incluye

Periféricos integrados en `JWPLC Basic`:

- I/O industrial por TCA6424A.
- Display TFT ST7789.
- Botonera tipo LOGO!/HMI.
- RTC DS3232.
- FRAM SPI.
- microSD.
- Ethernet W5500.
- RS-485.
- Modbus RTU base.

Placas disponibles:

| Placa | FQBN |
|---|---|
| ESP32 Board | `jwplc:esp32:esp32` |
| JWPLC Basic | `jwplc:esp32:jwplcbasic` |
| JWPLC Basic Core | `jwplc:esp32:jwplcbasiccore` |

FQBN recomendado:

```txt
jwplc:esp32:jwplcbasic
```

---

## Configuración estable

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

---

## Librerías incluidas

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

## Instalación

URL recomendada para usuarios finales:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

En Arduino IDE:

```txt
Archivo > Preferencias > Gestor de URLs adicionales de tarjetas
```

Luego:

```txt
Herramientas > Placa > Gestor de tarjetas
```

Buscar:

```txt
JW Control ESP32 Boards
```

Instalar:

```txt
2.0.0
```

---

## Canal público y canal dev

Desde `v2.0.0` se recomienda separar el canal público y el canal de desarrollo.

### Canal público

```txt
JWPLC/package_jwplc_index.json
```

Contenido:

```txt
Releases estables y betas públicas si se decide publicarlas.
```

### Canal dev / interno

```txt
JWPLC/package_jwplc_index_dev.json
```

Contenido:

```txt
Alphas, betas, RCs y releases estables.
```

Motivo:

```txt
Evitar que el usuario final vea decenas de versiones alpha en Boards Manager.
```

---

## Validación realizada

Esta release recoge validaciones realizadas en `alpha31` y `beta1`.

| Área | Resultado |
|---|---|
| Instalación limpia desde Boards Manager | OK |
| Instalación y manejo desde otra PC | OK |
| Arduino IDE | OK |
| Arduino CLI | OK |
| `ESP32 Board` | OK |
| `JWPLC Basic` | OK |
| `JWPLC Basic Core` | OK |
| Sketch vacío | OK |
| Sketch Serial mínimo | OK |
| Subida por USB | OK |
| Monitor serial | OK |
| Librerías bundled desde package | OK |
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

## Ejemplos recomendados

### I/O industrial

- `DigitalIO_BlockRead`
- `DigitalIO_BlockMirror`

### RS-485

- `RS485_USB_Bridge`

### Modbus RTU

- `ModbusRTU_CRC_Test`
- `ModbusRTU_Master_Extern`
- `ModbusRTU_Master_ReadHoldingRegisters`
- `ModbusRTU_Master_WriteSingleRegister`
- `ModbusRTU_Slave_Extern`
- `ModbusRTU_Slave_HoldingRegisters`

### RTC

- `BasicRead`

### FRAM

- `Basic_RW`

### microSD

- `CardInfo`
- `BasicReadWrite`

### Display

- `Display_DotAPI_Minimal`
- `Display_UserUI_Callbacks`
- `Display_Idle_Return_Modes`

### Ethernet

- `Ethernet_Auto_DHCP_Status`
- `Ethernet_Display_Status`
- `Ethernet_SPI_Coexistence`

---

## Decisiones mantenidas

- No se publica `bootloader.bin` precompilado como definitivo.
- No se implementa precarga global de caché.
- No se precompilan librerías internas.
- No se elimina ningún periférico del autoload normal por velocidad.
- `platform.txt` no se modifica salvo bloqueante real.
- `app-only` queda documentado como herramienta útil, no como solución principal.
- OpenPLC no está integrado todavía.
- OTA no está definido todavía.
- FlashFreq 80 MHz no se asume como configuración final.

---

## No incluido

Esta release no incluye:

- Integración real OpenPLC.
- OTA.
- Recovery partition.
- `bootloader.bin` definitivo.
- Flash 8 MB/16 MB.
- Precompilación de librerías.
- Coredump formal como feature de usuario.
- Rediseño multicore.

---

## Observación de flujo de releases

Durante el desarrollo de `v2.0.0`, `alpha31` actuó como validación técnica completa y `beta1` fue publicada como etapa de package validation.

Para futuros ciclos, se acuerda que no será obligatorio publicar siempre una beta si la última alpha de verificación ya fue validada con instalación limpia, IDE, CLI, subida real, otra PC y periféricos principales.

Flujo permitido en próximos ciclos:

```txt
alphas técnicas -> última alpha de verificación -> release estable
```

Beta queda como etapa opcional cuando se requiera validación pública o de terceros.

RC queda como etapa opcional si se necesita un candidato final adicional.

---

## Checksums

Completar al generar el ZIP final:

```txt
Archivo: jwplc-esp32-2.0.0.zip
Size: PENDIENTE
SHA-256: PENDIENTE
```

Comandos sugeridos en PowerShell:

```powershell
certutil -hashfile jwplc-esp32-2.0.0.zip SHA256
(Get-Item .\jwplc-esp32-2.0.0.zip).Length
```

---

## Estado final

```txt
Release estable inicial.
```
