# Alpha13 G10-I — contrato B03 de cierre diferido DNS / DHCP / UDP

**Estado:** REVIEW — diseño a implementar en overlay y probar en CI; **NO FIX, NO PASS DE INTEGRACIÓN**.
**Fecha:** 2026-10-14. **Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.
**Prioridad:** robustez ante `SPI_BUSY` en W5500 compartido. Este documento es un contrato de validación, no una adopción productiva.

## Defecto observado en fuentes actuales
- `JWPLC_Ethernet/src/Dns.cpp`: `DNSClient::cancelResolveAsync()` y `finishResolveAsync(int)` llaman a `iUdp.stop()` y después borran estado de operación DNS.
- `JWPLC_Ethernet/src/Dhcp.cpp`: `DhcpClass::finishAsync()` llama a `_dhcpUdpSocket.stop()` y después desactiva `_asyncActive`; existen cierres en caminos síncronos y previos a un nuevo begin.
- El overlay UDP actual implementa `EthernetUDP::stop()` con salida temprana si `BusScope(20)` no puede adquirir SPI, para evitar pérdida inmediata de token. Si el propietario superior abandona su servicio, el cierre puede no reintentarse.
- La reproducción C++ source-backed B03 actual marca `RISK_REPRODUCED`, no corrección. El CI integrado comprueba explícitamente que el riesgo sigue abierto.

## Invariantes obligatorias
1. **SPI_BUSY != SOCKET_CLOSED:** no liberar ni sobrescribir token, índice, puerto, listener o buffers como si el W5500 hubiese confirmado cierre.
2. **Propiedad / epoch:** todo cierre diferido conserva identidad de socket y su epoch; solo puede cerrarlo el titular vigente. Tras reciclaje o reset, el token antiguo se considera `STALE` y jamás puede cerrar un socket de otro dueño.
3. **Progreso garantizado:** el ciclo de servicio de UDP/DNS/DHCP dispone de una ruta de reintento invocada aunque la operación DNS/DHCP ya haya terminado. No depender solo de `pollResolveAsync()` cuando `iAsyncActive=false`, ni de `pollDHCP()` si `_asyncActive=false`.
4. **Sin espera bloqueante:** no mantener SPI ni lock durante `delay`, timeout, espera de paquete o reintento cooperativo.
5. **Reentrada:** comenzar una operación nueva no destruye un cierre pendiente. Retornar estado BUSY / pendiente conservando compatibilidad API legacy; no reusar una ranura cuyo cierre no se confirmó.
6. **Timeout:** el vencimiento de la operación de red NO equivale a liberación física de socket.
7. **Sin cambio productivo** en esta fase hasta comprobar suite nativa y compilación Arduino del overlay sobre el mismo HEAD. Sin modificar los cinco archives congelados, `boards.txt`, `platform.txt` ni GPIO4.

## Secuencia de implementación requerida en el ejecutor G10-I existente
A. Inspeccionar la semántica completa de `EthernetUDP::stop()`, `begin()`, `pollEndPacketAsync()`, token y `BusScope`, incluyendo destructor y copia si aplica.
B. Añadir al overlay una transición explícita `OPEN -> CLOSE_PENDING (BUS_BUSY) -> CLOSED` o `STALE`. El indicador de cierre y los datos de propiedad deben vivir más allá del fin lógico DNS/DHCP.
C. Proveer método interno de `serviceDeferredClose()` invocable sin operación DNS/DHCP activa y conectarlo a una llamada cooperativa real ya existente; si falta tal vía, mantener bloqueo G10-I.
D. Antes de `beginResolveAsync()` y `beginWithDHCPAsync()`, impedir la adquisición de una nueva sesión mientras hay cierre pendiente. Establecer resultado coherente con la API existente; evitar modificar firmas públicas sin revisión.
E. Reutilizar el mismo overlay fuente y las mismas fases CI existentes; prohibido dar PASS solo con mocks no conectados al código real.
F. Revisar otros caminos de salida: DNS errores de `BuildRequest`/timeout/cancel, DHCP ACK/NAK/timeout y variante síncrona de DHCP.

## Matriz mínima de aceptación automática
| Caso | Resultado exigido |
|---|---|
| Cierre normal con SPI libre | Socket liberado y token invalidado |
| DNS finish con SPI_BUSY | `CLOSE_PENDING`, token preservado, cero cierre confirmado |
| DNS cancel con SPI_BUSY | Igual; debe reintentarse sin poll DNS activo |
| DHCP finish con SPI_BUSY | Igual; debe reintentarse sin DHCP activo |
| Varios SPI_BUSY consecutivos | Sin pérdida de token ni bloqueo activo del mutex |
| Cierre finalmente adquiere SPI | Reintento único seguro y finalización |
| Socket reciclado / epoch alterado | `STALE`, no cerrar socket nuevo |
| Segundo DNS/DHCP begin con cierre pendiente | No reusar ni pisar el titular anterior |
| Timeout seguido de cancel / begin | Sin fuga de propiedad ni doble-close |
| Multitarea: TFT/SD/FRAM/RTC contienden | Validación nativa + integración de autoload en CLI; comportamiento físico queda G10-H |
| Mutantes: ignorar SPI_BUSY / forzar stale / perder token | Negativos deben fallar explícitamente |

## Evidencia necesaria para declarar G10-I READY_FOR_HARDWARE
- Ejecutar suite nativa Windows + Ubuntu con cuerpos C++ reales extraídos de la fuente compuesta (no fixtures independientes).
- Ejecutar 11 sketches baseline y 11 candidato como mínimo, y añadir un sketch combinado Ethernet/UDP/DNS/DHCP/ModbusTCP para verificar encabezados y link.
- Registrar `.d`, `.o`, ELF, selección de biblioteca, cinco archives congelados y autoload habitual sin excluir periféricos.
- Registrar `SUMMARY.log` / `MANIFEST.json`, commit SHA, CI run, todas las brechas B01–B08 y lo no demostrado.
- `G10-I=PASS` únicamente si no subsiste riesgo de pérdida de dueño/cierre para las rutas afectadas. De lo contrario `G10-I=REVIEW_STOP`.
- `G10-H` requiere autorización física separada. No flashear ni resetear desde CI.

**Al redactar este contrato:** `A13-006=ADOPTION_BLOCKED`, G10-H y G10-C no iniciados, estado vivo en `docs/v2.1.0-alpha.13/ALPHA13_STATUS.md`.
