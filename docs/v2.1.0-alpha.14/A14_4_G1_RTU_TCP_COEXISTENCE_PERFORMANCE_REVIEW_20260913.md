# Alpha14.4 G1 — RTU + TCP Coexistence Foundation

Fecha: 2026-09-13

## Clasificación

- Integración funcional: **PASS_PHYSICAL**
- Rendimiento: **PERFORMANCE_REVIEW**
- Gate global: **REVIEW**

## Configuración

- Modbus TCP Server activo.
- Modbus RTU Slave activo, ID 2, 115200 8N1.
- RTU sin tráfico externo durante el gate.
- Ambos transportes compartieron los mismos mapas de coils y holding registers.
- Full runtime activo: HMI Dirty/On-Demand, SD append/flush/verify, FRAM, RTC, botonera, TCA/I/O y probe SPI.
- Carga TCP: FC03, 125 registros, objetivo 1000 req/s durante 60 s.

## Resultado

- TCP achieved: **915.73 req/s**.
- TCP achieved: **91.573 %**.
- Resultado de frontera: `SATURATION_FAIL`.
- Requests procesadas correctamente: **54955 / 54955 enviadas**.
- Timeouts: 0.
- Transport errors: 0.
- Protocol errors: 0.
- Bus lock timeouts: 0.
- Cross count: PASS.
- P95: 1437.1 us.
- P99: 2768.9 us.
- Max: 23205.4 us.

## RTU

- RTU ready: YES.
- RX frames: 0.
- TX frames: 0.
- Requests OK: 0.
- CRC errors: 0.
- Exceptions: 0.
- Master timeouts: 0.
- RTU idle clean: YES.

## Full runtime

- Full runtime ready: YES.
- HMI ready: YES.
- Display frames: 588.
- HMI bar pushes: 599.
- SD append cycles: 60, failures 0.
- SD verify cycles: 12, failures 0.
- Peripheral failure count: 0.
- Full workload clean: YES.

## Interpretación

El gate demuestra coexistencia funcional correcta de ambos stacks y del runtime completo, pero revela un coste de servicio inesperado al mantener `JWPLC_ModbusRTU.task()` activo incluso sin tráfico RTU. La caída frente al full runtime de A14.3 es suficientemente grande como para no aceptar todavía el perfil como política final.

El 91.573 % no representa pérdida de requests ya enviadas: las 54955 solicitudes que sí se emitieron fueron atendidas correctamente. La saturación corresponde a incapacidad de sostener el ritmo artificial de 1000 lanzamientos/s bajo esta combinación de tareas.

## Decisión

No se considera requisito industrial que el full runtime sostenga 1000 req/s exactos. Para un scan de 20 ms, 500 req/s equivalen a 10 transacciones Modbus por scan, ya una carga muy alta si las E/S se agrupan en bloques. Sin embargo, antes de adoptar 500 req/s como qualification objetivo se debe aislar el coste de `JWPLC_ModbusRTU.task()` en idle.

## Siguiente gate

A14.4-G1b: medir el coste de servicio RTU idle variando únicamente la frecuencia de llamada a `JWPLC_ModbusRTU.task()`, sin tráfico RTU externo, conservando el resto del full runtime. Si una cadencia cooperativa razonable recupera rendimiento sin comprometer RTU, se evaluará su validez antes de cualquier cambio de librería. Después se realizará qualification a 500 req/s y luego tráfico RTU+TCP simultáneo real.
