# G10-I B01 — contrato optativo aprobado

Estado del overlay candidato: `B01=PASS_NONPHYSICAL_OPT_IN_LEGACY_LIMITED`.
La decisión de arquitectura conserva las firmas y retornos escalares Arduino.
No adopta el overlay en firmware productivo ni acredita hardware.

## Firmas públicas del candidato

```cpp
jwplc_w5500_candidate::StatusReadOutcome
EthernetClient::jwplcProbeSocketStatus(uint8_t& observed); // ya existente

jwplc_w5500_candidate::StatusReadOutcome
EthernetServer::jwplcProbeListening(bool& observed); // adición B01
```

Ambas usan `StatusReadOutcome::{Read,BusBusy,Stale}`. `observed` es una
observación actual únicamente cuando el resultado es `Read`; permanece
intacto ante `BusBusy` y `Stale`. No se usa un valor cacheado como estado
actual. En Server, `Stale` significa que no hay listener con lease vigente
para ese puerto, incluso antes del primer `begin()` o tras un reciclaje. Si
hay al menos uno vigente, se leen sus estados W5500 bajo el mismo mutex; el
resultado `Read(false)` significa que se leyó un listener vigente pero no
estaba en `LISTEN`. Varios listeners vigentes se reducen a `true` si alguno
está en `LISTEN`.

El probe Client ya proporcionaba el contrato triestado, por lo que no se
creó otro método Client. `EthernetClass::socketStatus(uint8_t)` recibe solo
un índice; sin identidad de propietario no puede distinguir un lease viejo.
Los consumidores con `EthernetClient` deben usar su probe; los de listener,
el de `EthernetServer`. No se ofrece un probe raw que simule ownership.

## Compatibilidad y consumidores

Las funciones `connected()`, `status()`, `socketStatus(uint8_t)` y
`EthernetServer::operator bool()` conservan firmas y retornos. Con BUS_BUSY
pueden devolver `0`, `SnSR::CLOSED` o `false`, indistinguibles de una
situación desconectada real para un llamador que solo use esas APIs. Los
sketches anteriores siguen compilando; sus decisiones de cierre/reconexión
siguen sujetas a ese límite. La migración a los probes es optativa para los
consumidores que necesitan certeza.

| Consumidor | Resultado B01 | Tratamiento de BUS_BUSY |
| --- | --- | --- |
| Modbus TCP Client `serviceConnection()` y `serviceResponse()` | PASS acotado | Probe Client y mutex preservan sesión y reintentan. |
| Modbus TCP Server `serviceServer()` | PASS acotado | Probe Client antes de ACK RX INT y lectura; preserva cliente. |
| Modbus TCP Server `ensureServerListening()` | PASS acotado | Probe Server antes de reabrir; BUS_BUSY conserva el intento. |
| Cliente/listener optativo externo | PASS de contrato | Recibe `BusBusy`, sin estado observado nuevo. |
| Consumidor externo exclusivamente escalar | REVIEW de integración | Puede interpretar BUS_BUSY como desconexión; requiere migración optativa si decide cerrar/reconectar. |

## Evidencia y límites

La prueba N1 compila los cuerpos compuestos C++ reales de Client/Server,
`socketStatus()` y `ensureServerListening()` contra una frontera W5500/RTOS
host instrumentada. Cubre conectado/desconectado leído, listener LISTEN/no
LISTEN leído, SPI ocupado, lease inválido, socket reciclado, recuperación y
ausencia de reapertura Modbus espuria. Siete mutaciones negativas detectan
pérdida de guard de lease, salida alterada en BUS_BUSY, aceptación de listener
reciclado y omisión de reapertura en STALE. N2 compila sketches Arduino
anteriores contra baseline/candidato y un sketch candidato que usa ambos
probes; comprueba procedencia source-first con `.d`, objetos y ELF.

Esto permite `N3_READY=YES_NONPHYSICAL`: B01/B02/B03/B04 tienen PASS no
físico acotado, B05 mantiene revisión física, y N3 debe validar el package
integrado completo. N3 PASS no autoriza adopción, flash ni hardware. Quedan
pendientes pruebas reales de reset, reconexión, tráfico y convivencia SPI.
