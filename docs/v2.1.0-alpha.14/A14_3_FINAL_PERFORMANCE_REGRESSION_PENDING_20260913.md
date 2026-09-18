# Alpha14 — Regresión final de rendimiento pendiente

Fecha: `2026-09-13`

## Motivo

Antes de cerrar A14.3 se debe repetir la referencia integrada de rendimiento con el estado final consolidado de Alpha14.

Ya existen dos referencias físicas importantes:

```text
TCP_ONLY_FC03_125_1000RPS_30MIN=PASS_PHYSICAL
FULL_RUNTIME_REALISTIC_FC03_125_1000RPS_60S=PASS_PHYSICAL
```

La prueba integrada de 60 s alcanzó:

```text
REQUESTED_REQ_S=1000
ACHIEVED_REQ_S=954.78
ACHIEVED_PCT=95.478
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
PERIPHERAL_FAILURE_COUNT=0
```

Cumplió el criterio formal `>=95 %`, pero con margen pequeño. Además, posteriormente se tomaron decisiones y se abrieron trabajos que todavía deben quedar consolidados antes de considerar el resultado definitivo:

- política microSD `KEEP_FILE_OPEN_FLUSH_EVERY_5_RECORDS`;
- cierre o mitigación explícita del problema de reacceso tras inactividad / aging L2;
- cualquier cambio final que afecte `JWPLC_Ethernet`, `JWPLC_ModbusTCP` o el harness `FULL_RUNTIME_REALISTIC`.

## Gate obligatorio antes de cerrar A14.3

Después de consolidar los cambios finales:

```text
FINAL_FULL_RUNTIME_1000RPS_60S_REGRESSION=NOT_EXECUTED
FINAL_FULL_RUNTIME_1000RPS_30MIN_SOAK=NOT_EXECUTED
```

Secuencia obligatoria:

1. repetir `FULL_RUNTIME_REALISTIC`, FC03, 125 registros, `1000 req/s`, 60 s;
2. exigir `TCP_CLEAN=YES`, `PERIPHERAL_FAILURE_COUNT=0` y `ACHIEVED_PCT>=95.0`;
3. si la regresión corta pasa, ejecutar el soak de 30 min en las mismas condiciones;
4. volver a exigir `TCP_CLEAN=YES`, `PERIPHERAL_FAILURE_COUNT=0` y `ACHIEVED_PCT>=95.0`;
5. comparar contra la referencia TCP-only de 30 min (`1000 req/s`, `2.168 Mbps` payload TCP, `2.000 Mbps` útiles Modbus).

## Criterio de cierre

No declarar todavía:

```text
MODBUS_TCP_PERFORMANCE_BENCHMARK=PASS_PHYSICAL
```

hasta disponer de la regresión final integrada y del soak integrado de 30 min.

El resultado esperado de cierre deberá poder expresarse como:

```text
TCP_ONLY_1000RPS_30MIN=PASS_PHYSICAL
FULL_RUNTIME_1000RPS_60S_FINAL=PASS_PHYSICAL
FULL_RUNTIME_1000RPS_30MIN_FINAL=PASS_PHYSICAL
MODBUS_TCP_PERFORMANCE_BENCHMARK=PASS_PHYSICAL
```

## Estado actual

```text
A14_3_FINAL_PERFORMANCE_REGRESSION=PENDING_MANDATORY
LONG_RUN_1000RPS=HOLD_UNTIL_IDLE_NETWORK_PATH_CLOSED
```
