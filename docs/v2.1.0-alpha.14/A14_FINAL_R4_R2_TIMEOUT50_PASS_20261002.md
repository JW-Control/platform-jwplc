# Alpha14 — R4-R2 TCP100 + RTU EXP-MIX timeout 50 ms — PASS 2026-10-02

## Resultado

`PASS_CHARACTERIZED`

Ventana: 300 s, full-runtime, TCP target 100 req/s y RTU EXP-MIX
`2DI + 2DO + 2AI + 2AO` unpaced.

### TCP

- target: 100 req/s;
- achieved: 99.999999 req/s;
- AVG: 1030.38 us;
- P95: 1470.71 us;
- P99: 3524.665 us;
- MAX: 10256.8 us;
- timeouts/transport/protocol: 0.

### RTU

- timeout de benchmark: 50 ms;
- started/success: 348768/348768;
- failed: 0;
- rejected: 0;
- verify: 0;
- master timeouts: 0;
- CRC Master/Slave: 0/0;
- scans: 43596;
- RTU rate: 1162.417 req/s;
- scan rate: 145.302 scans/s;
- scan period: 6.882 ms;
- transaction max: 11994 us;
- >20 ms: 0;
- DI/DO/AI/AO failures: 0/0/0/0;
- cross-count Master/Slave exacto;
- mapas finales de outputs exactos.

### Full-runtime

- FULL_RUNTIME_READY=YES;
- SERVER_READY=YES;
- ETH_READY=YES / ETH_LINK=UP;
- Display ready;
- FRAM failures=0;
- SD append/verify/commit failures=0;
- RTC stale/unavailable=0;
- I/O stale=0;
- buttons not ready=0;
- peripheral failures=0;
- SPI probe failures=0;
- reboot Master/Slave=0.

`COMBINED_RUNTIME_READY=NO` en el snapshot final es esperado: el snapshot se
captura después del stop quiescente y esa bandera incluye
`rtuTrafficEnabled`, que ya está en OFF.

## Decisión

El timeout de 25 ms del firmware de benchmark se considera demasiado
agresivo para full-runtime. Con 50 ms, TCP100 + RTU EXP-MIX quedó limpio y
el peor RTU observado fue 11.994 ms, por lo que 50 ms funciona como margen
de tolerancia y no como latencia normal.

El cambio sigue limitado al firmware de qualification; no modifica los
defaults públicos de la librería Modbus RTU.