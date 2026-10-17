# Alpha13 — G10-R: cierre no físico, campaña integrada y transferencia a Alpha15

**Fecha de preparación:** 16/10/2026 (Perú)  
**Rama:** `codex/alpha13-g10-r-recovery-prep` · PR [#108](https://github.com/JW-Control/platform-jwplc/pull/108) (**Draft**)  
**Producto:** `A13-006=ADOPTION_BLOCKED`. **No merge, no PreRelease, no upload ni acción en RJ45 por este documento.**

## 1. Regla del equipo: un único gate integrado, no micropruebas para el operador

Se mantiene el protocolo `01_PROTOCOLO_CONTINUIDAD_Y_CI.md`: N0 preflight, N1 C++ real y negativos, N2 Arduino fuente→objeto→ELF, N3 integración baseline→candidato, H banco físico autorizado y C cierre documental. Ejecutar las fases internas dentro de **una misma campaña**, con abortos y manifiesto único; no repetir gates cerrados sin cambio de código, flags, core o archivos congelados.

**Evidencia reutilizable:** G10-I N3 `11/11` baseline + `11/11` candidato [#38843599043](https://github.com/JW-Control/platform-jwplc/actions/runs/38843599043); G10-H smoke físico acotado `47 TCP/47 UDP/49 RTU` sin errores, `52` solicitudes Slave con CRC=0, archivado documentalmente en PR #106. No confundir ninguno con recuperación ni endurance.

## 2. PRE-H integrado: condiciones antes de tocar placas

| Capa | Cobertura integrada | Estado |
| --- | --- | --- |
| N0 | HEAD exacto, alcance sin producto, package temporal, contrato de consentimiento, fallback y negativas del runner | Implementado; volver a verificar en el último SHA |
| N1 / R3 | **14 métodos reales** DNS/DHCP/UDP extraídos de G10-I + SafeAccess.cpp con snapshots/traza de laboratorio, mutex host no recursivo; escenarios de timeout, cancel, rebegin, lease, multicast, BUS_BUSY y drenaje diferido | Suite `a13_g10_r_r3_protocol_native.py` integrada al job nativo; 3 mutaciones de alto impacto, además de 7 sobre SafeAccess y controles offline |
| N2 | Compilar ambos sketches COM4/COM14 bajo core y cinco archives congelados, constatar bibliotecas, `.d/.o/ELF`, VID/PID y hashes | Job fuente-first Windows/Ubuntu, **sin COM ni upload** |
| N3 | Matriz representativa de autoload de todos los periféricos, baseline↔candidato sobre fuentes exactas | PASS anterior de G10-I, revalidar cuando se adopte al producto; no repetir por cada cambio documental |
| H | **R1 + R2 + R3 + R4 dentro de una sola sesión controlada**, datos correlacionados PC/UART + hashes + observación de TFT/TCA | **BLOQUEADO**: R3 físico aún no implementado; el ejecutor `--live` devuelve `G10R_PHYSICAL_BLOCKED_R3_PHASE_MISSING_IN_INTEGRATED_CAMPAIGN` |
| R5 | Power-cycle, watchdog/arranque, compatibilidad Arduino IDE, package instalado | Separado, requiere autorización propia y N3 final |

El archivo `a13_g10_r_r3_protocol_native.py` no sustituye los 9 mutantes históricos de G10-I (ya comprobados): reutiliza el mismo código fuente en un único job N1. Un positivo nativo no autoriza adopción ni demuestra contención física.

## 3. Requisitos exactos para DESBLOQUEAR una única campaña H

1. **R3 físico dedicado:** instrumentación de fault injection de `SPI_BUSY` acotada y determinista desde el diagnóstico, sin GPIO4, sin alterar reglas productivas de propiedad de mutex, sin relés ni escritura SD/FRAM. Debe ejercitar, con puntos y duración controlados, `DNSClient::finish/cancel`, `DhcpClass::finish/cancel`, `EthernetUDP::stop`, `EthernetClient::stop/cancelConnectAsync`, junto con 1/2/4 clientes dentro de presupuesto de 8 sockets. El protocolo debe publicar inicio/fin de contención y eventos owner/epoch/CLOSE_PENDING/DRAIN/STALE identificables, no solo snapshots.
2. **Seguridad y abortos:** timeout máximo de cada inyección, liberación garantizada del lock, stop de sesión y observabilidad aunque falle el serial; detener campaña si aparece watchdog, caída inesperada o `dropped/overrun` en la traza. El modo de inyección nunca se ejecuta al arrancar por defecto.
3. **Cobertura de periféricos:** R1 (≥600 s), R2 (tres ciclos RJ45 con observación física `LinkOFF` y recuperación `LinkON`), R3 (cobertura completa de las rutas citadas) y R4 (≥1800 s) conservando autoload Display/Ethernet/SD/FRAM/RTC/botonera/RS-485/RTU/TCA. Fairness TFT/TCA y latencias deben quedar **medidas**, no inferidas solo de compilar o del heartbeat.
4. **Un solo paquete de evidencia:** `SUMMARY.log`, `MANIFEST.json`, logs COM4/COM14, trazas PC TCP/UDP/DNS/DHCP, timeline de RJ45/lock SPI y estadísticas. Vincular fuente→objeto→ELF→upload (dos autorizaciones diferenciadas). `R3=REVIEW_STOP` si hay cualquier hueco, double-close, cierre de dueño nuevo, callback bajo BUS_BUSY, socket huérfano o timeout sin liberación.
5. **Autorización separada:** confirmar banco sin cargas Q, identidad de COM y recuperación del firmware anterior; operador autoriza primero *flash*, luego *captura y RJ45*. **R5 power-cycle nunca implícito**.

**El gate H sigue NOT_READY.** No proporcionar aún comandos de campo ni cambiar a `R3_PHYSICAL_INJECTION_READY=True` sin superar los criterios anteriores, CI del HEAD final y aprobación explícita.

## 4. Transferencia ordenada Alpha13 → Alpha15 (trabajo de André)

**Destino identificado:** `v2.1.0-alpha.15-HmiDesigner`; [estado Alpha15](https://github.com/JW-Control/platform-jwplc/blob/v2.1.0-alpha.15-HmiDesigner/docs/v2.1.0-alpha.15/ALPHA15_STATUS.md). Alpha15 sigue avanzando con codegen, inspector y `JWPLC_TFT 0.2.0`, que **puede divergir** del archive TFT de Alpha13. Por ahora HMI_DESIGNER sigue OUT_OF_SCOPE para cambios Alpha13.

- **Se puede hacer ya:** comparación *read-only* de APIs, ABI, headers y archives entre la rama producto Alpha13 y Alpha15; trabajar en el Designer contra su perfil versionado, con un archivo de compatibilidad que distinga `AVAILABLE`, `PROPOSED` y `NOT_ADOPTED`. No inventar funciones por esperar G10.
- **No transferir todavía:** los PR Draft [#105](https://github.com/JW-Control/platform-jwplc/pull/105)–[#108](https://github.com/JW-Control/platform-jwplc/pull/108) en bloque; contienen overlays, hooks `Lab*`, diagnóstico, tests y una decisión de **diferir** adopción. No introducir esas APIs de laboratorio en HMI Designer.
- **Cuando se autorice adopción:** inventariar los commits **productivos** G1–G9 ya cerrados frente al HEAD exacto Alpha15; incorporar solo diferencias pendientes y aprobadas, evitando sobrescribir las primitivas TFT 0.2.0 del equipo de André. Para G10, integrar exclusivamente fuentes productivas aprobadas **sin** `LabSocketSnapshot`, `LabEvent`, hooks `labReadEvents` ni sketch de banco. Revisar link de archives (core, TFT, FRAM, RTU) y consumidores Arduino/HMI.
- **Gate final de compatibilidad:** N3 de paquete instalado, Arduino IDE, matriz básica HMI generada y pantalla real; revisar conflictos de API/ABI y registrar SHA padre/destino con PR de transferencia separado. No asumir que CI experimental G10-R sustituye las pruebas de distribución.

## 5. Estado de cierre de esta preparación

```text
G10-R-PREH=INTEGRATED_OFFLINE_GATE_PREPARED
G10-R-R3-NATIVE=DNS_DHCP_UDP_14_REAL_METHODS_AND_TRACE_HOST
G10-R-LIVE=BLOCKED_INTENTIONALLY_R3_PHYSICAL_PHASE_MISSING
G10-R-R1_R2_R3_R4-PHYSICAL=NOT_RUN
G10-R-R5=NOT_AUTHORIZED
A13-006=ADOPTION_BLOCKED
G10-C=CLOSED_DECISION_DEFER_ADOPTION
PRODUCT_MODIFIED=NO
G10-R-ALPHA15-TRANSFER=READ_ONLY_COMPATIBILITY_ALLOWED_PRODUCT_TRANSFER_BLOCKED
NEXT_GATE=R3_DETERMINISTIC_LAB_FAULT_INJECTION_DESIGN_CI_THEN_ONE_PHYSICAL_CAMPAIGN
```


## 6. Avance incremental R3 — contención real acotada sobre UDP de laboratorio

Se añadió al sketch **temporal** `g10_r_integrated.ino` una primera subfase `G10R_R3_UDP:<token>`, **inactiva por defecto** y exigida por sesión. Un task FreeRTOS independiente obtiene `BusScope(0)` del **mismo mutex W5500** y programa una espera **nominal** `vTaskDelay(pdMS_TO_TICKS(80))`, pero **no presume límite máximo real**: mide `micros()` y exige 60–120 ms, con `HOLD_TIMING_REVIEW` fuera del rango; no intercambia datos SPI, no toca GPIO4, no acciona Q ni modifica SD/FRAM. El loop invoca el **`EthernetUDP::stop()` real** sobre un UDP temporal port 15021 durante la ventana de bloqueo. Si no se solapan, informa `CLOSE_RACE_REVIEW`, jamás PASS. El task libera el lock por destructor RAII y elimina su propia tarea.

**Identidad sin inferencias:** antes de armar la inyección, el mismo scope SPI cubre snapshots `LabSocketSnapshot[8]` antes/después de `EthernetUDP::begin(15021)`; se exige **exactamente una nueva asignación**. Se imprime `G10R_R3_LEASE:<token> SID/EPOCH/ALLOC/OWNER`. Las trazas de solicitud de ticket `QueueRequest` y de cierre `DrainClose` solo pueden correlacionarse con ese mismo lease. Las fallas de lock, creación de task, preparación de UDP o expiración fuera de ventana son resultados de revisión, nunca éxito. El socket temporal se intenta liberar incluso en rutas de error. No puede iniciarse una nueva sesión mientras el task anterior sigue trabajando.

**Evaluación sin hardware:** `a13_g10_r_r3_probe_contract.py --selftest` verifica la secuencia UART de `ARMED`, `CLOSE_DURING_HOLD`, `AWAIT_LIFECYCLE_REVIEW` y los eventos del lease exacto, con **16 mutaciones negativas**. `a13_g10_r_build_candidate.py` impide compilar un sketch sin esta protección. Este preflight es sintético; falta evidencia W5500 real.

**Límites y bloqueo:** aunque esta subfase usa un lock físico real cuando finalmente se ejecute en la placa, **no cubre todavía DNS/DHCP/TCP ni los clientes concurrentes 1/2/4**. Por tanto `R3_PHYSICAL_INJECTION_READY=False` en el runner de campo: `--live` continúa fallando antes de cualquier COM, upload o maniobra. No está autorizada una prueba aislada de la sonda UDP. El objetivo sigue siendo **una sola campaña H integrada R1→R2→R3 completo→R4**, con la misma captura consolidada. R5/power-cycle es otro gate.

```text
G10-R-R3-UDP-BOUNDED_PROBE=SOURCE_IMPLEMENTED
G10-R-R3-UDP-HOLD_NOMINAL_MS=80
G10-R-R3-UDP-HOLD_ACCEPT_US=60000_TO_120000
G10-R-R3-UDP-LEASE=EXACT_SOURCE_BACKED_SNAPSHOT
G10-R-R3-UDP-REPLAY=OFFLINE_16_NEGATIVES
G10-R-R3-DNS_DHCP_TCP=NOT_IMPLEMENTED_FOR_PHYSICAL_INJECTION
G10-R-H=BLOCKED_FAIL_CLOSED
ALPHA15_PRODUCT_TRANSFER=NOT_AUTHORIZED
```
