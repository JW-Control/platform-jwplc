# Alpha14 — A14.3 — Aislamiento SD append-only

Fecha: 2026-09-13

## Resultado

`A14_3_SD_APPEND_ONLY_ISOLATION=PASS_DIAGNOSTIC`

Se ejecutó el perfil `FULL_RUNTIME_REALISTIC` a FC03/125 y 1000 req/s durante 60 s con la microSD inicializada y disponible, manteniendo activo `serviceSdAppend()` con archivo persistente y `flush` cada 5 registros, pero deshabilitando únicamente `serviceSdVerify()` en el scheduler.

## Evidencia física

- Requested: 1000 req/s
- Achieved: 914.27 req/s
- Achieved: 91.427 %
- Resultado TCP: `SATURATION_FAIL`
- TCP clean: YES
- OK: 54857 / 60000
- Timeouts: 0
- Transport errors: 0
- Protocol errors: 0
- Bus lock timeouts: 0
- Cross-count: PASS
- Latencia media: 1083.6 us
- P95: 1270.6 us
- P99: 16694.0 us
- Máxima: 203400.0 us
- Loop gap avg: 426 us
- Loop gap max: 21107 us

### Runtime no-SD

- `FULL_RUNTIME_READY=YES`
- Display activo
- FRAM activa, 0 fallos
- RTC activa, 0 stale
- I/O activo, 0 stale
- Botonera activa, 0 fallos
- SPI probe: 0 fallos
- `PERIPHERAL_FAILURE_COUNT=0`

### microSD

- `SD_READY=YES`
- `SD_APPEND_FILE_OPEN=YES`
- `SD_APPEND_CYCLES=59`
- `SD_APPEND_FAILS=0`
- `SD_APPEND_MAX_US=20410`
- `SD_FLUSH_EVERY_RECORDS=5`
- `SD_FLUSH_CYCLES=11`
- `SD_VERIFY_CYCLES=0`
- `SD_VERIFY_FAILS=0`

## Comparación A/B acumulada

| Perfil | Achieved |
|---|---:|
| SD workload OFF | 95.993 % |
| Append ON / Verify OFF | 91.427 % |
| Append + Verify | 88.610 % |

Tomando estas corridas como evidencia diagnóstica, el workload SD periódico explica la pérdida principal de throughput. El bloque append/flush aporta aproximadamente 4.566 puntos porcentuales de caída respecto al perfil sin workload SD, mientras que verify añade aproximadamente 2.817 puntos porcentuales adicionales. Debido a que son corridas físicas individuales, estos deltas se consideran orientativos y no una descomposición exacta.

## Clasificación

`SD_APPEND_ONLY_CLASSIFICATION=BOTH_CONTRIBUTE`

La evidencia indica que ambas rutas contribuyen, con mayor peso aparente de `append/flush`. El valor `SD_APPEND_MAX_US=20410` y la cadencia `flush/5` hacen necesario aislar a continuación el coste de `write` frente al coste del `flush` explícito.

## Estado

- No se cambió `JW_SD`.
- No se retiró SD del autoload.
- No se cambió Ethernet.
- El harness tracked fue restaurado al finalizar.
- El firmware actualmente cargado es una variante diagnóstica append-only.
- `FINAL_FULL_RUNTIME_1000RPS_60S=PENDING_DIAGNOSTIC`
- `FINAL_FULL_RUNTIME_1000RPS_30MIN=ON_HOLD`

## Siguiente gate

Ejecutar un A/B con append activo y verify deshabilitado, suprimiendo únicamente el `flush()` explícito durante la ventana de 60 s. El objetivo es separar `write` de `flush`:

- recuperación >=95 %: flush dominante;
- 91–95 %: write y flush contribuyen;
- ~91 % sin cambio material: write dominante o coste interno de FS/SD.
