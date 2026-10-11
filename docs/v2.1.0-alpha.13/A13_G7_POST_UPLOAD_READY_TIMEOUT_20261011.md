# G7 — F120: reloj TFT detenido y READY_TIMEOUT tras upload (2026-10-11)

## Evidencia inicial

```text
G7_CANDIDATE_REGRESSIONS=PASS
G7_PHYSICAL_BUILD=PASS
APP_SHA256=30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a
BANCO_FRAM=CONFIRMED
UPLOAD_G7_EXIT=0
PHASE=PHYSICAL_UPLOAD_PASS
FRAM_PHYSICAL_SERIAL_EXIT=3
G7_SERIAL_ERROR=READY_TIMEOUT
G7_FRAM_RESTORE_VERIFIED=FALSE_NOT_TESTED
G7_PRODUCT_ADOPTION=NOT_EXECUTED
```

Operador: durante el flash la hora visible de TFT se congeló en
16:46:54 y no retomó. No declarar hardware defectuoso ni concluir
automáticamente que es normal por un sketch sin UI: el core JWPLC
contiene `jwplcSystemTask` que refresca RTC y pantalla por separado
de `loop()`. Si el callback de Display/RTC está enlazado y las
tareas funcionan, se esperaría refresco autónomo.

El sketch G7 imprime `G7_FRAM_READY=<TOKEN>` cada ~700 ms en
`loop()` hasta recibir el comando `G7_FRAM_RUN:<TOKEN>`.
El host **no llegó a enviar el comando** porque expiró su espera
de `READY`. Por ello no hay evidencia de escrituras FRAM de
la prueba ni de respaldo pendiente NVS; la marca
`fram_restored=False` sólo indica que el ensayo nunca completó.

## Acción de diagnóstico segura

`tools/alpha13/gates/a13_g7_boot_readonly.py`:
- Lee sólo manifiesto y log del intento fallido;
- abre COM4 con DTR/RTS desactivados; captura UART 115200 durante 20 s;
- no transmite comandos al equipo ni cambia software;
- guarda un log local ignorado;
- nunca interpreta cero mensajes como PASS.

No reflashear, no borrar NVS, no repetir el gate, no usar
`G7_FRAM_RUN` hasta saber si arranca. Si hay pánico/WDT,
conservar backtrace. Si llegan `READY`, valorar el parser
serial y refresco TFT. Si no llegan bytes, próximo paso será
análisis de boot/reset controlado, sólo tras revisar esta evidencia.

```text
F120=G7_POST_UPLOAD_READY_TIMEOUT_TFT_FROZEN
F120_CLASS=UNKNOWN_PRODUCT_OR_ENVIRONMENT
F120_HARDWARE_CAUSE=NOT_ESTABLISHED
F120_PASSIVE_SERIAL=PREPARED_NOT_EXECUTED
G7_PHYSICAL=REVIEW_STOP
NEXT_FAILURE_ID=F121
```
