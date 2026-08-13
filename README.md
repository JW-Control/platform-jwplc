# JWPLC Platform for Arduino IDE

Package personalizado de **JW Control** para programar placas basadas en **ESP32** desde Arduino IDE y Arduino CLI, orientado al ecosistema **JWPLC Basic**.

El objetivo del package es ofrecer una experiencia más directa que un core ESP32 genérico: menos variantes visibles, APIs de alto nivel y periféricos industriales integrados al runtime normal del JWPLC.

---

## Estado actual

| Canal | Versión | Estado | Índice |
|---|---|---|---|
| Público / estable | `v2.0.0` | Release estable recomendada para usuarios finales | `JWPLC/package_jwplc_index.json` |
| Dev / validación | `v2.1.0-alpha.4` | PreRelease técnica publicada y validada | `JWPLC/package_jwplc_index_dev.json` |

`v2.1.0-alpha.4` no reemplaza todavía a `v2.0.0` como versión estable pública. Su objetivo principal fue **reducir el tiempo de compilación manteniendo el autoload normal y todos los periféricos integrados**.

La Alpha4 fue validada desde un entorno limpio de Arduino CLI, instalando el package directamente desde el índice dev, sin una versión JWPLC previa instalada. Después de la instalación se validaron compilación, subida por USB y funcionamiento sobre un JWPLC Basic físico.

Release Alpha4:

```txt
https://github.com/JW-Control/platform-jwplc/releases/tag/v2.1.0-alpha.4
```

Artefacto publicado:

```txt
jwplc-esp32-2.1.0-alpha.4.zip
SHA-256: 4bdbdf383bb863d8f1a3b22f2b88ce47c68d2044a22f0f4afb6d883ccfdade5b
Tamaño: 24,698,966 bytes
```

---

## Qué cambió en v2.1.0-alpha.4

Alpha4 se concentra en optimización de compilación/cache y en cerrar la configuración de JWPLC Basic v2.0.

Puntos principales:

- Core de `JWPLC Basic` precompilado y empaquetado.
- Precompilación controlada de librerías seleccionadas.
- Backend W5x00 vendorizado a partir de Arduino Ethernet 2.0.2 y adaptado para JWPLC.
- Stack Adafruit utilizado por el TFT distribuido dentro del package y precompilado.
- `FS`, `SD`, `Wire`, `SPI` y librerías JWPLC seleccionadas precompiladas.
- Autoload normal conservado para Display, Ethernet, SD, FRAM, RTC, botonera, RS-485, Modbus RTU y TCA/I/O.
- Mutex SPI compartido conservado.
- Flash de JWPLC Basic fijada a **40 MHz** para el hardware v2.0.
- Nueva partición `jwplc_max_app_4mb` para JWPLC Basic.
- `JWPLC Basic Core` conserva `huge_app` y se evalúa de forma independiente.
- `app-only` queda como herramienta auxiliar de desarrollo, no como modo de upload público por defecto.
- No se publica un `bootloader.bin` precompilado definitivo.
- OTA no queda definida por Alpha4.
- OpenPLC continúa como integración externa/opcional y no se asume integrado al package Arduino.

---

## Rendimiento de compilación

El benchmark formal de Alpha4 utilizó como referencia una instalación oficial de Alpha3.

| Medición | Tiempo cold | TUs / compilaciones |
|---|---:|---:|
| Alpha3 instalada | **136.509 s** | 102 |
| Alpha4 P8 final | **59.901 s** | 5 |
| Reducción | **76.608 s** | — |
| Mejora relativa | **~56.12 %** | — |

En una comparación A-B-B-A en el mismo host, el estado P8 precompilado obtuvo **59.901 s** frente a **64.885 s** de la variante equivalente con `Wire` + `SPI` desde fuente: una mejora adicional de **4.985 s / 7.68 %**.

La optimización no se obtuvo retirando periféricos del autoload normal.

### Validación standalone adicional

Después de publicar Alpha4 se creó un entorno aislado de Arduino CLI sin ninguna plataforma JWPLC instalada previamente.

Resultados observados en ese entorno:

| Prueba | Resultado | Tiempo |
|---|---|---:|
| Instalación `jwplc:esp32@2.1.0-alpha.4` | PASS | — |
| Cold compile `01_empty` | PASS | 44.102 s |
| Cold compile `03_autoload_contract` | PASS | 42.460 s |
| Compile + upload del gate físico | PASS | — |
| Gate físico local | PASS | — |

Estos tiempos standalone son evidencia de que el package publicado conserva la optimización, pero **no deben compararse directamente** con el benchmark P8 porque la metodología y el estado del host no son idénticos.

Documentación detallada:

