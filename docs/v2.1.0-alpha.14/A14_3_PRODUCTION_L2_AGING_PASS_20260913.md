# Alpha14.3 — Aging físico del refresh L2 productivo

Fecha: 2026-09-13

## Resultado

La implementación productiva de refresh L2 periódico quedó validada físicamente durante una ventana de inactividad de 480 s.

El firmware ejecutado corresponde al source commit:

`ea8ea87c2c9adc63970e6b1fe78b3dfe1296b93b`

La rama se encontraba en `ab9d6889cf880acb3effff3d9f57df3152cffecf`, y se verificó que `JWPLC_Ethernet.h/.cpp` no habían cambiado respecto del source commit cargado en el dispositivo.

## Condiciones

- `REFLASH=NO`
- `DEVICE_RESET=NO`
- ventana de aging: `480 s`
- tráfico Ethernet desde el host durante aging: ninguno
- refresh L2 productivo: broadcast dirigido cada `120 s`
- full runtime activo con Display, FRAM, SD, RTC, TCA/I-O y botonera
- servidor Modbus TCP activo

## Salud durante aging

En los snapshots de 60, 120, 180, 240, 300, 360 y 420 s se mantuvo:

```text
AGING_RUNTIME_READY=YES
AGING_SERVER_READY=YES
AGING_FRAM_READY=YES
AGING_SD_READY=YES
AGING_PERIPHERAL_FAILURE_COUNT=0
```

Al finalizar los 480 s:

```text
AGED_RUNTIME_READY=YES
AGED_SERVER_READY=YES
AGED_PERIPHERAL_FAILURE_COUNT=0
```

La entrada neighbor del host permanecía con la MAC correcta del JWPLC y estado stale antes del primer TCP:

```text
NEIGHBOR_MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE=4
```

## Reaccept post-aging

Se ejecutaron 12 conexiones TCP secuenciales con probe FC03.

Resultado:

```text
ATTEMPTS=12
PASS_CYCLES=12
CONNECT_FAILURES=0
PROTOCOL_FAILURES=0
PING_FAILURES_AFTER_TCP_FAIL=0
CONNECT_MIN_MS=0.462
CONNECT_AVG_MS=11.617
CONNECT_MAX_MS=26.285
```

El primer TCP después del aging entró correctamente en `11.020 ms` y todos los FC03 fueron válidos.

## Clasificación

```text
A14_3_PRODUCTION_L2_AGING=PASS_PHYSICAL
AGED_REACCEPT_OUTAGE=NOT_REPRODUCED_WITH_PRODUCTION_MITIGATION
PRODUCTION_L2_MITIGATION=VALIDATED
A14_3_TCP_REACCEPT_LATENCY=CLOSED_WITH_MITIGATION
```

La anomalía previamente reproducida tras inactividad, tanto con DHCP como con IP estática, no se reprodujo usando la implementación productiva de refresh L2.

## Decisión

Se conserva la mitigación productiva implementada en `JWPLC_Ethernet`:

- período por defecto: `120000 ms`;
- destino: broadcast dirigido de la subred;
- payload: `JWL2` de 4 bytes;
- socket UDP local efímero;
- política best-effort no fatal;
- omitida durante mantenimiento DHCP cooperativo;
- desactivable en compilación mediante `JWPLC_ETH_L2_REFRESH_PERIOD_MS=0`.

No se requiere seguir bloqueando Alpha14.3 por la incidencia de aged-reaccept.

## Pendientes de Alpha14.3

Antes de cerrar rendimiento siguen siendo obligatorios:

```text
FULL_RUNTIME_REALISTIC_SD_POLICY=KEEP_FILE_OPEN_FLUSH_EVERY_5_RECORDS
FINAL_FULL_RUNTIME_1000RPS_60S=PENDING_MANDATORY
FINAL_FULL_RUNTIME_1000RPS_30MIN=PENDING_MANDATORY
```

El siguiente paso es actualizar el harness versionado `a14_perf_full_runtime_realistic.ino` para reflejar la política microSD ya decidida, validar compilación y después repetir los benchmarks finales a 1000 req/s.