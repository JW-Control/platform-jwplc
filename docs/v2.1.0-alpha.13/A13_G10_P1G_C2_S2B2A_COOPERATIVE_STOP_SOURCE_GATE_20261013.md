# Alpha13 G10-P1G — cierre S2B1 Windows y gate S2B2A de cierre TCP

**Fecha:** 2026-10-13. **Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.

## 1. Cierre de S2B1 en Windows

El operador ejecutó con checkout limpio en
`HEAD=9d59ff4d1471cc563d59c078903367d60d59ae40`:

```text
G10_P1G_C2_S2B1_POSITIVE_NATIVE=PASS_REAL_CLIENT_SERVER_TOKEN_AND_CLOSE
G10_P1G_C2_S2B1_NEGATIVE_CTOR_NO_TOKEN=PASS_REJECTED
G10_P1G_C2_S2B1_NEGATIVE_CANCEL_NO_LEASE_CHECK=PASS_REJECTED
G10_P1G_C2_S2B1_NEGATIVE_SERVER_AVAILABLE_UNLOCKED=PASS_REJECTED
G10_P1G_C2_S2B1_NEGATIVE_BUS_BUSY_DROPS_HANDLE=PASS_REJECTED
G10_P1G_C2_S2B1_REAL_METHODS=4
G10_P1G_C2_S2B1_HEADER_CONSTRUCTOR=1
G10_P1G_C2_S2B1_OVERLAY_FILES=3
G10_P1G_C2_S2B1_NEGATIVES=4
G10_P1G_C2_S2B1_FROZEN_ARCHIVES=5
G10_P1G_C2_S2B1_PRODUCT_CHANGED=NO
G10_P1G_C2_S2B1_STATUS=CLIENT_SERVER_TOKEN_CANCEL_SOURCE_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
G10_P1G_C2_S2B1_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_c2_s2b1_20261013_120240_ccb0080a
```

```text
G10_P1G_C2_S2B1_LOCAL=CLOSED_PASS
G10_P1G_C2_S2B1_CI=PASS_WINDOWS_LINUX_RUN_38433524896
```

## 2. División de S2B2

Revisión directa de `EthernetClient.cpp` mostró:

- `beginStopAsync()` ejecuta `cancelFlushAsync()` y
  `cancelWriteAsync()` antes de verificar status de socket.
  En integración real pueden ser destructivas y deben estar
  después de adquirir guard y validar token.
- `pollStopAsync()` hace `socketClose()` al vencer
  timeout; requiere misma exclusión para validar token
  y emitir CLOSE y no destruir un socket reciclado.
- `stop()` hace polling con `delay(1)`; si el
  SPI permanece ocupado el mecanismo experimental debe
  garantizar espera acotada sin perder handle.
- `beginConnectAsync()/pollConnectAsync()` de clientes
  salientes tienen un lifecycle diferente. Se difieren
  explícitamente a **S2B2B**; no mezclar dos
  máquinas de estados en un mismo gate.

## 3. S2B2A — overlay source-backed

Nuevos archivos:

- `tools/alpha13/gates/a13_g10_p1g_c2_s2b2a_cooperative_stop.py`
- `tools/alpha13/gates/run_a13_g10_p1g_c2_s2b2a_cooperative_stop.bat`
- `tools/alpha13/candidates/g10_p1g_c/native_c2_s2b2a_cooperative_stop_test.cpp`
- `.github/workflows/alpha13-g10-p1g-c2-s2b2a-cooperative-stop.yml`

El gate toma el overlay S2B1, añade modificaciones
limitadas a los **cuerpos de los tres métodos reales**
`beginStopAsync()`, `pollStopAsync()`, `stop()`
y compila estos cuerpos extraídos en C++ host con
stubs de FreeRTOS, SPI y W5500.

El candidato implementa:

1. Adquisición W5500 y validación de propietario antes
   de cancelar operaciones pendientes o emitir
   `DISCONNECT/CLOSE`.
2. Si falla el mutex (`SPI_BUSY`), conserva el índice,
   token y estado software para reintento.
3. `pollStopAsync()` vuelve a iniciar el cierre si
   `beginStopAsync()` no pudo comenzar por contención.
4. Validación atómica de token bajo exclusión cuando
   se hace cierre forzado por timeout; lease invalidado
   después de `CLOSE`.
5. `stop()` deja de esperar tras `_timeout`.
   **Limitación visible:** al agotar timeout con mutex
   ocupado, puede retornar con socket todavía vivo
   y sin cierre automático garantizado. Se deberá
   definir retry de alto nivel antes de adopción.

Cuatro pruebas negativas que deben fallar si:
`NO_BEGIN_GUARD`, `NO_OWNER_VALIDATION`,
`BUSY_DROPS_HANDLE`, `UNBOUNDED_STOP_LIFECYCLE`.

La prueba nativa de tres métodos **no** verifica
el sketch, C++ completo del package, ni el
efecto sobre ModbusTCP, UDP, DHCP, SD, FRAM
o TFT. Su paso no corrige automáticamente A13-006.

## 4. CI y restricciones

CI:
https://github.com/JW-Control/platform-jwplc/actions/runs/38436363316

```text
G10_P1G_C2_S2B2A_CI=VALIDATION_PENDING_FINAL_RESULT
G10_P1G_C2_S2B2A_SOURCE_CPP_METHODS=3
G10_P1G_C2_S2B2A_NEGATIVES=4_REQUIRED
G10_P1G_C2_S2B2A_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2B2A_FROZEN_ARCHIVES=5_EXPECTED
G10_P1G_C2_S2B2A_FIRMWARE_PRODUCT=UNCHANGED
G10_P1G_C2_S2B2A_GPIO4_DEFAULT=255
G10_P1G_C2_S2B2A_ADOPTION=BLOCKED
G10_P1G_C2_S2B2B=OUTBOUND_CONNECT_PENDING
G10_FINAL=OPEN
```

La primera CI de S2B2A falló por una expectativa
incorrecta en un negativo: al eliminar guard de
`beginStopAsync()` la comprobación de lease rechaza
al propietario antes de acceder a hardware. Se ajustó
el mensaje de error esperado, no el producto ni el
candidato positivo.

Se prohíbe modificar `JWPLC/2.1.0`, habilitar
RST GPIO4 o publicar sin aceptación nativa + Arduino
y validación física separada.

## 5. Próxima ejecución local Windows

Después de confirmar CI final PASS, ejecutar:

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_c2_s2b2a_cooperative_stop.bat
```

El preflight exige manifest local S2B1
`g10_p1g_c2_s2b1_20261013_120240_ccb0080a`.

La salida esperada incluye:

```text
G10_P1G_C2_S2B2A_POSITIVE_NATIVE=PASS_REAL_COOPERATIVE_STOP_METHODS
G10_P1G_C2_S2B2A_REAL_CPP_METHODS=3
G10_P1G_C2_S2B2A_OVERLAY_FILES=3
G10_P1G_C2_S2B2A_NEGATIVES=4
G10_P1G_C2_S2B2A_FROZEN_ARCHIVES=5
G10_P1G_C2_S2B2A_PRODUCT_CHANGED=NO
G10_P1G_C2_S2B2A_STATUS=COOPERATIVE_STOP_REAL_METHODS_PASS_INCOMPLETE_ADOPTION_BLOCKED
```
