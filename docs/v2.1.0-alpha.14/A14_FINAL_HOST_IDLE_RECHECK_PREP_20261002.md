# Alpha14 - Recheck final TCP x RTU con host idle

Fecha de preparación: 2026-10-02

## Motivo

La campaña R7/R8 anterior dejó resultados internamente limpios, pero el barrido R8 mostró una dispersión TCP no monotónica:

- TCP-only unpaced: ~998 req/s.
- TCP1000 + RTU=0: ~817 req/s.
- TCP1000 + RTU=250: 1000 req/s, aunque el RTU ya mostraba skips.

Durante esa campaña el PC generador de tráfico estuvo ejecutando Rocket League. Como el benchmark TCP depende del scheduling y la latencia del host Windows a escala sub-milisegundo, se registra la carga del host como posible factor de contaminación. Esto es una hipótesis; no se atribuye causalidad sin repetir con el PC en reposo.

## Objetivo

1. Confirmar que el baseline TCP del host permanece estable antes y después de la campaña.
2. Repetir la frontera RTU-unpaced alrededor de 900-1000 TCP req/s.
3. Repetir TCP1000 con RTU fijo dos veces y en orden opuesto para separar una frontera física de una deriva temporal.
4. Confirmar durante 600 s el mejor RTU que cumpla simultáneamente:
   - runtime limpio;
   - RTU >=99 % del target;
   - cero periodos RTU skipped;
   - TCP >=99.9 % de 1000 req/s en ambas repeticiones.

## Política del host

Durante toda la medición:

- no juegos;
- no streaming;
- no descargas pesadas;
- no benchmark paralelo;
- mantener el PC de prueba lo más idle posible.

No se exige tocar prioridades de procesos ni deshabilitar servicios de Windows.

## Secuencia

### Control TCP-only

- TCP_ONLY_BEFORE: 600 s, TCP unpaced, RTU OFF.
- TCP_ONLY_AFTER: 600 s, TCP unpaced, RTU OFF.

### R7 - RTU EXP-MIX unpaced

Cada caso dura 600 s:

- TCP OFF
- TCP 900
- TCP 925
- TCP 950
- TCP 975
- TCP 1000

Carga RTU: 2DI + 2DO + 2AI + 2AO.

### R8 - TCP1000 con RTU fijo

Cada caso dura 300 s.

Repetición 1 ascendente:

0, 50, 100, 150, 200, 250, 300 req/s.

Repetición 2 descendente:

300, 250, 200, 150, 100, 50, 0 req/s.

La inversión de orden permite detectar deriva temporal del host o del sistema.

### Confirmación

Si existe una tasa RTU positiva que cumpla el criterio estricto en ambas repeticiones, se selecciona la mayor y se repite durante 600 s.

## Control automático de baseline

El harness marca el baseline del host como válido sólo si:

- ambos TCP-only son >=980 req/s;
- la diferencia TCP-only before/after es <=30 req/s;
- ambos casos TCP1000 + RTU0 son >=990 req/s;
- el rango entre ambos RTU0 es <=30 req/s;
- CV de TCP en RTU0 <=1.5 %.

Si esto no se cumple, la campaña no publica una frontera TCP1000 x RTU: queda como revisión de host/harness.

## Fuentes

- Runner: `tools/modbus-tcp-benchmark/pc/a14_final_tcp_rtu_host_idle_recheck.py`
- Gate: `tools/modbus-tcp-benchmark/gates/a14_final_tcp_rtu_host_idle_recheck.ps1`

El gate conserva la prueba source-first de JWPLC_ModbusRTU y JWPLC_ModbusTCP, compila y sube una sola vez, ejecuta toda la secuencia y conserva snapshots/CSV/JSON por caso.
