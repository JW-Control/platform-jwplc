# Alpha13-G8-P0 — Cierre baseline reproducible Modbus RTU y RS-485

**Fecha:** 2026-10-11. **Estado:** `G8_P0=CLOSED_PASS_READ_ONLY`. **Alpha13 continúa**.

## Ejecución Windows entregada por operador

```text
RUN_ID=g8_p0_20261011_185223_e0c4e0cc
PREFLIGHT=PASS
SOURCE_CONTRACT=PASS
NORMAL_COMPILES=PASS_3_OF_3
FINAL_AUDIT=PASS
PRODUCT_OR_HARDWARE_CHANGE=NONE
CLI_VERSION=PASS
CLI_CORES=PASS
```

Sketches productivos compilados con Arduino CLI 1.5.1:

- `01.ModbusRTU_Slave_Holding`: compile exit 0, enlace a archive RTU con ruta y flag exactos, PASS.
- `02.ModbusRTU_Master_Read`: compile exit 0, enlace a archive RTU con ruta y flag exactos, PASS.
- `03.RS485_Status`: compile exit 0, `JWPLC_RS485.cpp.o` desde fuente, PASS.

## Defectos originales observados, sin corregir todavía

- **A13-010:** `_stats.rxBytes++` duplicado en Slave no-bulk; faltante en Master no-bulk.
- **A13-011:** `JWPLC_RS485Class::begin()` fija `_ready=true` sin validar inicio UART.
- `JWPLC_ModbusRTU` usa `precompiled=full`: exigir rebuild, paridad objeto/archive y prueba del linker tras parche.

```text
G8_P0=CLOSED_PASS_READ_ONLY
A13-010=PENDING_IMPLEMENTATION
A13-011=PENDING_IMPLEMENTATION
G8_PRODUCT_CHANGED=NO
G8_UPLOAD_SERIAL_FRAM=NONE
G8_NEXT=PREPARE_P1_P2_CANDIDATE_AND_FIXTURES
G7C=CLOSED_PASS
```

La salida de Windows reportada no prueba comportamiento runtime, no establece que el archive antiguo sea correcto ni equivale al cierre de G8. Los manifests detallados residen en `tools/alpha13/results/g8_p0_20261011_185223_e0c4e0cc` de Windows (ignorados por Git), y no se presupone acceso directo desde GitHub.

Referencia: [plan G8-P0](A13_G8_P0_MODBUS_RTU_RS485_BASELINE_PLAN_20261011.md).
