# JWPLC ESP32 v2.1.0-alpha.3

Pre-release interna del package Arduino **JW Control ESP32 Boards** para **JWPLC Basic**.

Esta alpha está enfocada en la consolidación del instalador de OpenPLC, pruebas de estrés de Ethernet con TFT y correcciones críticas en Modbus RTU.

---

## Resumen

`v2.1.0-alpha.3` consolida múltiples características e integraciones desarrolladas recientemente:

- Integración del instalador OpenPLC y preparación para Backplane VPP.
- Validación robusta de Ethernet mediante pruebas continuas de estrés por capas (HTTP, TFT, FRAM, RTC, botonera).
- Corrección de bloqueos de sincronización críticos en `JWPLC_ModbusRTU`.
- Reorganización de la documentación de Remote I/O RTU.

---

## Cambios principales

### OpenPLC y VPP
- Integración del instalador de OpenPLC Editor 4.2.7.
- Limpieza y refactorización del descubrimiento JWPLC.
- Preparación del soporte Backplane VPP.

### Ethernet
- Se agregaron pruebas continuas de estrés por capas con TFT integradas (`feature/ethernet-stress-tft-v3`).
- Diagnóstico HTTP completo validando simultáneamente TFT, FRAM, RTC y botonera.

### Modbus RTU
- Se solucionó un problema crítico de sincronización en `JWPLC_ModbusRTU` que podía causar bloqueos intermitentes o pérdida de comunicación.
- Se agregó el código de validación específico `ModbusRTU_Tool_Test.ino`.

### Documentación
- Reorganización y limpieza de la documentación asociada a Remote I/O RTU.

---

## Códigos internos de prueba

Se agregaron códigos de prueba adicionales para validación en campo, incluyendo:

- `ModbusRTU_Tool_Test.ino` (para validar correcciones del bus Modbus).
- Ejemplos de diagnóstico HTTP y TFT para validación concurrente.

---

## Limitaciones / decisiones mantenidas

Esta alpha no cambia las decisiones principales del package establecidas en versiones anteriores:

- No se integra OpenPLC de forma destructiva al package Arduino estable.
- No se define OTA.
- No se fija Flash Frequency final más allá de la configuración ya validada.
- No se publica `bootloader.bin` como definitivo.
- No se eliminan periféricos del autoload normal por velocidad.

---

## Canal de instalación

Para validar esta alpha desde Boards Manager, usar el índice dev:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Para desarrollo local antes de publicar, usar:

```txt
jwplc_local:esp32:jwplcbasic
```

---

## Estado

```txt
Estado: Pre-release
Versión: v2.1.0-alpha.3
Tipo: alpha técnica
Tema: OpenPLC / Ethernet / Modbus
Validación: hardware real JWPLC Basic
```

Esta alpha se da por consolidada y documentada, lista para compilar su ZIP y actualizar el índice del canal dev.
