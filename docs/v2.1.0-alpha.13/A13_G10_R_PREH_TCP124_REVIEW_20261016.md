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
