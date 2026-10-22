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


## 7. Endurecimiento de procedencia y evidencia PRE-H — 16/10/2026

**Bug de diagnóstico corregido (en fuente de laboratorio y host, no producto):** el primer evaluador de la sonda UDP consideraba inválida una sesión si también se cerraba correctamente otro socket. Eso era contraproducente para el propósito **concurrente** de R3: el gate ahora identifica la sonda mediante `SID/EPOCH/ALLOC/OWNER` exacto, exige un único `DrainClose` de **ese lease** posterior a su `QueueRequest`, **permite** cierres legítimos de otros sockets y **rechaza** doble drenaje del lease de prueba. El fixture aporta un caso positivo con tráfico paralelo y ahora tiene **17 negativos**.

**Evidencia USB/PC:** `a13_g10_r_field_runner.py` ya separa `r3_udp_lines`, `r3_udp_sessions` y `r3_udp_verdicts` en `MANIFEST.json` cuando se capturen sesiones R3. El identificador de la sesión y los mensajes `G10R_R3_LEASE`, `G10R_R3_HOLD` y `G10R_R3_UDP` se conservan con fase/tiempo; se aplica `a13_g10_r_r3_probe_contract.py` sobre esas evidencias y el historial R3, **sin** convertir el resultado en PASS físico.

**Doble puerta de seguridad:** `a13_g10_r_field_runner.py --live` sigue bloqueado desde antes de abrir COM (`R3_PHYSICAL_INJECTION_READY=False`) y ahora **`a13_g10_r_build_candidate.py --upload` también rechaza la operación antes de iniciar compilación/USB** mediante `R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=False`. El `--selftest` exige conservar este cierre. Ninguno de estos flags se levantará por CI nativa, sin inyección DNS/DHCP/TCP y orquestación R1→R2→R3→R4 completa.

