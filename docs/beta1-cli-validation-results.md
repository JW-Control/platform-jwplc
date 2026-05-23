# Beta1 - Resultados de validación Arduino CLI

Branch:

```txt
develop/beta1-package-validation
```

Versión instalada validada:

```txt
JW Control ESP32 Boards 2.0.0-alpha.31
```

Objetivo:

```txt
Cerrar el pendiente explícito de Arduino CLI heredado desde alpha31 antes de empaquetar beta1.
```

---

## 1. Arduino CLI

Versión usada:

```txt
arduino-cli 1.0.2
```

Nota:

```txt
Arduino CLI indicó que existe una versión más reciente, pero la versión 1.0.2 ejecutó correctamente las pruebas requeridas.
```

---

## 2. Core instalado

Resultado de `arduino-cli core list`:

| Core | Installed | Latest | Name |
|---|---:|---:|---|
| `jwplc:esp32` | `2.0.0-alpha.31` | `2.0.0-alpha.31` | JW Control ESP32 Boards |

Resultado:

```txt
OK
```

---

## 3. Placas detectadas

Resultado de `arduino-cli board listall jwplc`:

| Board Name | FQBN |
|---|---|
| ESP32 Board | `jwplc:esp32:esp32` |
| JWPLC Basic | `jwplc:esp32:jwplcbasic` |
| JWPLC Basic Core | `jwplc:esp32:jwplcbasiccore` |

Resultado:

```txt
OK
```

---

## 4. Compilación por CLI

Sketch usado:

```txt
C:\JWPLC_CLI_TEST\Serial_Minimal\Serial_Minimal.ino
```

### 4.1 JWPLC Basic

Comando:

```bat
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic .
```

Resultado:

```txt
Sketch uses 414325 bytes (13%) of program storage space. Maximum is 3145728 bytes.
Global variables use 27620 bytes (8%) of dynamic memory, leaving 300060 bytes for local variables. Maximum is 327680 bytes.
```

Estado:

```txt
OK
```

### 4.2 JWPLC Basic Core

Comando:

```bat
arduino-cli compile --fqbn jwplc:esp32:jwplcbasiccore .
```

Resultado:

```txt
Sketch uses 363576 bytes (11%) of program storage space. Maximum is 3145728 bytes.
Global variables use 26828 bytes (8%) of dynamic memory, leaving 300852 bytes for local variables. Maximum is 327680 bytes.
```

Estado:

```txt
OK
```

### 4.3 ESP32 Board

Comando:

```bat
arduino-cli compile --fqbn jwplc:esp32:esp32 .
```

Resultado:

```txt
Sketch uses 280712 bytes (21%) of program storage space. Maximum is 1310720 bytes.
Global variables use 22084 bytes (6%) of dynamic memory, leaving 305596 bytes for local variables. Maximum is 327680 bytes.
```

Estado:

```txt
OK
```

---

## 5. Librerías detectadas desde el package

Durante la compilación por CLI de `JWPLC Basic`, se confirmó el uso de librerías JW desde:

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

También se confirmó:

```txt
Used platform Version        Path
jwplc:esp32   2.0.0-alpha.31 C:\Users\jeykc\AppData\Local\Arduino15\packages\jwplc\hardware\esp32\2.0.0-alpha.31
```

---

## 6. Upload por CLI

Comando:

```bat
arduino-cli upload -p COM14 --fqbn jwplc:esp32:jwplcbasic .
```

Resultado:

```txt
esptool v5.2.0
Connected to ESP32 on COM14
Chip type: ESP32-D0WD-V3
Changing baud rate to 921600
Hash of data verified
Hard resetting via RTS pin
New upload port: COM14 (serial)
```

Estado:

```txt
OK
```

---

## 7. Monitor serial por CLI

Comando:

```bat
arduino-cli monitor -p COM14 -c baudrate=115200
```

Resultado observado:

```txt
JWPLC alpha31 CLI OK
JWPLC alpha31 CLI OK
JWPLC alpha31 CLI OK
```

Estado:

```txt
OK
```

---

## 8. Conclusión

```txt
Arduino CLI post-publicación queda validado correctamente sobre el package instalado desde Boards Manager.
```

Resultado general:

| Prueba | Resultado |
|---|---|
| `arduino-cli core list` | OK |
| `arduino-cli board listall jwplc` | OK |
| Compile `jwplc:esp32:jwplcbasic` | OK |
| Compile `jwplc:esp32:jwplcbasiccore` | OK |
| Compile `jwplc:esp32:esp32` | OK |
| Upload CLI a `JWPLC Basic` por COM14 | OK |
| Monitor CLI 115200 | OK |

Decisión:

```txt
Pendiente CLI heredado desde alpha31: CERRADO.
```
