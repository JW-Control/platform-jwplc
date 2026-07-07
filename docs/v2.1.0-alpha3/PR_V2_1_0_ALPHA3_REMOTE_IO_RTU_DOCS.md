# PR sugerido - docs: prepare remote io rtu docs for v2.1.0-alpha.3

## Resumen

Este PR agrega la documentación base para preparar **JWPLC Remote I/O por Modbus RTU / RS-485** como parte de la etapa `v2.1.0-alpha.3`.

## Cambios

- Agrega protocolo `JWPLC Remote I/O RTU v1`.
- Define el mapa inicial de entradas/salidas por Modbus RTU.
- Documenta el plan de implementación por PoC.
- Agrega checklist de validación física para PoC 1.
- Registra el alcance y límites de `v2.1.0-alpha.3`.

## Decisiones

- Remote I/O se mantiene opcional.
- No se modifica `platform.txt`.
- No se actualiza todavía `package_jwplc_index_dev.json`.
- No se toca `package_jwplc_index.json` público.
- No se asume OpenPLC integrado en el runtime normal.

## No incluido

- Firmware final de slave RTU.
- Commissioning por UID/MAC implementado.
- Integración visual final en OpenPLC Editor.
- ZIP de package `v2.1.0-alpha.3`.

## Checklist

- [ ] Documentación en `docs/openplc` agregada.
- [ ] Notas de etapa agregadas en `docs/releases/v2.1.0-alpha.3`.
- [ ] README principal revisado si corresponde.
- [ ] Confirmado que no se modifica el package público estable.
- [ ] Confirmado que no se rompe el flujo Arduino normal.
