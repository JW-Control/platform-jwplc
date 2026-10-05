# Alpha14 — Client FC15/FC16 parser compile PASS

Fecha: 2026-09-11

Branch: `v2.1.0-alpha.14/feature/modbus-tcp`

## Resultado

```text
SERVER_REGRESSION_EXIT=0
CLIENT_FC15_FC16_PARSER_COMPILE_EXIT=0
A14_2_SERVER_AFTER_FC15_FC16_PARSER=PASS
A14_2_CLIENT_FC15_FC16_PARSER_COMPILE=PASS
CORE_PATCH_COMMITTED=NO
FC15_FC16_RUNTIME=NOT_EXECUTED
MBLOCK_POC_TOUCHED=NO
```

## Tamaño del probe

```text
Sketch uses 405425 bytes (9%) of program storage space.
Global variables use 29044 bytes (8%) of dynamic memory.
```

El probe previo a añadir la validación de respuesta FC15/FC16 usaba `405349` bytes de programa y `29044` bytes de RAM global, por lo que el parser añade `76` bytes de programa y no cambia la RAM global del probe.

## Alcance

La rama de parser añadida para `OP_WRITE_MULTIPLE_COILS` y `OP_WRITE_MULTIPLE_REGISTERS` valida la respuesta Modbus estándar de 12 bytes:

```text
Function Code = esperado
Start Address = solicitado
Quantity = solicitada
```

No se considera todavía evidencia de runtime físico. El siguiente gate es consolidar únicamente el patch del parser y después ejecutar FC15 + FC16 sobre una única conexión TCP persistente.
