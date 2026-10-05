# Alpha14 — Modbus RTU source-first — 2026-09-30

## Decisión

```text
JWPLC_ModbusRTU=SOURCE_FIRST_DEVELOPMENT
MODBUS_RTU_SOURCE_FIRST=PASS
```

`JWPLC_ModbusRTU` se compila desde source durante los siguientes gates de
desarrollo. El archive cualificado se conserva como artefacto histórico y de
release, pero no participa en estos builds.

## Cambio causal

Se retiró exclusivamente esta propiedad de
`JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/library.properties`:

```text
precompiled=full
```

No se modificaron:

- `src/JWPLC_ModbusRTU.cpp`;
- `src/JWPLC_ModbusRTU.h`;
- `src/esp32/libJWPLC_ModbusRTU.a`;
- las APIs Arduino legacy, cooperativas o Sync;
- los defaults y candidatos RTU.

El archive conservado es:

```text
PATH=JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a
SHA256=486BE38AE088B94898E516FFBC125855F22C2EC5EE8A6FA9E10F35D7CAC3A3BE
ROLE=HISTORICAL_RELEASE_ARTIFACT
DEVELOPMENT_LINKAGE=FORBIDDEN
```

## Validación con MCP JWPLC

Se añadieron dos perfiles locales al `projects.toml` ignorado del MCP para que
`jwplc_build` compile directamente los firmware P5 Master y Slave. No se usó
Arduino CLI manual.

### Master

```text
MCP_PROJECT=platform-jwplc-p5-master
SKETCH=tools/modbus-tcp-benchmark/firmware/a14_p5_full_runtime_master
JWPLC_BUILD_CLEAN=YES
STATUS=PASS
EXIT_CODE=0
WARNINGS=0
ERRORS=0
FLASH_USED_BYTES=475625
RAM_USED_BYTES=39052
```

Evidencia de composición producida por el mismo build:

```text
MODBUS_RTU_SELECTED=YES
MODBUS_RTU_PRECOMPILED_MARKER_COUNT=0
MODBUS_RTU_SOURCE_OBJECT=JWPLC_ModbusRTU.cpp.o
MODBUS_RTU_SOURCE_OBJECT_BYTES=274788
MAP_SOURCE_OBJECT_HITS=339
MAP_ARCHIVE_HITS=0
BUILD_TREE_ARCHIVE_COPY_COUNT=0
```

### Slave

```text
MCP_PROJECT=platform-jwplc-p5-slave
SKETCH=tools/modbus-tcp-benchmark/firmware/a14_p5_rtu_slave
JWPLC_BUILD_CLEAN=YES
STATUS=PASS
EXIT_CODE=0
WARNINGS=0
ERRORS=0
FLASH_USED_BYTES=453477
RAM_USED_BYTES=34892
```

Evidencia de composición:

```text
MODBUS_RTU_SELECTED=YES
MODBUS_RTU_PRECOMPILED_MARKER_COUNT=0
MODBUS_RTU_SOURCE_OBJECT=JWPLC_ModbusRTU.cpp.o
MODBUS_RTU_SOURCE_OBJECT_BYTES=274788
MAP_SOURCE_OBJECT_HITS=339
MAP_ARCHIVE_HITS=0
BUILD_TREE_ARCHIVE_COPY_COUNT=0
```

## APIs públicas

Los firmware completos Master y Slave compilaron contra el mismo header público
sin modificarlo. Entre las rutas ejercitadas por contrato de compilación están:

- `begin()` / `end()`;
- `motor(ASYNC)`;
- `task()`;
- `requestReadHoldingRegisters()`;
- `masterBusy()` / `masterDone()` / `masterSucceeded()`;
- `masterResult()` / `clearMasterResult()`;
- configuración de frame gap, queued TX, bulk RX, CRC y framing;
- mapas Slave y estadísticas.

Por tanto:

```text
MODBUS_RTU_PUBLIC_HEADER_CHANGED=NO
MODBUS_RTU_LEGACY_API_BUILD=PASS
MODBUS_RTU_MASTER_BUILD=PASS
MODBUS_RTU_SLAVE_BUILD=PASS
MODBUS_RTU_ARCHIVE_LINKED=NO
MODBUS_RTU_SOURCE_COMPILED=YES
```

## Alcance

No se ejecutó `jwplc_flash` ni `jwplc_run_gate`. Este gate demuestra la
composición de build requerida; no repite la prueba física H3E-R ya aceptada.

```text
FLASH_EXECUTED=NO
PHYSICAL_GATE_EXECUTED=NO
NEXT=P4_2_COPY_OUT_64B_PLAN
```
