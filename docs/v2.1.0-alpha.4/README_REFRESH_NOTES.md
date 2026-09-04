# Notas del refresh documental final de Alpha4

Este archivo registra el criterio usado para actualizar el `README.md` principal después de publicar y validar `v2.1.0-alpha.4`.

## Objetivo

El README general debe cumplir dos funciones, en este orden:

1. explicar con claridad qué aporta el package JWPLC a un usuario o cliente potencial;
2. permitir que un desarrollador profundice en la implementación y validación sin convertir la portada del repositorio en un informe de ingeniería.

Por ello, la prioridad del README es mostrar:

- ventajas del package frente a trabajar con un core ESP32 genérico;
- facilidad de instalación y programación;
- periféricos integrados;
- APIs de alto nivel;
- comunicaciones industriales;
- almacenamiento y HMI;
- mejoras reales de cada release, especialmente la optimización de compilación de Alpha4;
- estado de validación sobre hardware real.

## Estructura adoptada

La información se organiza en dos niveles.

### Nivel principal — usuario / cliente / integrador

Se muestran directamente:

- qué aporta el package;
- estado estable y PreRelease;
- mejora de compilación de Alpha4;
- instalación por Boards Manager;
- placas disponibles;
- E/S industriales;
- Display y botonera;
- RTC, FRAM y microSD;
- Ethernet;
- RS-485 y Modbus RTU;
- librerías JWPLC con enlace a sus README;
- OpenPLC como integración externa/opcional;
- resumen de validaciones físicas.

### Nivel técnico — desarrolladores del package

Los detalles de mantenimiento se conservan, pero quedan agrupados en secciones desplegables o documentos específicos:

- configuración de flash y particionado;
- `toolsDependencies` de Boards Manager;
- estrategia de precompilación;
- backend Ethernet W5x00;
- SPI compartido;
- app-only;
- bootloader;
- OTA;
- evidencias completas de benchmark y gates.

## Librerías

Se restauraron en el README general las referencias directas a la documentación de las librerías principales.

Librerías internas del package:

- `JWPLC_Display`;
- `JWPLC_Ethernet`;
- `JWPLC_RS485`;
- `JWPLC_ModbusRTU`.

Librerías JW distribuibles:

- `JW_FRAM`;
- `JW_RTC`;
- `JW_SD`;
- `JW_MatrixButtons`;
- `JW_DWIN_RS485` como complemento externo al runtime base.

## Información corregida respecto al README anterior

Se actualizaron datos que ya no representaban el estado vigente del package:

- `v2.1.0-alpha.3` dejó de ser la PreRelease actual;
- Alpha4 pasa a ser la PreRelease dev vigente;
- `JWPLC Basic` ya no usa `huge_app` como partición por defecto en Alpha4;
- el máximo de aplicación de JWPLC Basic pasa a `4,063,232 bytes`;
- los tiempos preliminares de alpha30 se sustituyen por el resultado formal P8;
- se registra la instalación standalone del package publicado;
- el checklist de publicación de Alpha4 queda cerrado.

## Documentación técnica complementaria

El detalle histórico y de ingeniería continúa disponible en:

```txt
tools/build-speed-benchmark/
docs/alpha32_openplc_integration/
docs/v2.1.0-alpha.4/
JWPLC/Test_Codes/
```

De esta forma el README permanece útil como presentación del package, mientras la evidencia técnica completa sigue versionada y accesible para mantenimiento y auditoría.
