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

Esta configuración queda cerrada para el hardware JWPLC Basic v2.0 validado en alpha4.

En particular:

- `FlashFreq=40 MHz` queda fijado para v2.0.
- No se abre en alpha4 una migración a 80 MHz.
- La evaluación de 80 MHz se difiere a la revisión v2.1, donde deberá existir una validación específica del nuevo perfil de flash/boot.
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

## Revisión B1-B3 de alpha4

Durante el cierre físico de alpha4 se reabrió la auditoría porque el árbol contenía:

```txt
JWPLC/2.1.0/variants/jwplcbasic/bootloader.bin
```

aunque la decisión documentada era mantener generación automática.

### B1 - equivalencia contra generación actual

Se ocultó temporalmente el `bootloader.bin` de variante y se ejecutó una compilación limpia con la configuración normal de `JWPLC Basic`.

Resultado generado:

```txt
Fuente: bootloader_qio_40m.elf
Tamaño: 25072 bytes
SHA-256: 68263F0CD7FE3306DDA2EC64AAF61C8E8E27091F6CA0E0C3FE30D7A29FF80931
Flash mode: DIO
Flash frequency: 40 MHz
Flash size: 4 MB
```

Este SHA coincide exactamente con el bootloader ensayado históricamente en alpha30.

El archivo que estaba versionado en la variante era distinto:

```txt
Tamaño: 25024 bytes
SHA-256: 1191EB3D913873C709C2FA100F829F4B824971000C0B8831B33F05F98BE3282A
```

### B2 - inspección del binario versionado

`esptool image-info` confirmó:

| Parámetro | Versionado | Generado por configuración actual |
|---|---:|---:|
| Target | ESP32 | ESP32 |
| Flash size | 4 MB | 4 MB |
| Flash mode | DIO | DIO |
| Flash frequency | **80 MHz** | **40 MHz** |
| ESP-IDF | v5.5.4 | v5.5.4 |

Por tanto, el archivo versionado no correspondía al `FlashFreq=40 MHz` fijado para JWPLC Basic v2.0.

El historial mostró que el archivo fue agregado durante alpha30 y posteriormente movido durante la reorganización de alpha32.

### B3 - validación física del bootloader de 40 MHz

Se generó y cargó físicamente el bootloader reproducible:

```txt
SHA-256:
68263F0CD7FE3306DDA2EC64AAF61C8E8E27091F6CA0E0C3FE30D7A29FF80931
```

manteniendo sin cambios:

```txt
Flash size = 4 MB
Flash mode = DIO
Flash frequency = 40 MHz
build.boot = QIO
Partition = huge_app
```

El gate físico local terminó:

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

Posteriormente se retiró el `bootloader.bin` versionado y se realizó una compilación limpia normal.

Resultado:

```txt
COMPILE_EXIT=0
GENERATED_SIZE=25072
GENERATED_SHA256=68263F0CD7FE3306DDA2EC64AAF61C8E8E27091F6CA0E0C3FE30D7A29FF80931
BOOTLOADER_REMOVAL_GATE=PASS
```

La receta volvió correctamente a la generación automática desde:

```txt
bootloader_qio_40m.elf
```

## Decisión alpha4

`bootloader.bin` precompilado queda **CERRADO** con la siguiente decisión:

- **Eliminar `JWPLC/2.1.0/variants/jwplcbasic/bootloader.bin` del package.**
- No publicar un `bootloader.bin` precompilado como artefacto definitivo de alpha4.
- Mantener la generación normal del bootloader desde el ELF del SDK correspondiente.
- Fijar para JWPLC Basic v2.0:
  - CPU 240 MHz.
  - Flash 4 MB.
  - FlashFreq 40 MHz.
  - FlashMode DIO.
  - `build.boot=qio`.
  - dirección de bootloader `0x1000`.
- Mantener por ahora `huge_app` como baseline hasta cerrar por separado la evaluación de partición.
- No introducir una optimización nueva de `platform.txt` para forzar un bootloader fijo.
- Diferir la evaluación de FlashFreq 80 MHz a la revisión v2.1.
- Si una release futura adopta un bootloader precompilado, deberá generarse y validarse para su combinación exacta de MCU, FlashFreq, FlashMode, `build.boot`, FlashSize, SDK/core y dirección.

## Motivo principal

Aunque la configuración de flash/boot queda ahora fijada para JWPLC Basic v2.0, mantener un `bootloader.bin` precompilado no aporta una mejora de build suficientemente consistente como para justificar un artefacto adicional que pueda quedar desalineado respecto de `boards.txt`.

La auditoría B1-B2 demostró precisamente ese riesgo: el archivo versionado utilizaba 80 MHz mientras la configuración efectiva del hardware v2.0 utiliza 40 MHz.

B3 confirmó físicamente que el bootloader generado automáticamente desde `bootloader_qio_40m.elf`, con DIO / 40 MHz / 4 MB, arranca correctamente y mantiene operativo el conjunto de periféricos validado.

Por ello, para alpha4 se prioriza una única fuente de verdad: la configuración de build. Un bootloader precompilado sólo deberá reconsiderarse en una revisión futura si existe una ventaja medible y se genera, versiona y valida específicamente para el perfil de hardware correspondiente.

## Conclusión final

```txt
BOOTLOADER PRECOMPILADO: EVALUADO Y NO ADOPTADO EN v2.1.0-alpha.4.

BOOTLOADER.BIN DE VARIANTE A 80 MHz:
RETIRADO POR NO CORRESPONDER A LA CONFIGURACIÓN FIJA DE v2.0.

JWPLC BASIC v2.0:
FLASHFREQ = 40 MHz.
FLASHMODE = DIO.
FLASH SIZE = 4 MB.
BUILD.BOOT = QIO.

BOOTLOADER:
GENERACIÓN NORMAL DESDE bootloader_qio_40m.elf.

SHA VALIDADO EN ALPHA4:
68263F0CD7FE3306DDA2EC64AAF61C8E8E27091F6CA0E0C3FE30D7A29FF80931

GATE FÍSICO B3 = PASS.
GATE DE COMPILACIÓN SIN BOOTLOADER VERSIONADO = PASS.

80 MHz:
DIFERIDO PARA EVALUACIÓN EN v2.1.
```

El siguiente pendiente de configuración es evaluar y cerrar la partición final de 4 MB para JWPLC Basic v2.0, manteniendo `huge_app` como baseline hasta que esa prueba sea completada.
