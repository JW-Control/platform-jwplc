# JWPLC v2.1.0-alpha.4 — conclusión sobre bootloader precompilado

## Objetivo

Cerrar el pendiente de `bootloader.bin` precompilado para `v2.1.0-alpha.4` sin confundir una optimización puntual de build con una decisión definitiva de configuración de producto.

## Evidencia histórica de alpha30

En alpha30 se realizó una comparación A/B de `JWPLC Basic` con y sin `bootloader.bin` precompilado usando la configuración entonces validada:

- CPU: 240 MHz.
- Flash size: 4 MB.
- Flash frequency: 40 MHz.
- Flash mode: DIO.
- Bootloader base: QIO.
- Partición: `huge_app`.

SHA-256 del `bootloader.bin` ensayado:

```txt
68263F0CD7FE3306DDA2EC64AAF61C8E8E27091F6CA0E0C3FE30D7A29FF80931
```

Resultados preservados:

| Caso | Tiempo |
|---|---:|
| Sin `bootloader.bin` — build limpio | 02:01.014 |
| Sin `bootloader.bin` — build incremental | 00:31.861 |
| Con `bootloader.bin` — build limpio | 01:58.141 |
| Con `bootloader.bin` — build incremental | 00:34.321 |

Lectura:

- En cold/build limpio el precompilado mejoró aproximadamente **2.873 s**.
- En incremental empeoró aproximadamente **2.460 s**.
- La mejora no fue significativa ni consistente.

La decisión de alpha30 fue no publicar `bootloader.bin` y mantener la generación automática desde el ELF del SDK correspondiente.

## Condición de seguridad

Un `bootloader.bin` precompilado sólo puede considerarse válido cuando coincide con la configuración que lo generó. Como mínimo deben quedar fijados y versionados:

- MCU/target.
- Flash frequency.
- Flash mode.
- `build.boot` / bootloader base.
- Flash size.
- versión del package/core/SDK.
- dirección de bootloader.

Un binario generado para una combinación no debe reutilizarse silenciosamente con otra.

## Estado actual de alpha4

La configuración actualmente usada por `JWPLC Basic` en `boards.txt` es:

```txt
build.f_cpu=240000000L
build.flash_size=4MB
build.flash_freq=40m
build.flash_mode=dio
build.boot=qio
build.partitions=huge_app
build.bootloader_addr=0x1000
upload.speed=921600
```

`JWPLC Basic Core` usa actualmente los mismos parámetros de CPU/flash/boot/particiones, aunque mantiene un conjunto de periféricos distinto.

Esta configuración es la configuración **actual de trabajo y validación de alpha4**. No se declara aquí como configuración definitiva universal del producto; en particular, la política final de Flash Frequency sigue siendo una decisión que debe quedar explícitamente cerrada o registrada como pendiente antes de publicar un bootloader definitivo.

## Relación con las optimizaciones de alpha4

Alpha4 redujo el cold candidato de JWPLC Basic hasta:

```txt
67.322 s
12 compiles
29 invocaciones g++ -E
```

La mejora principal proviene de discovery y precompilación controlada de core/librerías, no de sustituir la generación del bootloader por un archivo fijo.

Comparado con un cold actual del orden de 67 s, el beneficio histórico de aproximadamente 2.9 s del bootloader precompilado sería pequeño y, además, no fue consistente en incremental.

La rama de alpha4 no modifica `JWPLC/2.1.0/platform.txt`; mantiene la receta base de generación/subida del bootloader. Las modificaciones de build se concentran en `platform.local.txt`, discovery y archives precompilados.

## Decisión alpha4

`bootloader.bin` precompilado queda **CERRADO** con la siguiente decisión:

- **No publicar `bootloader.bin` como definitivo en v2.1.0-alpha.4.**
- Mantener la generación normal del bootloader según la configuración seleccionada por el package/core.
- No introducir una nueva optimización de `platform.txt` para forzar un bootloader fijo.
- No repetir ahora un cold específico de bootloader: existe un A/B físico previo y alpha4 no cambió la receta base de `platform.txt`.
- Si en una release futura se desea publicar un bootloader precompilado, primero deben quedar fijados formalmente FlashFreq, FlashMode, `build.boot`, FlashSize, target, SDK/core y dirección, y luego regenerar/validar el binario para esa configuración exacta.
- El SHA histórico de alpha30 se conserva únicamente como evidencia del ensayo; **no debe tratarse como binario definitivo de alpha4**.

## Motivo principal

La optimización no justifica acoplar el package a un binario de arranque mientras la configuración final de producto todavía debe quedar explícitamente cerrada. La ganancia histórica fue pequeña/inconsistente y el costo de una incompatibilidad de bootloader es considerablemente mayor que esos segundos de ahorro.

## Conclusión final

```txt
BOOTLOADER PRECOMPILADO: EVALUADO Y NO ADOPTADO EN v2.1.0-alpha.4.
NO PUBLICAR bootloader.bin COMO DEFINITIVO.
MANTENER GENERACIÓN NORMAL SEGÚN LA CONFIGURACIÓN DE BUILD.
REABRIR SÓLO DESPUÉS DE FIJAR FORMALMENTE LA CONFIGURACIÓN FINAL DE FLASH/BOOT/SDK.
```

El siguiente pendiente recomendado es registrar por separado el estado de **configuración final**: qué valores se consideran cerrados para alpha4 y cuáles continúan explícitamente pendientes de decisión de producto.
