# Alpha5 - Conclusión de upload, bootloader y configuración

Fecha: 2026-08-24

Rama:

`v2.1.0-alpha.5/feature/esp32-precompiled-compatibility`

Commit de referencia:

`5549d131aa4ea12604280bfee615c168df164d7a`

## 1. Objetivo

Cerrar para Alpha5 las decisiones sobre:

- upload app-only;
- bootloader precompilado;
- particionado;
- configuración actual de flash/upload.

La decisión se basa en verificar si Alpha5 modificó de forma material
las recetas que sustentaron las validaciones físicas realizadas en Alpha4.

---

## 2. Comparación Alpha4 vs Alpha5

Referencia Alpha4:

`v2.1.0-alpha.4/feature/build-speed-cache`

Referencia Alpha5:

`v2.1.0-alpha.5/feature/esp32-precompiled-compatibility`

### platform.txt

El archivo es idéntico entre ambas ramas.

SHA:

`0a9c8bb9505954231c335ab774a7917bc00cae96`

Por tanto, Alpha5 no modifica:

- generación del bootloader;
- generación de partitions.bin;
- generación de application.bin;
- receta base de upload;
- offsets de upload;
- flujo de esptool.

Resultado:

`ALPHA5_PLATFORM_UPLOAD_RECIPE_UNCHANGED=PASS`

### boards.txt

El único cambio respecto al perfil Alpha4 analizado corresponde a la
normalización del nombre del core precompilado:

Alpha4:

`jwplcbasic.build.core=jwcontrol_p2`

Alpha5:

`jwplcbasic.build.core=jwcontrol_precompiled_stub`

Este cambio pertenece exclusivamente a la arquitectura del core
precompilado y no modifica la configuración de flash, particiones,
bootloader ni uploader.

Resultado:

`ALPHA5_BOARD_UPLOAD_CONFIGURATION_UNCHANGED=PASS`

### platform.local.txt

Los cambios observados corresponden únicamente a la actualización del
nombre `jwcontrol_p2` por `jwcontrol_precompiled_stub` dentro de la
documentación/comentarios del mecanismo P2.

No se modifica la receta de upload.

Resultado:

`ALPHA5_PLATFORM_LOCAL_UPLOAD_RECIPE_UNCHANGED=PASS`

---

## 3. App-only

Alpha4 validó físicamente el mecanismo app-only.

La conclusión fue:

- funciona;
- puede ser útil como herramienta de desarrollo;
- el ahorro es secundario frente al costo total de compilación;
- no debe convertirse en el upload normal por defecto;
- full upload permanece como camino seguro;
- app-only presupone bootloader, particiones y boot_app0 ya compatibles.

Alpha5 no modifica `platform.txt` ni la receta que sustenta este mecanismo.

Por tanto, no existe una razón técnica nueva que obligue a repetir el
mismo ensayo físico sólo para demostrar nuevamente el mecanismo.

Decisión Alpha5:

`APP_ONLY=VALIDATED_DEVELOPMENT_TOOL`

`APP_ONLY_DEFAULT_UPLOAD=NO`

`FULL_UPLOAD_DEFAULT=YES`

`APP_ONLY_RETEST_ALPHA5=NOT_REQUIRED`

---

## 4. Bootloader precompilado

Alpha4 comparó generación normal contra un bootloader.bin precompilado.

La mejora de tiempo fue pequeña e inconsistente:

- cold: aproximadamente 2.9 s de mejora;
- incremental: aproximadamente 2.5 s de empeoramiento.

Además, durante Alpha4 se detectó un bootloader.bin de variante generado
para 80 MHz que no correspondía al perfil efectivo de 40 MHz.

El archivo fue retirado y se validó físicamente la generación normal desde
el ELF del SDK.

Alpha5 no modifica `platform.txt` ni el mecanismo de generación del
bootloader.

Decisión Alpha5:

`BOOTLOADER_PRECOMPILED=NOT_ADOPTED`

`BOOTLOADER_GENERATION=SDK_ELF_AUTOMATIC`

`BOOTLOADER_BIN_FINAL_PUBLICATION=NO`

No se publica un bootloader.bin como artefacto definitivo únicamente para
obtener una reducción menor del tiempo de build.

---

## 5. Partición de JWPLC Basic

Alpha4 cerró y adoptó:

`jwplc_max_app_4mb`

El layout validado es:

| Partición | Offset | Tamaño |
|---|---:|---:|
| NVS | 0x9000 | 0x5000 |
| otadata | 0xE000 | 0x2000 |
| app0 | 0x10000 | 0x3E0000 |
| coredump | 0x3F0000 | 0x10000 |

APP máxima:

`4063232 bytes`

Alpha5 conserva esta configuración.

Decisión:

`JWPLC_BASIC_PARTITION=jwplc_max_app_4mb`

`ALPHA5_PARTITION_INHERITED_FROM_ALPHA4=PASS`

La presencia de `otadata` y subtipo `ota_0` no define una política OTA.

---

## 6. Perfil actualmente usado por JWPLC Basic

El perfil actual en boards.txt es:

| Parámetro | Valor actual |
|---|---|
| MCU | ESP32 |
| CPU | 240 MHz |
| Flash size | 4 MB |
| Flash frequency | 40 MHz |
| Flash image mode | DIO |
| build.boot | QIO |
| Bootloader address | 0x1000 |
| Partition | jwplc_max_app_4mb |
| Upload speed | 921600 |

Estos valores describen el perfil actualmente usado y validado.

No se debe interpretar esta tabla como una decisión universal para futuras
revisiones de hardware ni como autorización para publicar un bootloader.bin
precompilado definitivo.

En particular, Alpha5 no abre una nueva evaluación de FlashFreq.

Estado de cierre:

`CURRENT_FLASH_PROFILE=VALIDATED_CURRENT_PROFILE`

`FINAL_UNIVERSAL_FLASH_CONFIGURATION=PENDING`

`FLASHFREQ_REEVALUATION_ALPHA5=OUT_OF_SCOPE`

---

## 7. Conclusión

Alpha5 no introdujo cambios materiales en las recetas de upload,
bootloader o particionado respecto al estado final validado de Alpha4.

Por tanto:

- app-only mantiene su validación como herramienta auxiliar;
- full upload permanece como flujo por defecto;
- bootloader precompilado sigue no adoptado;
- el bootloader continúa generándose automáticamente desde el SDK;
- `jwplc_max_app_4mb` permanece como partición de JWPLC Basic;
- no se publica un bootloader.bin definitivo;
- el perfil de flash actual permanece documentado sin convertirlo en una
  decisión universal futura.

Resultado:

`ALPHA5_UPLOAD_STRATEGY=PASS`

`ALPHA5_BOOTLOADER_STRATEGY=PASS`

`ALPHA5_PARTITION_STRATEGY=PASS`

`ALPHA5_CONFIGURATION_STATUS=EXPLICIT`
