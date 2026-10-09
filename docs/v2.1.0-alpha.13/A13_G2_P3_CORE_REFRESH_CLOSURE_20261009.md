# Alpha13 — G2-P3 — Cierre refresh core.a

Fecha: 2026-10-09

## Resultado

```text
GATE=A13-G2-P3
STATUS=PASS
REASON=CORE_REFRESH_AND_NORMAL_LINK_PASS
PRODUCT_FAILURE=NO
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

## Entorno

```text
HEAD=0cfec7d38719bea643baab97a5c4d7dac9833c48
PWSH_VERSION=7.6.6
ARDUINO_CLI=1.0.2
```

## Generación source

```text
TIME_S=110.125
COMPILE_DB_ENTRIES=79
JWCONTROL_TUS=64
STUB_TUS=0
PERIPHERALS_INIT_COUNT=1
CORE_PRECOMPILED_BUILD=PASS
```

Archive anterior:

```text
BYTES=3042444
SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Archive nuevo:

```text
BYTES=3043670
SHA256=6f328eeb796091070c8d852a2c0e90f71047d8d2b5786fe3cd5079b2fb6ff983
```

## Verificación FQBN normal

```text
TIME_S=73.107
COMPILE_DB_ENTRIES=16
JWCONTROL_SOURCE_TUS=0
STUB_TUS=1
PRECOMPILED_STUB_COUNT=1
USING_STUB=True
USING_SOURCE=False
CORE_A_LINKED=True
APP_BYTES=411376
CORE_PRECOMPILED_VERIFY_BASIC=PASS
```

## Auditoría final

```text
BOARDS_LOCAL_UNCHANGED=True
TRACKED_DIRTY_FINAL=4
STAGED_FINAL=0
CANDIDATE_BLOBS_FINAL=True
DIFF_CHECK_FINAL=True
```

Los cuatro archivos productivos locales son:

```text
cores/jwcontrol/peripherals_init.cpp
cores/jwcontrol/jwplc_peripherals.cpp
cores/jwcontrol/jwplc_peripherals.h
precompiled/core/JWPLCBASIC/core.a
```

Decisión:

```text
G2_P3=CLOSED_PASS
PRODUCT_COMMIT=NO
NEXT=G2-P4 normal precompiled physical gate
```
