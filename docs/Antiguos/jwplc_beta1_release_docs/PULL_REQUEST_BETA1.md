# Pull Request - JWPLC Basic v2.0.0-beta.1

## Título sugerido

```txt
Beta1: package validation y preparación de canal público
```

## Ramas

```txt
Base: main
Compare: develop/beta1-package-validation
```

---

## Resumen

Este PR integra el trabajo de **JWPLC Basic v2.0.0-beta.1**, primera etapa beta del package Arduino **JW Control ESP32 Boards**.

Beta1 parte de `v2.0.0-alpha.31`, que quedó validada técnicamente con instalación limpia desde Boards Manager, pruebas en Arduino IDE, Arduino CLI, subida real por USB y validación de periféricos principales.

El objetivo de beta1 es pasar de una alpha validada a un package candidato para usuarios reales, con foco en documentación, empaquetado, package index, instalación limpia y ausencia de regresiones.

---

## Estado de la etapa

```txt
Versión objetivo: 2.0.0-beta.1
Branch: develop/beta1-package-validation
Tipo: beta / feature freeze
```

Regla de beta:

```txt
Beta = feature freeze.
```

No se agregan features grandes. Solo se permiten correcciones, documentación, packaging y ajustes críticos.

---

## Cambios principales

### 1. Inicio formal de beta1

Se creó la rama:

```txt
develop/beta1-package-validation
```

desde:

```txt
develop/alpha31-release-readiness
```

La etapa queda documentada en:

```txt
docs/beta1-validation-plan.md
```

---

### 2. Validación Arduino CLI post-publicación

Se cerró el pendiente explícito heredado desde alpha31:

```txt
Arduino CLI post-publicación: OK
```

Validado con:

```txt
arduino-cli 1.0.2
```

Comandos ejecutados:

```bat
arduino-cli core update-index
arduino-cli core list
arduino-cli board listall jwplc
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic .
arduino-cli compile --fqbn jwplc:esp32:jwplcbasiccore .
arduino-cli compile --fqbn jwplc:esp32:esp32 .
arduino-cli upload -p COM14 --fqbn jwplc:esp32:jwplcbasic .
arduino-cli monitor -p COM14 -c baudrate=115200
```

Resultados:

| Prueba CLI | Resultado |
|---|---|
| `core update-index` | OK |
| `core list` | OK |
| `board listall jwplc` | OK |
| Compile `JWPLC Basic` | OK |
| Compile `JWPLC Basic Core` | OK |
| Compile `ESP32 Board` | OK |
| Upload por COM14 | OK |
| Monitor serial | OK |

Resultado observado:

```txt
JWPLC alpha31 CLI OK
JWPLC alpha31 CLI OK
JWPLC alpha31 CLI OK
```

Documentado en:

```txt
docs/beta1-cli-validation-results.md
```

---

### 3. README principal actualizado para beta1

Se actualizó `README.md` para que represente la etapa beta1.

Cambios aplicados:

- Se reemplazó `Estado de alpha31` por `Estado de beta1`.
- Se declaró `v2.0.0-beta.1` como etapa de validación de package instalable.
- Se mantuvieron advertencias de OpenPLC, OTA, bootloader, app-only y autoload.
- Se mantuvo la configuración fija validada:
  - CPU 240 MHz.
  - Flash 4 MB.
  - Flash frequency 40 MHz.
  - Flash mode DIO.
  - Bootloader base QIO.
  - Partition scheme `huge_app`.
  - Upload speed 921600.
- Se actualizó la lista de ejemplos recomendados con nombres reales validados.
- Se añadió Arduino CLI al checklist rápido de validación.

Documentado en:

```txt
docs/beta1-readme-links-review.md
docs/beta1-readme-update-results.md
```

---

### 4. Política de links de librerías

Se revisó la política de links del README final.

Decisión:

```txt
Las librerías JW externas/distribuibles deben apuntar al main de sus repos oficiales.
```

Esto aplica a:

| Librería | Link público |
|---|---|
| `JW_FRAM` | `JW-Control/JW_FRAM` |
| `JW_RTC` | `JW-Control/JW_RTC` |
| `JW_SD` | `JW-Control/JW_SD` |
| `JW_MatrixButtons` | `JW-Control/JW_MatrixButtons` |
| `JW_DWIN_RS485` | `JW-Control/JW_DWIN_RS485` |

Motivo:

```txt
El README público final debe apuntar al punto canónico donde el usuario puede revisar o descargar cada librería independiente.
```

