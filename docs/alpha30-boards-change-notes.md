# Alpha30 - Cambios requeridos en boards.txt

Archivo:

```txt
JWPLC/JWPLC-2.0.0/boards.txt
```

## JWPLC Basic

Bloque final esperado:

```txt
###############################
###    JWPLC Basic Board    ###
###############################

jwplcbasic.name=JWPLC Basic

jwplcbasic.bootloader.tool=esptool_py
jwplcbasic.bootloader.tool.default=esptool_py

jwplcbasic.upload.tool=esptool_py
jwplcbasic.upload.tool.default=esptool_py
jwplcbasic.upload.tool.network=esp_ota

jwplcbasic.upload.maximum_size=3145728
jwplcbasic.upload.maximum_data_size=327680
jwplcbasic.upload.speed=921600
jwplcbasic.upload.erase_cmd=
jwplcbasic.upload.flags=
jwplcbasic.upload.extra_flags=

jwplcbasic.serial.disableDTR=true
jwplcbasic.serial.disableRTS=true

jwplcbasic.build.tarch=xtensa
jwplcbasic.build.bootloader_addr=0x1000
jwplcbasic.build.target=esp32
jwplcbasic.build.mcu=esp32
jwplcbasic.build.core=jwcontrol
jwplcbasic.build.variant=jwplcbasic
jwplcbasic.build.board=JWPLCBASIC

jwplcbasic.build.f_cpu=240000000L
jwplcbasic.build.flash_size=4MB
jwplcbasic.build.flash_freq=40m
jwplcbasic.build.flash_mode=dio
jwplcbasic.build.boot=qio
jwplcbasic.build.partitions=huge_app
jwplcbasic.build.defines=-DJWPLC_BASIC -DHAVE_TCA6424A -DJWPLC_HAS_RTC=1 -DJWPLC_HAS_FRAM=1 -DJWPLC_HAS_SD=1 -DJWPLC_HAS_ETHERNET=1 -DJWPLC_FRAM_SIZE_BYTES=8192
jwplcbasic.build.loop_core=
jwplcbasic.build.event_core=
```

## JWPLC Basic Core

Bloque final esperado:

```txt
###############################
### JWPLC Basic Core Board  ###
###############################

jwplcbasiccore.name=JWPLC Basic Core

jwplcbasiccore.bootloader.tool=esptool_py
jwplcbasiccore.bootloader.tool.default=esptool_py

jwplcbasiccore.upload.tool=esptool_py
jwplcbasiccore.upload.tool.default=esptool_py
jwplcbasiccore.upload.tool.network=esp_ota

jwplcbasiccore.upload.maximum_size=3145728
jwplcbasiccore.upload.maximum_data_size=327680
jwplcbasiccore.upload.speed=921600
jwplcbasiccore.upload.erase_cmd=
jwplcbasiccore.upload.flags=
jwplcbasiccore.upload.extra_flags=

jwplcbasiccore.serial.disableDTR=true
jwplcbasiccore.serial.disableRTS=true

jwplcbasiccore.build.tarch=xtensa
jwplcbasiccore.build.bootloader_addr=0x1000
jwplcbasiccore.build.target=esp32
jwplcbasiccore.build.mcu=esp32
jwplcbasiccore.build.core=jwcontrol
jwplcbasiccore.build.variant=jwplcbasic
jwplcbasiccore.build.board=JWPLCBASICCORE

jwplcbasiccore.build.f_cpu=240000000L
jwplcbasiccore.build.flash_size=4MB
jwplcbasiccore.build.flash_freq=40m
jwplcbasiccore.build.flash_mode=dio
jwplcbasiccore.build.boot=qio
jwplcbasiccore.build.partitions=huge_app
jwplcbasiccore.build.defines=-DJWPLC_BASIC -DHAVE_TCA6424A -DJWPLC_HAS_RTC=1 -DJWPLC_HAS_FRAM=0 -DJWPLC_HAS_SD=0 -DJWPLC_HAS_ETHERNET=0 -DJWPLC_FRAM_SIZE_BYTES=0
jwplcbasiccore.build.loop_core=
jwplcbasiccore.build.event_core=
```

## Bloques a eliminar para ambas placas

Eliminar los bloques específicos de `jwplcbasic` y `jwplcbasiccore` correspondientes a:

```txt
menu.PartitionScheme
menu.CPUFreq
menu.FlashFreq
menu.LoopCore
menu.EventsCore
```

No eliminar los menús del ESP32 genérico si se mantiene como placa base de pruebas.
