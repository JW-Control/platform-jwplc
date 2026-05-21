# Alpha31 - Revisión de release readiness

Branch:

```txt
develop/alpha31-release-readiness
```

Objetivo de alpha31:

```txt
Validar package completo instalado desde cero y preparar beta1.
```

Esta revisión documenta el estado de los archivos principales del package JWPLC Basic v2.0.0 y las mejoras documentales recomendadas antes de pasar a beta1.

---

## 1. Archivos técnicos principales

### `JWPLC/JWPLC-2.0.0/boards.txt`

Estado: **correcto / sin cambio requerido**.

Se valida que `JWPLC Basic` mantiene configuración fija definida en alpha30:

```txt
build.core=jwcontrol
build.variant=jwplcbasic
build.mcu=esp32
upload.speed=921600
build.f_cpu=240000000L
build.flash_size=4MB
build.flash_freq=40m
build.flash_mode=dio
build.boot=qio
build.partitions=huge_app
upload.maximum_size=3145728
build.loop_core=
build.event_core=
```

También se valida que `JWPLC Basic Core` mantiene la misma base de compilación, pero con periféricos opcionales deshabilitados por flags:

```txt
JWPLC_HAS_FRAM=0
JWPLC_HAS_SD=0
JWPLC_HAS_ETHERNET=0
JWPLC_FRAM_SIZE_BYTES=0
```

Conclusión:

- No reabrir menús de configuración para `JWPLC Basic` / `JWPLC Basic Core`.
- No cambiar CPU, flash, partición ni upload speed en alpha31.
- Mantener FQBN recomendado:

```txt
jwplc:esp32:jwplcbasic
```

---

### `JWPLC/JWPLC-2.0.0/platform.txt`

Estado: **correcto / sin cambio requerido**.

Se mantiene la decisión de alpha30:

- No tocar `platform.txt`.
- No publicar `bootloader.bin` precompilado como definitivo.
- Mantener generación automática del bootloader desde el ELF del SDK cuando no exista bootloader custom.

Conclusión:

- No modificar en alpha31 salvo que aparezca un bloqueante real de instalación/compilación.

---

### `JWPLC/package_jwplc_index.json`

Estado: **correcto como índice publicado actual**.

El índice todavía apunta a:

```txt
2.0.0-alpha.30
```

Esto es esperado mientras alpha31 no haya sido publicado.

Conclusión:

- No actualizar a alpha31 hasta generar ZIP, SHA-256 y tamaño reales.
- Para beta1, actualizar recién cuando exista el paquete final validado.

---

### `JWPLC/JWPLC-2.0.0/variants/jwplcbasic/pins_is.h`

Estado: **correcto / sin cambio requerido**.

Se valida mapa público:

```txt
I0_0 ... I0_7
Q0_0 ... Q0_7
I0_X
Q0_X
I0_COUNT
Q0_COUNT
EN_IO
```

El mapa TCA6424A mantiene:

```txt
I0_0 = 0x2207 ... I0_7 = 0x2200
Q0_0 = 0x2208 ... Q0_7 = 0x220F
EN_IO = 27
```

Conclusión:

- No cambiar nombres públicos.
- No cambiar orden lógico sin prueba física y documentación.

---

### `JWPLC/JWPLC-2.0.0/cores/jwcontrol/Arduino.h`

Estado: **correcto / sin cambio requerido**.

Se valida que con `JWPLC_BASIC` activo se redirigen:

```cpp
pinMode()
digitalWrite()
digitalRead()
digitalReadBlock()
digitalWriteBlock()
```

hacia los wrappers JWPLC.

Conclusión:

- Mantener compatibilidad Arduino.
- No tocar salvo error de compilación confirmado.

---

### `JWPLC/JWPLC-2.0.0/cores/jwcontrol/jwplc_peripherals.h`

Estado: **correcto / sin cambio requerido**.

Se valida exposición de:

```cpp
JWPLC_readInputs();
JWPLC_readOutputs();
JWPLC_writeOutputs(bitmap);
jwplc_digitalReadBlock(...);
jwplc_digitalWriteBlock(...);
jwplcSystemScanIO();
jwplcSystemTickRTC();
jwplcSystemDisplayHook();
```

Conclusión:

- API pública coherente con alpha29/alpha30.
- No cambiar antes de beta1 salvo bloqueante.

---

### `JWPLC/JWPLC-2.0.0/cores/jwcontrol/jwplc_peripherals.cpp`

Estado: **correcto / sin cambio requerido**.

Se valida:

- Cache de entradas.
- Shadow de salidas.
- Lectura lógica `I0_0...I0_7` con inversión de bits.
- Escritura por banco para `Q0_X`.
- Hooks débiles para display, RTC, FRAM, SD, botonera y Ethernet.

Conclusión:

- Coherente con integración TCA6424A.
- No tocar en alpha31 salvo fallo real.

---

### `JWPLC/JWPLC-2.0.0/cores/jwcontrol/peripherals_init.cpp`

Estado: **correcto / sin cambio requerido**.

Se valida arranque seguro:

1. `EN_IO` como salida.
2. `EN_IO = LOW` antes de inicializar.
3. Inicialización de estado JWPLC.
4. Inicio I2C.
5. RTC/FRAM/SD/botonera como no críticos.
6. Inicialización TCA6424A.
7. Salidas en `0x00`.
8. Dirección de bancos TCA.
9. `EN_IO = HIGH` al final.

Conclusión:

- Buen comportamiento industrial.
- Si falla I2C/TCA no se habilita `EN_IO`, lo cual es seguro.

---

## 2. README principal

