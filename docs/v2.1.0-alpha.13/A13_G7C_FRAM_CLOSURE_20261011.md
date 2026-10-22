# Alpha13-G7C — Cierre FRAM canónico (2026-10-11)

**Estado:** `G7C=CLOSED_PASS` técnico/productivo. `ALPHA13=IN_PROGRESS`.

## Alcance y corrección canónica

- **A13-009 = CLOSED_PASS:** `JW_FRAM::readString()` distingue error SPI de una longitud cero válida mediante `read(addr,&len,sizeof(len))`, comprueba `String::reserve()` cuando `len>0` y admite cadena vacía en la última dirección FRAM. No cambia formato `[longitud][payload]`, longitud máxima ni API.
- **A13-013 = CLOSED_PASS:** se retiraron únicamente los guards de constructor imposibles y las dos sobrecargas privadas sin uso de `debugPrint`; se conservan la rutina activa de debug, las firmas públicas y el SPI/autoload.
- **G7 original = CLOSED_PASS:** no se repitió su prueba de escritura/restauración de FRAM.

## Evidencia encadenada

| Etapa | Prueba y resultado |
|---|---|
| Fuentes candidatas | diff canónico exacto PASS |
| Build offline | `g7c_20261011_175217_3b6759cd`; objeto, archive, miembro extraído y enlace exacto PASS |
| Casos C++ nativos | 11/11 PASS; GitHub Actions [38183543776](https://github.com/JW-Control/platform-jwplc/actions/runs/38183543776) |
| Pre/Post adopción en CI | PASS [38185331838](https://github.com/JW-Control/platform-jwplc/actions/runs/38185331838) |
| Adopción productiva Windows | `g7c_adopt_20261011_182119_efba4050`; backup, integración de tres archivos, enlace normal, regresiones `GLOBAL_FRAM` y `CONFIG_BLOCK` PASS |
| Auditoría y cierre Git | `g7c_git_20261011_184501_0e9245c3`; preflight, staging exacto y push PASS |
| Commit productivo remoto | [`f2a93a8e5cbb736bbaa98ded9c7d2845582b7b32`](https://github.com/JW-Control/platform-jwplc/commit/f2a93a8e5cbb736bbaa98ded9c7d2845582b7b32) |
| CI tras commit productivo | PASS [38185454578](https://github.com/JW-Control/platform-jwplc/actions/runs/38185454578) |

Rutas productivas **exactas** del commit verificado:

```text
JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.cpp
JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.h
JWPLC/2.1.0/libraries/JW_FRAM/src/esp32/libJW_FRAM.a
```

```text
G7C_PRODUCT_COMMIT=f2a93a8e5cbb736bbaa98ded9c7d2845582b7b32
G7C_PRODUCT_PUSH=PASS_VERIFIED_REMOTE
G7C_EXPECTED_SOURCE_OBJECT_SHA256=2d635936aeb0096a7e3dd8051e2c4a23e56dc863922f0a114cc3b2b56323366c
G7C_ARCHIVE_SHA256=df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299
G7C_CI_POST_COMMIT=PASS
G7C_ADOPTION_PRODUCT_REGRESSIONS=PASS_2_OF_2
G7C_PHYSICAL_REUPLOAD=NONE
G7C_FRAM_WRITE=NONE
G7C_GIT_RELEASE_MERGE=NONE
G7C_PRE_RELEASE=NONE
```

La coincidencia de archive por SHA-256 proviene del manifiesto de adopción auditado en Windows y del cierre Git con comprobación de los bytes staged/committed; GitHub confirma la presencia del blob binario exacto en el mismo commit que las dos fuentes. El resultado de enlace se limita a `jwplc_local:esp32` con las dos regresiones reportadas por el operador, no al paquete aún publicado.

## Decisión sobre runtime físico adicional

**No repetir ahora** escrituras FRAM ni upload: el cambio G7C contiene comportamiento de fallos simulados en `readString()` más limpieza privada, con once fixtures nativos, dos regresiones Arduino y CI tras commit. G7 ya probó físicamente escritura, checksum, restauración y arranque tras power-cycle.

Esto **no** afirma que se haya probado físicamente la nueva ruta de fallo SPI de G7C. Se registrará la decisión de aceptación/riesgo en la matriz final de G10; una prueba física adicional solo se abrirá si aparece evidencia que la justifique.

## Límites y continuidad

- `F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING`: no atribuir origen, ni cerrarlo por el PASS de G7C.
- Mantener `OPENPLC=OUT_OF_SCOPE`, `HMI_DESIGNER=OUT_OF_SCOPE`, `TFT_NEW_FEATURES=OUT_OF_SCOPE`.
- No quitar periféricos del autoload ni modificar API pública.
- No equivaler integración productiva en la **rama Alpha13** a publicación del ZIP/Boards Manager.
- **NEXT_GATE=A13-G8_P0_MODBUS_RTU_RS485_BASELINE**. A13-010 y A13-011 siguen pendientes; no fueron parte del commit G7C.

## Clasificación

```text
G7C=CLOSED_PASS
A13-009=CLOSED_PASS
A13-013=CLOSED_PASS
ALPHA13=IN_PROGRESS
G8=NOT_STARTED
NEXT_GATE=A13-G8_P0_MODBUS_RTU_RS485_BASELINE
```
