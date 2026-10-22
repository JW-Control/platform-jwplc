# Alpha13 G9-P5 — Validación física COM4: microSD, RTC y botonera

**Fecha:** 2026-10-12. **Estado:** preparado; **no ejecutado sobre COM4**.

## Origen y configuración

- `JWPLC Basic` de **COM4**: el operador confirma tarjeta microSD insertada.
- `JWPLC Basic` de **COM14**: no se abre ni se programa.
- Producto G9 commiteado: `63e0291a7b1480af846e955559ab175aa78e7300`.
- Archive `JW_SD`: SHA-256
  `58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e`.
- Arduino CLI fijado 1.5.1, placa
  `jwplc_local:esp32:jwplcbasic`, periféricos del package
  cargados normalmente.
- Sketch:
  `tools/alpha13/sketches/g9_p5_sd_rtc_buttons/g9_p5_sd_rtc_buttons.ino`.
- BAT:
  `tools/alpha13/gates/run_a13_g9_p5_physical_com4.bat`.

## Seguridad y limites

**El BAT sobrescribe el firmware actual de COM4**, que según
la última prueba G8 era un Master de diagnóstico. No hay
restauración automática. El ejecutor exigirá que el usuario
escriba exactamente una frase de consentimiento,
después de comprobar build, hashes, puerto CH340 y
Git limpio, y ANTES de subir firmware.

Retirar actuadores/cargas y desconectar COM4 de maquinaria.
La tarjeta SD debe estar instalada y el monitor serie
cerrado. No requiere cable RS-485. No afecta COM14.

El sketch **NO escribe ni crea archivos**: realiza una
apertura `JWPLC_SD.open("/", FILE_READ)`, comprueba
`root.isDirectory()`, cierra el descriptor y devuelve
los estados `isCardPresent/isReady`. Un directorio
raíz vacío es aceptable. No se modifica reloj RTC,
no se escribe FRAM, no se tocan relés.

RTC utiliza `JWPLC_Time.present()/valid()/lostPower()`
del runtime. El último flag se reporta como diagnóstico;
si `RTC_PRESENT` o `RTC_VALID` son falsos, revisar
hardware/estado RTC antes de atribuirlo al cleanup.
El runtime continúa escaneando botonera. El sketch
consume `pressed/released` de una misma tecla,
sin llamar `JWPLC_Buttons.update()`.

## Ejecución

```powershell
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g9_p5_physical_com4.bat
```

Si preflight/build pasa, la consola solicita:
`G9 FLASH COM4 SD RTC BUTTONS`.
**Solo** al confirmar se hace upload en COM4. Al recibir
`READY`, el PC envía token de inicio por USB.
El operador debe **pulsar y soltar una tecla**
en los siguientes **70 segundos**. No pulsar antes
de ver la instrucción del BAT.

El resultado no puede declararse PASS hasta comprobar:
```text
G9_P5_SD_ROOT_READ_ONLY=PASS
G9_P5_RTC_PRESENT_VALID=PASS
G9_P5_BUTTON_PRESS_RELEASE=PASS_ID_...
G9_P5_HEARTBEATS_POST_TEST=PASS_3
G9_P5_PHASE=FINAL_AUDIT_PASS
G9_P5_STATUS=PHYSICAL_PASS_COM4_SD_READONLY_RTC_BUTTONS
G9_P5_COM14=NOT_TOUCHED
G9_P5_RESTORE_FIRMWARE=NOT_AUTOMATED
```

El resumen y las trazas se guardan bajo
`tools/alpha13/results/g9_p5_*/` en la máquina Windows.
La CI de contrato solo valida Python, parser,
negativos y APIs read-only; **no simula un PASS físico**.

3 heartbeats tras el evento son solamente una
observación breve de actividad, **NO resuelven**
`F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING`.

Si falta microSD, el volumen no se monta, RTC está
inválido o no se detectan PRESS/RELEASE, se emite
`REVIEW_STOP`; **no declarar G9 físico PASS**.
Si el upload ocurrió, COM4 queda cargado con el
sketch de diagnóstico aunque el gate falle.

```text
G9_P0=CLOSED_PASS
G9_P1_A13-017=CLOSED_PASS
G9_P2_A13-012=GIT_PRODUCT_PASS_PHYSICAL_PENDING
G9_P3_A13-014=GIT_PRODUCT_PASS_PHYSICAL_PENDING
G9_P5_HARDWARE=COM4_SD_INSTALLED
G9_P5_PHYSICAL=NOT_EXECUTED
G9_FUNCTIONAL=IN_PROGRESS
ALPHA13=IN_PROGRESS
RELEASE_MERGE_PR=NO
```
