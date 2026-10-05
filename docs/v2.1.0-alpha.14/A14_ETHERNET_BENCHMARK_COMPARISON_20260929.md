# Alpha14 — comparación de benchmarks Ethernet

Fecha: `2026-09-29`

## Cómo leer las métricas

- **TCP end-to-end** mide bytes recibidos por el JWPLC durante la ventana del
  runner. Incluye scheduler, sondeos y variación del host/red.
- **Payload efectivo SPI** deriva bytes/tiempo dentro de la lectura W5500. Es
  la métrica más estable para cambios en el helper SPI.
- **us/B** es latencia/coste interno; menor es mejor.
- **Hold máximo** mide cuánto tiempo una pasada mantiene el ownership SPI y es
  la métrica principal de fairness con TFT, SD, FRAM, RTC y otros periféricos.

Los porcentajes causales se toman de variantes alternadas dentro del mismo
gate. Las comparaciones históricas entre gates sirven como referencia porque
la instrumentación y la carga no siempre son idénticas.

## Mejoras causales de esta sesión

| Gate | Cambio | Velocidad | Latencia/coste | Seguridad | Decisión |
|---|---|---:|---:|---|---|
| P2 | `memset+transfer` → RX directo | payload interno +1.362% aprox. | no aislada | 0 corrupción reportada | OFF; efecto TCP variable |
| P3 | DIRECT_RX → FIFO_REUSE | payload +5.226% | us/B −4.966% | FNV PASS, 0 errores/resets | confirmar en P3R |
| P3R | DIRECT_RX → FIFO_REUSE | payload +5.141% | us/B −4.889% | FNV PASS, 0 errores/resets | validado, pendiente física |
| P6 | `available()+read()` → `read()` | payload +0.348%; TCP −1.437% | path RX −0.887%; hold +0.306% | FNV PASS, 0 errores/resets | efecto pequeño |
| P7 | commit inmediato → coalescido | payload +0.513%; TCP −29.849% | path RX +11.266%; hold +30.859% | FNV/reconnect PASS | rechazado |
| P8 | dos `connected()` → un resultado por pasada | payload −0.305%; TCP +6.501% | scheduler −4.556%; hold −4.619%; status time −64.431% | FNV/reconnect PASS | ganancia confirmada |
| P9 | batch 8 → 16 | TCP +13.762%; payload +0.176% | hold promedio −8.110%; hold máx. **+84.099%** | FNV/reconnect PASS | rechazado por fairness |
| P9 | batch 8 → 32 | TCP +12.125%; payload +0.119% | hold promedio −7.106%; hold máx. **+252.939%** | FNV/reconnect PASS | rechazado por fairness |

En la columna de latencia, un porcentaje negativo significa reducción. Los
valores positivos de P7 y del hold máximo P9 son empeoramientos.

## Evolución de velocidad RAW TCP RX

| Referencia | TCP RX | Cambio vs NB3-C 13.798245 | Cambio vs H4A0 13.577425 | Comparabilidad |
|---|---:|---:|---:|---|
| NB3-C histórica | 13.798245 Mbps | base | +1.626% | RAW corto, sin profiler nuevo |
| H4A0 post-H3E | 13.577425 Mbps | −1.600% | base | RAW package final anterior |
| P1 BASE | ~13.175 Mbps | −4.517% | −2.964% | profiler nuevo; punto inicial |
| P3 FIFO_REUSE | 13.736151 Mbps | −0.450% | +1.169% | A/B 3×15 s |
| P3R FIFO_REUSE | 12.976148 Mbps | −5.958% | −4.429% | A/B 5 repeticiones; host variable |
| P6 READ_DIRECT | 13.424499 Mbps | −2.709% | −1.126% | A/B 3×15 s |
| P8 SINGLE_STATUS | **14.129575 Mbps** | **+2.401%** | **+4.067%** | ganancia interna confirmada; TCP variable |
| P9 batch 16 experimental | 14.913620 Mbps | +8.083% | +9.841% | no aceptado: spread/fairness FAIL |

