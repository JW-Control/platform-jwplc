# Alpha14 - Cierre estricto TCP1000 + RTU100 — 2026-10-03

## Resultado

PASS_STRICT_CONFIRMED.

Se confirmó durante 600 s la coexistencia:

- Modbus TCP: target 1000 req/s.
- Modbus RTU: target 100 req/s.
- RTU workload: 2DI + 2DO + 2AI + 2AO.
- Full runtime activo: Display + SD + FRAM + RTC + I/O + botonera.

## Resultado medido

| Métrica | Resultado |
| --- | ---: |
| TCP target | 1000 req/s |
| TCP alcanzado | 1000.000561 req/s |
| TCP target | 100.000056 % |
| TCP requests OK | 600001 |
| TCP avg | 818.511 us |
| TCP P95 | 1199.3 us |
| TCP P99 | 3408.7 us |
| TCP max | 16589.2 us |
| RTU target | 100 req/s |
| RTU alcanzado | 100.000500 req/s |
| RTU target | 100.000500 % |
| RTU requests success | 60024 |
| RTU scans | 7503 |
| RTU scan rate | 12.500062 scans/s |
| RTU periods skipped | 0 |
| RTU transaction max | 11971 us |

## Integridad

Master:

- REQUESTS_OK=600001
- FRAME_TIMEOUTS=0
- BUS_LOCK_TIMEOUTS=0
- PROTOCOL_ERRORS=0
- RTU_REQUESTS_STARTED=60024
- RTU_REQUESTS_COMPLETED=60024
- RTU_REQUESTS_SUCCESS=60024
- RTU_REQUESTS_FAILED=0
- RTU_REQUESTS_REJECTED=0
- RTU_VERIFY_FAILS=0
- RTU_CRC_ERRORS=0
- RTU_MASTER_TIMEOUTS=0
- RTU_PERIODS_SKIPPED=0
- PERIPHERAL_FAILURE_COUNT=0
- SPI_PROBE_FAILS=0

Distribución EXP-MIX:

- DI success: 15006
- DO success: 15006
- AI success: 15006
- AO success: 15006
- fallos DI/DO/AI/AO: 0

Slave:

- RTU_RX_FRAMES=60024
- RTU_TX_FRAMES=60024
- RTU_REQUESTS_OK=60024
- RTU_CRC_ERRORS=0
- RTU_EXCEPTIONS_SENT=0
- RTU_SERVER_DISCARDED_TAILS=0
- RTU_SERVER_DISCARDED_BYTES=0

## Runtime/periféricos

Durante el cierre:

- FULL_RUNTIME_READY=YES
- SERVER_READY=YES
- ETH_READY=YES
- ETH_LINK=UP
- DISPLAY_READY=YES
- FRAM_READY=YES
- SD_READY=YES
- SD_DATALOG_FAILED_COMMITS=0
- RTC_PRESENT=YES
- IO_INITIALIZED=YES
- BUTTONS_READY=YES
- no reset=true

## Criterio estricto

Todos los requisitos quedaron satisfechos:

- TCP >= 99.9 % del target: PASS.
- RTU >= 99 % del target: PASS.
- RTU skipped = 0: PASS.
- runtime_clean=true: PASS.
- errores TCP=0: PASS.
- errores RTU/CRC/timeout/verify=0: PASS.
- tails Slave=0: PASS.
- fallos periféricos/SPI=0: PASS.
- sin resets inesperados: PASS.

Resultado formal:

`A14_FINAL_TCP1000_RTU100_GATE=PASS_STRICT_CONFIRMED`

## Conclusión

Queda cerrado el frente de coexistencia TCP/RTU para la configuración validada:

`1000 Modbus TCP req/s + 100 Modbus RTU req/s`

La cifra es una capacidad simultánea estrictamente confirmada bajo el workload y configuración documentados; no se debe reinterpretar como ceiling absoluto de TCP unpaced ni como ceiling RTU-only.

El ceiling RTU-only EXP-MIX observado separadamente permanece alrededor de 1346.7 req/s. El TCP unpaced ceiling sigue siendo sensible al host y no se usa para sustituir esta cifra contractual de coexistencia.

## Evidencia local de la corrida

Raíz generada:

`tools/modbus-tcp-benchmark/results/a14_tcp1000_rtu100_strict_confirm_20261003_103435/`

Archivos principales:

- FINAL_SUMMARY.json
- FINAL_STATUS.txt
- GATE_STATUS.txt
- STRICT_CONFIRMATION/FINAL_CONFIRM_TCP1000_RTU100/result.json
- master/slave pre y final snapshots
- master/slave boot y fast-profile snapshots
- runner.log
- SESSION.log
- MANIFEST.txt
