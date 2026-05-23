# Beta1 - Resultado de actualización del README

Branch:

```txt
develop/beta1-package-validation
```

Commit principal del README:

```txt
30266b5396d6afb03a340cf9e17ff28f34e37291
```

---

## 1. Resultado general

```txt
README principal actualizado y apto para empaquetado beta1.
```

El README fue actualizado para que represente la etapa actual `v2.0.0-beta.1` y deje de arrastrar el encabezado/documentación de alpha31.

---

## 2. Cambios aplicados

| Cambio | Resultado |
|---|---|
| Cambiar `Estado de alpha31` por `Estado de beta1` | OK |
| Declarar que beta1 es validación de package instalable | OK |
| Mantener explícitamente qué queda fuera de beta1 | OK |
| Mantener configuración fija heredada de alpha30/alpha31 | OK |
| Mantener FQBN recomendado `jwplc:esp32:jwplcbasic` | OK |
| Mantener advertencias OpenPLC/OTA/bootloader/app-only/autoload | OK |
| Actualizar sección de librerías incluidas | OK |
| Mantener links externos a `main` de repos oficiales | OK |
| Aclarar que `JW_DWIN_RS485` es complementaria | OK |
| Actualizar lista de ejemplos recomendados con nombres reales validados | OK |
| Añadir Arduino CLI al checklist rápido | OK |

---

## 3. Política de links confirmada

### Librerías internas del package

Se mantienen links públicos al repo `platform-jwplc`:

- `JWPLC_Display`
- `JWPLC_Ethernet`
- `JWPLC_RS485`
- `JWPLC_ModbusRTU`

### Librerías JW externas / distribuibles

Se mantienen links al branch `main` de sus repos oficiales:

- `JW_FRAM`
- `JW_RTC`
- `JW_SD`
- `JW_MatrixButtons`
- `JW_DWIN_RS485`

Motivo:

```txt
El README final público debe apuntar a la referencia canónica donde el usuario puede revisar o descargar la librería independiente.
```

---

## 4. Ejemplos recomendados actualizados

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

## 5. Decisión

```txt
Bloque README/links/ejemplos: APROBADO para beta1.
```

Siguiente paso:

```txt
Preparar empaquetado de v2.0.0-beta.1: ZIP, Pre-Release, SHA-256, size y package_jwplc_index.json.
```
