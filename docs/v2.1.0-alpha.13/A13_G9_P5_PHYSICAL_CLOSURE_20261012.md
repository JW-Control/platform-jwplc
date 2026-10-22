# Alpha13 G9-P5 — Cierre físico COM4 SD, RTC y botonera

**Fecha de banco:** 2026-10-12 (hora reportada en carpeta local 11:27, Perú).  
**Estado:** `PHYSICAL_PASS_COM4_SD_READONLY_RTC_BUTTONS`.  
**Conclusión de G9:** `G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS`.  
**Alpha13:** `IN_PROGRESS`; G10 pendiente.

## Evidencia directa reportada por operador

El operador ejecutó el BAT versionado
`tools/alpha13/gates/run_a13_g9_p5_physical_com4.bat`
después de la sincronización y la adopción de producto
[`63e0291a7b1480af846e955559ab175aa78e7300`](https://github.com/JW-Control/platform-jwplc/commit/63e0291a7b1480af846e955559ab175aa78e7300).
La consola informó exactamente:

```text
G9_P5_PHASE=PREFLIGHT_PASS
G9_P5_CLI_VERSION_EXIT=0
G9_P5_CLI_CORES_EXIT=0
G9_P5_PORT=COM4_CH340_CONFIRMED
G9_P5_COMPILE_EXIT=0
G9_P5_SD_ARCHIVE_LINK=PASS_EXACT_PATH_FLAG_SHA
G9_P5_PHASE=BUILD_PASS
G9_P5_PHASE=AUTHORIZATION_PASS
G9_P5_UPLOAD_COM4_EXIT=0
G9_P5_PHASE=UPLOAD_PASS
G9_P5_SERIAL_DTR_RTS=FALSE_FALSE
G9_P5_READY=PASS_COM4
G9_P5_PHASE=READY_PASS
G9_P5_START=SENT_COM4_WITH_TOKEN
G9_P5_TOKEN_ACK=PASS
G9_P5_SD_READ_ONLY_TELEMETRY=G9_P5_SD:784e25f7d9cb4d87965e57e2 PRESENT=1 READY=1 ROOT=1
G9_P5_BUTTON_PRESS=1
G9_P5_BUTTON_RELEASE=1
G9_P5_REPORT={"BTN_ID": 1, "BTN_PRESS": 1, "BTN_RELEASE": 1, "RTC_LOST": 0, "RTC_PRESENT": 1, "RTC_VALID": 1, "SD_PRESENT": 1, "SD_READY": 1, "SD_ROOT": 1}
G9_P5_SD_ROOT_READ_ONLY=PASS
G9_P5_RTC_PRESENT_VALID=PASS
G9_P5_RTC_LOST_POWER_FLAG=0
G9_P5_BUTTON_PRESS_RELEASE=PASS_ID_1
G9_P5_HEARTBEAT=1
G9_P5_HEARTBEAT=2
G9_P5_HEARTBEAT=3
G9_P5_HEARTBEATS_POST_TEST=PASS_3
G9_P5_PHASE=PHYSICAL_PASS
G9_P5_PHASE=FINAL_AUDIT_PASS
G9_P5_STATUS=PHYSICAL_PASS_COM4_SD_READONLY_RTC_BUTTONS
G9_P5_COM14=NOT_TOUCHED
G9_P5_RESTORE_FIRMWARE=NOT_AUTOMATED
```

Resultados locales declarados (no montados ni subidos al repo):
`tools/alpha13/results/g9_p5_20261012_112751_200b0593/`.
No afirmar que se haya leído directamente el `MANIFEST.json`
ni el `SERIAL_COM4.log` de esa ubicación: el soporte recibido
es la salida completa de consola del operador.

## Matriz de aceptación

| Prueba | Exigido | Observado | Estado |
|---|---|---|---|
| Git/package preflight | Limpio + SHA/versión válidos | `PREFLIGHT_PASS` | PASS |
| Arduino CLI sketch | Build productivo | `COMPILE_EXIT=0` | PASS |
| SD precompiled archive | Ruta, `-lJW_SD`, SHA | `PASS_EXACT_PATH_FLAG_SHA` | PASS |
| Consentimiento | Confirmación interactiva | `AUTHORIZATION_PASS` | PASS |
| Upload | COM4 únicamente | `UPLOAD_COM4_EXIT=0` | PASS |
| Firmware nuevo | READY + token de sesión | Ambos PASS | PASS |
| microSD | Presente + montada | `PRESENT=1 READY=1` | PASS |
| SD raíz read-only | Abre `/` y es directorio | `ROOT=1` | PASS |
| RTC | presente + válido | `RTC_PRESENT=1 VALID=1` | PASS |
| RTC perdida de alimentación | observación no mutante | `LOST=0` | PASS |
| Botonera física | PRESS + RELEASE misma tecla | ID 1 (UP) | PASS |
| Actividad tras prueba | tres heartbeats | 3 | PASS |
| Git integridad | auditoría final | `FINAL_AUDIT_PASS` | PASS |
| COM14 | no intervención | `NOT_TOUCHED` | PASS |

La prueba utilizó el sketch
`tools/alpha13/sketches/g9_p5_sd_rtc_buttons/g9_p5_sd_rtc_buttons.ino`.
Es un smoke **sin operaciones de escritura** de microSD,
FRAM, RTC o relés: `JWPLC_SD.open("/", FILE_READ)`,
estado cacheado `JWPLC_Time` y eventos de la botonera
mantenida por runtime. La verificación del contrato
en Windows/Linux CI [38274903580](https://github.com/JW-Control/platform-jwplc/actions/runs/38274903580)
fue PASS, sin acceso físico en CI.

## Conciliación G9 completa

| Hallazgo | Evidencia de cierre | Decisión |
|---|---|---|
| A13-017 | Actualización documental README y comentarios de headers; líneas no comentario intactas | `CLOSED_PASS` |
| A13-012 | NUL fuente reemplazado en offset auditado, objeto↔archive `libJW_SD.a`, build 4/4 y lectura SD física | `CLOSED_PASS_WITH_SMOKE_SCOPE` |
| A13-014 | Guard redundante RTC retirado conservando `JWPLC_HAS_RTC`, null check y throttle 25 ms; builds y RTC/botón físicos | `CLOSED_PASS_WITH_SMOKE_SCOPE` |

El producto de A13-012/A13-014 está en el commit
`63e0291a7b1480af846e955559ab175aa78e7300`,
con **solo tres rutas de producto**. SHA-256 del archive
que el gate comprobó:
`58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e`.
El cierre Git se documentó previamente en
[A13_G9_GIT_PRODUCT_CLOSURE_20261012.md](A13_G9_GIT_PRODUCT_CLOSURE_20261012.md).
Se completaron las regresiones de compilación SD Basic,
Global SD, RTC y Buttons 4/4.

## Límites y pendientes independientes

- El test de microSD realizó **solo la lectura de metadatos
  del directorio raíz**, no una lectura de datos de archivo,
  DataLog en escritura, extracción/reinserción, hotplug o estrés.
  No afirmar que esos escenarios tuvieron PASS físico.
- RTC se observó presente/válido sin modificarlo; no se
  probaron cambios de fecha, retención de batería ni fallos I2C.
  Se recibió PRESS/RELEASE de UP, no de cada tecla de la matriz.
- Los tres heartbeats prueban actividad solo en ventana
  corta; **no resuelven F120**. Mantener
  `F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING`.
- COM4 **continúa con el sketch G9**; COM14, según última
  evidencia, conserva el diagnóstico Slave de G8.
  Ninguno fue restaurado automáticamente. Antes de
  retornar a operación, reprogramar firmware de aplicación.
- La validación G9 no equivale al cierre de toda Alpha13.
  G10 debe conciliar auditoría canónica, CI,
  procedencia de archives y métricas; no PR/merge/release.

```text
G9_P0=CLOSED_PASS_READ_ONLY
G9_P1_A13-017=CLOSED_PASS_DOC
G9_P2_A13-012=CLOSED_PASS_SOURCE_ARCHIVE_PHYSICAL_SMOKE
G9_P3_A13-014=CLOSED_PASS_GLOBAL_SOURCE_PHYSICAL_SMOKE
G9_P4_GIT=CLOSED_PASS_63e0291a7b1480af846e955559ab175aa78e7300
G9_P5_PHYSICAL=CLOSED_PASS_COM4_SD_RTC_BUTTON_UP
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
ALPHA13=IN_PROGRESS
NEXT_GATE=G10-P0_READONLY_AUDIT_AND_FREEZE_BASELINE
F120=UNKNOWN_PENDING
RELEASE_PR_MERGE_PUBLICATION=NO
```
