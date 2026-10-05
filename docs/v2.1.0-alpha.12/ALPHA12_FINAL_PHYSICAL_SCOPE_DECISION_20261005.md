# Alpha12 — Decisión de cierre físico por alcance

Fecha: 2026-10-05

## Decisión

No se repite el gate físico completo de Display/RTC/FRAM/microSD/botonera/DI/DO
porque esos bloques ya cuentan con evidencia física cerrada en Alpha10/Alpha11 y
P8 no modificó sus runtimes.

La evidencia final se compone de:

```text
INHERITED_PHYSICAL_EVIDENCE=PASS
FINAL_ARDUINO_CLI_GATE=PASS
FINAL_COMPILE_REPRESENTATIVE_PHYSICAL_SKETCH=PENDING_ARDUINO_IDE
```

Para Arduino IDE basta con abrir y verificar/compilar:

```text
tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate/06_alpha4_local_physical_gate.ino
```

No se requiere upload ni ejecución física para este cierre.

## Protocolos

Modbus TCP:

```text
P8A_ALL_8_FC_LINK_SMOKE=PASS
SERVER_EXAMPLE_COMPILE=PASS
CLIENT_EXAMPLE_COMPILE=PASS
RUNTIME_BEHAVIOR_CHANGE_IN_P8=NO
PHYSICAL_RETEST=NOT_REQUIRED_BY_SCOPE
```

Modbus RTU:

```text
SLAVE_EXAMPLE_COMPILE=PASS
MASTER_READ_EXAMPLE_COMPILE=PASS
MASTER_WRITE_EXAMPLE_COMPILE=PASS
RUNTIME_CHANGE_IN_P8=NO
PHYSICAL_RETEST=NOT_REQUIRED_BY_SCOPE
```

## Regla

Si aparece un fallo en Arduino IDE al verificar el sketch representativo, el gate
se reabre. En ausencia de ese fallo, la evidencia física previa se considera
válida para Alpha12.
