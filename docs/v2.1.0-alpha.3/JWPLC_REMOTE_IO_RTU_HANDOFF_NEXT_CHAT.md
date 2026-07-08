# Handoff - Continuación JWPLC Remote I/O RTU

## Punto de partida

Venimos de revisar el ZIP:

```txt
documentación importada de Remote I/O RTU
```

El ZIP contenía:

```txt
JWPLC_REMOTE_IO_RTU_PROTOCOL.md
JWPLC_ALPHA13_REMOTE_IO_RTU_HANDOFF.md
```

Se generó una carpeta consolidada con documentación lista para versionar:

```txt
jwplc_remote_io_rtu_v2_1_0_alpha3_docs/
```

## Estado estable que no se debe romper

```txt
JWPLC Basic v2.0.0 estable publicada
Package Arduino: JW Control ESP32 Boards
FQBN principal: jwplc:esp32:jwplcbasic
```

Validado en `v2.0.0`:

- Arduino IDE.
- Arduino CLI.
- Subida USB.
- I/O por TCA6424A.
- Display TFT.
- Botonera.
- RTC.
- FRAM.
- microSD.
- Ethernet W5500.
- RS-485.
- Modbus RTU.
- Coexistencia SPI Ethernet + SD + FRAM + Display.

## Restricciones vigentes

- No romper `pinMode`.
- No romper `digitalRead`.
- No romper `digitalWrite`.
- No romper `digitalReadBlock`.
- No romper `digitalWriteBlock`.
- No romper `JWPLC_readInputs`.
- No romper `JWPLC_writeOutputs`.
- No romper `JWPLC_RS485`.
- No romper `JWPLC_ModbusRTU`.
- No quitar periféricos del autoload normal.
- No tocar `platform.txt` salvo bloqueante real.
- No asumir OTA.
- No cambiar FlashFreq sin justificación.
- No publicar bootloader definitivo.
- OpenPLC debe ser opcional o claramente documentado.

## Decisión técnica importada

Remote I/O principal:

```txt
Modbus RTU por RS-485
```

Arquitectura:

```txt
PC / OpenPLC Editor
        │ USB / Serial0
        ▼
JWPLC Basic Master
        │ RS-485 / Serial2
        ▼
JWPLC Basic Remote I/O Slave(s)
```

## Mapa operativo acordado

```txt
FC2  Discrete Inputs  0..7 = I0_0..I0_7
FC1  Coils readback   0..7 = feedback Q0_0..Q0_7
FC5  Write Coil       0..7 = Q0_x individual
FC15 Write Coils      0..7 = Q0_0..Q0_7
```

Identificación/configuración futura:

```txt
FC4 Input Registers    240..255 = identificación
FC3 Holding Registers  224..239 = configuración
```

## Rama a confirmar

Hay una diferencia de contexto entre documentos:

### Rama real para esta etapa

```txt
base:   main o rama actual posterior a v2.1.0-alpha.2
branch: develop/v2.1.0-alpha.3-remote-io-rtu
```

Usar si el repo ya tiene mergeado Backplane/VPP y discovery polish.

### Opción B - Continuación desde v2.0.0 estable

```txt
base:   main con v2.0.0 estable
branch: develop/alpha32-openplc-integration
```

Usar como referencia si se trabaja desde la línea estable o desde la alpha técnica anterior.

## Siguiente acción recomendada

Primero crear PR solo documental:

```txt
docs/openplc/JWPLC_REMOTE_IO_RTU_PROTOCOL.md
docs/openplc/JWPLC_REMOTE_IO_RTU_IMPLEMENTATION_PLAN.md
docs/openplc/JWPLC_REMOTE_IO_RTU_POC1_CHECKLIST.md
```

Commit sugerido:

```txt
docs(openplc): define JWPLC Remote I/O RTU protocol
```

## Después del PR documental

Implementar PoC 1:

```txt
JWPLC_RemoteIO_Slave_RTU
```

Mínimo:

- ID fijo.
- `Serial2` / RS-485.
- Modbus RTU Slave.
- `FC2` lee entradas.
- `FC15` escribe salidas.
- `FC5` escribe salida individual.
- `FC1` lee feedback de salidas.
- Logs mínimos por `Serial0`.
- Sin FRAM.
- Sin commissioning.

## Prompt recomendado para nuevo chat

```txt
Estoy continuando el desarrollo de JWPLC Remote I/O por Modbus RTU / RS-485 para JWPLC Basic.

Usa como base la carpeta de documentación `jwplc_remote_io_rtu_alpha13_review`, especialmente:

- `00_REVISION_IMPORTACION_ALPHA13_REMOTE_IO_RTU.md`
- `docs/openplc/JWPLC_REMOTE_IO_RTU_PROTOCOL.md`
- `docs/openplc/JWPLC_REMOTE_IO_RTU_IMPLEMENTATION_PLAN.md`
- `docs/openplc/JWPLC_REMOTE_IO_RTU_POC1_CHECKLIST.md`

Objetivo inmediato:
Crear primero un PR/documentación para fijar el protocolo Remote I/O RTU y luego implementar la PoC 1 del firmware `JWPLC_RemoteIO_Slave_RTU` con ID fijo.

Restricciones:
- No romper el uso normal Arduino del package.
- No tocar `platform.txt` salvo bloqueante real.
- No quitar periféricos del autoload normal.
- No asumir OTA.
- No cambiar FlashFreq sin validación.
- Mantener OpenPLC como integración opcional o claramente documentada.

Antes de codear, confirma si la rama real es:
- `develop/v2.1.0-alpha.3-remote-io-rtu` desde `v2.1.0-alpha.2 o main, según la rama que esté vigente`, o
- `develop/alpha32-openplc-integration` desde `main` con `v2.0.0` estable.
```

