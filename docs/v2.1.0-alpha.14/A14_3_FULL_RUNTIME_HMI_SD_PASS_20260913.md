# Alpha14.3 — Full runtime con HMI Dirty/On-Demand + SD full workload

Fecha: 2026-09-13

## Clasificación

**PASS_PHYSICAL**

`A14_3_FULL_RUNTIME_HMI_SD=FULL_RUNTIME_HMI_SD_PASS`

## Objetivo

Validar el rendimiento del JWPLC Basic a 1000 req/s con el runtime completo y la ruta HMI representativa del ecosistema JWPLC HMI Designer:

- Modbus TCP Server FC03 / 125 registros.
- HMI declarativa con Dirty/On-Demand.
- Barra dinámica cada 100 ms (~10 Hz).
- Alimentación de I/O a 20 ms mediante `setValue()`.
- Alimentación RTC a 100 ms mediante `setText()`.
- FRAM activa.
- RTC activo.
- Botonera activa.
- TCA / I-O activo.
- SPI probe activo.
- microSD con workload completo:
  - append cada 1 s;
  - flush cada 5 registros;
  - verify cada 5 s.

## Configuración de prueba

- Target: `192.168.0.31:502`
- Serial: `COM14`
- FC03 quantity: 125 registros
- Rate solicitado: 1000 req/s
- Duración: 2 x 60 s
- Display: `HMI_DIRTY_ON_DEMAND`
- Display reference period: 100 ms
- SD full workload: ON

## Resultados

### Run 1

- Achieved: **974.68 req/s**
- Achieved: **97.468 %**
- P95: 1346.2 us
- P99: 2420.4 us
- Max: 21959.5 us
- Loop avg: 458 us
- Loop max: 21169 us
- TCP clean: YES
- Runtime ready: YES
- HMI ready: YES
- Display frames: 587
- Display gap max: 200 ms
- HMI I/O pushes: 2982
- HMI RTC pushes: 600
- HMI bar pushes: 600
- SD append cycles: 60
- SD append failures: 0
- SD append max: 4313 us
- SD flush cycles: 12
- SD verify cycles: 12
- SD verify failures: 0
- SD verify max: 20373 us
- Peripheral failure count: 0
- Diagnostic clean: YES

### Run 2

- Achieved: **973.39 req/s**
- Achieved: **97.339 %**
- P95: 1351.7 us
- P99: 2523.6 us
- Max: 21500.8 us
- Loop avg: 458 us
- Loop max: 20871 us
- TCP clean: YES
- Runtime ready: YES
- HMI ready: YES
- Display frames: 589
- Display gap max: 202 ms
- HMI I/O pushes: 2981
- HMI RTC pushes: 600
- HMI bar pushes: 600
- SD append cycles: 60
- SD append failures: 0
- SD append max: 6303 us
- SD flush cycles: 12
- SD verify cycles: 12
- SD verify failures: 0
- SD verify max: 20080 us
- Peripheral failure count: 0
- Diagnostic clean: YES

## Resumen

- Average: **97.403 %**
- Minimum: **97.339 %**
- Maximum: **97.468 %**
- Spread: **0.130 pp**
- Coste del workload SD frente a HMI Dirty/On-Demand con SD OFF: **2.597 pp**
- Todas las condiciones diagnósticas: CLEAN

## Conclusiones

1. La HMI declarativa Dirty/On-Demand validada en D0k elimina la penalización material observada con el redraw manual periódico.
2. Al reactivar el workload SD completo, el sistema mantiene más del 97 % del objetivo de 1000 req/s en ambas corridas.
3. No se observaron errores TCP, timeouts, errores de protocolo, bus lock timeouts ni fallos periféricos.
4. El coste restante se asocia principalmente al workload físico SD, especialmente a las verificaciones con máximos cercanos a 20 ms.
5. El resultado es suficientemente estable para avanzar a un soak prolongado del mismo perfil antes de considerar cerrada la qualification de A14.3.

## Siguiente gate

**Soak prolongado 30 min, sin recompilar ni subir firmware**, usando exactamente la variante actualmente cargada:

`FULL_RUNTIME_HMI_DIRTY_SD_FULL`

Criterio principal:

- Achieved >= 95 %.
- TCP clean.
- HMI ready.
- Runtime completo ready.
- SD append/flush/verify activos y sin fallos.
- Peripheral failure count = 0.