Para librerías internas del package, se mantienen enlaces al repositorio `platform-jwplc`.

---

### 5. Ejemplos recomendados actualizados

La lista del README fue ajustada para coincidir con ejemplos reales usados/revisados durante alpha31/beta1.

#### I/O industrial

```txt
DigitalIO_BlockRead
DigitalIO_BlockMirror
```

#### RS-485

```txt
RS485_USB_Bridge
```

#### Modbus RTU

```txt
ModbusRTU_CRC_Test
ModbusRTU_Master_Extern
ModbusRTU_Master_ReadHoldingRegisters
ModbusRTU_Master_WriteSingleRegister
ModbusRTU_Slave_Extern
ModbusRTU_Slave_HoldingRegisters
```

#### RTC

```txt
BasicRead
```

#### FRAM

```txt
Basic_RW
```

#### microSD

```txt
CardInfo
BasicReadWrite
```

#### Display

```txt
Display_DotAPI_Minimal
Display_UserUI_Callbacks
Display_Idle_Return_Modes
```

#### Ethernet

```txt
Ethernet_Auto_DHCP_Status
Ethernet_Display_Status
Ethernet_SPI_Coexistence
```

---

### 6. Propuesta de canales de package index

Se propone separar los índices de Boards Manager en dos canales:

#### Canal público

```txt
JWPLC/package_jwplc_index.json
```

Uso:

```txt
Usuarios finales.
```

Contenido recomendado:

```txt
Betas, RCs y releases estables.
```

#### Canal desarrollo / interno

```txt
JWPLC/package_jwplc_index_dev.json
```

Uso:

```txt
Pruebas internas y validación de alphas.
```

Contenido recomendado:

```txt
Alphas, betas, RCs y releases estables.
```

Decisión recomendada para beta1:

```txt
2.0.0-beta.1 debe aparecer en ambos índices.
Las alphas deben permanecer solo en el índice dev.
```

---

## Validaciones heredadas desde alpha31

Alpha31 quedó validada con instalación limpia desde Boards Manager y pruebas reales de hardware.

| Área | Resultado |
|---|---|
| Instalación limpia Boards Manager | OK |
| Instalación y manejo desde otra PC | OK |
| Arduino IDE | OK |
| Arduino CLI | OK |
| `ESP32 Board` | OK |
| `JWPLC Basic` | OK |
| `JWPLC Basic Core` | OK |
| Sketch Serial mínimo | OK |
| Subida por USB | OK |
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

## Configuración validada

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

## Librerías bundled esperadas

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

## No incluido en beta1

Este PR no integra ni asume:

- OpenPLC real.
- OTA.
- FlashFreq 80 MHz.
- Publicación de `bootloader.bin` definitivo.
- Precompilación de librerías internas.
- Precarga global de caché.
- Eliminación de periféricos del autoload normal.
- Cambios de arquitectura multicore.
- Coredump formal como feature de usuario.

---

## Pendientes antes de merge/publicación

Antes de cerrar beta1, completar:

- [ ] Generar `jwplc-esp32-2.0.0-beta.1.zip`.
- [ ] Publicar GitHub Pre-Release `v2.0.0-beta.1`.
- [ ] Calcular SHA-256.
- [ ] Calcular size.
- [ ] Actualizar `package_jwplc_index.json`.
- [ ] Crear/actualizar `package_jwplc_index_dev.json`.
- [ ] Instalar beta1 desde Boards Manager.
- [ ] Validar Arduino IDE sobre beta1 real.
- [ ] Validar Arduino CLI sobre beta1 real.
- [ ] Confirmar que no hay regresión respecto a alpha31.

---

## Checklist de PR

- [x] Branch beta1 creado.
- [x] Plan de validación beta1 documentado.
- [x] Arduino CLI post-publicación validado.
- [x] README actualizado a beta1.
- [x] Links de librerías revisados.
- [x] Ejemplos recomendados actualizados.
- [x] No se abrieron features grandes.
- [x] Decisiones alpha30/alpha31 respetadas.
- [ ] ZIP beta1 generado.
- [ ] Pre-Release beta1 publicado.
- [ ] Package index público actualizado.
- [ ] Package index dev creado/actualizado.
- [ ] Instalación limpia beta1 validada.

---

## Decisión

Este PR deja `develop/beta1-package-validation` listo para la fase de empaquetado y publicación de:

```txt
v2.0.0-beta.1
```

Si la instalación limpia de beta1 no presenta bloqueantes, la siguiente etapa recomendada será:

```txt
develop/rc1-v2.0.0
```
