# Alpha30 - Build speed/cache

Rama de trabajo:

```txt
develop/alpha30-build-speed-cache
```

## Objetivo

Alpha30 evalúa mejoras de tiempo de compilación y subida para `JWPLC Basic v2.0.0`, manteniendo todos los periféricos integrados en el autoload normal:

- Display TFT ST7789.
- Ethernet W5500.
- microSD.
- FRAM.
- RTC.
- Botonera.
- RS-485.
- Modbus RTU.
- TCA/I/O.

No se eliminan periféricos del runtime normal solo por velocidad.

## Decisión principal

Para alpha30 se fija una única configuración de placa para `JWPLC Basic` y `JWPLC Basic Core`.

La mejora oficial de esta alpha no viene de publicar un `bootloader.bin` precompilado ni de precargar caché global, sino de:

- reducir combinaciones de menú;
- estabilizar el FQBN;
- evitar cambios accidentales de configuración;
- favorecer el uso correcto del caché incremental de Arduino CLI/IDE;
- documentar tiempos y límites reales.

## Configuración fija de placa

Configuración definida para `JWPLC Basic` y `JWPLC Basic Core`:

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

FQBN final usado para pruebas:

```txt
jwplc:esp32:jwplcbasic
```

La configuración `huge_app` deja 3 MB para aplicación:

```txt
El máximo es 3145728 bytes.
```

## Motivos de la configuración fija

### CPU fija en 240 MHz

Se mantiene `build.f_cpu=240000000L`.

Motivos:

- Se prioriza máximo rendimiento.
- El JWPLC Basic se alimenta desde fuente industrial de 24 VDC.
- El consumo medido durante pruebas fue bajo, alrededor de 1.3 W.
- No se busca una configuración de bajo consumo para esta línea de producto.

### Flash size fija en 4 MB

Se mantiene `build.flash_size=4MB`.

Motivos:

- El hardware actual usa ESP32 WROOM-32E de 4 MB.
- No conviene exponer 8 MB o 16 MB en una placa cuyo hardware real usa 4 MB.
- Las variantes de 8 MB/16 MB quedan como evaluación futura para OTA integrada.

### Flash frequency fija en 40 MHz

Se mantiene `build.flash_freq=40m`.

Motivos:

- Se prioriza estabilidad industrial.
- `build.flash_freq` controla la frecuencia de la flash externa del módulo ESP32, no la velocidad SPI de periféricos como TFT, W5500, SD o FRAM.
- No se asume 80 MHz como valor final.

### Flash mode y bootloader base

Se mantiene la combinación validada:

```txt
build.flash_mode=dio
build.boot=qio
```

Motivos:

- Es la combinación ya probada en el package.
- Evita introducir una variable nueva de riesgo en alpha30.
- Permite generar el bootloader desde `bootloader_qio_40m.elf` con imagen DIO.

### Partición fija `huge_app`

Se fija:

```txt
build.partitions=huge_app
upload.maximum_size=3145728
```

Motivos:

- OTA no forma parte de alpha30.
- Se prioriza espacio de aplicación para sketches industriales grandes.
- El hardware actual tiene microSD y FRAM, por lo que no se depende de SPIFFS/FATFS interno para datos de usuario.
- La partición `huge_app` conserva partición `coredump`, útil para diagnóstico futuro.

## Cambios esperados en `boards.txt`

Para `JWPLC Basic`:

```txt
jwplcbasic.upload.maximum_size=3145728
jwplcbasic.build.f_cpu=240000000L
jwplcbasic.build.flash_size=4MB
jwplcbasic.build.flash_freq=40m
jwplcbasic.build.flash_mode=dio
jwplcbasic.build.boot=qio
jwplcbasic.build.partitions=huge_app
jwplcbasic.build.loop_core=
jwplcbasic.build.event_core=
```

Para `JWPLC Basic Core`:

```txt
jwplcbasiccore.upload.maximum_size=3145728
jwplcbasiccore.build.f_cpu=240000000L
jwplcbasiccore.build.flash_size=4MB
jwplcbasiccore.build.flash_freq=40m
jwplcbasiccore.build.flash_mode=dio
jwplcbasiccore.build.boot=qio
jwplcbasiccore.build.partitions=huge_app
jwplcbasiccore.build.loop_core=
jwplcbasiccore.build.event_core=
```

Se eliminan para ambas placas:

```txt
menu.PartitionScheme
menu.CPUFreq
menu.FlashFreq
menu.LoopCore
menu.EventsCore
```

`platform.txt` no se modifica.

## Tiempos medidos

