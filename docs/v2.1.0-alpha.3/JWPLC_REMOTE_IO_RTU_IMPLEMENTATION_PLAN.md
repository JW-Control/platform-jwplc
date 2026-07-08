# JWPLC Remote I/O RTU - Plan de implementación

## Objetivo

Implementar y validar **JWPLC Remote I/O por Modbus RTU / RS-485** usando JWPLC Basic como módulo remoto de entradas/salidas.

La integración debe mantenerse opcional y no debe romper el uso normal Arduino del package.

## Ubicación

La documentación de esta etapa vive en:

```txt
docs/v2.1.0-alpha.3/
```

Branch de trabajo:

```txt
feature/v2.1.0-alpha.3-remote-io-rtu-poc1
```

## Alcance inicial

La primera implementación se limita a Remote I/O digital:

```txt
Entradas: I0_0..I0_7
Salidas:  Q0_0..Q0_7
Medio:    RS-485 / Serial2
Protocolo operativo: Modbus RTU estándar
```

Queda fuera de la primera PoC:

- Commissioning por UID.
- Cambio persistente de ID por FRAM.
- UI final de Backplane Scan en OpenPLC Editor.
- Modbus TCP.
- OTA.
- Cambios a `platform.txt`.
- Cambios de FlashFreq.
- Cambios a `package_jwplc_index_dev.json`.

## Arquitectura objetivo

```txt
PC / OpenPLC Editor
        | USB / Serial0
        v
JWPLC Basic Master
        | RS-485 / Serial2
        v
JWPLC Basic Remote I/O Slave(s)
```

## Etapas

### Etapa 0 - Documentación base

Crear/actualizar:

```txt
docs/v2.1.0-alpha.3/README.md
docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_PROTOCOL.md
docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_IMPLEMENTATION_PLAN.md
docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_POC1_CHECKLIST.md
docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_HANDOFF_NEXT_CHAT.md
docs/v2.1.0-alpha.3/JWPLC_V2_1_0_ALPHA3_REMOTE_IO_RTU_NOTES.md
```

Resultado esperado:

- Protocolo revisado.
- Mapa de registros definido.
- PoC 1 delimitada.
- Riesgos documentados.
- Convención `alpha.3` aplicada de forma consistente.

### Etapa 1 - PoC 1: Slave RTU mínimo con ID fijo

Objetivo:

```txt
Validar I/O físico real por RS-485 antes de integrar FRAM, commissioning o UI.
```

Características:

- Firmware `JWPLC_RemoteIO_Slave_RTU`.
- ID fijo inicial: `2`.
- Baudrate fijo inicial: `115200`.
- Formato inicial: `8N1`, salvo ajuste posterior.
- `Serial2` como bus RS-485.
- `Serial0` solo para logs mínimos si no interfiere.
- Sin FRAM.
- Sin commissioning.

Mapa mínimo:

| Función Modbus | Offset | JWPLC |
|---|---:|---|
| FC2 Discrete Inputs | 0..7 | `I0_0..I0_7` |
| FC1 Coils readback | 0..7 | feedback `Q0_0..Q0_7` |
| FC5 Write Single Coil | 0..7 | `Q0_x` individual |
| FC15 Write Multiple Coils | 0..7 | `Q0_0..Q0_7` |

Criterio de avance:

- Lectura real de entradas por RTU.
- Escritura real de salidas por RTU.
- Feedback de salidas consistente.
- Salidas apagadas al arranque.
- No rompe APIs Arduino normales.

### Etapa 2 - FRAM y configuración persistente

Objetivo:

```txt
Guardar identidad y configuración del slave de forma persistente.
```

Agregar:

- `magic`.
- `version`.
- `configured`.
- `slaveId`.
- `baudrate`.
- `parity`.
- `stopBits`.
- `mac[6]`.
- `uidCrc`.
- `configCrc`.

Mapa de identificación/configuración:

| Área | Rango | Uso |
|---|---:|---|
| Input Registers | 240..255 | Identificación solo lectura |
| Holding Registers | 224..239 | Configuración controlada |

Criterio de avance:

- MAC obtenida del ESP32.
- MAC guardada en FRAM.
- ID operativo guardado en FRAM.
- Reinicio conserva configuración.
- CRC detecta configuración inválida.

### Etapa 3 - Commissioning por UID/MAC

Objetivo:

```txt
Detectar y configurar varios slaves conectados simultáneamente aunque estén sin configurar o tengan IDs duplicados.
```

Agregar:

- Modo `JWPLC RTU Commissioning`.
- Scan por ventanas anti-colisión.
- Respuesta con UID/MAC/modelo/estado.
- Asignación de ID por UID.
- Verificación posterior por Modbus RTU estándar.

Comandos Serial0 iniciales:

```txt
JWPLC_SCAN_RTU baud=115200
JWPLC_ASSIGN_UID uid=AABBCCDDEEFF id=2
JWPLC_VERIFY_RTU id=2
JWPLC_LIST_RTU
```

Criterio de avance:

- Detecta al menos 4 slaves en el bus.
- Asigna IDs sin depender de broadcast Modbus estándar.
- Recupera caso de ID duplicado.
- Verifica cada equipo por FC4 luego de asignar.

### Etapa 4 - Integración con OpenPLC Editor / Backplane

Objetivo:

```txt
Permitir que el editor detecte Remote I/O y lo asigne a slots del Backplane.
```

Agregar en editor:

- Botón `Scan JWPLC RTU Devices`.
- Lista de slaves detectados.
- Asignación UID -> Slot/ID.
- Generación de configuración Remote Device RTU.
- Visualización de MAC/UID.

Criterio de avance:

- Slot 1 local conserva `%IX0.0` / `%QX0.0`.
- Slot 2 remoto usa `%IX1.0` / `%QX1.0`.
- La configuración generada coincide con IDs reales en bus.

## Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| Colisiones de slaves sin configurar | No usar broadcast Modbus estándar; usar commissioning por UID/MAC. |
| Confundir VPP con firmware | Documentar que VPP describe hardware, pero el comportamiento real vive en firmware. |
| Romper Arduino normal | Mantener integración opcional y validar APIs base. |
| Rango de registros alto incompatible | Mantener mapa operativo dentro de `0..255`. |
| UI antes de validar hardware | Bloquear avance a editor hasta completar PoC 1 física. |

## Archivos a revisar antes de codear

En `platform-jwplc`:

```txt
JWPLC_RS485
JWPLC_ModbusRTU
JW_FRAM
variants/jwplcbasic/pins_is.h
cores/jwcontrol/jwplc_peripherals.h
cores/jwcontrol/jwplc_peripherals.cpp
```

En `openplc-editor`:

```txt
resources/sources/Baremetal/ModbusSlave.h
resources/sources/Baremetal/ModbusSlave.cpp
Remote Device editor/generator
Backplane/VPP moduleSystem
```

## Regla de cierre de etapa

No cerrar ninguna etapa sin documentar:

- qué se probó;
- con qué hardware;
- qué comandos se usaron;
- qué quedó fuera;
- qué pendiente bloquea o condiciona la siguiente etapa.
