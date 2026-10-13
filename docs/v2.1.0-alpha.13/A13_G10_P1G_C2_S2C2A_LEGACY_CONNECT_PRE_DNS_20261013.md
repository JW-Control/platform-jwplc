# Alpha13 — G10-P1G-C2-S2C2A: connect(IPAddress), connect(host) y preparación DNS

**Fecha:** 2026-10-13. **Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.

## 1. Evidencia previa: S2C1 Windows CLOSED_PASS

El operador ejecutó S2C1 con checkout limpio, `HEAD=07e72ce9498208ed573dc80741b8fe6c5ef1ef57`:

```text
G10_P1G_C2_S2C1_POSITIVE_NATIVE=PASS_REAL_TX_CANCEL_WITH_OUTBOUND
G10_P1G_C2_S2C1_NEGATIVE_NO_TX_GUARD=PASS_REJECTED
G10_P1G_C2_S2C1_NEGATIVE_NO_TX_OWNER=PASS_REJECTED
G10_P1G_C2_S2C1_NEGATIVE_BUSY_DISCARDS_TX=PASS_REJECTED
G10_P1G_C2_S2C1_NEGATIVE_NO_TX_LEASE_INVALIDATION=PASS_REJECTED
G10_P1G_C2_S2C1_NEGATIVE_SKIP_TX_HARDWARE_CLOSE=PASS_REJECTED
G10_P1G_C2_S2C1_HEAD=07e72ce9498208ed573dc80741b8fe6c5ef1ef57
G10_P1G_C2_S2C1_REAL_CPP_METHODS=5
G10_P1G_C2_S2C1_OVERLAY_FILES=3
G10_P1G_C2_S2C1_NEGATIVES=5
G10_P1G_C2_S2C1_FROZEN_ARCHIVES=5
G10_P1G_C2_S2C1_PRODUCT_CHANGED=NO
G10_P1G_C2_S2C1_STATUS=TX_CANCEL_REAL_CPP_NATIVE_PASS_INCOMPLETE_ADOPTION_BLOCKED
G10_P1G_C2_S2C1_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_c2_s2c1_20261013_133216_bf7c8acb
```

```text
G10_P1G_C2_S2C1_LOCAL=CLOSED_PASS
G10_P1G_C2_S2C1_CI=PASS_WINDOWS_LINUX_RUN_38442717818
```

## 2. Hallazgos en fuentes reales

`JWPLC_Ethernet/src/EthernetClient.cpp` conserva dos métodos Arduino bloqueantes:

- `connect(IPAddress,uint16_t)` reutiliza el motor asíncrono; espera mediante `pollConnectAsync()` y `delay(1)` hasta timeout y llama a `cancelConnectAsync()`.
- `connect(const char*,uint16_t)` **cierra o desconecta una sesión anterior por índice sin mutex ni identidad**, luego llama `DNSClient::getHostByName()` y después a `connect(IPAddress)`.
- El método `DNSClient::getHostByName()` usa realmente `EthernetUDP`; los métodos raw de UDP no están totalmente protegidos por el mutex común.
- Mantener `BusScope` sobre toda la resolución DNS **no se adopta**: una consulta puede tardar mucho y bloquear el SPI compartido por Display, SD y FRAM.
- Si el timeout de `connect(IPAddress)` ocurre con un socket **ya asignado** y otro task mantiene SPI, `cancelConnectAsync()` experimental puede no cerrar inmediatamente. **No hay todavía garantía de cleanup automático**. Es una brecha de diseño explícita, no PASS end-to-end.

## 3. C2-S2C2A — alcance limitado y reproducible

Archivos añadidos:

- `tools/alpha13/gates/a13_g10_p1g_c2_s2c2a_legacy_connect.py`
- `tools/alpha13/gates/run_a13_g10_p1g_c2_s2c2a_legacy_connect.bat`
- `tools/alpha13/candidates/g10_p1g_c/native_c2_s2c2a_legacy_connect_test.cpp`
- `.github/workflows/alpha13-g10-p1g-c2-s2c2a-legacy-connect.yml`

El gate genera un overlay temporal de los **tres archivos heredados** de S2B1, con protección de cierre TX S2C1 y conexión cooperativa S2B2B. Compila realmente en C++ host **siete cuerpos fuente**:

1. `beginConnectAsync(IPAddress,uint16_t)`.
2. `pollConnectAsync()`.
3. `connectAsyncInProgress()`.
4. `cancelConnectAsync()`.
5. `cancelWriteAsync()`.
6. `connect(IPAddress,uint16_t)` (cuerpo original, sin cambios semánticos en este corte).
7. `connect(const char*,uint16_t)` (overlay con captura temporal de mutex+lease **solo durante la limpieza de la sesión anterior**).

Se prueban con mocks de SPI/W5500/DNS; el cuerpo real de `DNSClient::getHostByName()` **NO se compila en esta prueba**.