| Prueba | Tiempo |
|---|---:|
| Build limpio inicial antes de ajustes | 02:38.218 |
| Build incremental inicial antes de ajustes | 00:48.444 |
| Build incremental + upload full inicial | 00:44.787 |
| Compile + app-only total | 00:43.050 |
| App-only aislado | 00:06.352 |
| Build limpio con configuración fija, sin `bootloader.bin` | 02:01.014 |
| Build incremental con configuración fija, sin `bootloader.bin` | 00:31.861 |
| Build limpio con `bootloader.bin` precompilado | 01:58.141 |
| Build incremental con `bootloader.bin` precompilado | 00:34.321 |
| Primer build de segundo sketch usando mismo cache de core | 01:58.993 |
| Build incremental del segundo sketch | 00:31.296 |

## Evaluación de app-only

Se evaluó una receta app-only para subir únicamente la aplicación a `0x10000`.

Resultado medido:

| Prueba | Tiempo |
|---|---:|
| Compile + app-only total | 00:43.050 |
| App-only aislado | 00:06.352 |

Conclusión:

`app-only` reduce el tiempo de subida cuando el bootloader, particiones y `boot_app0.bin` ya están correctamente grabados. Sin embargo, no reduce el tiempo principal de compilación.

Decisión:

- Se mantiene como herramienta útil de desarrollo.
- No se considera solución principal para el tiempo de compilación.
- No se presenta como reemplazo del flujo normal de subida.

## Evaluación de `bootloader.bin` precompilado

Se probó incluir un `bootloader.bin` precompilado dentro de la variante `jwplcbasic`.

Configuración usada:

- CPU: 240 MHz.
- Flash size: 4 MB.
- Flash frequency: 40 MHz.
- Flash mode: DIO.
- Bootloader base: QIO.
- Partición: `huge_app`.

SHA-256 del `bootloader.bin` probado:

```txt
68263F0CD7FE3306DDA2EC64AAF61C8E8E27091F6CA0E0C3FE30D7A29FF80931
```

Resultados:

| Caso | Tiempo |
|---|---:|
| Sin `bootloader.bin` - build limpio | 02:01.014 |
| Sin `bootloader.bin` - build incremental | 00:31.861 |
| Con `bootloader.bin` - build limpio | 01:58.141 |
| Con `bootloader.bin` - build incremental | 00:34.321 |

Conclusión:

El `bootloader.bin` precompilado no aporta una mejora significativa ni consistente. El build limpio mejora aproximadamente 2.9 s, pero el build incremental empeora aproximadamente 2.5 s.

Decisión:

No se incluye `bootloader.bin` precompilado en alpha30. Se mantiene la generación automática desde:

```txt
bootloader_qio_40m.elf
```

## Evaluación de caché incremental

Se evaluó el comportamiento del caché de Arduino CLI con dos sketches distintos.

| Prueba | Tiempo |
|---|---:|
| Primer build de `DigitalIO_BlockMirror` | 02:01.014 |
| Build incremental de `DigitalIO_BlockMirror` | 00:31.861 |
| Primer build de `DigitalIO_BlockRead` usando el mismo cache de core | 01:58.993 |
| Build incremental de `DigitalIO_BlockRead` | 00:31.296 |

Conclusión:

El caché incremental reduce significativamente el tiempo cuando se recompila el mismo sketch. Sin embargo, al cambiar de sketch, el primer build vuelve a estar cerca de un build limpio, aun reutilizando el cache del core.

Por tanto, una precarga general del package no se considera una optimización suficiente para alpha30.

## Conclusión sobre precarga de caché

No se puede garantizar una primera compilación rápida dejando archivos precargados dentro del package.

Motivos:

- El caché depende del entorno local del usuario.
- Depende de rutas, versión del package, toolchain, FQBN y sistema operativo.
- Arduino CLI/IDE genera y administra el caché localmente.
- El cache del core no evita que un sketch nuevo vuelva a preparar librerías y enlazar artefactos.

Decisión:

No se implementa precarga global de caché en alpha30.

## Decisiones alpha30

- Se fija una única configuración de placa para `JWPLC Basic`.
- Se elimina el menú de particiones para `JWPLC Basic` y `JWPLC Basic Core`.
- Se elimina el menú de frecuencia CPU para `JWPLC Basic` y `JWPLC Basic Core`.
- Se elimina el menú de frecuencia Flash para `JWPLC Basic` y `JWPLC Basic Core`.
- Se elimina el menú de afinidad `LoopCore`/`EventsCore` para `JWPLC Basic` y `JWPLC Basic Core`.
- Se mantiene `platform.txt` sin cambios.
- No se publica `bootloader.bin` precompilado.
- No se implementa precarga global de caché.
- No se precompilan librerías en alpha30.
- OTA queda fuera de alpha30.
- No se elimina ningún periférico del autoload normal.

## Pendientes futuros

- Evaluar precompilación de librerías internas o runtime estable.
- Evaluar migrar parte del runtime JWPLC al core si conviene.
- Evaluar hardware de 8 MB/16 MB para OTA integrada.
- Evaluar partición recovery con UI de actualización.
- Evaluar coredump como herramienta formal de diagnóstico.
