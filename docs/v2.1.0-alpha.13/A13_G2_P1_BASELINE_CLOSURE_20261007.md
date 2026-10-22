# Alpha13 — G2-P1 — Cierre de reproducción baseline A13-002

Fecha: 2026-10-07

## Resultado

```text
GATE=A13-G2-P1
STATUS=PASS
REASON=BASELINE_DEFECT_REPRODUCED_SAFELY
PRODUCT_FAILURE=REPRODUCED_BASELINE_DEFECT
HARNESS_FAILURE=NO
HARDWARE_FAILURE=NO
ENVIRONMENT_FAILURE=NO
```

## Build source-first

Se compilaron seis piernas independientes: un control normal y cinco fallas
inyectadas.

```text
COMPILE_LEGS=6/6 PASS
USES_SOURCE_CORE=True
USES_STUB_CORE=False
ARCHIVE_LINKED=False
PERIPHERALS_INIT_COUNT=1
FULL_PROFILE=True
```

La instrumentación temporal se restauró antes de las subidas físicas:

```text
SOURCE_RESTORED=True
BOARDS_LOCAL_RESTORED=True
CORE_SHA256_AFTER_BUILD=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
DIRTY_AFTER_BUILD=0
STAGED_AFTER_BUILD=0
```

## Control normal

```text
FAULT_STEP=0
OP_OK_MASK=31
EN_IO_HIGH_REQUESTED=YES
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=HIGH
EN_IO_PAD_READBACK=LOW
PERIPHERALS_INITIALIZED=YES
IO_STATE_INITIALIZED=YES
IO_VIEW_READY=YES
CONTRACT_PASS=True
```

El pad readback LOW no se usa como contrato porque GPIO27 está configurado
output-only. El latch GPIO confirma el HIGH esperado.

## Fallas inyectadas

Las cinco operaciones críticas se probaron de forma individual.

```text
STEP1 OP_OK_MASK=30 CONTRACT_PASS=True
STEP2 OP_OK_MASK=29 CONTRACT_PASS=True
STEP3 OP_OK_MASK=27 CONTRACT_PASS=True
STEP4 OP_OK_MASK=23 CONTRACT_PASS=True
STEP5 OP_OK_MASK=15 CONTRACT_PASS=True
```

En todas las piernas de fallo:

```text
EN_IO_HIGH_REQUESTED=YES
EN_IO_OUTPUT_ENABLE=YES
EN_IO_OUTPUT_LATCH=LOW
PERIPHERALS_INITIALIZED=YES
IO_STATE_INITIALIZED=YES
IO_VIEW_READY=YES
```

El LOW físico/latch proviene del interlock del harness. La evidencia relevante
es que el baseline intenta habilitar EN_IO y declara I/O ready aunque una
operación de configuración TCA haya fallado.

## Auditoría final

```text
SOURCE_RESTORED_FINAL=True
CORE_SHA256_FINAL=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
TRACKED_DIRTY_FINAL=0
STAGED_FINAL=0
DIFF_CHECK_FINAL=True
```

## Decisión

```text
A13-002=CONFIRMED
G2_P1=CLOSED_PASS
NEXT=G2-P2 minimal source candidate
CORE_A_REFRESH=NOT_YET
```

El candidato G2-P2 deberá:
- comprobar las cinco operaciones TCA;
- abortar dejando EN_IO en LOW ante cualquier fallo;
- mantener JWPLC_IO.ready()==false ante fallo;
- declarar ready sólo después de completar correctamente el startup TCA;
- pasar source-first antes de regenerar core.a.