El mejor valor defendible de la sesión para el scheduler optimizado es P8:
`14.129575 Mbps`. El valor P9 de `14.913620 Mbps` es un ceiling experimental,
no una configuración recomendada.

## Evolución del payload efectivo y coste por byte

| Punto | Payload efectivo | Coste/latencia asociado | Cambio relevante |
|---|---:|---:|---:|
| P1 profiler inicial | ~15.68 Mbps | payload read dominante | baseline aproximada |
| P3 DIRECT_RX | 15.970 Mbps | 0.500949 us/B | baseline causal P3 |
| P3 FIFO_REUSE | 16.804 Mbps | 0.476072 us/B | +5.226% velocidad; −4.966% coste |
| P3R DIRECT_RX | 15.965 Mbps | 0.501105 us/B | baseline causal P3R |
| P3R FIFO_REUSE | 16.785 Mbps | 0.476604 us/B | +5.141% velocidad; −4.889% coste |
| P8 SINGLE_STATUS | 16.770394 Mbps | scheduler 0.543472 us/B | −4.556% scheduler dentro de P8 |
| P9 batch 8 | 16.804584 Mbps | hold 0.576864 us/B | configuración conservada |
| P9 batch 16 | 16.834180 Mbps | hold 0.530082 us/B | solo +0.176% payload vs 8 |
| P9 batch 32 | 16.824562 Mbps | hold 0.535875 us/B | solo +0.119% payload vs 8 |

El mayor payload observado fue `16.834180 Mbps`, pero el mayor valor repetible
sin la regresión de fairness de batches grandes sigue alrededor de
`16.804 Mbps`.

## Latencia y fairness frente a la referencia histórica

| Configuración | Hold máximo | Cambio vs NB3-C 5,238 us | Resultado |
|---|---:|---:|---|
| NB3-C histórica | 5,238 us | base | REVIEW, bajo techo duro 10 ms |
| P9 batch 8 | 5,138 us | −1.909% | conserva fairness histórica |
| P9 batch 16 | 9,459 us | +80.584% | cerca del techo duro; no promover |
| P9 batch 32 | 18,134 us | +246.201% | supera techo histórico; rechazar |

La comparación causal dentro de P9 es todavía más estricta: batch 16 aumentó
el hold máximo 84.099% y batch 32 lo aumentó 252.939% frente al batch 8 de la
misma ejecución.

## Benchmark Modbus anterior: otra capa

| Prueba de aplicación | Resultado anterior |
|---|---:|
| FC03, 1 registro | 2,981.1 req/s; 0.549 Mbps payload; p95 678.1 us |
| FC03, 64 registros | 1,684.0 req/s; 2.007 Mbps payload; p95 906.9 us |
| FC03, 125 registros | 1,142.4 req/s; 2.477 Mbps payload; 2.285 Mbps útiles; p95 1,196.1 us |

Estos valores incluyen parsing Modbus, request/response y procesamiento de
registros. No deben presentarse como regresión frente a los ~14 Mbps RAW. El
RAW actual muestra margen de transporte; el benchmark FC03 mide capacidad de
aplicación y latencia transaccional.

## Conclusión operativa

1. `FIFO_REUSE` aporta la mejora de velocidad interna más repetible: ~5.1% y
   ~4.9% menos coste por byte.
2. `SINGLE_STATUS` reduce ~4.6% el coste del scheduler/hold sin degradar
   materialmente el payload.
3. Commit coalescido empeora latencia y queda rechazado.
4. Batches 16/32 elevan el ceiling TCP, pero bloquean el SPI demasiado tiempo;
   batch 8 se conserva.
5. Todos los gates válidos cerraron con FNV correcto, cero errores SPI,
   cero errores de transporte y cero resets.

```text
PHYSICAL_STABILITY=PENDING_USER
```
