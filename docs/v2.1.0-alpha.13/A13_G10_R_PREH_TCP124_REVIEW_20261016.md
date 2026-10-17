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
