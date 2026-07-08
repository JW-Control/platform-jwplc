# Handoff - JWPLC Remote I/O RTU v2.1.0-alpha.3

## Punto de partida real

Esta etapa corresponde a:

```txt
JWPLC Basic v2.1.0-alpha.3
branch: feature/v2.1.0-alpha.3-remote-io-rtu-poc1
base: main
```

La carpeta oficial de documentación en `platform-jwplc` es:

```txt
docs/v2.1.0-alpha.3/
```

Se mantiene la convención `alpha.x` porque ordena correctamente las etapas del package y coincide con el versionado usado por Arduino Boards Manager.

## Estado estable que no se debe romper

```txt
JWPLC Basic v2.0.0 estable publicada
Package Arduino: JW Control ESP32 Boards
FQBN principal: jwplc:esp32:jwplcbasic
```

Validado en la línea estable y alphas posteriores:

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
- No cambiar FlashFreq sin validación.
- No publicar bootloader definitivo.
- OpenPLC debe mantenerse como integración opcional o claramente documentada.

## Decisión técnica

Remote I/O principal:

```txt
Modbus RTU por RS-485
```

Arquitectura objetivo:

```txt
PC / OpenPLC Editor
        | USB / Serial0
        v
JWPLC Basic Master
        | RS-485 / Serial2
        v
JWPLC Basic Remote I/O Slave(s)
```

## Mapa operativo acordado para PoC 1

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

## Objetivo inmediato

Implementar la PoC 1:

```txt
JWPLC_RemoteIO_Slave_RTU
```

Mínimo esperado:

- ID fijo inicial `2`.
- Baudrate inicial `115200`.
- Formato inicial `8N1`.
- `Serial2` / RS-485 físico.
- Logs mínimos por `Serial0`.
- `FC2` lee entradas digitales reales.
- `FC15` escribe salidas digitales en bloque.
- `FC5` escribe una salida individual.
- `FC1` lee feedback de salidas.
- Salidas apagadas al arranque.
- Sin FRAM.
- Sin commissioning.
- Sin Modbus TCP.
- Sin cambios a `package_jwplc_index_dev.json` hasta tener ZIP final.

## Archivos de referencia

```txt
docs/v2.1.0-alpha.3/README.md
docs/v2.1.0-alpha.3/JWPLC_V2_1_0_ALPHA3_REMOTE_IO_RTU_NOTES.md
docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_PROTOCOL.md
docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_IMPLEMENTATION_PLAN.md
docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_POC1_CHECKLIST.md
```

## Prompt recomendado para continuar

```txt
Estoy continuando el desarrollo de JWPLC Remote I/O por Modbus RTU / RS-485 para JWPLC Basic v2.1.0-alpha.3.

Usa como base `platform-jwplc`, branch `feature/v2.1.0-alpha.3-remote-io-rtu-poc1`, y la documentación en `docs/v2.1.0-alpha.3/`.

Objetivo inmediato:
Implementar la PoC 1 del firmware `JWPLC_RemoteIO_Slave_RTU` con ID fijo, `Serial2`, `FC2`, `FC1`, `FC5` y `FC15`.

Restricciones:
- No romper el uso normal Arduino del package.
- No tocar `platform.txt` salvo bloqueante real.
- No quitar periféricos del autoload normal.
- No asumir OTA.
- No cambiar FlashFreq sin validación.
- Mantener OpenPLC como integración opcional o claramente documentada.
```
