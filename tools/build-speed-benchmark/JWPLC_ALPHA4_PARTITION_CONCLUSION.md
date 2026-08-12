# JWPLC Alpha4 — conclusión de particionado de flash de 4 MB

## 1. Alcance

Esta conclusión corresponde al perfil **JWPLC Basic** de la rama
`v2.1.0-alpha.4/feature/build-speed-cache`.

El objetivo de esta fase fue definir un layout de flash de 4 MB que
maximice el espacio disponible para la aplicación sin alterar la
configuración de hardware ya validada ni eliminar el espacio reservado
para coredump.

La decisión se limita a **JWPLC Basic**.

`JWPLC Basic Core` permanece con su configuración anterior y requiere
una decisión independiente antes de adoptar este mismo esquema.

---

## 2. Decisión adoptada

Se adopta como partición por defecto de JWPLC Basic:

`jwplc_max_app_4mb`

Cambios asociados en `boards.txt`:

- `jwplcbasic.build.partitions=jwplc_max_app_4mb`
- `jwplcbasic.upload.maximum_size=4063232`

Se mantiene sin cambios:

- flash física: 4 MB;
- CPU: 240 MHz;
- FlashFreq: 40 MHz;
- FlashMode de imagen: DIO;
- boot profile: qio;
- `upload.maximum_data_size=327680`;
- coredump de 64 KiB.

No se añade un bootloader precompilado como parte de esta decisión.

---

## 3. Layout final

Archivo:

`JWPLC/2.1.0/tools/partitions/jwplc_max_app_4mb.csv`

| Partición | Tipo | Subtipo | Offset | Tamaño |
|---|---|---|---:|---:|
| nvs | data | nvs | `0x9000` | `0x5000` |
| otadata | data | ota | `0xE000` | `0x2000` |
| app0 | app | ota_0 | `0x10000` | `0x3E0000` |
| coredump | data | coredump | `0x3F0000` | `0x10000` |

La aplicación ocupa:

- inicio: `0x10000`;
- fin exclusivo: `0x3F0000`;
- tamaño: `0x3E0000`;
- 4,063,232 bytes;
- 3,968 KiB;
- 3.875 MiB.

El coredump conserva los últimos 64 KiB:

- inicio: `0x3F0000`;
- tamaño: `0x10000`;
- fin de flash: `0x400000`.

El layout termina exactamente en los 4,194,304 bytes de la flash.

---

## 4. Comparación frente a `huge_app`

El esquema anterior `huge_app` reservaba:

- APP: `0x300000` = 3,145,728 bytes;
- SPIFFS: `0xE0000`;
- coredump: `0x10000`.

El nuevo esquema elimina la partición SPIFFS y entrega ese espacio a la
aplicación, conservando NVS, `otadata` y coredump.

Ganancia de APP:

- 917,504 bytes;
- 896 KiB;
- +29.17 % respecto a los 3 MiB anteriores.

La eliminación de SPIFFS es deliberada para JWPLC Basic: la plataforma
dispone de FRAM y microSD para las necesidades de persistencia previstas
actualmente.

Esta decisión no elimina ni modifica las APIs de FRAM o microSD.

---

## 5. Consideración sobre OTA

Se conservan `otadata` y el subtipo `ota_0` para minimizar cambios
estructurales respecto al esquema existente.

Esto **no define una política OTA para JWPLC Basic** y no debe
interpretarse como una decisión de arquitectura OTA.

La definición de OTA continúa fuera del alcance de Alpha4.

---

## 6. Relación con el bootloader

El cambio de particiones no requiere un bootloader precompilado nuevo.

Durante toda la validación se mantuvo la configuración cerrada para
JWPLC Basic v2.0:

- ESP32;
- flash 4 MB;
- DIO;
- 40 MHz;
- bootloader generado desde `bootloader_qio_40m.elf`.

Bootloader validado:

- tamaño: 25,072 bytes;
- SHA-256:
  `68263F0CD7FE3306DDA2EC64AAF61C8E8E27091F6CA0E0C3FE30D7A29FF80931`.

La conclusión independiente sobre bootloader permanece documentada en
`JWPLC_ALPHA4_BOOTLOADER_CONCLUSION.md`.

---

## 7. Evidencia de validación

### PRT1 — layout

Se creó `jwplc_max_app_4mb.csv` y se validó con
`gen_esp32part.exe`.

Resultado:

