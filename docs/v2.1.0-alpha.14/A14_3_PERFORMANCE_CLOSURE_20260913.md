# Alpha14.3 — Cierre de Performance

Fecha: 2026-09-13

## Clasificación

**PASS_PHYSICAL**

A14.3 Performance queda cerrada tras validar Modbus TCP FC03/125 a 1000 req/s con runtime completo representativo y periféricos activos.

## Hallazgo principal

La caída de rendimiento observada inicialmente no correspondía al periférico TFT en sí, sino al patrón de redraw manual periódico usado por el harness original.

El harness legacy forzaba `jwplcUserDisplayRefreshNeededCallback() = true` y redibujaba manualmente múltiples regiones cada ciclo. Al sustituir ese camino por la API HMI declarativa usada por JWPLC HMI Designer —`setFields()`, `setValue()`, `setText()`, `setBool()`, `setBar()` y `USER_REFRESH_ON_DEMAND`— el rendimiento volvió prácticamente al baseline TCP-only aun manteniendo una barra dinámica cada 100 ms.

## D0k — HMI Dirty / On-Demand, SD workload OFF

Dos corridas de 60 s, FC03/125 @1000 req/s:

- Run 1: 60000/60000, 100.000 %
- Run 2: 60000/60000, 99.999 %
- Promedio: 100.000 %
- Spread: 0.001 pp
- HMI dinámica: ~10 Hz
- HMI fields ready: YES
- TCP errors/timeouts/bus-lock: 0
- Peripheral failures: 0

Resultado: `HMI_DIRTY_ON_DEMAND_STRONG_PASS`.

## D0l — HMI Dirty / On-Demand + SD full workload

Dos corridas de 60 s, FC03/125 @1000 req/s:

- Run 1: 97.468 %
- Run 2: 97.339 %
- Promedio: 97.403 %
- Spread: 0.130 pp
- SD append: 60 ciclos/run
- SD flush: 12 ciclos/run
- SD verify: 12 ciclos/run
- SD failures: 0
- Peripheral failures: 0

Resultado: `FULL_RUNTIME_HMI_SD_PASS`.

El coste medido del workload SD frente al perfil HMI con SD workload OFF fue ~2.597 pp en estas corridas cortas.

## D0m — Soak 30 min

El soak ejecutó correctamente la ventana completa de 1800 s. El runner falló únicamente durante el postproceso por intentar leer una clave inexistente `row['ok']`; no hubo fallo del dispositivo ni del protocolo.

Resultado Modbus preservado:

- Requested: 1000 req/s
- Achieved: 999.34 req/s
- Achieved: 99.934 %
- Requests OK: 1,798,804 / 1,800,000
- P95: 1323.7 us
- P99: 2406.6 us
- Max: 27319.1 us
- Timeouts: 0
- Transport errors: 0
- Protocol errors: 0
- Bus-lock timeouts: 0
- Cross-count: PASS
- Formal result: `STABLE_PASS`

## D0m-R — Recuperación del snapshot final

Se leyó únicamente `S`, sin compile, upload ni reset de estadísticas.

Estado recuperado:

- FULL_RUNTIME_READY=YES
- SERVER_READY=YES
- CLIENT_CONNECTED=NO
- DISPLAY_READY=YES
- HMI_FIELDS_READY=YES
- HMI_REFRESH_MODE=ON_DEMAND
- HMI_FIELD_COUNT=5
- DISPLAY_FRAMES=19926
- DISPLAY_GAP_MAX_MS=203
- HMI_IO_PUSH_CYCLES=101116
- HMI_RTC_PUSH_CYCLES=20332
- HMI_BAR_PUSH_CYCLES=20332

### SD

- SD_APPEND_CYCLES=2035
- SD_APPEND_FAILS=0
- SD_APPEND_MAX_US=17087
- SD_FLUSH_CYCLES=407
- SD_VERIFY_CYCLES=407
- SD_VERIFY_FAILS=0
- SD_VERIFY_MAX_US=26039

### FRAM

- FRAM_CYCLES=8139
- FRAM_FAILS=0
- FRAM_MAX_US=7169

### RTC

- RTC_SAMPLES=8138
- RTC_UNAVAILABLE=0
- RTC_STALE=0
- RTC_MAX_AGE_MS=1003

### I/O

- IO_SAMPLES=101020
- IO_STALE=0
- IO_MAX_AGE_MS=28

### Botonera

- BUTTON_SAMPLES=100817
- BUTTON_NOT_READY=0
- BUTTON_SAMPLE_GAP_MAX_MS=124

### SPI

- SPI_PROBE_SAMPLES=20290
- SPI_PROBE_FAILS=0
- SPI_PROBE_OVER_1MS=0
- SPI_PROBE_OVER_10MS=0
- SPI_PROBE_MAX_WAIT_US=144

### Salud general

- PERIPHERAL_FAILURE_COUNT=0
- ALL_RECOVERED_DIAGNOSTICS_CLEAN=YES

Resultado final D0m: `FULL_RUNTIME_30MIN_STRONG_PASS` / `PASS_PHYSICAL`.

## Decisiones

1. No reducir globalmente la TFT a 250 ms como solución de rendimiento.
2. Mantener y favorecer el camino HMI declarativo Dirty/On-Demand del ecosistema JWPLC HMI Designer.
3. Conservar compatibilidad con callbacks legacy, pero no usarlos como referencia de carga HMI optimizada.
4. El workload SD sigue siendo la principal fuente medible de pausas largas dentro del full runtime, sin fallos funcionales.
5. No se elimina ningún periférico del autoload normal.
6. A14.3 queda cerrada y Alpha14 avanza a A14.4: coexistencia Modbus RTU + Modbus TCP.

## Siguiente etapa

**A14.4 — RTU + TCP simultáneo**

Objetivo: demostrar que el JWPLC Basic puede mantener simultáneamente Modbus TCP por Ethernet y Modbus RTU por RS-485, sin degradación funcional ni starvation entre transportes, manteniendo el resto del runtime integrado.
