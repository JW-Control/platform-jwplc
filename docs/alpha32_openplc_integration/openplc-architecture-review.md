# Revisión de arquitectura OpenPLC para JWPLC Basic

Fecha: 2026-05-23  
Etapa: `alpha32-openplc-integration`

## Resumen

La integración OpenPLC para JWPLC Basic se implementó como adaptación externa al OpenPLC Editor v4, sin modificar el package estable `platform-jwplc v2.0.0`.

El JWPLC Basic se comporta como target Arduino/Baremetal dentro de OpenPLC Editor.

## Arquitectura resultante

```txt
OpenPLC Editor v4
        ↓
Target Baremetal Arduino
        ↓
HAL JWPLC Basic: jwplcbasic.cpp
        ↓
Arduino API del package JWPLC
        ↓
Core jwcontrol + peripherals_init.cpp
        ↓
TCA6424A / W5500 / TFT / RTC / FRAM / SD / RS-485
```

## Flujo de inicialización

1. Arranca el core `jwcontrol`.
2. El core ejecuta `initPeripherals()`.
3. Se inicializan periféricos normales del JWPLC Basic.
4. Se configura TCA6424A y estado seguro de salidas.
5. OpenPLC entra a `setup()`.
6. `hardwareInit()` del HAL JWPLC no reinicializa E/S.
7. OpenPLC ejecuta el ciclo PLC y llama a `updateInputBuffers()` y `updateOutputBuffers()`.

## Ventajas

- No altera el comportamiento Arduino normal.
- Mantiene `v2.0.0` estable limpio.
- Evita duplicar inicialización de E/S.
- Evita tocar `EN_IO` desde OpenPLC.
- Usa las APIs Arduino ya validadas del package.
- El soporte OpenPLC queda opcional y documentado.

## Modbus TCP

OpenPLC Baremetal trae implementación propia de Modbus TCP en `ModbusSlave.cpp/.h`.

Para JWPLC Basic se requiere una adaptación porque el Ethernet físico es W5500 por SPI.

### Ruta incorrecta a evitar

```cpp
ETH.begin();
```

### Ruta correcta para JWPLC Basic

```cpp
#if defined(JWPLC_BASIC)
#include <JWPLC_Ethernet.h>
#endif
```

y uso de `EthernetServer` sobre W5500.

## Coexistencia SPI

El JWPLC Basic comparte SPI entre:

- W5500 Ethernet
- TFT ST7789
- FRAM
- microSD

Por eso es preferible usar la capa `JWPLC_Ethernet` en vez de llamar directamente a `Ethernet.begin(...)` desde OpenPLC.

## Estado de comunicaciones

| Modo | Estado |
|---|---|
| OpenPLC Debugger | Validado |
| Modbus TCP solo | Validado |
| Modbus RTU solo | Validado |
| Modbus TCP + RTU simultáneo | Parcial: TCP validado, RTU no trabaja en la prueba actual |

## Riesgos conocidos

### RTU + TCP simultáneo

Cuando se habilitan ambos modos, TCP funciona correctamente pero RTU no trabaja en la prueba actual.

Posibles causas:

- Conflicto con `Serial` usado por OpenPLC/debugger.
- Bloqueo o prioridad del ciclo TCP frente al serial.
- Configuración de interfaz RTU no orientada aún a RS-485 físico del JWPLC.
- Necesidad de separar `Serial` de programación/debug y puerto RS-485 real.

### MAC fija

La MAC `DE:AD:BE:EF:DE:AD` sirve para pruebas, pero no debe usarse en múltiples equipos simultáneos.

Pendiente:

- Definir si OpenPLC dejará MAC editable por usuario.
- Definir si se usará la MAC generada por `JWPLC_Ethernet`.
- Recomendar MAC única por equipo.

### Debug por Serial

`JWPLC_MBTCP_DEBUG` debe quedar desactivado por defecto.

Si se activa, puede interferir con Modbus RTU si ambos usan el mismo puerto serial.

## Conclusión

La integración es técnicamente correcta como target OpenPLC externo/opcional.

No se recomienda integrar OpenPLC directamente dentro del package Arduino estable en esta etapa.
