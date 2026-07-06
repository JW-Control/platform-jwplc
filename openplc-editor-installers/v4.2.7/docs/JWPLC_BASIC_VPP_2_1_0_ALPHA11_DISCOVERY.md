# JWPLC Basic VPP v2.1.0-alpha.11 - OpenPLC Editor 4.2.8 JWPLC Edition

## Estado

VPP regenerado, firmado, instalado y validado con OpenPLC Editor - JWPLC Edition 4.2.8-jwplc.1.

## Package

- Package ID: com.jwcontrol.jwplc-basic
- Package version: 2.1.0-alpha.11
- Board: JWPLC BASIC [2.0.0]
- FQBN: jwplc:esp32:jwplcbasic
- Target: JWPLC Basic v2.0.0

## Objetivo de esta alpha

Agregar soporte de experiencia completa para depuración Modbus TCP usando Ethernet W5500, DHCP, IP estática y descubrimiento de dispositivo desde la interfaz del editor.

## Cambios del VPP

### Pantalla Modbus

Se añadieron campos específicos para JWPLC Basic:

- JWPLC Device Discovery
- Debug IP Address

### Modbus TCP

Se validó el uso de:

- Ethernet - W5500.
- IP estática.
- DHCP.
- Puerto TCP 502.
- Debugger Modbus TCP.

### Fallback de depuración

El debugger usa Debug IP Address como IP principal para Modbus TCP.

Si Debug IP Address queda vacío, se puede usar como respaldo la IP configurada en Modbus TCP > IP Address.

## Flujo validado

1. Instalar VPP firmado.
2. Seleccionar JWPLC BASIC [2.0.0].
3. Activar Modbus TCP.
4. Seleccionar Ethernet - W5500.
5. Activar DHCP o configurar IP estática.
6. Compilar y subir al JWPLC Basic.
7. Buscar el dispositivo con JWPLC Device Discovery.
8. Usar la IP detectada para Debug IP Address.
9. Iniciar debugger.
10. Confirmar lectura de variables en tiempo real.

## Checklist de validación

- [x] VPP instalado en Package Manager.
- [x] Board Settings visible.
- [x] Imagen del JWPLC Basic visible.
- [x] Specs visibles.
- [x] Pin Mapping visible.
- [x] Modbus RTU Serial0 validado.
- [x] Modbus RTU Serial2 / RS-485 validado.
- [x] Modbus TCP con IP estática validado.
- [x] Modbus TCP con DHCP validado.
- [x] JWPLC Device Discovery validado.
- [x] Debugger TCP validado con Debug IP Address.
- [x] Debugger RTU validado con Debug Port.

## Notas técnicas

El VPP no modifica por sí mismo el core Arduino. El soporte runtime depende de los archivos Baremetal del editor y del package Arduino instalado.

El discovery no asigna IPs automáticamente. Detecta dispositivos accesibles por red local mediante conexión TCP al puerto Modbus configurado.

## Decisiones

- No asumir OpenPLC integrado dentro del package Arduino.
- No definir OTA.
- No fijar FlashFreq final.
- No publicar bootloader.bin como definitivo.
- Mantener periféricos JWPLC Basic integrados.
- Mantener compatibilidad Arduino IDE.

## Resultado

JWPLC Basic puede usarse desde OpenPLC Editor - JWPLC Edition con flujo funcional de compilación, subida, Modbus RTU, Modbus TCP, DHCP y discovery.