```txt
tools/build-speed-benchmark/JWPLC_ALPHA4_BUILD_SPEED_TIMINGS.md
tools/build-speed-benchmark/PRECOMPILE_STRATEGY.md
docs/v2.1.0-alpha.4/ALPHA4_CLOSURE_CHECKLIST.md
```

---

## Instalación

### Canal público recomendado

Para usuarios finales:

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

La versión estable publicada actualmente es:

```txt
2.0.0
```

### Canal dev / interno

Para validar alphas del ciclo 2.1.0:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Versiones publicadas en este canal:

```txt
2.1.0-alpha.1
2.1.0-alpha.2
2.1.0-alpha.3
2.1.0-alpha.4
```

Instalación directa de Alpha4 con Arduino CLI:

```powershell
arduino-cli core install jwplc:esp32@2.1.0-alpha.4
```

Para desarrollo local del repositorio se puede mantener un package separado, por ejemplo:

```txt
jwplc_local:esp32:jwplcbasic
```

y enlazarlo por junction/symlink hacia:

```txt
JWPLC/2.1.0
```

Esto evita modificar la instalación `jwplc:esp32` administrada por Boards Manager.

---

## Placas y FQBN

| Placa | FQBN | Uso recomendado |
|---|---|---|
| ESP32 Board | `jwplc:esp32:esp32` | Desarrollo ESP32 genérico dentro del package JWPLC |
| JWPLC Basic | `jwplc:esp32:jwplcbasic` | Hardware completo JWPLC Basic |
| JWPLC Basic Core | `jwplc:esp32:jwplcbasiccore` | Validación del core y perfil reducido sin FRAM, SD ni Ethernet |

FQBN normal de JWPLC Basic:

```txt
jwplc:esp32:jwplcbasic
```

---

## Configuración de JWPLC Basic en Alpha4

La configuración validada de `JWPLC Basic` queda fijada para reducir combinaciones no probadas.

| Parámetro | JWPLC Basic | JWPLC Basic Core |
|---|---|---|
| CPU | 240 MHz | 240 MHz |
| Flash size | 4 MB | 4 MB |
| Flash frequency | **40 MHz** | 40 MHz |
| Flash mode | DIO | DIO |
| Boot base | QIO | QIO |
| Bootloader address | `0x1000` | `0x1000` |
| Partition scheme | **`jwplc_max_app_4mb`** | `huge_app` |
| Máximo de aplicación | **4,063,232 bytes** | 3,145,728 bytes |
| Upload speed | 921600 | 921600 |

En Alpha4, `JWPLC Basic` usa core precompilado mediante:

```txt
jwplcbasic.build.core=jwcontrol_p2
jwplcbasic.build.extra_libs=.../precompiled/core/JWPLCBASIC/core.a
```

`JWPLC Basic Core` conserva el core fuente y `huge_app`.

### Partición Max App de 4 MB

`jwplc_max_app_4mb` conserva:

```txt
NVS       0x9000   0x5000
otadata   0xE000   0x2000
app0      0x10000  0x3E0000
coredump  0x3F0000 0x10000
```

Capacidad de aplicación:

```txt
4,063,232 bytes
3968 KiB
3.875 MiB
```

Ganancia frente a `huge_app`:

```txt
917,504 bytes
896 KiB
+29.17 %
```

Se retiró SPIFFS de este perfil para priorizar la aplicación. La presencia de `otadata` y del subtipo `ota_0` **no define por sí sola una política OTA**; OTA continúa pendiente de decisión.

---

## Periféricos integrados

`JWPLC Basic` conserva en el flujo normal:

- I/O industrial por TCA6424A.
- Display TFT ST7789.
- Botonera frontal.
- RTC.
- FRAM de 8 KB.
- microSD.
- Ethernet W5500.
- RS-485.
- Modbus RTU base.

Objetos globales principales:

```cpp
JWPLC_Display
JWPLC_Ethernet
JWPLC_SD
JWPLC_FRAM
JWPLC_RTC
JWPLC_Buttons
JWPLC_RS485
JWPLC_ModbusRTU
```

El usuario puede trabajar con las E/S industriales usando la sintaxis habitual de Arduino:

```cpp
pinMode(I0_0, INPUT);
pinMode(Q0_0, OUTPUT);

bool state = digitalRead(I0_0);
digitalWrite(Q0_0, state);
```

APIs por bloque:

```cpp
digitalReadBlock(I0_X);
digitalWriteBlock(Q0_X, bitmap);

JWPLC_readInputs();
JWPLC_readOutputs();
JWPLC_writeOutputs(bitmap);
```

---

## Dependencias de Boards Manager

Las `toolsDependencies` externas del package Alpha4 son exactamente:

