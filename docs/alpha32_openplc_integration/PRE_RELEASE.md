# v2.1.0-alpha.1 - Integración OpenPLC Editor v4

## Resumen

Esta pre-release inicia la etapa **alpha32-openplc-integration** del proyecto `platform-jwplc`.

El objetivo de esta etapa es habilitar el uso del **JWPLC Basic v2.0.0** como target externo dentro de **OpenPLC Editor v4**, sin alterar el flujo normal de programación desde Arduino IDE.

La integración se entrega como un **patch externo para OpenPLC Editor v4**.

## Estado de la release

```txt
Tipo: Pre-release
Rama base: develop/alpha32-openplc-integration
Estado: validación funcional inicial completada
Package Arduino estable relacionado: v2.0.0
OpenPLC Editor objetivo: v4
```

## Importante

Esta pre-release **no reemplaza** al package Arduino estable `platform-jwplc v2.0.0`.

El JWPLC Basic sigue siendo programable normalmente desde Arduino IDE. OpenPLC queda como una integración externa/opcional.

## Cambios incluidos

- Soporte del JWPLC Basic como board seleccionable en OpenPLC Editor v4.
- Archivos modificados de OpenPLC Editor documentados en:

```txt
docs/alpha32_openplc_integration/open-plc-editor/
```

- Documentación técnica de integración.
- Mapa de I/O para OpenPLC.
- Guía de pruebas Modbus.
- Reporte de validación.
- Checklist de cierre alpha32.
- Instalador Windows auditable para aplicar el patch.

## Estado validado

Se validó correctamente:

- OpenPLC Editor reconoce `JWPLC BASIC [2.0.0]`.
- Compilación desde OpenPLC usando Arduino CLI.
- Subida por USB al JWPLC Basic.
- Debugger de OpenPLC operativo.
- Pin Mapping funcional.
- Lectura de entradas digitales `I0_0..I0_7`.
- Escritura de salidas digitales `Q0_0..Q0_7`.
- Concordancia entre OpenPLC, E/S físicas y TFT.
- Modbus TCP sobre Ethernet W5500.
- DHCP funcional.
- Puerto TCP `502` funcional.
- Comunicación con master Modbus TCP externo usando ModbusTool.

## Mapa I/O validado

| JWPLC Basic | OpenPLC | Modbus TCP |
|---|---|---|
| `I0_0` | `%IX0.0` | Discrete Input 0 |
| `I0_1` | `%IX0.1` | Discrete Input 1 |
| `I0_2` | `%IX0.2` | Discrete Input 2 |
| `I0_3` | `%IX0.3` | Discrete Input 3 |
| `I0_4` | `%IX0.4` | Discrete Input 4 |
| `I0_5` | `%IX0.5` | Discrete Input 5 |
| `I0_6` | `%IX0.6` | Discrete Input 6 |
| `I0_7` | `%IX0.7` | Discrete Input 7 |
| `Q0_0` | `%QX0.0` | Coil 0 |
| `Q0_1` | `%QX0.1` | Coil 1 |
| `Q0_2` | `%QX0.2` | Coil 2 |
| `Q0_3` | `%QX0.3` | Coil 3 |
| `Q0_4` | `%QX0.4` | Coil 4 |
| `Q0_5` | `%QX0.5` | Coil 5 |
| `Q0_6` | `%QX0.6` | Coil 6 |
| `Q0_7` | `%QX0.7` | Coil 7 |

## Instalación del patch

### Método manual

1. Instalar OpenPLC Editor v4.
2. Instalar Arduino IDE y el package JWPLC Basic v2.0.0.
3. Cerrar OpenPLC Editor.
4. Copiar el contenido de:

```txt
docs/alpha32_openplc_integration/open-plc-editor/
```

sobre la carpeta local de OpenPLC Editor, respetando la estructura de carpetas.

### Método con script

Ejecutar:

```txt
docs/alpha32_openplc_integration/installer/install_openplc_jwplc_patch.bat
```

El script:

- solicita la ruta de OpenPLC Editor;
- valida la estructura destino;
- crea un backup automático;
- copia los archivos modificados;
- muestra un resumen final.

## Limitación conocida

Modbus RTU y Modbus TCP fueron validados de forma independiente.

Estado actual:

```txt
RTU solo: funcional.
TCP solo: funcional.
RTU + TCP simultáneo: TCP funciona; RTU queda pendiente de revisión.
```

Esta limitación queda documentada y será revisada en una etapa posterior.

## No incluido en esta pre-release

Esta pre-release no incluye:

- Cambios en `package_jwplc_index.json`.
- Cambios en `package_jwplc_index_dev.json`.
- Cambios en `platform.txt`.
- Cambios obligatorios al autoload normal de periféricos.
- Cambios de FlashFreq.
- Cambios de particiones.
- OTA.
- Integración de OpenPLC como runtime obligatorio del package Arduino.

## Archivos/documentos principales

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

## Recomendación de uso

Para clases, talleres o laboratorios, se recomienda preparar previamente:

- OpenPLC Editor v4 instalado.
- Arduino IDE instalado.
- Package JWPLC Basic v2.0.0 instalado.
- Patch OpenPLC aplicado.
- Proyecto de prueba cargado.
- ModbusTool u otro master Modbus TCP disponible para validación.

## Nota final

Esta pre-release confirma que el JWPLC Basic puede trabajar con OpenPLC Editor v4 como entorno alternativo de programación, manteniendo intacto el uso normal desde Arduino IDE.

La integración queda marcada como externa, opcional y documentada.
