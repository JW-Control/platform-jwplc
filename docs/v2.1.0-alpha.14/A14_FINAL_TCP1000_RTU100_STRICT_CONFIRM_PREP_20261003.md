# Alpha14 - Confirmación estricta TCP1000 + RTU100

Fecha: 2026-10-03

## Antecedente

La repetición con el PC en reposo mostró dos barridos R8 reproducibles:

- TCP target: 1000 req/s.
- RTU 50 req/s: PASS estricto en ambas pasadas.
- RTU 100 req/s: PASS estricto en ambas pasadas, cero skips.
- RTU 150 req/s: throughput completo, pero 5 y 1 periodos skipped.
- RTU 200+ req/s: aparecen skips crecientes y posteriormente pérdida del target RTU.

Por ello, 100 req/s es el mayor candidato RTU que cumple simultáneamente el criterio estricto en ambas direcciones del sweep.

## Objetivo

Confirmar durante 600 s una única combinación:

- Modbus TCP: 1000 req/s.
- Modbus RTU: 100 req/s.
- Workload RTU: 2DI + 2DO + 2AI + 2AO.
- Full runtime: Display + SD + FRAM + RTC + I/O + botones.

## Criterio PASS

Todos deben cumplirse:

- TCP >= 99.9 % del target de 1000 req/s.
- RTU >= 99 % del target de 100 req/s.
- RTU_PERIODS_SKIPPED = 0.
- runtime_clean = true.
- cero errores TCP.
- cero CRC/timeouts/rejected/verify RTU.
- cero tails descartados en Slave.
- cero fallos de periféricos/SPI.
- sin resets inesperados.
- perfil fast sin drift.

## Política de firmware

No se recompila ni sube nuevamente para este gate.

La corrida host-idle inmediatamente anterior ya compiló y subió los sketches full-runtime y confirmó:

- un objeto fuente JWPLC_ModbusRTU en Master;
- un objeto fuente JWPLC_ModbusTCP en Master;
- un objeto fuente JWPLC_ModbusRTU en Slave;
- sin marcador de librería precompilada RTU;
- sin marcador de librería precompilada TCP.

Desde esa corrida no se modificó código productivo; sólo scripts/harness de prueba. El runner además vuelve a validar boot/readiness y el perfil fast antes de abrir la ventana de 600 s. Si el firmware cargado no corresponde al esperado, el preflight aborta.

## Archivos

- Runner:
  `tools/modbus-tcp-benchmark/pc/a14_final_tcp1000_rtu100_strict_confirm.py`
- Gate:
  `tools/modbus-tcp-benchmark/gates/a14_final_tcp1000_rtu100_strict_confirm.ps1`

## Evidencia esperada

- `MANIFEST.txt`
- `runner.log`
- `master_boot.log`
- `slave_boot.log`
- `master_fast_profile.log`
- `slave_fast_profile.log`
- snapshots del caso bajo `STRICT_CONFIRMATION/`
- `FINAL_SUMMARY.json`
- `FINAL_STATUS.txt`
- `GATE_STATUS.txt`

El resultado formal esperado para cerrar este punto es:

`A14_FINAL_TCP1000_RTU100_GATE=PASS_STRICT_CONFIRMED`