Estado: **correcto, con mejoras menores recomendadas**.

El README principal ya contiene:

- Instalación desde Boards Manager.
- URL del package index.
- Placas visibles.
- Configuración fija alpha30.
- FQBN recomendado.
- I/O por TCA6424A.
- Objetos globales del ecosistema JWPLC.
- Links a READMEs de librerías.
- Decisiones sobre app-only, bootloader precompilado, OTA y coredump.
- Checklist rápido de validación.

Mejoras recomendadas antes de beta1:

1. Cambiar referencias de estado `alpha30` a una redacción neutral:

```txt
Desde alpha30...
Revisado en alpha31...
```

2. Agregar una sección corta:

```md
## Estado de alpha31

Alpha31 no agrega features grandes. Su objetivo es validar instalación limpia, compilación IDE/CLI, ejemplos principales y periféricos antes de pasar a beta1.
```

3. Agregar advertencia visible:

```md
OpenPLC no está integrado todavía. El mapa Modbus/OpenPLC es preliminar.
```

---

## 3. READMEs de librerías internas

### `JWPLC_Display/README.md`

Estado: **bueno / mejora menor**.

Mejora recomendada:

- Actualizar el bloque final de estado:

```txt
Documentación revisada para JWPLC ESP32 2.0.0-alpha.31.
La API corresponde a JWPLC_Display 1.0.0.
```

No requiere cambios funcionales.

---

### `JWPLC_Ethernet/README.md`

Estado: **bueno / mejora menor**.

Mejora recomendada:

- Actualizar el bloque final de estado:

```txt
Documentación revisada para JWPLC ESP32 2.0.0-alpha.31.
En alpha31 se valida como parte de la instalación limpia del package completo.
No se agregan cambios funcionales a Ethernet.
```

No requiere cambios funcionales.

---

### `JWPLC_RS485/README.md`

Estado: **bueno / mejora menor**.

Mejora recomendada:

Agregar sección:

```md
## Validación alpha31

Para alpha31 se recomienda validar:

- `JWPLC_RS485.begin()`.
- `RS485_Status`.
- `RS485_Basic_Send`.
- `RS485_Basic_Echo`.
- `RS485_USB_Bridge`.
- RX2 = IO16.
- TX2 = IO17.
- MAX13487 sin DE/RE manual.
```

---

### `JWPLC_ModbusRTU/README.md`

Estado: **bueno / mejora menor**.

Mejora recomendada:

Agregar nota visible cerca del inicio:

```md
> OpenPLC no está integrado todavía. Esta librería provee la base Modbus RTU propia del package JWPLC y puede servir para futuras pruebas de integración OpenPLC/Modbus.
```

---

## 4. READMEs de librerías externas

### `JW_FRAM/README.md`

Estado: **bueno / corrección recomendada**.

Problema detectado:

- Existe una cita residual no válida para README público:

```txt
citeturn948721view0
```

Cambio recomendado:

```md
## Nota de licencia

La base original de Adafruit FRAM SPI usa licencia BSD. Si esta librería deriva de ese trabajo, se debe conservar la atribución correspondiente y mantener el texto de licencia aplicable dentro del repositorio.
```

---

### `JW_SD/README.md`

Estado: **funcional pero mejorable**.

Mejoras recomendadas:

- Corregir tildes.
- Ampliar objetivo.
- Agregar notas FAT32/MBR.
- Agregar ejemplos de log, lectura y listado.
- Explicar `open()` vs `openNative()`.
- Agregar problemas comunes.

Este README es el principal candidato a ampliación antes de beta1.

---

### `JW_RTC/README.md`

Estado: **técnicamente completo, pero en inglés**.

Mejora recomendada:

- Traducir a español o agregar `README_ES.md`.
- Explicar claramente que la versión actual usa `jwplc_i2c_bridge` y está orientada al package JWPLC.
- Mantener nota de backend futuro con `Wire`.

---

### `JW_MatrixButtons/README.md`

Estado: **bueno / sin cambio urgente**.

Mejora opcional:

- Agregar nota de integración con `JWPLC_Buttons` dentro del package JWPLC Basic.

---

### `JW_DWIN_RS485/README.md`

Estado: **correcto / mejora menor**.

Mejora recomendada:

Agregar nota para evitar confusión:

```md
> Esta librería no forma parte del runtime base JWPLC Basic v2.0.0. No reemplaza a `JWPLC_RS485` ni a `JWPLC_ModbusRTU`; es una librería adicional para pantallas DWIN por RS-485/Modbus RTU.
```

---

## 5. Conclusión alpha31 preliminar

No se detectan bloqueantes técnicos en:

- `boards.txt`.
- `platform.txt`.
- `package_jwplc_index.json` publicado actual.
- `pins_is.h`.
- `Arduino.h`.
- `jwplc_peripherals.h`.
- `jwplc_peripherals.cpp`.
- `peripherals_init.cpp`.

Bloqueantes actuales:

```txt
Ninguno confirmado por revisión estática.
```

Pendientes antes de beta1:

- Validar instalación limpia desde Boards Manager.
- Validar Arduino IDE.
- Validar Arduino CLI.
- Compilar placas principales.
- Compilar ejemplos principales.
- Validar subida USB.
- Ejecutar periféricos principales.
- Registrar tabla de validación.
- Decidir si se pasa a beta1.

---

## 6. Recomendación de siguiente paso

Siguiente acción recomendada:

```txt
Ejecutar validación real de instalación limpia + compilación IDE/CLI.
```

No abrir features grandes en alpha31.

No tocar arquitectura salvo bloqueante real.
