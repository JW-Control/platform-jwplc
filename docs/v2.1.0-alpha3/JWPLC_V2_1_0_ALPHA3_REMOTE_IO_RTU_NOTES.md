# JWPLC Basic v2.1.0-alpha.3 - Remote I/O RTU docs

## Estado

Pre-release técnica propuesta para preparar **JWPLC Remote I/O por Modbus RTU / RS-485**.

Esta alpha no reemplaza a `v2.0.0` como canal estable público.

## Objetivo

Documentar el protocolo, mapa Modbus y plan de implementación para usar uno o más **JWPLC Basic** como módulos remotos de entradas/salidas conectados por RS-485 a un JWPLC Basic Master.

## Cambios incluidos

- Protocolo `JWPLC Remote I/O RTU v1`.
- Arquitectura PC/OpenPLC Editor ↔ JWPLC Master ↔ JWPLC Remote I/O Slaves.
- Mapa operativo inicial:
  - `FC2` para `I0_0..I0_7`.
  - `FC1`, `FC5` y `FC15` para `Q0_0..Q0_7`.
- Zona de identificación por `FC4` en registros `240..255`.
- Zona futura de configuración por holding registers `224..239`.
- Plan por PoC:
  - PoC 1: slave RTU mínimo con ID fijo.
  - PoC 2: FRAM y configuración persistente.
  - PoC 3: commissioning por UID/MAC.
  - PoC 4: integración con OpenPLC Editor / Backplane.
- Checklist de validación física para PoC 1.

## Decisiones

- Remote I/O se mantiene como integración opcional.
- No se modifica el runtime estable normal de Arduino por esta documentación.
- No se usan function codes custom `0x41..0x45` porque pueden chocar con usos internos/debug de OpenPLC.
- No se usarán registros altos tipo `1000`; el mapa se mantiene dentro de `0..255`.
- La MAC base del ESP32 será la identidad raíz del slave y se guardará en FRAM en etapas posteriores.

## No incluido

- Firmware final de Remote I/O Slave.
- Commissioning implementado.
- UI final en OpenPLC Editor.
- Modbus TCP.
- OTA.
- Cambios de `platform.txt`.
- Cambios de FlashFreq.
- Actualización del package index dev.

## Criterio para avanzar a firmware

Puede iniciarse la PoC 1 cuando queden versionados:

```txt
docs/openplc/JWPLC_REMOTE_IO_RTU_PROTOCOL.md
docs/openplc/JWPLC_REMOTE_IO_RTU_IMPLEMENTATION_PLAN.md
docs/openplc/JWPLC_REMOTE_IO_RTU_POC1_CHECKLIST.md
```

La PoC 1 debe validar un JWPLC Basic slave con ID fijo exponiendo entradas por `FC2`, salidas por `FC5/FC15` y feedback por `FC1` sin romper las APIs Arduino normales.
