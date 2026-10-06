# Alpha14 — P4.2 COPY_OUT 64 B — preflight 2026-09-30

## Source contract

```text
P4_2_SOURCE_CONTRACT=PASS
COPY_OUT_64_DEFAULT=0
COPY_OUT_64_CONDITION=c_len == 64U
EXPLICIT_VOLATILE_WORD_COPIES=16
MMIO_MEMCPY=NO
TAIL_1_63_PATH_CHANGED=NO
PUBLIC_API_CHANGED=NO
```

La modificación queda limitada a `jwplcSpiReadBytesReuseFifoNL()` y al switch
compile-time `JWPLC_SPI_FIFO_REUSE_COPY_OUT_64`. El candidato permanece OFF por
defecto antes de validación física.

## Build preflight

```text
PROFILE=smoke
SKETCH=JWPLC_IO_BlockMirror
FQBN=jwplc_local:esp32:jwplcbasic
BUILD_PATH=C:\JWPLC_Build\jwplcbasic
CLEAN=true
STATUS=PASS
EXIT_CODE=0
WARNINGS=0
ERRORS=0

SPI_SOURCE_COMPILED=true
SPI_SOURCE_LINKED=true
SPI_PRECOMPILED_ARCHIVE_PRESENT=true
SPI_PRECOMPILED_ARCHIVE_LINKED=false
SPI_CONFIDENCE=high

JWPLC_ETHERNET_SOURCE_COMPILED=true
JWPLC_ETHERNET_SOURCE_LINKED=true
JWPLC_ETHERNET_PRECOMPILED_ARCHIVE_PRESENT=false
JWPLC_ETHERNET_PRECOMPILED_ARCHIVE_LINKED=null
JWPLC_ETHERNET_CONFIDENCE=medium

EVIDENCE_MISSING=[]
```

Artefactos disponibles para el run
`20261001T044328Z_platform-jwplc_build`: `build_log`, `build_manifest`,
`linker_map` y `result_json`.

```text
P4_2_PREFLIGHT=PASS
P4_2_PHYSICAL_EXECUTION=NOT_RUN
```

## Candidate ON compile

Compilación temporal con `JWPLC_SPI_FIFO_REUSE_COPY_OUT_64=1`; finalizado el
build, el default se restauró a `0`.

```text
PROFILE=smoke
SKETCH=JWPLC_IO_BlockMirror
FQBN=jwplc_local:esp32:jwplcbasic
BUILD_PATH=C:\JWPLC_Build\jwplcbasic
CLEAN=true
STATUS=PASS
EXIT_CODE=0
WARNINGS=0
ERRORS=0

SPI_SOURCE_COMPILED=true
SPI_SOURCE_LINKED=true
SPI_PRECOMPILED_ARCHIVE_PRESENT=true
SPI_PRECOMPILED_ARCHIVE_LINKED=false
SPI_CONFIDENCE=high

EVIDENCE_MISSING=[]
```

Artefactos disponibles para el run
`20261001T045646Z_platform-jwplc_build`: `build_log`, `build_manifest`,
`linker_map` y `result_json`.

```text
P4_2_CANDIDATE_ON_COMPILE=PASS
P4_2_DEFAULT_RESTORED=PASS
P4_2_PHYSICAL_AB=NOT_RUN
P4_2_PROMOTION=NOT_DECIDED
```