El wrapper host experimental:
- Adquiere guard W5500 al cerrar la sesión anterior por nombre.
- Si SPI_BUSY, retorna fallo compatible con API Arduino sin eliminar un handle vivo ni comenzar DNS.
- Si el índice corresponde a una copia obsoleta, no toca el hardware de otro dueño.
- Si la sesión es legítima, desconecta bajo guard, invalida su lease y suelta SPI **antes de resolver DNS**.
- Preserva el `connect(IPAddress)` bloqueante y verifica que, ante espera diferida por SPI_BUSY **sin socket asignado**, el timeout cancela la conexión diferida para impedir un establecimiento tardío involuntario.

Casos positivos del harness: conexión IPv4 bloqueante, timeout sin conexión fantasma, DNS simulado con cierre seguro de sesión previa, host SPI_BUSY sin pérdidas de handle, host obsoleto, nombre no resuelto. Los cinco métodos previos de S2C1 se compilan junto a los dos nuevos.

Cinco mutaciones negativas sobre cuerpos reales:
1. `HOST_NO_GUARD`: omitir protección durante la limpieza pre-DNS.
2. `HOST_NO_OWNER_VALIDATION`: permitir desconexión de índice reciclado.
3. `HOST_BUSY_DROPS_HANDLE`: descartar sesión ante SPI_BUSY.
4. `HOST_DNS_ACQUIRES_LONG_LOCK`: retener mutex durante DNS.
5. `IP_TIMEOUT_NO_CANCEL`: dejar una conexión diferida activa después de timeout.

Cada mutación debe **compilar y fallar su aserción nativa**.

## 4. Evidencia CI

[GitHub Actions Windows y Linux, run 38448242437](https://github.com/JW-Control/platform-jwplc/actions/runs/38448242437).

```text
G10_P1G_C2_S2C2A_CI=PASS_WINDOWS_LINUX_RUN_38448242437
G10_P1G_C2_S2C2A_CI_HEAD=ab7ee0ea6b0f2235c16b17cd87edf77c286c328d
G10_P1G_C2_S2C2A_POSITIVE_NATIVE=PASS_SOURCE_IP_HOST_PRE_DNS
G10_P1G_C2_S2C2A_REAL_CPP_METHODS=7
G10_P1G_C2_S2C2A_OVERLAY_FILES=3
G10_P1G_C2_S2C2A_NEGATIVES=5_PASS_REJECTED
G10_P1G_C2_S2C2A_FROZEN_ARCHIVES=5
G10_P1G_C2_S2C2A_PRODUCT_CHANGED=NO
G10_P1G_C2_S2C2A_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2C2A_STATUS=LEGACY_CONNECT_IP_HOST_PREDNS_NATIVE_PASS_DNS_UNPROTECTED_ADOPTION_BLOCKED
```

CI no equivale a cierre del gate local Windows, a prueba DNS real ni a compilación Arduino integrada.

## 5. Límites y condición de avance

**S2C2A no arregla DNS completo ni A13-006.** Permanece pendiente:

- S2C2B: rutas reales `Dns.cpp` y `EthernetUdp.cpp` sin mantener mutex durante esperas de red.
- `connect(IPAddress)` con socket ya asignado, timeout y SPI ocupado: cleanup diferido automático y reintento seguro sin intervención manual.
- Escritura completa (`beginWriteAsync`, `pollWriteAsync`, `write`) y lectura RX bajo lease.
- `EthernetServer`/UDP/DHCP, reset y mapeo de listeners entre epochs.
- Arduino CLI/IDE completo y fairness real compartiendo SPI con TFT/SD/FRAM/RTC.
- GPIO4 continúa en configuración por defecto 255; no activar reset físico sin interlock probado.

```text
G10_P1G_C2_S2C2A_PRODUCT_CHANGED=NO
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

## 6. Ejecución local Windows

Desde `platform-jwplc` con árbol limpio:

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_c2_s2c2a_legacy_connect.bat
```

Preflight requiere manifest PASS local S2C1
`g10_p1g_c2_s2c1_20261013_133216_bf7c8acb`.

Esperado:

```text
G10_P1G_C2_S2C2A_POSITIVE_NATIVE=PASS_SOURCE_IP_HOST_PRE_DNS
G10_P1G_C2_S2C2A_REAL_CPP_METHODS=7
G10_P1G_C2_S2C2A_OVERLAY_FILES=3
G10_P1G_C2_S2C2A_NEGATIVES=5
G10_P1G_C2_S2C2A_FROZEN_ARCHIVES=5
G10_P1G_C2_S2C2A_PRODUCT_CHANGED=NO
G10_P1G_C2_S2C2A_STATUS=LEGACY_CONNECT_IP_HOST_PREDNS_NATIVE_PASS_DNS_UNPROTECTED_ADOPTION_BLOCKED
```
