# Integración OpenPLC Editor v4 para JWPLC Basic

## Resumen

Este PR documenta y respalda la integración del **JWPLC Basic v2.0.0** con **OpenPLC Editor v4** mediante un patch externo.

La integración no modifica el package Arduino estable `platform-jwplc v2.0.0`. El uso normal desde Arduino IDE se mantiene sin cambios.

## Cambios principales

- Se agrega documentación de integración OpenPLC en:

```txt
docs/alpha32_openplc_integration/
```

- Se agregan archivos modificados de OpenPLC Editor en:

```txt
docs/alpha32_openplc_integration/open-plc-editor/
```

- Se agrega instalador Windows auditable para aplicar el patch:

```txt
docs/alpha32_openplc_integration/installer/install_openplc_jwplc_patch.bat
docs/alpha32_openplc_integration/installer/install_openplc_jwplc_patch.ps1
```

- Se documenta el mapa de I/O entre:
  - pines JWPLC Basic;
  - direcciones OpenPLC;
  - direcciones Modbus TCP.

- Se agrega guía de pruebas Modbus TCP/RTU.
- Se agrega reporte de validación funcional.
- Se agrega checklist de cierre de la etapa alpha32.

## Estado validado

Se validó correctamente:

- Reconocimiento de `JWPLC BASIC [2.0.0]` en OpenPLC Editor v4.
- Compilación desde OpenPLC usando Arduino CLI.
- Subida por USB al JWPLC Basic.
- Debugger de OpenPLC operativo.
- Lectura de entradas digitales `I0_0..I0_7`.
- Activación de salidas digitales `Q0_0..Q0_7`.
- Pin Mapping compatible con nombres JWPLC.
- Concordancia entre OpenPLC, E/S físicas y TFT.
- Modbus TCP sobre Ethernet W5500.
- DHCP funcional.
- Puerto TCP 502 validado.
- Pruebas con ModbusTool como master TCP.

## Limitación conocida

Modbus RTU y Modbus TCP fueron validados de forma independiente.

Estado actual:

```txt
RTU solo: funcional.
TCP solo: funcional.
RTU + TCP simultáneo: TCP funciona; RTU queda pendiente de revisión.
```

Esta limitación queda documentada y no bloquea el uso de OpenPLC con I/O digital ni Modbus TCP.

## Alcance

Este PR **sí** incluye:

- Documentación de integración.
- Patch externo para OpenPLC Editor v4.
- Scripts de instalación para Windows.
- Guías de validación.
- Registro de limitaciones conocidas.

Este PR **no** incluye:

- Cambios en `platform.txt`.
- Cambios en `boards.txt`.
- Cambios en `package_jwplc_index.json`.
- Cambios en el runtime normal Arduino del JWPLC Basic.
- Integración obligatoria de OpenPLC dentro del package Arduino.
- Definición de OTA.
- Cambios de FlashFreq o particiones.

## Archivos principales

```txt
docs/alpha32_openplc_integration/README.md
docs/alpha32_openplc_integration/openplc-integration-plan.md
docs/alpha32_openplc_integration/openplc-io-map.md
docs/alpha32_openplc_integration/openplc-architecture-review.md
docs/alpha32_openplc_integration/openplc-validation-report.md
docs/alpha32_openplc_integration/openplc-modbus-test-guide.md
docs/alpha32_openplc_integration/openplc-files-to-keep.md
docs/alpha32_openplc_integration/openplc-alpha32-checklist.md
docs/alpha32_openplc_integration/open-plc-editor/
docs/alpha32_openplc_integration/installer/
```

## Instalación del patch

La instalación puede hacerse de dos formas.

### Método manual

Copiar el contenido de:

```txt
docs/alpha32_openplc_integration/open-plc-editor/
```

sobre la carpeta correspondiente de OpenPLC Editor v4, respetando la estructura de carpetas.

### Método con script

Ejecutar:

```txt
docs/alpha32_openplc_integration/installer/install_openplc_jwplc_patch.bat
```

El script solicita la ruta de OpenPLC Editor, crea un backup automático y copia los archivos modificados.

## Validación recomendada

Después de aplicar el patch:

1. Abrir OpenPLC Editor v4.
2. Crear o abrir un proyecto.
3. Seleccionar `JWPLC BASIC [2.0.0]`.
4. Crear una variable `%QX0.0`.
5. Compilar y subir al JWPLC Basic.
6. Verificar salida física y TFT.
7. Habilitar Modbus TCP con DHCP.
8. Probar conexión al puerto `502`.
9. Leer coils desde ModbusTool u otro master Modbus TCP.

## Nota final

OpenPLC queda como integración externa/opcional para usuarios que quieran programar el JWPLC Basic desde OpenPLC Editor v4.

El flujo normal con Arduino IDE sigue siendo el flujo base y no se ve afectado por este PR.
