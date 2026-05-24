# Pre-Release - JWPLC Basic v2.0.0-alpha.31

## Título sugerido

```txt
JWPLC Basic v2.0.0-alpha.31 - Release readiness y snapshot de librerías JW
```

## Tag sugerido

```txt
v2.0.0-alpha.31
```

## Asset esperado

```txt
jwplc-esp32-2.0.0-alpha.31.zip
```

---

## Resumen

`v2.0.0-alpha.31` es una versión de **release readiness** para el package Arduino **JWPLC Basic v2.0.0**.

El objetivo principal es validar el package completo instalado desde cero y preparar el camino hacia `beta1-package-validation`.

Esta alpha no introduce features grandes. Su foco es:

- validar configuración final de placa;
- integrar snapshots actualizados de librerías JW;
- preparar documentación de validación;
- confirmar compilación local;
- dejar listo el flujo de publicación para pruebas limpias desde Boards Manager.

---

## Configuración de placa mantenida

Para `JWPLC Basic` y `JWPLC Basic Core` se mantiene la configuración definida en alpha30:

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

## Librerías JW incluidas

Esta versión incluye snapshots actualizados dentro de:

```txt
JWPLC/JWPLC-2.0.0/libraries/
```

| Librería | Versión |
|---|---:|
| `JW_MatrixButtons` | `1.0.4` |
| `JW_FRAM` | `1.0.2` |
| `JW_RTC` | `1.0.2` |
| `JW_SD` | `1.0.2` |

Estas librerías quedan incluidas dentro del package para que estén disponibles al instalar JWPLC desde Boards Manager.

---

## Cambios destacados

### Release readiness

Se agregan documentos de validación y revisión:

```txt
docs/alpha31-release-readiness-review.md
docs/alpha31-validation-checklist.md
docs/jw-libraries-snapshot.md
```

Estos archivos registran:

- revisión estática de archivos principales;
- checklist de instalación limpia;
- validación Arduino IDE;
- validación Arduino CLI;
- validación de ejemplos;
- validación de periféricos;
- criterio de avance a beta1;
- snapshot de librerías JW incluidas.

---

### Sincronización de librerías JW

Se integró el flujo de sincronización desde:

```txt
JW-Control/JW-Libraries
```

hacia:

```txt
JW-Control/platform-jwplc
```

mediante rama `sync/*` y PR, evitando empujar directamente a ramas alpha/beta/main.

Esto permite mantener `JW-Libraries` como fuente maestra y `platform-jwplc` como snapshot instalable mediante Boards Manager.

---

### Validación local preliminar

Se realizó una prueba local copiando el contenido actualizado del repo en la carpeta local de Arduino.

Resultado:

```txt
Compilación OK.
```

La prueba confirmó el uso de librerías JW actualizadas desde la carpeta del package:

```txt
JW_RTC 1.0.2
JW_FRAM 1.0.2
JW_SD 1.0.2
JW_MatrixButtons 1.0.4
```

Resultado de compilación observado:

```txt
Sketch usa 404497 bytes (12%) del espacio de programa.
Maximum is 3145728 bytes.
Variables globales usan 27620 bytes (8%) de RAM dinámica.
Upload speed 921600.
```

---

## Decisiones mantenidas

Esta versión mantiene las decisiones cerradas en alpha30:

- No publicar `bootloader.bin` precompilado como definitivo.
- No implementar precarga global de caché.
- No precompilar librerías internas.
- No tocar `platform.txt`.
- OTA queda fuera de esta alpha.
- OpenPLC no está integrado todavía.
- No quitar periféricos del autoload normal solo por velocidad.
- No asumir FlashFreq 80 MHz.

---

## Limitaciones conocidas

### OpenPLC

OpenPLC no está integrado todavía.

El mapa Modbus/OpenPLC documentado es preliminar y queda como base para futuras pruebas, no como integración funcional final.

### OTA

OTA no está definido todavía.

La configuración actual usa `huge_app` para maximizar espacio de aplicación en hardware con 4 MB de flash.

### Prioridad de librerías externas

Arduino puede resolver algunas dependencias externas desde `Documents/Arduino/libraries` si el usuario ya las tiene instaladas.

Durante la prueba local se observó selección desde sketchbook para algunas dependencias externas como Adafruit/Ethernet. Las librerías JW críticas sí fueron tomadas desde la carpeta del package.

---

## Validación recomendada después de publicar

Luego de publicar esta Pre-Release y actualizar `package_jwplc_index.json`:

1. Cerrar Arduino IDE.
2. Borrar o renombrar instalación local anterior:

```txt
C:\Users\<usuario>\AppData\Local\Arduino15\packages\jwplc
```

3. Abrir Arduino IDE.
4. Instalar `JW Control ESP32 Boards` desde Boards Manager.
5. Confirmar instalación de `2.0.0-alpha.31`.
6. Verificar que las librerías JW estén en:

```txt
C:\Users\<usuario>\AppData\Local\Arduino15\packages\jwplc\hardware\esp32\2.0.0-alpha.31\libraries
```

7. Compilar:
   - `ESP32 Board`
   - `JWPLC Basic`
   - `JWPLC Basic Core`

8. Compilar ejemplos principales:
   - `DigitalIO_BlockRead`
   - `DigitalIO_BlockMirror`
   - `Ethernet_Display_Status`
   - `Ethernet_SPI_Coexistence`
   - `RS485_USB_Bridge`
   - `ModbusRTU_CRC_Test`
   - `ModbusRTU_Slave_HoldingRegisters`
   - `ModbusRTU_Master_ReadHoldingRegisters`
   - `ModbusRTU_Master_WriteSingleRegister`

9. Validar periféricos:
   - I/O TCA6424A
   - Display
   - Botonera
   - RTC
   - FRAM
   - microSD
   - Ethernet
   - RS-485
   - Modbus RTU

---

## Checksums

Completar luego de generar el ZIP final:

```txt
Archivo: jwplc-esp32-2.0.0-alpha.31.zip
Size: PENDIENTE
SHA-256: PENDIENTE
```

Comandos sugeridos en PowerShell:

```powershell
certutil -hashfile jwplc-esp32-2.0.0-alpha.31.zip SHA256
(Get-Item .\jwplc-esp32-2.0.0-alpha.31.zip).Length
```

---

## Estado

```txt
Pre-Release de validación.
No recomendado aún como beta estable.
```

La siguiente etapa esperada, si la instalación limpia y pruebas principales pasan, es:

```txt
develop/beta1-package-validation
```
