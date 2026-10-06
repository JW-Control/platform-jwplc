# Alpha14 — Política de pruebas microSD / DataLog

Fecha: 2026-09-30

## Decisión

La API de alto nivel `JWPLCDataLog` forma parte del comportamiento de producto
del package JWPLC y es la ruta de referencia para cualquier prueba posterior que
declare validar microSD dentro de un runtime normal de PLC.

Esta decisión se apoya en el trabajo previo de Alpha14 que diseñó y validó:

- ring buffer RAM;
- archivo persistente;
- commit por threshold o timeout;
- servicio cooperativo automático desde el runtime;
- ausencia de llamada manual obligatoria a `service()`;
- coexistencia con el arbitraje SPI compartido.

## Regla obligatoria de validación

A partir de esta decisión:

```text
RUNTIME_SD_TEST_PATH=JWPLCDataLog
DATALOG_AUTOSERVICE=REQUIRED
MANUAL_DATALOG_SERVICE=FORBIDDEN_IN_NORMAL_RUNTIME_GATES
LOW_LEVEL_JW_SD_TESTS=EXPLICIT_ONLY
```

Toda prueba posterior que reutilice microSD como parte de un escenario de
producto/full-runtime debe usar la API DataLog, salvo que el objetivo declarado
sea específicamente medir o diagnosticar la capa SD de bajo nivel.

## Ruta de producto

La ruta esperada es:

```text
Sketch / runtime
    -> JWPLCDataLog.write(...)
    -> ring buffer RAM
    -> jwplcSystemTask
    -> jwplcDataLogTickCallback()
    -> JWPLC_SD.serviceDataLogs()
    -> commit por threshold/timeout
    -> JWPLCFile / microSD
```

El sketch normal no debe llamar manualmente `JWPLCDataLog::service()`.

## Configuración de referencia Alpha14

Hasta nueva decisión explícita, los gates de runtime usarán como referencia:

```text
BUFFER_BYTES=4096
COMMIT_THRESHOLD_BYTES=512
COMMIT_TIMEOUT_MS=5000
```

Los valores pueden cambiar únicamente mediante un gate que mida y documente la
nueva política.

## Qué queda permitido con JWPLC_SD/JWPLCFile directo

La API de bajo nivel se conserva por compatibilidad Arduino y sigue siendo
válida para:

- pruebas físicas de filesystem;
- diagnóstico de `open/read/write/flush/close`;
- retiro/reinserción de tarjeta;
- recovery;
- benchmarks del backend SD;
- pruebas cuyo objetivo sea aislar la capa de bajo nivel.

En esos casos el gate debe declarar explícitamente:

```text
SD_ACCESS_POLICY=LOW_LEVEL_EXPLICIT
PRODUCT_RUNTIME_REPRESENTATIVE=NO
```

No debe utilizarse un acceso directo como sustituto silencioso de DataLog en un
gate full-runtime.

## Relación con S2

S2 DIRECT, ya cerrado, validó correctamente la coexistencia física de la SD y
el bus compartido usando `JWPLC_SD/JWPLCFile`, pero no validó la ruta DataLog
de producto.

Por eso se creó S2D:

```text
S2_DIRECT=LOW_LEVEL_SD_COEXISTENCE
S2D_DATALOG=PRODUCT_RUNTIME_SD_PATH
ONLY_VARIABLE=SD_ACCESS_POLICY
```

Los picos de jitter observados en S2 DIRECT no se atribuyen al DataLogger antes
de disponer del resultado S2D.

## Reglas para gates posteriores

Cualquier gate posterior que incluya microSD dentro de un escenario de producto
debe comprobar, como mínimo:

```text
DATALOG_ACTIVE=YES
DATALOG_ACCEPTED_WRITES>0
DATALOG_ACCEPTED_BYTES>0
DATALOG_COMMITTED_BYTES>0
DATALOG_COMMIT_COUNT>0
DATALOG_FAILED_COMMITS=0
DATALOG_MANUAL_SERVICE_CALL=NO
```

Además debe conservar los criterios propios del gate:

- cero corrupción;
- cero errores de lock SPI;
- cero fallos de transporte;
- cero resets inesperados;
- convivencia con TFT/Ethernet/FRAM/RTC/I/O/RS-485 cuando corresponda.

## Alcance de documentación

Esta política debe propagarse al cierre del trabajo a:

- documentación Alpha14;
- estado Alpha correspondiente;
- contexto/arquitectura del Proyecto;
- librerías/fuente de verdad del Proyecto;
- checklist de release si la validación microSD forma parte del cierre.

No se debe perder esta decisión al avanzar a P4 ni a alphas posteriores.

## Estado

```text
DATALOG_PRODUCT_PATH=MANDATORY_FOR_RUNTIME_SD_TESTS
LOW_LEVEL_SD_API=KEEP_COMPATIBLE
LOW_LEVEL_SD_IN_FULL_RUNTIME=EXPLICIT_DIAGNOSTIC_ONLY
S2D=IN_PROGRESS
P4_BLOCKED_BY_S2D_RESULT=YES
```