| Tool | Versión |
|---|---|
| `esp-x32` | `2601` |
| `esptool_py` | `5.2.0` |
| `mkspiffs` | `0.2.3` |
| `mklittlefs` | `4.0.2-db0513a` |
| `esp32-libs` | `3.3.8` |

Estas herramientas son distintas de las librerías bundled/precompiladas que viajan dentro del ZIP del package.

La instalación standalone de Alpha4 confirmó que Arduino CLI descarga estas cinco dependencias automáticamente antes de instalar `jwplc:esp32@2.1.0-alpha.4`.

---

## Librerías bundled y precompiladas en Alpha4

La siguiente tabla corresponde al package Alpha4 instalado y utilizado durante el gate físico standalone.

| Librería | Versión | Estado Alpha4 |
|---|---:|---|
| `JWPLC_Display` | 1.0.1 | Precompilada |
| `JWPLC_GlobalPeripherals` | 1.0.0 | Bundled / autoload liviano |
| `Adafruit ST7735 and ST7789 Library` | 1.11.0 | Precompilada |
| `Adafruit GFX Library` | 1.12.4 | Precompilada |
| `Adafruit BusIO` | 1.17.4 | Precompilada |
| `Wire` | 3.3.8 | Precompilada |
| `SPI` | 3.3.8 | Precompilada |
| `JW_RTC` | 1.0.2 | Precompilada |
| `JW_FRAM` | 1.0.3 | Precompilada |
| `JW_SD` | 1.0.2 | Precompilada |
| `SD` | 3.3.8 | Precompilada |
| `FS` | 3.3.8 | Precompilada |
| `JW_MatrixButtons` | 1.0.5 | Precompilada |
| `JWPLC_Ethernet` | 1.0.0 | Wrapper JWPLC |
| `JWPLC Ethernet W5x00 Backend` | 2.0.2 | Vendorizada + precompilada |
| `JWPLC_RS485` | 1.0.1 | Bundled |
| `JWPLC_ModbusRTU` | 1.0.0 | Precompilada |

El core de `JWPLC Basic` se distribuye además como archive precompilado independiente:

```txt
JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
```

### Backend Ethernet

El backend W5x00 parte de **Arduino Ethernet 2.0.2**, pero Alpha4 incorpora un patch específico de JWPLC.

La adaptación elimina una espera genérica de aproximadamente 560 ms dentro de la inicialización W5x00 que era redundante en JWPLC Basic porque el hardware ya controla el reset físico del W5500 antes de adquirir el mutex SPI.

Se mantiene:

- el mutex SPI global;
- la exclusión entre periféricos;
- el autoload Ethernet;
- las APIs públicas existentes.

Por ello debe describirse como:

```txt
Arduino Ethernet 2.0.2 + patch específico JWPLC
```

---

## Bus SPI compartido

En JWPLC Basic comparten SPI:

- TFT ST7789;
- W5500 Ethernet;
- microSD;
- FRAM.

Pines principales del bus:

```txt
MOSI    GPIO23
MISO    GPIO19
SCK     GPIO18
TFT CS  GPIO33
SD CS   GPIO32
FRAM CS GPIO13
ETH CS  GPIO5
```

El core conserva la protección mediante:

```cpp
jwplcSPI_acquire();
jwplcSPI_release();
```

Para callbacks gráficos repetitivos se recomienda leer Ethernet, SD o FRAM fuera del callback y dibujar usando valores ya cacheados.

---

## Display y botonera

`JWPLC_Display` se inicializa automáticamente en JWPLC Basic.

Configuraciones principales pueden definirse desde `setup()`:

```cpp
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
JWPLC_Display.setIdleRefreshPeriodMs(1000);
JWPLC_Display.setUserRefreshPeriodMs(100);

JWPLC_Display.setRunLed(true);
JWPLC_Display.setErrLed(false);
JWPLC_Display.setBusLedAuto(true);
JWPLC_Display.setEthLedAuto(true);
```

`setUserRefreshPeriodMs(100)` corresponde a un objetivo de 10 FPS para la pantalla USER.

Los seis botones físicos validados son:

```txt
UP
DOWN
LEFT
RIGHT
CANCEL
OK
```

CANCEL se representa internamente como `BTN_ESC`.

---

## Ethernet

`JWPLC_Ethernet` integra el W5500 con el runtime de JWPLC Basic.

El runtime normal puede encargarse de:

- inicialización automática;
- DHCP;
- reconexión;
- estado de link;
- coexistencia SPI;
- indicador `ETH` del display.

Durante Alpha4 se validaron físicamente:

- W5500;
- DHCP;
- link Ethernet;
- coexistencia Ethernet + TFT + FRAM + SD;
- servidor HTTP.

---

## RS-485 y Modbus RTU

JWPLC Basic usa RS-485 sobre `Serial2`.

