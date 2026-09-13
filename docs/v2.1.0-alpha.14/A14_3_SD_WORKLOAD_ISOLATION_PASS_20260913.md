# Alpha14.3 — A/B diagnóstico con workload microSD deshabilitado

Fecha: 2026-09-13

## Resultado

```text
A14_3_SD_WORKLOAD_ISOLATION=PASS_DIAGNOSTIC
SD_WORKLOAD_OFF_CLASSIFICATION=RECOVERED_GTE_95
REQUESTED_REQ_S=1000
ACHIEVED_REQ_S=959.93
ACHIEVED_PCT=95.993
TCP_CLEAN=YES
NON_SD_PERIPHERALS_PASS=YES
SD_AUTOLOAD_STILL_READY=YES
SD_WORKLOAD_DISABLED_AS_EXPECTED=YES
PERIPHERAL_FAILURE_COUNT=0
```

## Configuración del diagnóstico

Se partió del `FULL_RUNTIME_REALISTIC` actual y se aplicó únicamente una variante temporal en el scheduler del harness:

- microSD continuó inicializada y lista;
- el archivo persistente continuó abierto;
- no se removió SD del autoload;
- no se modificó `JW_SD`;
- no se modificó Ethernet;
- `serviceSdAppend()` y `serviceSdVerify()` quedaron temporalmente fuera del scheduler durante la ventana medida;
- TFT, FRAM, RTC, TCA/I/O, botonera y probe SPI permanecieron activos;
- al terminar, el harness tracked fue restaurado y el worktree quedó limpio.

El firmware diagnóstico quedó cargado físicamente en el dispositivo y deberá reemplazarse antes de la siguiente prueba final.

## Evidencia de rendimiento

```text
FORMAL_RESULT RATE=1000
ACHIEVED=959.9
ACHIEVED_PCT=95.99
TOTAL_MBPS=2.081
USEFUL_MBPS=1.920
P95_US=1230.6
P99_US=16590.8
MAX_US=26986.8
LOOP_AVG_US=469
LOOP_MAX_US=17978
RESULT=STABLE_PASS

OK=57597/60000
TIMEOUTS=0
TRANSPORT_ERRORS=0
PROTOCOL_ERRORS=0
BUS_LOCK_TIMEOUTS=0
CROSS_COUNT_PASS=YES
```

El resultado recuperó el criterio estable `>=95 %` frente a las corridas finales con workload SD habilitado:

- corrida previa: `90.089 %` con bug de verificación SD;
- retry posterior al fix de handle único: `88.610 %`, con SD y resto de periféricos sanos;
- A/B actual con SD inicializada pero append/verify fuera del scheduler: `95.993 %`.

## Estado de periféricos

Durante el A/B:

```text
DISPLAY_FRAMES=592
FRAM_CYCLES=236
FRAM_FAILS=0
RTC_SAMPLES=236
RTC_STALE=0
IO_SAMPLES=2955
IO_STALE=0
BUTTON_SAMPLES=2955
BUTTON_NOT_READY=0
SPI_PROBE_SAMPLES=591
SPI_PROBE_FAILS=0
PERIPHERAL_FAILURE_COUNT=0

SD_READY=YES
SD_APPEND_FILE_OPEN=YES
SD_APPEND_CYCLES=0
SD_VERIFY_CYCLES=0
```

## Conclusión

El cuello de botella de rendimiento queda aislado materialmente al workload periódico de microSD del harness (`append` y/o `verify`), no a la mera presencia/autoload de SD ni al resto del full runtime.

Este gate todavía no distingue cuál de las dos rutas — `serviceSdAppend()` o `serviceSdVerify()` — aporta la mayor parte de la pérdida. Por ello no se modifica todavía la política final ni se libera el soak de 30 minutos.

## Siguiente paso

Realizar un A/B adicional con:

1. `serviceSdAppend()` habilitado con la política actual;
2. `serviceSdVerify()` temporalmente fuera del scheduler;
3. resto del full runtime sin cambios;
4. `1000 req/s × 60 s`.

Interpretación prevista:

- si vuelve a `>=95 %`, el coste dominante está en `verify` (flush/cierre/lectura/reapertura);
- si vuelve a ~`88–90 %`, el coste dominante ya está en la ruta de append/flush;
- si queda entre ambos, ambas rutas contribuyen y se cuantificará después la segunda mitad.

## Estado

```text
FINAL_FULL_RUNTIME_1000RPS_60S=PENDING_DIAGNOSTIC_ISOLATION
FINAL_FULL_RUNTIME_1000RPS_30MIN=ON_HOLD
DEVICE_FIRMWARE_VARIANT=DIAGNOSTIC_SD_WORKLOAD_OFF
DEVICE_REFLASH_REQUIRED_BEFORE_FINAL_TEST=YES
```
