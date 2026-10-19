# Alpha13 — H0-F1: SS Ethernet seguro desde el package (19/10/2026)

**Estado:** IMPLEMENTADO EN RAMA AISLADA / N0–N3 PENDIENTES DE CI / H NO AUTORIZADO.
**Base:** v2.1.0-alpha.13/feature/cleanup-robustness @ a9c96273274170195498ff282decf93f0388a450.
**Rama:** codex/alpha13-h0-f1-ss-pin-default-20261019.
**Registro existente:** F130 (proceso), F131 (defecto de producto). **NEXT_FAILURE_ID:** F132; no crear otro ID para el mismo incidente.
**Diagnóstico canónico:** A13_H0_ROOT_CAUSE_SS_PIN_20261019.md. **No cerrar H0 con CI sola.**

## Alcance de cambio aprobado

- ÚNICO archivo productivo modificado: JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp.
- F1: antes de seleccionar SS_PIN_DEFAULT, incorporar jwplc_spi_bus.h y definir PIN_SPI_SS_ETHERNET_LIB desde JWPLC_ETH_CS cuando la macro de Ethernet no exista.
- En ESP32, error de compilación si SS_PIN_DEFAULT cae en GPIO6–11.
- API pública, autoload, periféricos, variant, GPIO4, archives congelados, Alpha15 y hardware físico intactos. F2 descartada.
- El usuario final solo programa su sketch: nunca debe editar el variant ni configurar el CS integrado.

## Prueba dirigida y matriz

| Nivel | Control | Estado inicial |
|---|---|---|
| E1 previo | Recompilar desde la fuente anterior y consultar con GDB W5100Class::ss_pin en ELF, se espera 10 | PENDIENTE |
| E2 previo | CONFIG_ESPTOOLPY_FLASHMODE_QIO=1 en sdkconfig.h del core efectivo | PENDIENTE |
| N0 | Comparación con SHA base; diff de package solo w5100.cpp; cuatro negativos por eliminación de guardas | PENDIENTE CI |
| N1 | Compilar bloque literal SS del w5100.cpp anterior (10) y corregido (5) con variant SS static const = 5; error con -DPIN_SPI_SS_ETHERNET_LIB=10 | PENDIENTE CI |
| N2 | Build source-first del sketch linkStatus() en setup() en jwplcbasic y jwplcbasiccore; verificar .d, .o, ELF y biblioteca seleccionada | PENDIENTE CI |
| N3 | Compilar 11 sketches representativos DIRECTAMENTE desde el package de esta rama, sin overlay G10; auditar fuente→objeto→ELF | PENDIENTE CI |
| H | COM4 autorizado por el operador: primer READY PHY real, estabilidad 15 min, RJ45 OFF/ON, 10 power-cycle y 10 resets EN | NO AUTORIZADO |
| C | Estado, fallos, manifest y sincronización con base mediante PR aprobado | PENDIENTE |

Runner: tools/alpha13/gates/a13_h0_f1_test.py y a13_h0_f1_arduino.py.
Workflow: .github/workflows/alpha13-h0-f1-package.yml.
El runner escribe manifest.json y logs de compilación como artifact; no publica ELF, firmware, datos del banco ni secretos. No requiere USB/COM.

## Criterios de parada

- Si E1 o E2 no pueden verificarse, informarlos expresamente como PENDING/REVIEW. No extrapolar a PASS físico.
- Si el archivo productivo cambió fuera de w5100.cpp, o se altera un archive, GPIO4 o Alpha15: STOP.
- Si el ELF compila desde fuente incorrecta, no se declara PASS. Los PASS del overlay previos no sustituyen este N3 del package.
- Si cualquier prueba bloquea la compilación de un FQBN o sketch: REVIEW_STOP, corregir sobre la misma rama sin reabrir H0-A–F.

## Promoción G10 — PROPUESTA, NO IMPLEMENTADA

**PENDIENTE_DE_PROMOCIÓN:** JWPLC_W5500_SafeAccess.cpp/.h; EthernetClient.cpp; EthernetUdp.cpp; EthernetServer.cpp; socket.cpp; JWPLC_Ethernet.cpp; JWPLC_W5x00_Ethernet.h; JWPLC_ModbusTCP.cpp y JWPLC_ModbusTCP_Client.cpp, y cualquier dependencia efectiva detectada en diff del overlay de 14 fuentes.

Secuencia propuesta: (1) inventario exacto de parches compuestos B08 y eliminación de modificaciones solo diagnósticas; (2) promoción SafeAccess/leases bajo mutex SPI no recursivo, N1 negativos; (3) sockets TCP/UDP, DNS/DHCP y Modbus TCP según su dependencia, sin romper API; (4) N2/N3 del package INSTALADO y físico H autorizado por separado; (5) cierre con PENDIENTE_DE_PROMOCIÓN vacío y PR de integración aprobado.

No copiar al package marcas RTC H0, LabSocketSnapshot, eventos UART y amigos temporales DHCP/R3. Cualquier código de laboratorio no imprescindible se excluye de producto.

## Estado pendiente al abrir PR

H0_FIX_F1=IMPLEMENTED_ISOLATED_UNVERIFIED
H0_FIX_F2=DISCARDED
G10_PACKAGE_PROMOTION=PENDING_OVERLAY_ONLY
G10-R-R1_R2_R3_R4=NOT_RUN
G10-R-H=NOT_AUTHORIZED
ALPHA15=UNMODIFIED
NEXT_GATE=H0_F1_CI_N0_N1_N2_N3_THEN_REVIEW_E1_E2_THEN_EXPLICIT_COM4_AUTH