`PRT1_PARTITION_LAYOUT_GATE=PASS`

El `partitions.bin` generado tuvo:

- tamaño: 3,072 bytes;
- SHA-256:
  `E46BCC25536E4494AA9BFA9D6C9A05EDD608EECDA14B95540D2D7EC292983E8D`.

### PRT2 — compilación con override temporal

Se compiló `03_autoload_contract` usando un `partitions.csv` temporal,
sin modificar todavía el default del board.

Resultado:

`PRT2_MAX_APP_COMPILE_GATE=PASS`

El binario de particiones coincidió exactamente con PRT1 y el
bootloader permaneció sin cambios.

### PRT3 — validación física antes de cambiar el default

Se construyó y cargó `06_alpha4_local_physical_gate` usando la
partición candidata mediante override temporal.

Resultados:

- `PRT3A_PHYSICAL_BUILD_GATE=PASS`
- `PRT3B_MAX_APP_UPLOAD_GATE=PASS`
- `PRT3C_MAX_APP_PHYSICAL_GATE=PASS`

El gate físico verificó:

- Display;
- RTC;
- FRAM;
- microSD;
- botonera física;
- 8 entradas digitales;
- 8 salidas / relés;
- funcionamiento visual del TFT.

### PRT4 — adopción como default del board

Se modificó únicamente el perfil `jwplcbasic`:

- `upload.maximum_size`: 3,145,728 → 4,063,232;
- `build.partitions`: `huge_app` → `jwplc_max_app_4mb`.

`JWPLC Basic Core` permaneció sin cambios.

Resultado:

`PRT4_BASIC_DEFAULT_MAX_APP_GATE=PASS`

La compilación normal del package confirmó:

- `ARDUINO_PARTITION_jwplc_max_app_4mb`;
- máximo de aplicación de 4,063,232 bytes;
- mismo `partitions.bin`;
- mismo bootloader de 40 MHz.

### PRT5 — validación desde el default real

Se compiló nuevamente `06_alpha4_local_physical_gate` sin
`partitions.csv` local.

El hook normal del package copió:

`tools/partitions/jwplc_max_app_4mb.csv`

al directorio de build y `gen_esp32part.exe` generó desde allí el
binario de particiones.

Después se cargaron exactamente esos artefactos en el JWPLC Basic.

Resultados:

- `PRT5A_DEFAULT_PHYSICAL_BUILD_GATE=PASS`
- `PRT5B_DEFAULT_MAX_APP_UPLOAD_GATE=PASS`
- `PRT5C_DEFAULT_MAX_APP_PHYSICAL_GATE=PASS`

El último gate físico confirmó nuevamente:

- `ALPHA4_DISPLAY_READY=PASS`
- `ALPHA4_RTC=PASS`
- `ALPHA4_FRAM=PASS`
- `ALPHA4_SD=PASS`
- `ALPHA4_BUTTONS=PASS`
- `ALPHA4_INPUTS=PASS`
- `ALPHA4_OUTPUTS=PASS`
- `ALPHA4_DISPLAY_VISUAL=PASS`
- `ALPHA4_LOCAL_PHYSICAL_GATE=PASS`

El arranque ROM mostró:

`mode:DIO, clock div:2`

---

## 8. Estado de JWPLC Basic Core

`JWPLC Basic Core` no forma parte de esta adopción.

Permanece temporalmente con:

- `jwplcbasiccore.upload.maximum_size=3145728`;
- `jwplcbasiccore.build.partitions=huge_app`.

No se extrapola automáticamente la validación física de JWPLC Basic a
Basic Core.

Si se desea migrar Basic Core al mismo layout, deberá hacerse como una
decisión separada y con sus propios gates.

---

## 9. Conclusión

La partición `jwplc_max_app_4mb` queda **aprobada para JWPLC Basic en
Alpha4** y se adopta como su configuración por defecto.

La configuración final validada para este punto es:

- flash: 4 MB;
- FlashFreq: 40 MHz;
- imagen: DIO;
- APP máxima: 4,063,232 bytes;
- coredump: 64 KiB;
- SPIFFS: no presente;
- bootloader: generado normalmente desde el SDK, no publicado como
  binario precompilado definitivo.

La ganancia frente a `huge_app` es de 896 KiB de espacio adicional para
la aplicación, equivalente a +29.17 %.

Con PRT1–PRT5 completados, la fase experimental de particionado de
JWPLC Basic queda cerrada para Alpha4.
