# Alpha14.3 — HMI Dirty / On-Demand — Strong PASS físico

Fecha: 2026-09-13

Rama: `v2.1.0-alpha.14/feature/modbus-tcp`

## Objetivo

Validar si el cuello de rendimiento observado en el perfil `FULL_RUNTIME_REALISTIC` provenía de la TFT como periférico o del patrón legacy/manual de redibujado periódico.

Se reemplazó temporalmente el dibujo manual completo a 100 ms por la API declarativa HMI integrada en `JWPLC_Display` y usada por el ecosistema JWPLC HMI Designer:

- `JWPLC_Display.setFields(...)`
- `USER_REFRESH_ON_DEMAND`
- `JWPLC_Display.setValue(...)`
- `JWPLC_Display.setText(...)`
- `JWPLC_Display.setBool(...)`
- `JWPLC_Display.setBar(...)`

El workload físico de microSD permaneció deshabilitado durante este gate para aislar únicamente el efecto del camino de render HMI.

## Perfil HMI

- 5 fields HMI.
- I/O alimentado a la API cada 20 ms.
- RTC alimentado a la API cada 100 ms.
- Barra dinámica modificada deliberadamente cada 100 ms para conservar una carga gráfica real de aproximadamente 10 Hz.
- Dirty rendering / on-demand activo.
- Sin override legacy `jwplcUserDisplayRefreshNeededCallback()`.
- Sin callback manual `jwplcUserDisplayRefreshCallback()`.
- Display, FRAM, RTC, botones, TCA/I-O, SPI probe, Ethernet y Modbus TCP activos.
- SD inicializada/autoload normal, archivo abierto, pero append/verify scheduler OFF.

## Compilación y subida

PASS físico.

- Sketch: 436837 bytes (10 %).
- RAM global: 35196 bytes (10 %).
- Verificación de flash: PASS.
- `BUILD_UPLOAD_EXIT=0`.

## Benchmark

FC03 / 125 registros, 1000 req/s solicitados, 2 × 60 s.

### Run 1

- `OK=60000/60000`
- `ACHIEVED_REQ_S=1000.00`
- `ACHIEVED_PCT=100.000`
- P95 = 1260.5 us
- P99 = 1858.1 us
- MAX = 10800.0 us
- LOOP_AVG = 378 us
- LOOP_MAX = 8399 us
- `DISPLAY_FRAMES=587`
- `HMI_IO_PUSH_CYCLES=2987`
- `HMI_RTC_PUSH_CYCLES=600`
- `HMI_BAR_PUSH_CYCLES=600`
- TCP clean: YES
- Runtime ready: YES
- HMI ready: YES
- Peripheral failures: 0

### Run 2

- `OK=60000/60000`
- `ACHIEVED_REQ_S=999.99`
- `ACHIEVED_PCT=99.999`
- P95 = 1282.7 us
- P99 = 1780.0 us
- MAX = 8411.6 us
- LOOP_AVG = 378 us
- LOOP_MAX = 8416 us
- `DISPLAY_FRAMES=588`
- `HMI_IO_PUSH_CYCLES=2989`
- `HMI_RTC_PUSH_CYCLES=600`
- `HMI_BAR_PUSH_CYCLES=600`
- TCP clean: YES
- Runtime ready: YES
- HMI ready: YES
- Peripheral failures: 0

## Resumen

- `AVG_PCT=100.000`
- `MIN_PCT=99.999`
- `MAX_PCT=100.000`
- `SPREAD_PP=0.001`
- `RECOVERY_VS_MANUAL_100MS_PP=10.157`
- `GAP_TO_TFT_OFF_PP=0.000`
- `GAP_TO_TCP_ONLY_PP=-0.008`
- `ALL_DIAGNOSTIC_CONDITIONS_CLEAN=YES`

Clasificación:

`A14_3_HMI_DIRTY_ON_DEMAND=HMI_DIRTY_ON_DEMAND_STRONG_PASS`

Resultado de gate: **PASS_PHYSICAL**.

## Conclusión

La TFT como periférico no es el cuello de botella dominante observado previamente.

El problema estaba en el patrón legacy/manual del harness, que forzaba refresco y redibujaba varias regiones en cada ciclo periódico aun cuando el contenido no cambiaba.

La API declarativa HMI con dirty rendering y `USER_REFRESH_ON_DEMAND` recupera prácticamente todo el rendimiento del baseline TCP-only incluso manteniendo una barra dinámica a ~10 Hz.

Por tanto:

1. No se justifica degradar globalmente la frecuencia de refresh de `JWPLC_Display` a 175/200/250 ms como solución principal.
2. Los resultados 100/150/175/200/250 ms del camino manual se conservan como diagnóstico del coste del redraw bruto.
3. Para el ecosistema JWPLC HMI Designer, el camino de referencia debe ser la API declarativa Dirty/On-Demand.
4. El siguiente gate debe reintroducir el workload SD manteniendo la HMI Dirty/On-Demand.

## Siguiente gate

`REINTRODUCE_SD_WORKLOAD_WITH_HMI_DIRTY`

Se mantendrá el perfil HMI validado y se restaurarán append + flush + verify de microSD para medir el full runtime completo con el renderer representativo del ecosistema actual.
