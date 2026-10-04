# Alpha14.3 — Confirmación de estabilidad TFT a 200 ms

Fecha: 2026-09-13

## Objetivo

Confirmar si el resultado previo de TFT a 200 ms era estable o si la variación observada podía indicar operación demasiado cercana al límite.

Este gate reutiliza el firmware temporal ya cargado con:

- full runtime activo;
- Display inicializado;
- refresco dinámico TFT a 200 ms (5 Hz);
- SD inicializada y disponible;
- workload periódico SD append/verify desactivado;
- FRAM, RTC, TCA/I/O, botonera, SPI probe, Ethernet y Modbus TCP activos.

No se recompiló ni subió firmware en este gate.

## Procedimiento

Prueba PC-only:

- FC03;
- 125 registros;
- 1000 req/s solicitados;
- 3 corridas de 60 s;
- 2 s de pausa entre corridas.

## Resultados

| Métrica | Run 1 | Run 2 | Run 3 |
|---|---:|---:|---:|
| Throughput | 99.793 % | 98.399 % | 99.272 % |
| Req/s | 997.93 | 983.99 | 992.72 |
| P95 | 1246.1 us | 1268.6 us | 1263.3 us |
| P99 | 1478.6 us | 1474.4 us | 1461.5 us |
| Máximo | 18571.6 us | 18856.1 us | 18696.6 us |
| Loop avg | 369 us | 356 us | 364 us |
| Loop max | 18347 us | 18441 us | 18362 us |
| Display frames | 298 | 298 | 298 |
| Display gap max | 206 ms | 205 ms | 206 ms |
| TCP limpio | YES | YES | YES |
| Runtime ready | YES | YES | YES |
| SD append cycles | 0 | 0 | 0 |
| SD verify cycles | 0 | 0 | 0 |
| Peripheral failures | 0 | 0 | 0 |

Resumen:

```text
AVG_PCT=99.155
MEDIAN_PCT=99.272
MIN_PCT=98.399
MAX_PCT=99.793
SPREAD_PP=1.394
ALL_RUNS_GTE95=YES
ALL_DIAGNOSTIC_CONDITIONS_CLEAN=YES
```

## Clasificación

```text
A14_3_TFT_200MS_REPEAT=TFT_200MS_STABILITY_CONFIRMED
RESULT=PASS_DIAGNOSTIC
NEXT=TEST_TFT_175MS
```

## Conclusión

El periodo de refresco dinámico TFT de 200 ms queda confirmado como estable para el escenario full runtime sin workload periódico SD.

Frente al resultado inicial de 200 ms con dos corridas, la repetición de tres corridas muestra:

- las tres corridas por encima de 95 %;
- mínimo de 98.399 %;
- promedio de 99.155 %;
- spread de 1.394 pp;
- sin errores TCP;
- sin fallos de periféricos;
- cadencia TFT correcta y consistente (~5 Hz).

Por tanto, 200 ms deja de ser sólo un punto viable y pasa a ser un candidato estable de refresco periódico.

Aún no se fija como valor final. El siguiente gate reduce el periodo a 175 ms para buscar el menor periodo que conserve margen suficiente sin sacrificar rendimiento Modbus TCP.

## Estado de seguridad

- No se modificó la API pública.
- No se cambió el autoload de periféricos.
- No se tocó JW_SD.
- No se tocó JWPLC_Ethernet.
- No se tocó JW_FRAM.
- No se modificó `tools/mblock-poc/`.
- No hubo commit de source code en este gate.
