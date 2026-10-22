# Alpha13 G10-R — PRE-H Bloque 2: DHCP y equidad de periféricos

**Estado:** `CLOSED_NONPHYSICAL_SOURCE_FIRST` (17/oct/2026). No significa ensayo físico aprobado ni adopción al package.  
**Implementación:** `codex/alpha13-preh-tcp124-review-20261016`, código B2 hasta `53546f7e93831f675012b7f0c0d3ffd6069ddc6a`.

## Propósito y alcance

Instrumentar lo imprescindible para que **una sola campaña física R1→R2→R3→R4** permita demostrar recuperación Ethernet/DHCP y ausencia de inanición de periféricos **sin eliminar autoload**, cambiar API de usuario, GPIO4, archives o firmware productivo. El banco dispone de **dos JWPLC Basic, una PC y un router**. Las operaciones físicas siguen bloqueadas y requieren autorización separada.

## Fuente-first y criterios observables

1. **DHCP real:** `a13_g10_r_b2_dhcp_stage.py` añade hook C weak **solo al staging** de `JWPLC_Ethernet.cpp`, tras `Ethernet.maintainAsync()`, `Ethernet.dhcpMaintenanceInProgress()` y `releaseBus()`. No fuerza negociación ni reconfigura DHCP. El callback del sketch incrementa contadores atómicos: `DHCP_TICKS/PEND`, `DHCP_RENEW_OK/FAIL`, `DHCP_REBIND_OK/FAIL`. Los códigos 1/2/3/4 se mantienen diferenciados. También conserva `DHCP_STATE`, `DHCP_ERR`, IP, PHY link y uptime.
2. **TFT real:** el sketch activa el **perfilador H3E1 preexistente** exclusivamente en diagnóstico; registra `TFT_DRAW` mediante `release_bus.calls` (refrescos TFT con liberación del bus), `TFT_FAIL` por intentos fallidos, `TFT_MAX_US`, `TFT_SPI_WAIT_MAX_US` y `TFT_READY`. No implementa APIs nuevas HMI/TFT. El propio profiler genera carga adicional medible; no confundir tiempos instrumentados con mediciones de firmware productivo.
3. **TCA/I/O y RTC:** `JWPLC_IO.ready()/lastScanMs()` y `JWPLC_Time.lastUpdateMs()` son vistas **cacheadas** del runtime; se publican `IO_READY/IO_AGE_MS/RTC_AGE_MS` sin transacciones extra. La edad del escaneo evidencia progreso del runtime, **no mide independientemente cada chip TCA/I2C**.
4. **SD, FRAM, RTU, UDP y botonera:** se conservan contadores de lecturas y comunicaciones del sketch de banco; no se escribe SD/FRAM, no se accionan relés y no se asume que eventos de teclas deban ocurrir espontáneamente.
5. **Validador B2:** `a13_g10_r_b2_contract.py` rechaza pérdida de heartbeat, cambio de BOOT, retroceso de contadores DHCP/TFT globales, IO/RTC congelados, TFT sin progreso, caída/recovery R2 sin observación, periféricos no listos o métricas omitidas. Los contadores SD/FRAM/RTU se comparan **dentro de cada (fase, token)**; R3 crea varias sesiones y reinicia esos contadores en forma legítima. Retorna `REVIEW_STOP` ante evidencia insuficiente.
6. **Runner:** el evaluador B2 se incorpora al `MANIFEST.json` de la única campaña y puede abortar la aceptación. No altera parámetros de campo, salidas Q, DHCP real ni umbrales de reintento del runtime.

## Evidencia y límites

- [B2 CI Windows/Ubuntu #39014708958](https://github.com/JW-Control/platform-jwplc/actions/runs/39014708958): **2/2 SUCCESS**, evalúa estado y negativos para el SHA de código indicado.
- [Contratos TCP124 integrados #39014708863](https://github.com/JW-Control/platform-jwplc/actions/runs/39014708863): **2/2 SUCCESS**, reuse de bloque 1 en el mismo HEAD.
- [Compilación source-first #39014564729](https://github.com/JW-Control/platform-jwplc/actions/runs/39014564729): **2/2 SUCCESS** Windows/Ubuntu con el **mismo sketch y composición staged DHCP/H3E1** del HEAD B2; los cambios posteriores al código fuente corresponden al parser y al evaluador. CI source-first de HEAD `53546f7e`: [#39014708876](https://github.com/JW-Control/platform-jwplc/actions/runs/39014708876) (verificar conclusión final por separado).
- La herramienta de build comprueba `JWPLC_Ethernet.cpp` y `JWPLC_ModbusTCP.cpp` en sus archivos `.d/.o`, además del overlay W5500. No se han recompilado archives congelados ni modificado `JWPLC/2.1.0`.
- El CI histórico N0/G10-R usa una lista fija anterior a PRE-H; el rechazo de rutas nuevas no se atribuye a funcionalidad Ethernet. Debe reconciliarse en B3 antes de dar por verde el conjunto.

**No se efectuó en esta fase** un DHCP renew/rebind real con el router, un corte de enlace físico, una prueba de latencia TFT en placa ni pruebas de larga duración. Los contadores permitirán observarlo en H. Si el período de lease del router no produce renovación/rebind durante la ventana de campaña, registrar `NOT_OBSERVED` y no declarar esa transición validada; planificar ventana adicional justificada.

## Estado operativo

```text
PREH_BLOCK_1_TCP124=CLOSED_NONPHYSICAL_SOURCE_FIRST
PREH_BLOCK_2_DHCP_FAIRNESS=CLOSED_NONPHYSICAL_SOURCE_FIRST
PREH_BLOCK_3_INTEGRATED_N0_N1_N2_N3=OPEN
PREH_BLOCK_4_PHYSICAL_SAFETY_BACKUP_AUTH=OPEN
PREH_STEPS_REMAINING=2
G10-R-DHCP_RENEW_REBIND_PHYSICAL=NOT_RUN
G10-R-TFT_TCA_FAIRNESS_PHYSICAL=NOT_RUN
G10-R-H=BLOCKED
A13-006=ADOPTION_BLOCKED
R3_PHYSICAL_INJECTION_READY=FALSE
R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=FALSE
NEXT_GATE=PREH_BLOCK_3_INTEGRATED_N0_N1_N2_N3
```
