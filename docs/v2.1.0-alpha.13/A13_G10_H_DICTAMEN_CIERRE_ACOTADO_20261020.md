# Alpha13 — Dictamen de integración G10 con cobertura física abreviada

**Fecha:** 20/10/2026 (Perú). **Estado:** ACEPTACIÓN FUNCIONAL PARA REVISIÓN DE MERGE, no equivale a cierre total ni publicación de Alpha13. **PR:** #124, candidata `codex/alpha13-g10-promote-functional-20261019`. **Código efectivamente probado/flasheado:** fuentes promovidas `ee27c73d97ddc00a70b3b11664caced73cd983ae`, verificación del firmware HEAD `7c1974c659be4aa88c877a849db6efc32673da60`; posteriores commits solo documentan/harness.

## Resultado físico sin repetir ensayos

| Gate | Evidencia real | Dictamen estricto |
| --- | --- | --- |
| PH0 | COM4/COM14, UART roles/BOOT, RTU maestro-esclavo real, DHCP y FC03 TCP 2/2 | PASS funcional |
| R1, 605 s | 589 solicitudes FC03 correctas COM4, 589 COM14, 589 UDP echo COM4; RTU, SD, FRAM, RTC, IO, TFT avanzando en UART posterior; sin reinicios observados | PASS con UART posventana |
| R2, 3 ciclos red | Pérdida TCP 3/3 y recuperación TCP + UDP en 1,69/10,05/6,03 s (límite 96 s) con COM14 respondiendo | PASS red 3/3 |
| R2 complemento UART | **Archivo operador** `Texto pegado(20261020-194542).txt`, SHA-256 `439f1f6ef6cf025ab77d3bccf4e52263e3274f5c4e51e76b00f4022a0da49cf0`. 73 HB consecutivos 4748–4820, mismo BOOT `FEEDC504`; dos ciclos físicos observados: PHY 1→2 HB4763; 2→1 HB4778; IP/ETH/TCP READY HB4782; segundo PHY 1→2 HB4792, 2→1 HB4808, READY HB4814. RTU_OK 5779→5868; RTU_FAIL 40 constante; SD_OK/FRAM_READ 6330→6426, SD_FAIL 0, RTC/IO/TFT=1 | PASS recuperación física PHY + servicios, sin adjudicar esta UART a los tres ciclos originales |
| R3 — ciclo externo | El cliente R1 crea **una conexión TCP nueva por cada FC03** bajo `socket.create_connection(...)` y `with`, para COM4/COM14 cada una de las 589 rondas. Cierre al salir de `with` ejercita churning **secuencial** de conexión/cierre y posterior aceptación. Repetir 90 s secuenciales no añadiría cobertura de modo relevante. | COBERTURA EXTERNA POR REUSO R1; R3 INTERNO NO PROBADO |
| R4 — resistencia | 605 s de tráfico combinado R1 y continuidad boot/hb posterior, sin observar WDT. **No se ejecutó** ventana canónica R4 de 1805 s, heap/free sockets ni inanición SPI instrumental; no atribuir PASS a esa ventana. | R4 CANÓNICO NO EJECUTADO, riesgo residual aceptado para revisión rápida |

## Criterio de cierre abreviado

El operador indicó explícitamente que no desea esperas prolongadas adicionales y desea integrar mejoras Ethernet y cerrar Alpha13. Se considera aceptado **reutilizar R1** como smoke sostenido y evidencia de rotación secuencial de sesiones; se propone **dispensar la ventana R4 de 1805 s** con excepción rastreable, sin falsear `R4=PASS`. El gate físico G10-H queda **PASS FUNCIONAL CON RIESGOS RESIDUALES DECLARADOS** para la revisión de integración, no `PASS_R1_R2_R3_R4_INTEGRALES`.

**Cobertura explícitamente pendiente / riesgos aceptables para integrar:** mutex `BUS_BUSY` real forzado, lease stale/recycle, close diferido concurrente, cancel TCP/DNS/DHCP, resolución DNS real, DHCP renewal/rebind, sockets/heap/latencias tras 30 minutos de tráfico continuo. N1 fuente nativa cubre escenarios de concurrencia de forma no física, N2 ambos FQBN y N3 11/11 PASS del package, sin sustituir fault injection física. G10-E1 `PENDING_GDB_UNAVAILABLE`, G10-E2 `PENDING_SDKCONFIG_NOT_LOCATED`; no declararlos PASS ni convertirlos en falsos bloqueos de seguridad.

**F1 W5500 protegido:** `JWPLC_ETH_CS=5` y guarda GPIO6–11 permanecen en `w5100.cpp` del package de base; no promover `h0_w5100_stage` experimental, GPIO4, reset automatizado ni instrumentación RTC/headers `friend`. Inventario de promoción **10 MODIFIED + 2 ADDED** en `JWPLC_Ethernet` y `JWPLC_ModbusTCP`; sin alterar autoload, API existente, precompilados `.a`, FlashFreq, bootloader, OpenPLC ni HMI.

## Decisiones finales pendientes

1. PR #124: comprobar HEAD limpio, N0/N1/N2/N3 aplicables al código y CI de los commits; propietario determina merge. **La aprobación de pruebas no equivale a afirmar publicación del package**.
2. Integrar en `v2.1.0-alpha.13/feature/cleanup-robustness`, actualizar estado vivo de la base **después del merge** y confirmar que `PENDIENTE_DE_PROMOCION` de G10 queda vacío. PR #123 es antecedente Draft y se revisará si debe cerrarse como supersedido.
3. Revisar los demás pendientes de Alpha13 (no solo G10), sincronizar conocimiento a `jwplc-dev-mcp/knowledge/main` mediante PR documental, preparar checklist/PR/PreRelease en español y solamente entonces declarar Alpha13 `CLOSED` / publicar.
4. Las pruebas no físicas adicionales no se repiten si siguen idénticas las 12 fuentes, core, flags y archives. Si la base/integración cambia cualquiera, reevaluar gates afectados.

```text
G10_PH0=PASS_FUNCTIONAL_PHYSICAL
G10_R1=PASS_WITH_POST_WINDOW_UART
G10_R2_NETWORK=PASS_3_OF_3
G10_R2_UART_SUPPLEMENT=PASS_TWO_PHYSICAL_LINK_RECOVERIES
G10_R3_EXTERNAL=EVIDENCE_REUSED_FROM_R1_589_TCP_SESSIONS_PER_PLC
G10_R3_INTERNAL=NOT_EXECUTED_ACCEPTED_RISK
G10_R4_1805S=NOT_EXECUTED_ACCEPTED_SHORTENING
G10_H=ACCEPTED_FUNCTIONAL_WITH_DISCLOSED_COVERAGE_GAPS
G10_E1=PENDING_GDB_UNAVAILABLE
G10_E2=PENDING_SDKCONFIG_NOT_LOCATED
NEXT_FAILURE_ID=F136
BASE_SYNC=PENDING_UNTIL_PR124_MERGE
KNOWLEDGE_SYNC=PENDING_PRIVATE_DOC_PR
NEXT_GATE=PR124_MERGE_READINESS_AND_ALPHA13_BASE_SYNC
```
