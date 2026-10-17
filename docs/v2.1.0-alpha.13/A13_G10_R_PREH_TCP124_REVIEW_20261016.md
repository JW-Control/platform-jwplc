# Alpha13 G10-R — PRE-H TCP 1/2/4 y aceptación de concurrencia (revisión source-first)

**Estado:** `REVIEW_REQUIRED` — análisis no físico, sin firmware ni CI nuevos.  
**Base revisada:** `codex/alpha13-g10-r-recovery-prep` @ `1492336b8b56a72c0e4e1014dee8069f3f89e504`.  
**Ámbito:** candidato G10-R; `A13-006=ADOPTION_BLOCKED`. No adopción, merge, upload, RJ45, GPIO4, escritura SD/FRAM ni regeneración de archives.

## Evidencia reutilizada sin reejecución

- CI del HEAD exacto: [#39000434161](https://github.com/JW-Control/platform-jwplc/actions/runs/39000434161), cinco jobs SUCCESS (dos Arduino source-first COM4/COM14, dos replay/negativos y un C++ host R3).
- N0: [#39000434177](https://github.com/JW-Control/platform-jwplc/actions/runs/39000434177), dos jobs SUCCESS.
- El gate previo G10-H validó un smoke físico de alcance corto, **no** escenarios R3 ni múltiples clientes.
- `a13_g10_r_r3_probe_contract.py` devuelve explícitamente `coverage.tcp_1_2_4_clients=False`; aunque el contrato de correlación de lease y sus negativos pase, no se considera prueba de concurrencia TCP.
- `a13_g10_r_field_runner.py` prepara cinco sondas consecutivas en `r3_suite()` y ejecuta tráfico cooperativo; no contiene aún un gate demostrativo independiente de 1/2/4 clientes **simultáneamente vivos**.
- `a13_g10_r_build_candidate.py` mantiene `R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=False`; `a13_g10_r_field_runner.py` mantiene `R3_PHYSICAL_INJECTION_READY=False`. **Conservar ambos**.

## Contrato de aceptación propuesto (no implementado)

1. **Identidad por cliente:** asignar identificador de sesión, socket, epoch, allocation y owner a cada conexión; registrar inicio, tráfico FC03, cierre solicitado y cierre efectivo. Rechazar confusiones con otras sesiones o sockets reciclados.
2. **Escenarios 1/2/4:** demostrar simultaneidad real, no solo cuatro solicitudes secuenciales. Mostrar `active_peak` y ventanas de solapamiento por sesión; conservar contador de conexiones exitosas/fallidas, latencias, timeouts y respuestas válidas.
3. **Presupuesto de ocho sockets W5500:** contabilizar sockets de cliente, servidor, UDP y DNS/DHCP de laboratorio. Abortar ante sobreasignación, reutilización de lease todavía vivo, dobles cierres o pérdida de propietario.
4. **SPI_BUSY bajo carga:** para cada lease afectado distinguir QueueRequest, DrainClose, CloseStale y Recycle; `BUS_BUSY` jamás prueba desconexión ni liberación. No bloquear el mutex global durante esperas activas o tráfico de otro periférico.
5. **Veredicto reproducible:** fixtures positivos y negativos host con código real donde aplique, prueba N2 dirigida a fuentes nuevas, y N3 **solo si** cambia composición del package o integración relevante. El replay debe emitir `REVIEW_STOP` ante huecos de secuencia, ausencia de simultaneidad, propietarios ambiguos o evidencias incompletas.
6. **Fairness y recuperación:** medir progreso TFT/TCA/I/O, RTC, FRAM, SD, RTU y comportamiento real DHCP renew/rebind en la campaña única R1→R2→R3→R4. Estos aspectos **no** se aprueban mediante el contrato TCP host.

## Orden no físico antes de liberar H

- Implementar **host-first** el generador/colector y evaluador TCP 1/2/4 con contratos de simultaneidad y 8 sockets, bajo runner aislado. Mantener los dos bloqueos físicos en False.
- Ejecutar N1 nativo y negativos; N2 Arduino dirigido si se modifica el sketch/overlay; comparar SHA exacto de CI y código.
- Integrar métricas con el runner de una sola campaña; revisar procedimientos de backup/restauración y cargas desconectadas; decidir autorización específica del operador **después** del cierre PRE-H, nunca por este documento.

## Límites y NEXT_GATE

```text
G10-R-PREH-TCP124=REVIEW_REQUIRED_NOT_IMPLEMENTED
G10-R-CI_BASE=PASS_5_OF_5_39000434161_HEAD_1492336b
G10-R-N0_BASE=PASS_2_OF_2_39000434177_HEAD_1492336b
G10-R-H=BLOCKED
G10-R-UPLOAD=BLOCKED
A13-006=ADOPTION_BLOCKED
NEXT_GATE=PREH_TCP124_HOST_CONTRACT_AND_SOURCE_FIRST_IMPLEMENTATION
```

Este documento describe únicamente el próximo contrato de ingeniería; no atribuye una prueba ni una corrección nueva al código productivo.


## Actualización — banco disponible: dos JWPLC Basic (16/oct/2026)

**Topología aceptable:** 2 placas JWPLC Basic conectadas al mismo router y una PC generadora de tráfico. Los escenarios 1/2/4 se refieren a **sesiones TCP cliente simultáneas**, no a cuatro placas. La PC puede generar cuatro conexiones simultáneas hacia uno o ambos PLC; hay que documentar distribución, IPs, servidor/cliente bajo prueba y sentido de cada sesión. Cuatro conexiones hacia un solo dispositivo **no** validan comportamiento entre cuatro dispositivos distintos.

**Gate host añadido:** `tools/alpha13/gates/a13_g10_r_tcp124_contract.py --selftest` y workflow `.github/workflows/alpha13-preh-tcp124-offline.yml`. Fixture puramente sintético: positivos 1/2/4 y nueve negativos (ventanas sin solapamiento, socket duplicado, allocation inválido, FC03 fallido, cierre no confirmado, exceso de ocho sockets, clientes incompletos, client id repetido y owner ausente). No abre COM, tráfico TCP, router ni hardware y **no** declara prueba source-backed del servidor ni firmware. El evaluador exige evidencia explícita de peak concurrente; un resultado del fixture no certifica implementación del generador ni captura física.

**Integración pendiente:** conectar productor de eventos y colector PC al formato del evaluador, definir reservas de sockets a partir de snapshots reales, verificar concurrencia de FC03 y cierres después de mantener sesiones abiertas; incorporar R3/RTU/periféricos al veredicto integral. **No activar** `R3_PHYSICAL_INJECTION_READY` o `R3_COMPLETE_PHYSICAL_CAMPAIGN_READY` por este gate.

```text
G10-R-TCP124=OFFLINE_CONTRACT_COMMITTED_CI_PENDING
G10-R-TCP124-NONPHYSICAL=FIXTURE_ONLY_NOT_SOURCE_BACKED
G10-R-TCP124-PHYSICAL=NOT_RUN
G10-R-BENCH=UP_TO_2_JWPLC_PLUS_PC_AND_ROUTER
A13-006=ADOPTION_BLOCKED
NEXT_GATE=TCP124_EVENT_PRODUCER_AND_CAPTURE_SOURCE_INTEGRATION
```


## Implementación host TCP con sockets reales, exclusivamente loopback (17/oct/2026)

- `tools/alpha13/gates/a13_g10_r_tcp124_host.py` ejecuta 1/2/4 clientes TCP usando sockets reales **127.0.0.1**, un servidor FC03 sintético que responde a cada transacción y una barrera de inicio. La prueba espera la recepción de respuesta correcta de todos los clientes antes de liberar conexiones; registra intervalos monotónicos de socket abierto y calcula `active_peak`.
- Se añadió al workflow dirigido Windows/Ubuntu. Este test verifica simultaneidad de sockets host, **no** la librería Ethernet del package, no sesiones TCP del W5500, no trazas de lease W5500 y no genera tráfico físico.
- Los tests antiguos de contrato host son sintéticos; el nuevo selftest ejercita sockets reales de loopback, pero todavía **NO** sustituye la integración source-backed del generador/collector de PC con identidades observables del candidato.
- Se verifica el CI del HEAD exacto, no se transfieren automáticamente PASS anteriores. No integrar en la rama Alpha15 ni habilitar hardware.

```text
TCP124_LOOPBACK_HOST=IMPLEMENTED_CI_PENDING
TCP124_SOURCE_BACKED_W5500=NOT_IMPLEMENTED
TCP124_PHYSICAL=NOT_RUN
G10-R-R3_PHYSICAL_INJECTION_READY=FALSE
G10-R-UPLOAD=BLOCKED
NEXT_GATE=TCP124_HOST_CI_AND_LEASE_CORRELATION_DESIGN
```


## PRE-H revisión fuente y CI de loopback (17/oct/2026)

La corrida [#39006047197](https://github.com/JW-Control/platform-jwplc/actions/runs/39006047197) **NO fue PASS**: Ubuntu PASS, Windows FAIL. La falla Windows documenta una sesión con marcas de inicio y fin iguales en la resolución observada (`NOT_SIMULTANEOUS`, target 1). Se corrigió el arnés en `29cf46e8` introduciendo una ventana de 75 ms **después de completar las respuestas FC03 y antes de liberar** las sesiones; no se alteró el criterio de solapamiento ni se inventaron timestamps. La CI de ese commit debe consultarse antes de declarar PASS.

Inspección adicional de `tools/alpha13/sketches/g10_r_integrated/g10_r_integrated.ino`: el banco posee servidor `JWPLC_ModbusTCP.beginServer(1,502)`, pero **no emite identificador por transacción FC03 vinculado con SID/EPOCH/ALLOC/OWNER de cada sesión TCP del W5500**. Las respuestas PC demuestran tráfico y simultaneidad del HOST cuando haya medición real; los snapshots W5500 son observaciones de estado y no correlación causal por transacción. No inferir `source-backed TCP124` a partir de un fixture o cuatro sockets PC exitosos.

**NEXT_GATE:** (a) CI Windows+Ubuntu de loopback con HEAD exacto; (b) instrumentación temporal por sesión de Modbus TCP que relacione transacción con identidad de lease y cierre real sin tocar package producto, con negativos de ambigüedad/reciclado; (c) 1/2/4 clientes PC concurrentes y fairness/TCA, DHCP, luego PRE-H de campaña integral. No cambiar `R3_PHYSICAL_INJECTION_READY=False` ni `R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=False`; Alpha15 no se toca.

```text
G10-R-TCP124-HOST-PRIOR_CI=FAIL_WINDOWS_RUN_39006047197
G10-R-TCP124-HOST-FIX=COMMITTED_29cf46e8_CI_PENDING
G10-R-TCP124-LEASE_TRANSACTION_CORRELATION=NOT_IMPLEMENTED
G10-R-H=BLOCKED
A13-006=ADOPTION_BLOCKED
```


## PRE-H — límite de concurrencia del servidor existente (17/oct/2026)

Inspección **source-backed** de `JWPLC_ModbusTCP.cpp::serviceServer()`: el servidor conserva un solo `_client`/`_clientActive`; solo cuando no hay cliente activo obtiene `EthernetClient candidate = _server.available()`. Por ello es incorrecto equiparar **cuatro conexiones TCP abiertas en red** con **cuatro sesiones Modbus TCP atendidas simultáneamente** por el runtime actual.

Separar métricas y veredictos:
1. `HOST_TCP_OPEN_PEAK=1/2/4`: simultaneidad de conexiones de la PC verificada por tiempos de socket host, nunca evidencia de W5500.
2. `PLC_TCP_ACCEPTED/ACTIVE`: observar snapshots y eventos propios de firmware sin atribuir a priori cuatro consumidores Modbus paralelos. La política de servicio de un único cliente puede introducir espera o timeouts legítimos para conexiones adicionales.
3. `FC03_COMPLETION`: rendimiento correcto para clientes de acuerdo con la política vigente, **sin** inventar paralelismo del servidor. El loopback host con servidor sintético prueba el arnés, no el diseño del servidor del package.
4. `W5500_LEASE`: correlación por propietario/epoch/allocation/transaction real y cierre; no deducirla a partir del puerto local ni del transaction ID MBAP.

El objetivo PRE-H de 4 clientes conserva su utilidad como **stress de apertura y cierre de sockets**, pero la aceptación de cuatro FC03 simultáneamente servidos queda `NOT_REQUIRED_UNTIL_SERVER_MULTI_CLIENT_API_DESIGN`. No ampliar número de clientes activos en producto sin análisis de impacto/API y decisión explícita. La prueba física seguirá requiriendo banco autorizado.

[CI corregida del arnés](https://github.com/JW-Control/platform-jwplc/actions/runs/39007148836): 2/2 SUCCESS Windows y Ubuntu para commit `29cf46e841dff729284dc9246a1d6dca7cd923f4` (contrato sintético + loopback host). No atribuir a hardware.

```text
G10-R-TCP124-HOST_LOOPBACK_CI=PASS_2_OF_2_RUN_39007148836
G10-R-TCP124-SERVER_CONCURRENCY=ONE_ACTIVE_CLIENT_SOURCE_OBSERVED
G10-R-TCP124-FOUR_MODBUS_PARALLEL=NOT_DEMONSTRATED_NOT_REQUIRED
G10-R-TCP124-W5500_TRANSACTION_LEASE=NOT_INSTRUMENTED
G10-R-H=BLOCKED
NEXT_GATE=PREH_TCP124_SOURCE_OBSERVABLE_AND_HOST_PLC_STRESS_DESIGN
```


## Contrato de procedencia fail-closed (17/oct/2026)

Se incorporó `a13_g10_r_tcp124_provenance.py` y su ejecución en el workflow PRE-H TCP124. Sólo usa fixtures offline; exige `G10R_UART_LAB`, declaración de fuente física (dato **no suficiente como attestation**), ausencia de pérdida de trace, binding explícito de transacción/lease UART, y evento `QueueRequest` anterior a `DrainClose` para la misma tupla SID/EPOCH/ALLOC/OWNER. Once alteraciones negativas se rechazan. **No** extrae `uart_binding_verified` de fuente real, por lo que un JSON aportado arbitrariamente puede afirmarlo y el evaluador no debe usarse como certificación física. El próximo integrador debe derivarlo de UART real/procedencia reproducible y no aceptar el campo como testimonio independiente.

El servidor productivo sigue con cliente activo único. Se mantienen diferenciados peak de sockets PC, atención Modbus y pruebas de W5500. No se han habilitado flash/live ni transferido nada a Alpha15.

```text
TCP124_PROVENANCE=OFFLINE_FIXTURE_ONLY_CI_PENDING
TCP124_BINDING_REAL_UART=NOT_IMPLEMENTED
TCP124_PHYSICAL=NOT_RUN
G10-R-H=BLOCKED
A13-006=ADOPTION_BLOCKED
NEXT_GATE=PREH_REAL_UART_TRANSACTION_BINDING_AND_PERIPHERAL_FAIRNESS
```

## Contador PRE-H de cuatro bloques — 17/oct/2026

**Meta:** banco con máximo dos JWPLC Basic + una PC + router. Los cuatro clientes TCP son conexiones de la PC, NO cuatro placas. Ni el servidor Modbus single-client ni el W5500 se certifican por el test host.

| Bloque | Criterio para cerrarlo | Estado |
|---|---|---|
| 1 TCP y procedencia | Fuente UART real correlacionable por transacción a SID/EPOCH/ALLOC/OWNER, capturador PC 1/2/4 y negativos contra reciclaje, sin afirmaciones por JSON autorreportado | ABIERTO |
| 2 DHCP y periferia | Recover/link, renew/rebind, progreso y fairness TFT/TCA/SD/FRAM/RTC/RTU observables, sin GPIO4 ni cargas | ABIERTO |
| 3 N0/N1/N2/N3 final | Gates dirigidos más full platform del HEAD integrado, diferencias de fuentes y precompilados, artifacts y ausencia de skipped | ABIERTO |
| 4 PRE-H seguridad | Campaign runner R1→R2→R3→R4 sin huecos; backup/restauración; banco sin actuadores; aprobación por operador y dos autorizaciones separadas | ABIERTO |

Nuevo `tools/alpha13/gates/a13_g10_r_tcp124_router.py`: estructura de cliente PC para holding registers FC03, ventana de sockets 1/2/4, intención de estresar serialización single-client y registro de latencias; CLI `--live` **rechaza siempre** hasta completar contrato/banco, incluso con parámetros válidos. `--selftest` no abre red ni COM. No confundir el código preparatorio con ensayo físico ni con aprobación de PRE-H.

```text
PREH_STEPS_REMAINING=4
PREH_STEP_1=PARTIAL_HOST_ROUTER_GENERATOR_NOT_ENABLED
PREH_STEP_2=OPEN
PREH_STEP_3=OPEN
PREH_STEP_4=OPEN
R3_PHYSICAL_INJECTION_READY=FALSE
R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=FALSE
G10-R-H=BLOCKED
A13-006=ADOPTION_BLOCKED
```


## Revisión de fidelidad PC/W5500 (17/oct, iteración posterior)

Se corrigió `a13_g10_r_tcp124_router.py` para nombrar `pc_socket_closed` únicamente cuando se cierra el socket host y `w5500_close=UNKNOWN`. El nombre anterior `close_confirmed` inducía a considerar un cierre PC como prueba de liberación física W5500. Se mantiene `--live` deshabilitado. La prueba N0 dirigida Windows/Ubuntu deberá verificarse para el SHA de cambio, sin adjudicar PASS anticipado.

**Bloque TCP (1/4) sigue abierto.** Su criterio de cierre requiere vinculación instrumentada source-backed desde firmware temporal y capture PC/UART por transacción, además de negativos y CI. No se rebaja el criterio para acelerar la campaña.

```text
PREH_STEPS_REMAINING=4
TCP124_HOST_CLOSE=ONLY_PC_SOCKET
TCP124_W5500_CLOSE=UNKNOWN
TCP124_UART_BINDING=NOT_IMPLEMENTED
R3_PHYSICAL_INJECTION_READY=FALSE
G10-R-H=BLOCKED
```
