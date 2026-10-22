# Alpha13 — G10-P1G-C2-S2C2B1: ciclo de vida UDP y SPI

**Fecha:** 2026-10-13. **Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.

## 1. Cierre local anterior: S2C2A

El operador confirmó el 13 de octubre de 2026 el resultado
del gate **S2C2A** sobre
`HEAD=116dd26a953e8838e4d14ce376ae4d8ff063bd32`:

```text
G10_P1G_C2_S2C2A_POSITIVE_NATIVE=PASS_SOURCE_IP_HOST_PRE_DNS
G10_P1G_C2_S2C2A_NEGATIVE_HOST_NO_GUARD=PASS_REJECTED
G10_P1G_C2_S2C2A_NEGATIVE_HOST_NO_OWNER_VALIDATION=PASS_REJECTED
G10_P1G_C2_S2C2A_NEGATIVE_HOST_BUSY_DROPS_HANDLE=PASS_REJECTED
G10_P1G_C2_S2C2A_NEGATIVE_HOST_DNS_ACQUIRES_LONG_LOCK=PASS_REJECTED
G10_P1G_C2_S2C2A_NEGATIVE_IP_TIMEOUT_NO_CANCEL=PASS_REJECTED
G10_P1G_C2_S2C2A_REAL_CPP_METHODS=7
G10_P1G_C2_S2C2A_OVERLAY_FILES=3
G10_P1G_C2_S2C2A_NEGATIVES=5
G10_P1G_C2_S2C2A_FROZEN_ARCHIVES=5
G10_P1G_C2_S2C2A_PRODUCT_CHANGED=NO
G10_P1G_C2_S2C2A_STATUS=LEGACY_CONNECT_IP_HOST_PREDNS_NATIVE_PASS_DNS_UNPROTECTED_ADOPTION_BLOCKED
G10_P1G_C2_S2C2A_RESULTS=C:\Users\jeykc\Documentos\GitHub\platform-jwplc\tools\alpha13\results\g10_p1g_c2_s2c2a_20261013_134702_6bdf6565
```

```text
G10_P1G_C2_S2C2A_WINDOWS_LOCAL=CLOSED_PASS
G10_P1G_C2_S2C2A_CI=PASS_WINDOWS_LINUX_RUN_38448242437
```

**Importante:** S2C2A usa DNS simulado. Su pase no
demuestra el funcionamiento ni la seguridad de `Dns.cpp`.

## 2. Por qué se subdividió S2C2B

El examen de `EthernetUdp.cpp` muestra:

- `begin()` puede llamar directamente a
  `socketClose(sockindex)` y reutilizar el índice.
- `stop()` limpia estado y cierra por índice sin
  comprobar generación y owner.
- `beginPacket(IPAddress)`, `beginEndPacketAsync()` y
  `pollEndPacketAsync()` acceden a funciones
  `Ethernet.socket*` sin guard SPI propio. El código
  de la capa inferior tiene sus propias garantías
  parciales, pero estas APIs no comprueban el lease
  de su instancia UDP.
- `pollEndPacketAsync()` puede confundir una espera
  de mutex con el estado del SEND. Un inicio diferido
  debe distinguirse de `SEND` ya emitido.
- `Dns.cpp` llama a los métodos UDP de envío,
  lectura, parsing y cierre. Integrar `Dns.cpp`
  antes de asegurar la identidad UDP sería inseguro.

Por estabilidad, **S2C2B1** se limita al ciclo de vida
UDP y envío cooperativo. **S2C2B2** cubrirá DNS real
y las operaciones UDP adicionales que DNS necesita.

## 3. Corte C2-S2C2B1 implementado, sin producto

Se añadieron:

- `tools/alpha13/gates/a13_g10_p1g_c2_s2c2b1_udp_lifecycle.py`
- `tools/alpha13/gates/run_a13_g10_p1g_c2_s2c2b1_udp_lifecycle.bat`
- `tools/alpha13/candidates/g10_p1g_c/native_c2_s2c2b1_udp_lifecycle_test.cpp`
- `.github/workflows/alpha13-g10-p1g-c2-s2c2b1-udp-lifecycle.yml`

El gate genera un **overlay en carpeta local de resultados**
de dos archivos reales:

1. `JWPLC_W5x00_Ethernet.h` (token y flag de comienzo
   SEND diferido, solamente en overlay).
2. `EthernetUdp.cpp`, con seis cuerpos C++ de método
   extraídos, modificados y compilados en nativo:

| Método real | Contrato candidato |
|---|---|
| `EthernetUDP::begin(uint16_t)` | Guard SPI, cierre condicionado a token vigente, asignación de nueva identidad |
| `EthernetUDP::stop()` | Con SPI_BUSY conservar handle y estado; con lease obsoleto no cerrar hardware |
| `EthernetUDP::beginPacket(IPAddress,uint16_t)` | Exigir guard/owner antes de `socketStartUDP` |
| `EthernetUDP::beginEndPacketAsync()` | Diferir inicio de SEND cuando SPI_BUSY, conservando pendiente |
| `EthernetUDP::pollEndPacketAsync()` | Distinguir comienzo diferido de polling SEND ya emitido; revisar token |
| `EthernetUDP::cancelEndPacketAsync()` | Limpiar flags local y diferido, sin tocar el W5500 |

