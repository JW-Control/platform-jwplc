# Alpha13-G8-P1/P2 — Candidato integrado RTU y RS-485 (2026-10-11)

**Estado:** `CANDIDATE_PREPARED_NOT_EXECUTED_WINDOWS`. No producto adoptado.

## Baseline previo

El gate `G8-P0=CLOSED_PASS_READ_ONLY` en Windows terminó con
`RUN_ID=g8_p0_20261011_185223_e0c4e0cc`, tres sketches compilados
y enlace precompilado RTU exacto. Documento:
[A13_G8_P0_BASELINE_CLOSURE_20261011.md](A13_G8_P0_BASELINE_CLOSURE_20261011.md).

## Contrato exacto de las correcciones

### A13-010 — Modbus RTU

En las dos rutas no-bulk, `pollServer()` y `pollMaster()`:
`_stats.rxBytes` debe incrementarse una vez por cada valor
`JWPLC_RS485.read()` no negativo. El incremento va **después**
del control `value < 0` y **antes** del control de overflow de
`_rxLength`: incluso el byte consumido que sobrepasa el buffer
se contabiliza. Ningún byte que permanezca en la UART por falta
de espacio se contará aún. Las dos rutas bulk mantienen
`_stats.rxBytes += readBytes` cuando se lee un bloque positivo.

No cambiar CRC, framing, respuestas, timeout, IDs, motor cooperativo,
buffering o API pública.

### A13-011 — RS-485

Tras `HardwareSerial::begin()`, que retorna `void`,
usar `operator bool()` existente en el core JWPLC para
consultar `uartIsDriverInstalled(_uart)`; en caso negativo,
dejar `_ready=false`, devolver `false` y registrar
`JWPLC_RS485_UNKNOWN_ERROR` ya existente. No introducir
nuevos valores de enum ni modificar la API, callbacks ni
AutoDirection MAX13487E. El core original y su archive
permanecen inmutables.

## Validación y límites

1. Contrato de delta exacto: el selftest genera desde el baseline
   las dos fuentes candidatas con **únicamente** los cambios permitidos.
2. 20 pruebas C++ nativas ejecutan ramas extraídas del candidato
   compiladas con mocks, 12 sobre Slave/Master × bulk/no-bulk,
   casos cero, `read=-1`, overflow y 8 sobre UART
   begin/end/rebegin, fallo de driver, reloj y recuperación.
   CI [38186444071](https://github.com/JW-Control/platform-jwplc/actions/runs/38186444071):
   PASS. No son ensayos eléctricos.
3. Ejecutor `tools/alpha13/gates/a13_g8_p1_p2_candidate.py`:
   preflight hash/HEAD/procedencia de P0, overlay fuente temporal
   de las dos bibliotecas, compilación `.o`, creación de
   `libJWPLC_ModbusRTU.a`, extracción y paridad de objeto,
   enlace `-L/-lJWPLC_ModbusRTU`, tres regresiones contra
   bibliotecas candidatas y auditoría de producto sin cambios.
4. No modificar ni versionar los archivos productivos aún.
   Archive RTU original sigue en el package mientras se valida
   el candidato. El PASS del candidato no sustituye adopción,
   prueba física si procede ni cierre Git posterior.

## Flujo de intervención

```text
P0=CLOSED_PASS_READ_ONLY
P1_SOURCE_TESTS=PASS_CI_NATIVE_20_CASES
P2_WINDOWS_SOURCE_REBUILD=NOT_EXECUTED
P2_WINDOWS_ARCHIVE_OBJECT_PARITY=NOT_EXECUTED
P2_WINDOWS_REGRESSIONS=NOT_EXECUTED
PRODUCT_ADOPTED=NO
GIT_PRODUCT_COMMIT=NO
RS485_PHYSICAL_TEST=NOT_EXECUTED
NEXT_ACTION=RUN_G8_P1P2_CANDIDATE_BAT
```

Tras ese resultado, revisar si hace falta un gate físico
RS-485 con un peer seguro (no asumir equipo disponible).
Solo entonces adoptar fuentes RTU/RS485 y archive RTU
con backup y regresiones normales, y cerrar Git.

No tocar OpenPLC, HMI Designer, nuevas funciones TFT, autoload
o configuración final de FlashFreq/bootloader.
