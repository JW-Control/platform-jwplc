# A19_WEB_1 — Servidor web `JWPLC_Web` 0.1.0 (JWPLC Basic v2)

Rama: `v2.1.0-alpha.19/JWPLC-Sim/feature/servidor-web` (sale de `…/feature/wifi-y-red`, PR #255: usa
`JWPLC_RedServer`). Propuesta y estudio de otras marcas: documento «Servidor web del JWPLC Basic» (28/11).

```text
A19_WEB_1_COMPILA_SIM=PASS          [ejecutado] 4 ejemplos, jwplc-sim feature/alpha19 + este package (CI fijado en 1918401)
A19_WEB_1_JWSIM_WEB=PASS            [ejecutado] scripts/gate-web.mjs (jwplc-sim), 12 criterios
A19_WEB_1_PRUEBAS_EJEMPLOS=PASS     [ejecutado] tests/examples/JWPLC_Web (4/4) y JWPLC_Runtime (4/4)
A19_WEB_1_PARIDAD=PASS              [ejecutado] jwsim-cli parity (JWPLC_Web entra, JWPLC_WebScreen sale)
A19_WEB_1_COMPILA_ESP32=PASS        [ejecutado] arduino-cli jwplc_local:esp32:jwplcbasic, 4 ejemplos, sin warnings propios
A19_WEB_1_PLACA=PENDING             [no verificado] celular y PC contra un JWPLC Basic v2 real (cable y WiFi)
```

## Qué entra

- Librería nueva `JWPLC_Web` 0.1.0 (modo fuente): panel que se arma con `publish()`, alarmas con reconocimiento,
  tendencia con CSV, pestaña Pantalla (la TFT en vivo y la botonera, protocolo JWS2) y auditoría. Ver su README.
- Seguridad: primer ingreso con código de 6 dígitos que muestra el equipo (sin clave de fábrica); PBKDF2 + reto
  HMAC-SHA256 (la contraseña no viaja); 4 roles; cookie HttpOnly + SameSite=Strict; `X-JW` en todo POST; bloqueo
  de 5 min tras 5 fallos; CSP y cabeceras de seguridad.
- Se borra `JWPLC_WebScreen` (decisión del propietario: una sola librería que mantener). Sus dos ejemplos pasan a
  `JWPLC_Web/03.Pantalla_Web` y `04.Runtime_En_Vivo`. Último SHA con ella: `aa53fdb`.
- `JWPLC_Runtime`: fila «Panel web» (antes «Pantalla web») y *MENU › Red › Conectar celular* muestra el QR con el
  código de primer ingreso y los segundos que le quedan (ganchos débiles: sin `JWPLC_Web` avisa que falta).
- Comentarios de `JWPLC_TFT` y `JW_MatrixButtons` que nombraban a `JWPLC_WebScreen` (solo texto: los `.a` no
  cambian).

## Medidas (ESP32 real, ejemplo 02)

```text
Flash del sketch   557 985 B (13 % de 4 MB)      página embebida 11,5 KB gzip
RAM global         63 428 B                       JWPLC_Web: 8,4 KB estado + ~4 KB búferes HTTP
En marcha          + 54 KB copia de la pantalla + 7 KB pila de la tarea web
```

## Hallazgos corregidos en la prueba

- La cabecera CSP se cortaba (búfer de 200 B) y el navegador bloqueaba la imagen `data:`; búferes ampliados.
- `url()` desde el dibujo de la TFT devolvía `0.0.0.0`: `JWPLC_Ethernet.localIP()` pide el bus SPI, que el
  dibujo ya tiene. Ahora la tarea web guarda la IP y `url()` no toca el bus.
- `setAlarm()` ya no toma el candado si la alarma no cambia (el loop no espera a la tarea web).

## Pendiente

- Prueba en placa (PLACA). En jwplc-sim: PR con `gate-web.mjs`, las pruebas de `JWPLC_Web` y el borrado de
  `gate-webscreen.mjs`.
- v3 (ESP32-S3 N16R2): OTA firmado por la web, más tendencia, HTTPS con `JWPLC_TLS`.
