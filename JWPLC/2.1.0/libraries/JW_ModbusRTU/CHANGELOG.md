# Changelog

## [0.1.1] - 2026-10-22

### Corregido
- `rxBytes` contaba dos veces cada byte en el esclavo y no contaba en el maestro (lectura byte a byte). Ahora cuenta cada byte consumido, incluido el que desborda el buffer. Igual que `JWPLC_ModbusRTU` en `release/v2.1.x`.
- La prueba de equivalencia compara también `rxBytes` y usa como referencia `release/v2.1.x`.

## [0.1.0] - 2026-10-22

### Añadido
- Primera versión. Motor extraído sin cambios de lógica de `JWPLC_ModbusRTU` 1.0.0 (platform-jwplc 2.1.0-alpha.12).
- Clase instanciable `JW_ModbusRTU(JW_RS485&)`: una instancia por bus.
- Prefijos propios (`JW_MODBUS_*`, `JWModbus*`) para convivir con `JWPLC_ModbusRTU`.
- Motor seleccionable `JW_MODBUS_SYNC` / `JW_MODBUS_ASYNC`.
- Prueba N1 de equivalencia en host contra el `JWPLC_ModbusRTU.cpp` original (`extras/test`).