**Evidencia anterior inmutable:** CI [#38998540944](https://github.com/JW-Control/platform-jwplc/actions/runs/38998540944) = 5/5 SUCCESS del commit `5e4a6d78`, *antes* de los cambios de esta sección. Las CI de los commits siguientes se consultan **por SHA exacto**. No adjudicar el PASS anterior al HEAD más nuevo.

```text
G10-R-UDP-COLLECTOR=INTEGRATED_IN_HOST_NOT_PHYSICALLY_RUN
G10-R-UDP-CONTRACT=17_NEGATIVES_PLUS_CONCURRENT_POSITIVE
G10-R-UDP-PHYSICAL=NOT_RUN
G10-R-FLASH_GATE=FAIL_CLOSED
G10-R-LIVE_GATE=FAIL_CLOSED
G10-R-DNS_DHCP_TCP_PHYSICAL_INJECTION=NOT_READY
G10-R-H=BLOCKED
A13-006=ADOPTION_BLOCKED
```


## 8. R3 — cinco inyecciones de cierre sobre el hardware real, PREPARADAS PERO NO FLASHEADAS

Se añadieron **cinco modos source-backed** al **mismo** `g10_r_integrated.ino`, subordinados a `G10R_START:<token>` y a una orden explícita del runner, sin arranque automático:

| Modo R3 | Objeto REAL | Operación durante retención SPI | Qué **no** demuestra |
| --- | --- | --- | --- |
| `UDP` | `EthernetUDP` temporal puerto 15021 | `stop()` | DNS, DHCP, TCP |
| `DNS` | `DNSClient` temporal y consulta `jwplc-g10r.invalid` | `cancelResolveAsync()` | DNS con respuesta exitosa |
| `TCP` | `EthernetClient` temporal y destino de benchmarking 198.18.0.1:15022 | `cancelConnectAsync()` | 1/2/4 conexiones de cliente reales |
| `DHCP_CANCEL` | `DhcpClass` temporal | `cancelDHCP()` | Adquisición/renovación DHCP de la instancia global |
| `DHCP_FINISH` | `DhcpClass` temporal | `finishAsync()` | ACK/NAK, renew/rebind end-to-end |

**Condición DHCP de seguridad:** solo se agrega en el overlay temporal de 14 fuentes un `friend class G10RLabDhcpProbe` a `DhcpClass`. El helper abre exclusivamente su miembro UDP interno en **puerto de laboratorio 15023**; **NO llama** `DhcpClass::beginWithDHCPAsync()`, `EthernetClass::cancelDHCP()`, ni envía `DHCP_DISCOVER`. Por tanto puede probar que los callbacks de limpieza publican/drenan un cierre diferido durante `SPI_BUSY` sin sustituir ni afectar directamente el gestor DHCP del autoload. La fuente productiva, archivos `.a` congelados y APIs públicas permanecen intactos.

**Protocolo de procedencia actualizado:** `G10R_PROTOCOL:R1R2R3R4_FAULT_LAB_V2` y `READY VER=R1R2R3R4_FAULT_LAB_V2`. Un firmware anterior que solo anuncie `R1R2R4_R3SNAPSHOT_V1` ya no cumple el handshake del runner, independientemente del puerto COM.

El `a13_g10_r_field_runner.py` dispone de `r3_suite()` que agrupa los cinco modos como **una sola fase R3** dentro de R1→R2→R3→R4: cada subfase tiene token, captura UART+anillo de lifecycle, identificación del socket, espera de drenaje y aborto inmediato ante `PREPARE_FAILED/MUTEX_NOT_ACQUIRED/HOLD_TIMING_REVIEW/TRACE_GAP`. Ninguna subfase requiere del operador enviar comandos separados. Este método **todavía no puede entrar en ejecución**, porque `R3_PHYSICAL_INJECTION_READY=False` y `R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=False`.

### Condiciones pendientes antes de un único comando físico

- **C1 — CI del HEAD exacto:** cinco jobs de N0/N1/N2 source-first en Windows/Ubuntu, sin errores ni `skipped`; revisión del parche DHCP solo-en-stage y de logs `.d/.o/ELF`.
- **C2 — Contención TCP 1/2/4 y presupuesto de 8 sockets:** diseñar una carga TCP concurrente con criterio de aceptación compatible con el servidor Modbus TCP actual (unit ID 1 **no** representa cuatro clientes), sin falsear PASS por aperturas fallidas; documentar puerto/peer y métricas.
- **C3 — DHCP de red real:** añadir a R2 la observación y evidencia de recuperación de IP, DNS y DHCP del gestor productivo tras link-loss, sin disparar una segunda negociación sobre una LAN industrial. El test `DHCP_CANCEL/FINISH` cubre únicamente las rutas de limpieza bajo contención.
- **C4 — Equidad de periféricos:** obtener métricas físicas de TFT/TCA/I/O y confirmar que autoload, SD, FRAM, RTC, RS-485/RTU siguen atendidos durante R3/R4; no basta el conteo de heartbeats.
- **C5 — Banco:** COM4/COM14, PLC sin cargas Q, switch aislado, inventario de firmware anterior/restauración, consentimientos separados para upload y captura/RJ45, y parada segura por timeout o pérdida de eventos.

**Criterio de no-regresión:** no elevar `R3_PHYSICAL_INJECTION_READY` mientras exista cualquiera de C1–C5 pendiente. No inferir que un test N1 simulado equivale a una inyección física ejecutada. Al alcanzar PRE-H PASS se preparará **un solo** flujo de operador, nunca cinco microgates.

```text
G10-R-R3-LAB_MODES=UDP_DNS_TCP_DHCP_CANCEL_DHCP_FINISH
G10-R-R3-SOURCE=STAGED_ONLY_NO_PRODUCT_APIS
G10-R-R3-UART=R1R2R3R4_FAULT_LAB_V2
G10-R-R3-DHCP_EXTRA_DISCOVER=NO
G10-R-R3-TCP_PARALLEL_1_2_4=NOT_QUALIFIED
G10-R-R3-DHCP_LEASE_RECOVERY=NOT_QUALIFIED
G10-R-R3-TFT_TCA_FAIRNESS=NOT_MEASURED_PHYSICALLY
G10-R-R3-UPLOAD=BLOCKED
G10-R-R3-LIVE=BLOCKED
G10-R-R3-PHYSICAL=NOT_RUN
A13-006=ADOPTION_BLOCKED
```
