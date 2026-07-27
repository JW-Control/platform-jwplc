# v2.1.0-alpha.3 - Alpha 3 Release

Lanzamiento consolidado que incluye todas las últimas integraciones hasta `openplc-installer`, además de correcciones de estabilidad y nuevas pruebas de estrés.

## Novedades y Correcciones Integradas

- **Corrección de Bloqueos Modbus RTU:** Se resolvió un problema crítico de sincronización en `JWPLC_ModbusRTU` que podía bloquear la ejecución (`fix-modbus-blocking`).
- **Nuevas Funcionalidades Ethernet:**
  - Pruebas continuas de estrés por capas con TFT integradas.
  - Diagnóstico HTTP con soporte de TFT, FRAM, RTC y botonera.
- **OpenPLC y VPP:**
  - Integración del instalador OpenPLC.
  - Limpieza y refactorización de descubrimiento JWPLC.
  - Soporte preparatorio para OpenPLC Editor 4.2.7 y Backplane VPP.
- **Documentación:** Limpieza y actualización general de guías para Remote I/O RTU.