```cpp
JWPLC_RS485.begin(115200, SERIAL_8N1);
```

| Señal | ESP32 |
|---|---:|
| RX2 | IO16 |
| TX2 | IO17 |

El hardware utiliza un transceptor con autodirección, por lo que no se controla DE/RE manualmente desde el sketch normal.

Modbus RTU trabaja sobre `JWPLC_RS485`:

```cpp
JWPLC_ModbusRTU.begin();
```

Funciones slave disponibles:

| Código | Función |
|---:|---|
| `0x03` | Read Holding Registers |
| `0x06` | Write Single Register |
| `0x10` | Write Multiple Registers |

APIs master principales:

```cpp
readHoldingRegisters();
writeSingleRegister();
```

En Alpha4 se validaron físicamente FC03 y FC06, además del enlace RS-485.

---

## App-only, bootloader y OTA

### App-only

El flujo app-only permite actualizar únicamente la aplicación en `0x10000` cuando la placa ya posee bootloader y particiones compatibles.

Conclusión de Alpha4:

- es útil como herramienta auxiliar de desarrollo;
- no se adopta como upload normal por defecto;
- no se agrega un menú público `UploadMode` en Alpha4;
- el upload completo sigue siendo la ruta normal y segura de Arduino IDE.

### Bootloader

Configuración cerrada para JWPLC Basic v2.0:

```txt
FlashFreq: 40 MHz
Flash mode: DIO
build.boot: qio
bootloader address: 0x1000
```

El bootloader reproducible de 40 MHz fue validado físicamente, pero **no se publica `bootloader.bin` como artefacto definitivo de variante**.

El antiguo `bootloader.bin` de variante asociado a 80 MHz fue retirado.

### OTA

OTA no queda definida en Alpha4.

La partición Max App conserva `otadata`, pero esto no debe interpretarse como una política OTA terminada.

---

## OpenPLC Editor

OpenPLC no forma parte obligatoria del package Arduino.

La integración existente con OpenPLC Editor v4 se mantiene como **externa/opcional mediante patch**, conservada en:

```txt
docs/alpha32_openplc_integration/
```

El uso normal de JWPLC Basic desde Arduino IDE o Arduino CLI no requiere OpenPLC.

No se debe asumir OpenPLC integrado al runtime de Alpha4.

---

## Validación física de Alpha4

Gate local final:

| Área | Resultado |
|---|---|
| Display ready | PASS |
| RTC | PASS |
| FRAM 8 KB | PASS |
| microSD | PASS |
| UP/DOWN/LEFT/RIGHT/CANCEL/OK | PASS |
| 8 entradas digitales | PASS |
| 8 salidas / relés | PASS |
| TFT visual | PASS |
| W5500 / DHCP | PASS |
| Ethernet + TFT + FRAM + SD | PASS |
| HTTP | PASS |
| RS-485 | PASS |
| Modbus RTU FC03 | PASS |
| Modbus RTU FC06 | PASS |

Durante el gate standalone posterior a la publicación también se observó en el ROM boot:

```txt
mode:DIO, clock div:2
```

Resultado global del gate local:

```txt
ALPHA4_LOCAL_PHYSICAL_GATE=PASS
```

Documentación:

```txt
tools/build-speed-benchmark/JWPLC_ALPHA4_LOCAL_PHYSICAL_GATE.md
tools/build-speed-benchmark/JWPLC_ALPHA4_COMMUNICATION_PHYSICAL_GATES.md
docs/v2.1.0-alpha.4/ALPHA4_PUBLICATION_VALIDATION.md
```

---

## Estructura principal

```txt
JWPLC/
  package_jwplc_index.json
  package_jwplc_index_dev.json

  2.1.0/
    boards.txt
    platform.txt
    platform.local.txt
    cores/
    variants/
    libraries/
    precompiled/
      core/
    tools/
      partitions/

  JWPLC-2.0.0/
    ...

  Test_Codes/
    ...

docs/
  alpha32_openplc_integration/
  v2.1.0-alpha.4/

tools/
  build-speed-benchmark/
```

Notas:

- `JWPLC/2.1.0` es la carpeta activa del ciclo de desarrollo 2.1.0.
- `JWPLC/JWPLC-2.0.0` conserva la fuente histórica/estable de v2.0.0.
- `JWPLC/Test_Codes` conserva pruebas internas y sketches de validación.
- `tools/build-speed-benchmark` conserva scripts, decisiones y resultados técnicos de Alpha4; los directorios locales de build/resultados pesados están excluidos mediante `.gitignore`.

---

## Repositorio

```txt
https://github.com/JW-Control/platform-jwplc
```

---

## Licencia

Este repositorio mantiene la licencia indicada en el archivo `LICENSE`.
