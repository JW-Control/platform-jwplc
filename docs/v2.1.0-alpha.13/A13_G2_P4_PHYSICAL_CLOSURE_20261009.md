# Alpha13 — G2-P4 — Cierre físico package normal A13-002

Fecha: 2026-10-09

## Resultado

```text
GATE=A13-G2-P4
STATUS=PASS
REASON=NORMAL_PRECOMPILED_PHYSICAL_PASS
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

## Entorno

```text
HEAD=5cf8dba1495de150bc9c7161a0233732417bb4b7
SERIAL_PORT=COM4
ARDUINO_CLI=1.0.2
PWSH=7.6.6
```

## Prueba de ruta de producción

```text
COMPILE_EXIT=0
USES_STUB_CORE=True
USES_SOURCE_CORE=False
CORE_A_LINKED=True
SOURCE_TU_COUNT=0
STUB_TU_COUNT=1
PRECOMPILED_STUB_COUNT=1
FULL_PROFILE=True
```

Esto demuestra que el FQBN normal `jwplcbasic` no recompiló el source core y
que el firmware físico usó `jwcontrol_precompiled_stub + core.a`.

## Contrato físico

```text
UPLOAD_EXIT=0
CLIENT_EXIT=0
IO_READY=YES
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=HIGH
INPUTS=0
OUTPUTS=0
LAST_SCAN_MS=2280
UPTIME_MS=2296
CLIENT_PASS=YES
```

## Identidad final del candidato

```text
peripherals_init.cpp=23efb3935a34e6b5649875b57c804e60538827cd
jwplc_peripherals.cpp=c3d53566d274e95b7dda111327140b5393f7db35
jwplc_peripherals.h=288667f1caa08142e2a155b8c85f24b2aa5beb44
core.a SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
```

## Auditoría final del gate

```text
SOURCE_BLOBS_FINAL=True
TRACKED_DIRTY_FINAL=4
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
```

## Decisión

```text
G2_P4=CLOSED_PASS
G2_PRODUCT=QUALIFIED
PRODUCT_COMMIT=NOT_YET
NEXT=G2-P5 final diff audit + explicit staging + product commit
```