Las pruebas host simulan SPI/W5500 y compilan el
**`JWPLC_W5500_SafeAccess.cpp` candidato real**,
no una imitación de su mecanismo. El mock de
`EthernetClass::socketBegin` representa únicamente
el contrato probado por S2A; no es compilación real
de toda la implementación de socket.

Escenarios positivos: apertura/envío/poll/cierre;
BUS_BUSY al cerrar y abrir; copia UDP obsoleta;
diferir SEND y reanudarlo tras liberar SPI;
BUS_BUSY de `beginPacket`; reset de la generación
y rechazo de un poll sobre el socket viejo.

Se ejecutan cinco negativos:
`OPEN_NO_GUARD`,
`STOP_NO_OWNER`,
`STOP_BUSY_DROPS_HANDLE`,
`SEND_BUSY_NOT_DEFERRED`,
`POLL_NO_LEASE`.

## 4. Evidencia de CI

[GitHub Actions — run 38450704316](https://github.com/JW-Control/platform-jwplc/actions/runs/38450704316):

```text
G10_P1G_C2_S2C2B1_CI=PASS_WINDOWS_LINUX_RUN_38450704316
G10_P1G_C2_S2C2B1_CI_HEAD=1ddab59452bcfa10fa7cb5f30c3453cae9608416
G10_P1G_C2_S2C2B1_POSITIVE_NATIVE=PASS_SIX_REAL_UDP_METHODS
G10_P1G_C2_S2C2B1_REAL_CPP_METHODS=6
G10_P1G_C2_S2C2B1_OVERLAY_FILES=2
G10_P1G_C2_S2C2B1_NEGATIVES=5_PASS_REJECTED
G10_P1G_C2_S2C2B1_FROZEN_ARCHIVES=5
G10_P1G_C2_S2C2B1_PRODUCT_CHANGED=NO
G10_P1G_C2_S2C2B1_WINDOWS_LOCAL=NOT_YET_EXECUTED
G10_P1G_C2_S2C2B1_STATUS=UDP_REAL_CPP_LIFECYCLE_NATIVE_PASS_DNS_PENDING_ADOPTION_BLOCKED
```

La primera CI falló porque en `POLL_NO_LEASE` el
backend nativo rechazó `UDP_POLL_WITHOUT_TOKEN`
antes de que la aserción superior pudiera detectar
el acceso. El negativo ya compilaba y fallaba como
se esperaba; se corrigió la cadena requerida para
reconocer el **primer fallo observable**. La CI
definitiva pasó Windows y Linux. El cuerpo positivo
y el producto no se modificaron por esa corrección.

## 5. Límites de prueba y bloqueos

**Esta es una prueba parcial, no un fix adoptado.**

```text
S2C2B1=CI_PASS_NATIVE_UDP_LIFECYCLE_ONLY
S2C2B1_WINDOWS_LOCAL=PENDING
S2C2B2=DNS_REAL_UDP_PARSE_WRITE_RECEIVE_PENDING
UDP_MULTICAST=NOT_COVERED
UDP_READ_WRITE_PACKET_PARSER=NOT_COVERED
DHCP=NOT_INTEGRATED
SOCKET_BACKEND_ALL_PATHS=NOT_COVERED
SPI_FAIRNESS_DISPLAY_SD_FRAM=NOT_TESTED
ARDUINO_CLI_BASELINE_CANDIDATE=NOT_RUN
HARDWARE_ETHERNET=NOT_TESTED
GPIO4_DEFAULT=255
PRODUCT_SOURCE_FILES=UNCHANGED
PRECOMPILED_ARCHIVES=5_FROZEN
A13-006=ADOPTION_BLOCKED
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
```

Se requiere resolver RX/TX UDP real, parser DNS,
DHCP y compatibilidad Arduino antes de adoptar.
Ni la CI nativa ni la prueba host demuestran
latencias o ausencia de interbloqueos físicos.

## 6. Ejecución local Windows

Desde PowerShell en la raíz de `platform-jwplc`:

```powershell
git status --short
git pull --ff-only origin v2.1.0-alpha.13/feature/cleanup-robustness
.\tools\alpha13\gates\run_a13_g10_p1g_c2_s2c2b1_udp_lifecycle.bat
```

El gate verifica el manifest PASS de S2C2A:
`g10_p1g_c2_s2c2a_20261013_134702_6bdf6565`.

Salida esperada:

```text
G10_P1G_C2_S2C2B1_POSITIVE_NATIVE=PASS_SIX_REAL_UDP_METHODS
G10_P1G_C2_S2C2B1_REAL_CPP_METHODS=6
G10_P1G_C2_S2C2B1_OVERLAY_FILES=2
G10_P1G_C2_S2C2B1_NEGATIVES=5
G10_P1G_C2_S2C2B1_FROZEN_ARCHIVES=5
G10_P1G_C2_S2C2B1_PRODUCT_CHANGED=NO
G10_P1G_C2_S2C2B1_STATUS=UDP_REAL_CPP_LIFECYCLE_NATIVE_PASS_DNS_PENDING_ADOPTION_BLOCKED
```

No habilitar GPIO4 ni realizar upload. No se
ha modificado `JWPLC/2.1.0`.
