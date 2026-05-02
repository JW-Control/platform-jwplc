# Alpha30 - Optimización de compilación y subida

## Branch

```txt
develop/alpha30-build-speed-cache
```

## Objetivo

Reducir el tiempo de compilación/subida del JWPLC Basic sin sacrificar el objetivo principal del package:

> El JWPLC Basic debe mantener disponible el ecosistema completo: Display, Ethernet, microSD, FRAM, RTC, botonera, RS-485, Modbus RTU y TCA/I/O industrial.

No se busca crear una versión liviana que quite periféricos del flujo normal.

---

## Observación inicial

Durante alpha29 se observó:

- Primera compilación/subida: puede tardar varios minutos.
- Segunda compilación/subida del mismo sketch sin cambios: aproximadamente 55-60 segundos.
- Arduino IDE muestra uso de elementos previamente compilados.
- La subida serial final no parece ser el cuello principal.
- El tiempo se reparte entre:
  - prebuild hooks
  - detección de librerías
  - preprocesamiento
  - compilación incremental
  - link
  - generación de binarios
  - upload

---

## Configuración actual observada

FQBN usado:

```txt
jwplc:esp32:jwplcbasic
```

Board:

```txt
JWPLC Basic
```

Core:

```txt
jwcontrol
```

Configuración esperada del JWPLC Basic actual:

```txt
CPUFreq = 240 MHz
FlashSize = 4MB
FlashMode = dio
UploadSpeed = 921600
LoopCore = 1
EventsCore = 1
```

Pendiente validar si el default final debe quedar con:

```txt
FlashFreq = 80 MHz
```

o si se mantiene:

```txt
FlashFreq = 40 MHz
```

según estabilidad de hardware.

---

## Hipótesis de alpha30

### H1 - Build cache

Arduino IDE ya reutiliza parte de la compilación, pero su carpeta temporal puede cambiar según sketch/estado.

Arduino CLI con rutas fijas podría mejorar la repetibilidad:

```txt
--build-path
--build-cache-path
```

### H2 - Bootloader precompilado

El flujo actual busca:

```txt
variants/jwplcbasic/bootloader.bin
```

Si no existe, genera el bootloader desde el ELF del SDK.

Agregar un `bootloader.bin` estable por variante podría ahorrar algunos segundos y hacer más limpio el prebuild.

### H3 - Upload app-only

El flujo normal sube:

```txt
0x1000  bootloader.bin
0x8000  partitions.bin
0xe000  boot_app0.bin
0x10000 app.bin
```

Para desarrollo repetitivo, podría existir un modo que suba solo:

```txt
0x10000 app.bin
```

Esto no reduce compilación, pero puede reducir upload.

### H4 - Menos combinaciones de menú

Si el JWPLC Basic es hardware cerrado, se puede reducir el número de combinaciones:

- CPUFreq
- FlashFreq
- LoopCore
- EventsCore
- PartitionScheme

Esto mejora consistencia y facilita precompilación futura.

### H5 - Runtime/core precompilado

Se podría evaluar una librería/core precompilado si:

- reduce bastante el tiempo
- no aumenta demasiado el peso
- no complica mantenimiento
- no rompe compatibilidad con Arduino IDE

---

## Pruebas de alpha30

### Prueba 1 - Medición Arduino IDE

Medir manualmente:

| Caso | Tiempo |
|---|---:|
| Build limpio | pendiente |
| Segunda subida sin cambios | 55-60 s aprox |
| Cambio mínimo en sketch | pendiente |
| Cambio en librería interna | pendiente |
| Cambio en core | pendiente |

### Prueba 2 - Arduino CLI con cache fija

Medir:

```powershell
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic --build-path C:\JWPLC_Build\jwplcbasic --build-cache-path C:\JWPLC_Build\cache <SKETCH>
```

y comparar:

- primera compilación
- segunda compilación sin cambios
- compilación tras cambio mínimo
- compile + upload

### Prueba 3 - Bootloader precompilado

1. Generar o copiar `bootloader.bin` compatible.
2. Colocarlo en:

```txt
JWPLC/JWPLC-2.0.0/variants/jwplcbasic/bootloader.bin
```

3. Verificar que el prebuild ya no ejecute `esptool elf2image` para bootloader.
4. Medir ahorro real.

### Prueba 4 - App-only upload

Evaluar si el modo `esptool_py_app_only` ya disponible en `platform.txt` puede exponerse desde `boards.txt` como opción de menú.

Modo esperado:

```txt
Upload Mode:
- Full image
- App only
```

### Prueba 5 - Configuración final fija

Proponer configuración final para beta:

```txt
CPUFreq = 240 MHz
FlashFreq = 80 MHz o 40 MHz según estabilidad
FlashSize = 4MB
FlashMode = dio
LoopCore = 1
EventsCore = 1
UploadSpeed = 921600
```

Particiones candidatas:

```txt
Default
OTA
No OTA / app grande
```

---

## Criterios de cierre

Alpha30 se puede cerrar cuando exista:

- Tabla de tiempos antes/después.
- Decisión sobre bootloader precompilado.
- Decisión sobre app-only upload.
- Decisión sobre configuración final fija.
- Flujo recomendado de desarrollo rápido.
- Conclusión sobre si runtime/core precompilado conviene o no.
