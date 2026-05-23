# Beta1 - Revisión de README, links y ejemplos

Branch:

```txt
develop/beta1-package-validation
```

Versión objetivo:

```txt
JWPLC Basic v2.0.0-beta.1
```

---

## 1. Objetivo

Revisar el README principal, los enlaces de librerías y la lista de ejemplos antes de empaquetar `2.0.0-beta.1`.

Esta revisión no cambia arquitectura, core, variantes, `boards.txt` ni `platform.txt`.

---

## 2. Resultado general

```txt
README principal: funcional, pero requiere actualización de estado alpha31 -> beta1.
Links internos: válidos, pero se recomienda usar rutas relativas.
Links externos: válidos.
Lista de ejemplos: requiere ajuste para coincidir con ejemplos realmente usados/validados.
Bloqueantes técnicos: ninguno.
```

---

## 3. README principal

Archivo revisado:

```txt
README.md
```

El README contiene correctamente:

- descripción general de JWPLC Platform;
- instalación desde Boards Manager;
- URL del package index;
- placas disponibles;
- configuración fija de `JWPLC Basic` y `JWPLC Basic Core`;
- FQBN recomendado `jwplc:esp32:jwplcbasic`;
- periféricos integrados;
- I/O industrial por TCA6424A;
- APIs globales JWPLC;
- advertencias sobre OpenPLC, OTA, app-only y bootloader;
- reglas de coexistencia SPI;
- tiempos de compilación;
- checklist rápido de validación.

### Ajuste recomendado antes de beta1

El README todavía inicia con una sección:

```txt
Estado de alpha31
```

Para beta1 debe reemplazarse por:

```txt
Estado de beta1
```

Texto sugerido:

```md
## Estado de beta1

`v2.0.0-beta.1` corresponde a una etapa de validación de package instalable.

Objetivo:

```txt
Validar el package completo como candidato beta después de alpha31.
```

Beta1 no agrega features grandes. Su foco es:

- confirmar instalación limpia desde Boards Manager;
- validar Arduino IDE y Arduino CLI;
- revisar README principal;
- revisar links de librerías;
- confirmar ejemplos visibles en Arduino IDE;
- publicar y validar `package_jwplc_index.json` con `2.0.0-beta.1`;
- confirmar que no existen regresiones respecto a alpha31.

Queda fuera de beta1:

- integración real con OpenPLC;
- definición final de OTA;
- publicación de `bootloader.bin` como definitivo;
- precompilación de librerías internas;
- cambios de arquitectura multicore;
- eliminación de periféricos del autoload normal solo por velocidad.
```

---

## 4. Links internos de librerías

Links internos actuales:

- `JWPLC_Display`
- `JWPLC_Ethernet`
- `JWPLC_RS485`
- `JWPLC_ModbusRTU`

Estado:

```txt
Válidos.
```

Observación:

Actualmente apuntan a URLs absolutas con branch `main`, por ejemplo:

```txt
https://github.com/JW-Control/platform-jwplc/blob/main/JWPLC/JWPLC-2.0.0/libraries/JWPLC_Display/README.md
```

Para beta1 se recomienda cambiar a rutas relativas para que el README apunte a la documentación de la misma rama/tag:

```md
[JWPLC_Display](JWPLC/JWPLC-2.0.0/libraries/JWPLC_Display/README.md)
[JWPLC_Ethernet](JWPLC/JWPLC-2.0.0/libraries/JWPLC_Ethernet/README.md)
[JWPLC_RS485](JWPLC/JWPLC-2.0.0/libraries/JWPLC_RS485/README.md)
[JWPLC_ModbusRTU](JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/README.md)
```

Ventaja:

```txt
Los links funcionarán correctamente al navegar por branch, tag, beta o main sin quedar amarrados a main.
```

---

## 5. Links externos de librerías

Links externos revisados:

| Librería | Estado |
|---|---|
| `JW_FRAM` | OK |
| `JW_RTC` | OK |
| `JW_SD` | OK |
| `JW_MatrixButtons` | OK |
| `JW_DWIN_RS485` | OK |

Observación:

`JW_DWIN_RS485` no forma parte del runtime base de JWPLC Basic v2.0.0. Puede mantenerse en el README como librería complementaria, siempre que quede claro que no reemplaza a `JWPLC_RS485` ni a `JWPLC_ModbusRTU`.

---

## 6. Lista de ejemplos

El README actual mantiene una lista de ejemplos recomendados con nombres de etapas anteriores, por ejemplo:

```txt
FRAM_BootCounter
FRAM_ReadWrite
FRAM_ConfigStorage
SD_FileList
SD_ReadWrite
SD_DisplayBrowser
RTC_ReadTime
RTC_SetTime
RTC_DisplayClock
```

Durante la validación real de alpha31/beta1 se usaron y confirmaron estos nombres visibles/prácticos:

### I/O

```txt
DigitalIO_BlockRead
DigitalIO_BlockMirror
```

### RS-485

```txt
RS485_USB_Bridge
```

### Modbus RTU

```txt
ModbusRTU_CRC_Test
ModbusRTU_Master_Extern
ModbusRTU_Master_ReadHoldingRegisters
ModbusRTU_Master_WriteSingleRegister
ModbusRTU_Slave_Extern
ModbusRTU_Slave_HoldingRegisters
```

### RTC

```txt
BasicRead
```

### FRAM

```txt
Basic_RW
```

### microSD

```txt
CardInfo
BasicReadWrite
```

### Display

```txt
Display_DotAPI_Minimal
Display_UserUI_Callbacks
Display_Idle_Return_Modes
```

### Ethernet

```txt
Ethernet_Auto_DHCP_Status
Ethernet_Display_Status
Ethernet_SPI_Coexistence
```

Recomendación:

```txt
Actualizar la sección de ejemplos del README principal para que use estos nombres reales antes de publicar beta1.
```

---

## 7. Advertencias que deben mantenerse

El README debe mantener explícitamente:

- OpenPLC no está integrado todavía.
- OTA no está definido todavía.
- FlashFreq final no cambia a 80 MHz.
- No se publica `bootloader.bin` como definitivo.
- `app-only` no resuelve compilación.
- No se quitan periféricos del autoload normal por velocidad.
- `JWPLC Basic Core` puede reportar Ethernet/SD/FRAM disabled.

---

## 8. Decisión

```txt
README principal apto como base, pero debe recibir ajustes antes de empaquetar beta1.
```

Cambios recomendados antes de publicar `2.0.0-beta.1`:

1. Cambiar sección `Estado de alpha31` por `Estado de beta1`.
2. Cambiar links internos absolutos a rutas relativas.
3. Actualizar lista de ejemplos recomendados con nombres reales validados.
4. Mantener advertencias OpenPLC/OTA/bootloader/app-only/autoload.

Resultado:

```txt
No hay bloqueante técnico. Hay ajustes documentales recomendados antes del ZIP beta1.
```
