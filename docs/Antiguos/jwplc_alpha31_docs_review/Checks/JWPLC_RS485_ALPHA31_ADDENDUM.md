# JWPLC_RS485 - Addendum alpha31

Archivo destino sugerido:

```txt
JWPLC/JWPLC-2.0.0/libraries/JWPLC_RS485/README.md
```

## Cambio sugerido

Agregar después de la sección de ejemplos o antes de problemas comunes.

```md
## Validación alpha31

Para alpha31 se recomienda validar:

- `JWPLC_RS485.begin()`;
- `JWPLC_RS485.printStatus(Serial)`;
- envío básico;
- recepción básica;
- bridge USB ↔ RS-485;
- `RS485_Status`;
- `RS485_Basic_Send`;
- `RS485_Basic_Echo`;
- `RS485_USB_Bridge`;
- RX2 = IO16;
- TX2 = IO17;
- MAX13487 con auto-direccionamiento, sin DE/RE manual.

`JWPLC_RS485` no se auto-inicializa. Esto es intencional porque `Serial2` también puede ser usado por `JWPLC_ModbusRTU`.
```

## Bloque de estado sugerido

```md
## Estado

Documentación revisada para:

```text
JWPLC ESP32 2.0.0-alpha.31
JWPLC_RS485 1.0.0
```
```
