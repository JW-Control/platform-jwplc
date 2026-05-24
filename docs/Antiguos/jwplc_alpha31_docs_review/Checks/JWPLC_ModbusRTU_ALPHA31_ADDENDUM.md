# JWPLC_ModbusRTU - Addendum alpha31

Archivo destino sugerido:

```txt
JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/README.md
```

## Nota visible recomendada

Agregar cerca del inicio, después de la descripción principal.

```md
> OpenPLC no está integrado todavía. `JWPLC_ModbusRTU` provee la base Modbus RTU propia del package JWPLC y puede servir para futuras pruebas de integración OpenPLC/Modbus, pero alpha31 no incluye OpenPLC real.
```

## Validación alpha31

Agregar antes de la sección de futuro OpenPLC.

```md
## Validación alpha31

Para alpha31 se recomienda validar:

- `ModbusRTU_CRC_Test`;
- `ModbusRTU_Slave_HoldingRegisters`;
- `ModbusRTU_Master_ReadHoldingRegisters`;
- `ModbusRTU_Master_WriteSingleRegister`;
- CRC16 con vector conocido;
- slave holding registers;
- master read;
- master write;
- prueba física por RS-485 si hay equipo externo disponible;
- `JWPLC_RS485` no inicializado simultáneamente con otra configuración incompatible.

OpenPLC queda fuera de alpha31.
```

## Bloque de estado sugerido

```md
## Estado

Documentación revisada para:

```text
JWPLC ESP32 2.0.0-alpha.31
JWPLC_ModbusRTU 1.0.0
```
```
