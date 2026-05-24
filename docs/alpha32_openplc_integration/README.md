# Integración OpenPLC Editor v4 para JWPLC Basic

Este directorio contiene la documentación y los archivos necesarios para habilitar el soporte del **JWPLC Basic v2.0.0** dentro de **OpenPLC Editor v4**.

La integración se entrega como un **patch externo para OpenPLC Editor**. No modifica el package Arduino estable `platform-jwplc v2.0.0`.

## Estado validado

- OpenPLC Editor reconoce `JWPLC BASIC [2.0.0]`.
- Compilación desde OpenPLC usando Arduino CLI.
- Subida por USB al JWPLC Basic.
- Debugger de OpenPLC operativo.
- Pin Mapping para `I0_0..I0_7` y `Q0_0..Q0_7`.
- Lectura de entradas digitales.
- Activación de salidas digitales.
- Concordancia entre lógica OpenPLC, E/S físicas y TFT.
- Modbus TCP sobre Ethernet W5500.
- DHCP, puerto TCP 502 y pruebas con ModbusTool como master TCP.

## Limitación conocida

Modbus RTU y Modbus TCP fueron validados de forma independiente:

```txt
RTU solo: funcional.
TCP solo: funcional.
RTU + TCP simultáneo: TCP funciona; RTU queda pendiente de revisión.
```

Esta limitación no bloquea el uso de OpenPLC con I/O digital y Modbus TCP.

## Contenido principal

| Archivo / carpeta | Descripción |
|---|---|
| `open-plc-editor/` | Archivos modificados para copiar sobre la instalación de OpenPLC Editor. |
| `installer/` | Scripts para aplicar el patch con backup automático. |
| `openplc-integration-plan.md` | Plan y decisiones de integración. |
| `openplc-io-map.md` | Mapa entre I/O JWPLC, direcciones OpenPLC y Modbus. |
| `openplc-architecture-review.md` | Revisión de arquitectura y riesgos. |
| `openplc-validation-report.md` | Reporte de validación funcional. |
| `openplc-modbus-test-guide.md` | Guía de pruebas Modbus TCP/RTU. |
| `openplc-alpha32-checklist.md` | Checklist de cierre alpha32. |

## Instalación manual

1. Instalar OpenPLC Editor v4.
2. Instalar Arduino IDE y el package `JW Control ESP32 Boards` versión `2.0.0`.
3. Cerrar OpenPLC Editor.
4. Hacer backup de la carpeta de OpenPLC Editor.
5. Copiar el contenido de:

```txt
docs/alpha32_openplc_integration/open-plc-editor/
```

sobre la carpeta raíz de OpenPLC Editor, respetando la estructura de carpetas.

6. Abrir OpenPLC Editor.
7. Crear o abrir un proyecto.
8. Ir a `Device > Configuration`.
9. Seleccionar:

```txt
JWPLC BASIC [2.0.0]
```

## Instalación con script

También se puede usar:

```txt
docs/alpha32_openplc_integration/installer/install_openplc_jwplc_patch.bat
```

El script solicita la ruta de instalación de OpenPLC Editor, crea un backup automático y copia los archivos modificados.

## Prueba rápida recomendada

1. Crear una variable `RELAY` tipo `BOOL` en `%QX0.0`.
2. Crear lógica Ladder de blink o una salida controlada por entrada.
3. Compilar y subir al JWPLC Basic.
4. Verificar el estado en TFT.
5. Para Modbus TCP:

```txt
IP: IP asignada por DHCP
Port: 502
Slave ID: 0
Function: Read coils
Start Address: 0
Size: 8
Display Format: LED
```

## Importante

Esta integración no significa que OpenPLC forme parte obligatoria del runtime Arduino normal. El JWPLC Basic sigue siendo programable normalmente desde Arduino IDE usando el package `platform-jwplc`.
