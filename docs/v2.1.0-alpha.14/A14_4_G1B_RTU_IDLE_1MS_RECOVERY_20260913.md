# A14.4 G1b — recuperación de coste RTU idle con servicio cada 1 ms

Fecha: 2026-09-13

## Clasificación

`PASS_DIAGNOSTIC`

Resultado de política: `RTU_IDLE_1MS_COST_RECOVERED`

## Contexto

El gate A14.4-G1 activó simultáneamente Modbus TCP Server y Modbus RTU Slave dentro del full runtime, pero atendiendo `JWPLC_ModbusRTU.task()` en cada vuelta del `loop()`. Aunque el RTU estaba completamente idle y sin tráfico externo, el throughput TCP cayó a 91.573 % a una carga objetivo de 1000 req/s.

G1b repitió el escenario manteniendo:

- Modbus TCP Server activo.
- Modbus RTU Slave ID 2 @115200 8N1 activo.
- HMI Dirty / On-Demand activa.
- workload SD completo activo.
- FRAM, RTC, botonera y TCA/I/O activos.
- cero tráfico RTU externo.

La única diferencia funcional fue limitar el servicio RTU a una llamada cada 1000 us, en lugar de una llamada en cada iteración del `loop()`.

## Incidente de compilación previo

El primer intento de G1b no compiló porque el generador temporal insertó `serviceRtuCooperative()` antes del bloque de constantes globales. El preprocesamiento de Arduino generó entonces prototipos antes de la declaración de `SD_RECORD_BYTES`, provocando:

```text
error: 'SD_RECORD_BYTES' was not declared in this scope
```

No se modificó código fuente estable. Se corrigió únicamente el orden del sketch temporal y se repitió el gate.

## Resultados físicos

### Run 1

- objetivo: 1000 req/s
- logrado: 999.93 req/s
- porcentaje: 99.993 %
- P95: 1311.7 us
- P99: 2295.5 us
- máximo: 21179.2 us
- TCP clean: YES
- RTU service calls: 41394
- RTU service avg: 15 us
- RTU service max: 1645 us
- RTU service gap max: 138081 us
- RTU RX/TX: 0/0
- RTU CRC errors: 0
- full runtime ready: YES
- HMI ready: YES
- peripheral failures: 0

### Run 2

- objetivo: 1000 req/s
- logrado: 999.54 req/s
- porcentaje: 99.954 %
- P95: 1318.7 us
- P99: 2258.2 us
- máximo: 20675.0 us
- TCP clean: YES
- RTU service calls: 41288
- RTU service avg: 15 us
- RTU service max: 2072 us
- RTU service gap max: 139283 us
- RTU RX/TX: 0/0
- RTU CRC errors: 0
- full runtime ready: YES
- HMI ready: YES
- peripheral failures: 0

## Resumen

- promedio TCP: 99.973 %
- mínimo: 99.954 %
- máximo: 99.993 %
- spread: 0.039 pp
- recuperación frente a RTU `task()` cada loop: +8.400 pp
- diferencia frente al full runtime A14.3: -2.570 pp según la referencia corta usada por el runner; en la práctica G1b alcanzó ~100 % en ambas corridas.

## Interpretación

La caída observada en G1 no era inherente a tener simultáneamente los stacks TCP y RTU activos. El coste estaba dominado por llamar el servicio RTU idle en cada vuelta del loop.

Con una política periódica de 1 ms:

- se recupera prácticamente el 100 % del throughput TCP objetivo;
- no aparecen errores TCP;
- no aparecen fallos periféricos;
- el RTU permanece READY y sin errores estando idle.

El runner marcó `REVIEW` porque sus umbrales esperaban ~50–65 mil llamadas RTU por minuto y un `RTU_SERVICE_GAP_MAX_US < 30000`. Esos criterios resultaron demasiado estrictos para un full runtime con trabajo SD/HMI y no invalidan la recuperación de rendimiento.

Sin embargo, los gaps máximos de servicio RTU de ~138–139 ms sí requieren validación física con tráfico RTU real antes de adoptar 1 ms como política definitiva. La UART puede seguir recibiendo bytes durante esos gaps, pero A14.4 debe comprobar que no aparecen pérdidas, overflow, CRC o timeouts bajo tráfico RTU real concurrente.

## Decisión

- No volver a llamar `JWPLC_ModbusRTU.task()` en cada vuelta del loop dentro del perfil dual-stack.
- Mantener 1 ms como candidato diagnóstico para el siguiente gate.
- No promover todavía esta política a cambio estable de librería/core.
- Antes de tráfico RTU físico, validar una referencia industrial de 500 req/s TCP con el mismo firmware para confirmar 100 % sostenido a una carga representativa de ~10 transacciones por scan de 20 ms.
- Después, ejecutar tráfico RTU real simultáneo.

## Estado A14.4

- G1 coexistencia funcional: PASS funcional / PERFORMANCE_REVIEW.
- G1b política RTU idle 1 ms: `PASS_DIAGNOSTIC` / `RTU_IDLE_1MS_COST_RECOVERED`.
- tráfico RTU físico simultáneo: pendiente.
