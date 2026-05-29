# Archivos que se deben conservar aparte

Fecha: 2026-05-23  
Etapa: `alpha32-openplc-integration`

## Objetivo

Listar qué archivos se deben conservar fuera del package estable `platform-jwplc v2.0.0`.

## Carpeta recomendada

Crear una carpeta aparte, por ejemplo:

```txt
OpenPLC_JWPLC_Basic_v2.0.0_integration/
```

Dentro de ella:

```txt
01_openplc_editor_patch/
02_examples/
03_docs/
04_validation_logs/
05_images/
06_tools_configs/
```

## 1. Patch de OpenPLC Editor

Guardar los archivos finales modificados:

```txt
editor/arduino/examples/Baremetal/hals.json
editor/arduino/src/hal/jwplcbasic.cpp
editor/arduino/examples/Baremetal/ModbusSlave.cpp
editor/arduino/examples/Baremetal/ModbusSlave.h
```

Motivo:

Estos archivos son la integración real con OpenPLC. No forman parte del package Arduino JWPLC estable.

## 2. Imagen preview

Guardar la imagen usada en `hals.json`, por ejemplo:

```txt
JWPLC_v200.png
jwplcbasic.png
```

Motivo:

OpenPLC Editor la usa para mostrar la placa en Device Configuration.

## 3. Proyecto ejemplo OpenPLC

Guardar al menos dos proyectos:

```txt
JWPLC_Basic_Blink_Q0_0/
JWPLC_Basic_IO_Test_I0_to_Q0/
```

Contenido mínimo recomendado:

- Blink en `Q0_0`.
- Entrada `I0_0` activa salida `Q0_0`.
- Variables mapeadas a `%IX0.0` y `%QX0.0`.
- Si se valida mapa completo, incluir `I0_0..I0_7` y `Q0_0..Q0_7`.

## 4. Configuraciones de ModbusTool

Guardar configuraciones exportadas para clase:

```txt
modbustool_jwplc_tcp_read_coils.json
modbustool_jwplc_tcp_read_discrete.json
modbustool_jwplc_tcp_write_coil.json
```

Si ModbusTool exporta en otro formato, conservarlo con nombre descriptivo.

## 5. Logs y evidencias

Guardar:

```txt
compile_log_openplc_jwplc.txt
upload_log_openplc_jwplc.txt
modbus_tcp_validation_log.txt
screenshots/
```

Capturas recomendadas:

- OpenPLC con `JWPLC BASIC [2.0.0]`.
- Debugger funcionando.
- ModbusTool leyendo coils.
- ModbusTool leyendo discrete inputs.
- Router mostrando IP/MAC.
- PowerShell con `TcpTestSucceeded : True`.

## 6. Documentación

Guardar:

```txt
openplc-integration-plan.md
openplc-io-map.md
openplc-architecture-review.md
openplc-validation-report.md
openplc-modbus-test-guide.md
openplc-alpha32-checklist.md
```

## 7. Pendientes técnicos

Mantener registro de:

```txt
RTU + TCP simultáneo: pendiente/parcial.
MAC fija de prueba: reemplazar o documentar.
JWPLC_MBTCP_DEBUG: dejar en 0 por defecto.
```

## 8. Qué NO guardar como modificación del package

No modificar ni guardar como parte de esta integración:

```txt
platform.txt
boards.txt
Arduino.h
peripherals_init.cpp
jwplc_peripherals.cpp
bootloader.bin
particiones
FlashFreq
```

## Conclusión

La integración OpenPLC debe conservarse como patch/documentación externa sobre OpenPLC Editor, manteniendo intacto el package Arduino estable del JWPLC Basic.
