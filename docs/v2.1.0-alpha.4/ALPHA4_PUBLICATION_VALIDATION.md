# JWPLC v2.1.0-alpha.4 — validación de publicación standalone

Fecha de cierre: 2026-08-13

## 1. Objetivo

Verificar que `v2.1.0-alpha.4` puede instalarse y utilizarse directamente desde el índice dev publicado, sin depender de:

- una instalación previa de `jwplc:esp32`;
- `jwplc_local`;
- junction/symlink hacia el repositorio local;
- librerías JWPLC externas instaladas manualmente.

La prueba se realizó con Arduino CLI en un árbol de datos aislado.

## 2. Índice utilizado

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Alpha4 publicada:

```txt
jwplc:esp32@2.1.0-alpha.4
```

Antes de la instalación el entorno aislado reportó:

```txt
No platforms installed.
```

El índice dev expuso correctamente:

```txt
jwplc:esp32 2.1.0-alpha.4 JW Control ESP32 Boards
```

Resultado:

```txt
ALPHA4_STANDALONE_PREINSTALL_ISOLATION=PASS
```

## 3. Instalación standalone

Arduino CLI descargó e instaló automáticamente:

```txt
jwplc:esp-x32@2601
jwplc:esptool_py@5.2.0
jwplc:mkspiffs@0.2.3
jwplc:mklittlefs@4.0.2-db0513a
jwplc:esp32-libs@3.3.8
jwplc:esp32@2.1.0-alpha.4
```

Resultado:

```txt
INSTALL_EXIT_CODE=0
ALPHA4_STANDALONE_INSTALL=PASS
```

La plataforma quedó instalada en el árbol aislado como:

```txt
jwplc:esp32 2.1.0-alpha.4
```

## 4. Compilación standalone

Se compilaron dos sketches de benchmark desde el package instalado:

```txt
tools/build-speed-benchmark/sketches/01_empty
tools/build-speed-benchmark/sketches/03_autoload_contract
```

Resultados:

| Prueba | Resultado | Tiempo |
|---|---|---:|
| `01_empty` cold | PASS | 44.102 s |
| `03_autoload_contract` cold | PASS | 42.460 s |

Resultado:

```txt
ALPHA4_STANDALONE_EMPTY_COMPILE=PASS
ALPHA4_STANDALONE_AUTOLOAD_COMPILE=PASS
ALPHA4_STANDALONE_COMPILE_GATE=PASS
```

Estos tiempos no sustituyen el benchmark formal P8 de Alpha4 porque la metodología y el estado del host no son idénticos. Su función es verificar que el package publicado conserva el comportamiento compilable y autocontenido.

## 5. Upload standalone

Se cargó `03_autoload_contract` al JWPLC Basic físico por `COM14` usando exclusivamente:

- el package `jwplc:esp32@2.1.0-alpha.4` instalado en el árbol aislado;
- `esptool_py 5.2.0` descargado por Boards Manager;
- binarios generados por la compilación standalone.

El upload escribió:

```txt
0x1000  bootloader
0x8000  partitions
0xE000  boot_app0
0x10000 aplicación
```

Todos los bloques fueron verificados por hash y el ESP32 realizó hard reset al terminar.

Resultado:

```txt
UPLOAD_EXIT_CODE=0
ALPHA4_STANDALONE_UPLOAD=PASS
```

## 6. Gate físico desde package publicado

Después se compiló y cargó:

```txt
tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate/
```

El log de compilación confirmó:

```txt
Used platform Version
jwplc:esp32 2.1.0-alpha.4
```

También confirmó el límite real de aplicación de Alpha4:

```txt
El máximo es 4063232 bytes.
```

Librerías reportadas durante el build:

| Librería | Versión | Precompilada durante el gate |
|---|---:|---|
| `JWPLC_Display` | 1.0.1 | Sí |
| `JWPLC_GlobalPeripherals` | 1.0.0 | No declarada precompiled |
| `Adafruit ST7735 and ST7789 Library` | 1.11.0 | Sí |
| `Adafruit GFX Library` | 1.12.4 | Sí |
| `Adafruit BusIO` | 1.17.4 | Sí |
| `Wire` | 3.3.8 | Sí |
| `SPI` | 3.3.8 | Sí |
| `JW_RTC` | 1.0.2 | Sí |
| `JW_FRAM` | 1.0.3 | Sí |
| `JW_SD` | 1.0.2 | Sí |
| `SD` | 3.3.8 | Sí |
| `FS` | 3.3.8 | Sí |
| `JW_MatrixButtons` | 1.0.5 | Sí |
| `JWPLC_Ethernet` | 1.0.0 | No declarada precompiled |
| `JWPLC Ethernet W5x00 Backend` | 2.0.2 | Sí |
| `JWPLC_RS485` | 1.0.1 | No declarada precompiled |
| `JWPLC_ModbusRTU` | 1.0.0 | Sí |

Resultado compile/upload:

```txt
PHYSICAL_GATE_COMPILE_UPLOAD=PASS
```

## 7. Resultado físico final

El monitor serie mostró:

```txt
ALPHA4_DISPLAY_READY=PASS
ALPHA4_RTC=PASS
ALPHA4_FRAM=PASS
ALPHA4_SD=PASS
ALPHA4_BUTTONS=PASS
ALPHA4_INPUTS=PASS
ALPHA4_OUTPUTS=PASS
ALPHA4_DISPLAY_VISUAL=PASS

ALPHA4_LOCAL_PHYSICAL_GATE=PASS
```

Se comprobaron físicamente:

- Display/TFT.
- RTC.
- FRAM de 8192 bytes.
- microSD con escritura, lectura y borrado.
- Botones UP, DOWN, LEFT, RIGHT, CANCEL y OK.
- Entradas `I0_0` a `I0_7`.
- Salidas/relés `Q0_0` a `Q0_7`.

Durante el arranque se observó:

```txt
mode:DIO, clock div:2
```

coherente con la configuración de flash validada para JWPLC Basic v2.0.

Ethernet y RS-485/Modbus ya contaban con gates físicos separados dentro del cierre técnico de Alpha4.

## 8. Conclusión

Resultado de publicación standalone:

```txt
ALPHA4_STANDALONE_PREINSTALL_ISOLATION=PASS
ALPHA4_STANDALONE_INSTALL=PASS
ALPHA4_STANDALONE_COMPILE_GATE=PASS
ALPHA4_STANDALONE_UPLOAD=PASS
ALPHA4_LOCAL_PHYSICAL_GATE=PASS
```

Conclusión:

**JWPLC ESP32 v2.1.0-alpha.4 puede instalarse directamente desde el índice dev en un entorno limpio, sin requerir una versión JWPLC previa; compila, se carga por USB y opera correctamente sobre un JWPLC Basic físico.**

Esta validación cierra el riesgo de que las optimizaciones de Alpha4 funcionen únicamente mediante el package local/junction de desarrollo.
