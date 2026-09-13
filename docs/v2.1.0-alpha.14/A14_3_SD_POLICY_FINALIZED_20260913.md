# Alpha14.3 — Política microSD final del FULL_RUNTIME_REALISTIC

Fecha: 2026-09-13

## Resultado

Se consolidó la política microSD definida para el harness `FULL_RUNTIME_REALISTIC` antes de repetir la regresión final de rendimiento a 1000 req/s.

Commit del harness:

`a4d20adefc6a93b8583917a9f1a9c6c62ff45594`

Mensaje:

`test(alpha14): aplicar política SD persistente al full runtime`

## Alcance

Se modificó únicamente:

`tools/modbus-tcp-benchmark/firmware/a14_perf_full_runtime_realistic/a14_perf_full_runtime_realistic.ino`

No se modificó `JW_SD`, no se cambiaron APIs productivas y no se tocaron periféricos ni autoload.

## Política final

```text
SD_FILE_POLICY=KEEP_OPEN
SD_APPEND_PERIOD_MS=1000
SD_FLUSH_EVERY_RECORDS=5
SD_VERIFY_PERIOD_MS=5000
JW_SD_LIBRARY_CHANGE=NO
```

El harness mantiene un `JWPLCFile` abierto durante la prueba, realiza un append de 32 bytes cada 1 s y ejecuta `flush()` cada 5 registros. La verificación de lectura continúa cada 5 s.

El reset estadístico ejecuta un flush fuera de la ventana medida y realinea el contador de lote para que la medición comience desde cero.

## Telemetría añadida

El snapshot del harness incorpora:

- `SD_APPEND_FILE_OPEN`
- `SD_FLUSH_EVERY_RECORDS`
- `SD_RECORDS_SINCE_FLUSH`
- `SD_FLUSH_CYCLES`

Esto permitirá confirmar físicamente durante el benchmark que el handle sigue abierto y que la política `flush/5` está activa.

## Gate de compilación

```text
HARNESS_SCOPE=PASS
COMPILE=PASS
COMPILE_WARNINGS=0
PUSH=PASS
WORKTREE_AFTER=PASS
DEVICE_FIRMWARE_UPDATED=NO
```

La compilación final del harness utilizó:

- `jwplc_local:esp32:jwplcbasic`
- `jwplc_local:esp32 2.1.0-dev`

Resultado observado:

```text
Sketch uses 428485 bytes (10%) of program storage space.
Global variables use 29860 bytes (9%) of dynamic memory.
```

## Estado de A14.3

```text
PRODUCTION_L2_MITIGATION=VALIDATED
FULL_RUNTIME_SD_POLICY=FINALIZED
FINAL_1000RPS_60S=PENDING_MANDATORY
FINAL_1000RPS_30MIN=PENDING_MANDATORY
```

## Próximo gate

Recompilar/subir el harness `FULL_RUNTIME_REALISTIC` ya consolidado y repetir la calificación física `FC03/125 @ 1000 req/s` durante 60 s.

Criterio mínimo:

- rendimiento alcanzado >= 95 % del objetivo;
- cero timeouts;
- cero errores de transporte;
- cero errores de protocolo;
- cero `BUS_LOCK_TIMEOUTS`;
- `PERIPHERAL_FAILURE_COUNT=0`;
- periféricos activos y frescos;
- `SD_APPEND_FILE_OPEN=YES`;
- `SD_FLUSH_EVERY_RECORDS=5`;
- ciclos de flush observados durante la ventana.

Si pasa, ejecutar el soak final `FULL_RUNTIME_REALISTIC @ 1000 req/s` durante 30 minutos.