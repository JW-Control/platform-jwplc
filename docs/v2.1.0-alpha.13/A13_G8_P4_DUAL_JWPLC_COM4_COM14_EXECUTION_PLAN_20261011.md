# Alpha13 G8-P4 — Gate físico con dos JWPLC Basic

**Fecha:** 2026-10-11. **Estado:** PREPARADO, **NO EJECUTADO FÍSICAMENTE**.
**Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.
**Código productivo fijo:** `fb71d65eb2c842b3f46e6949760e693f3b73d95e`.

## Montaje confirmado por el operador

| Función | Placa | Puerto USB-CH340 | Programa |
|---|---|---|---|
| Master | JWPLC Basic 1 | COM4 | `g8_p4_master.ino` |
| Slave ID 2 | JWPLC Basic 2 | COM14 | `g8_p4_slave.ino` |

Ambos usan **115200, SERIAL_8N1**, Modbus RTU sobre el RS-485
integrado y consola de telemetría por USB a 115200. Conectar
**P con P y N con N** de las dos placas (**P=A, N=B** en JWPLC
Basic); no conectar P/N al 24 V, no duplicar innecesariamente
terminaciones/bias, y revisar condiciones de seguridad eléctrica
y topología del banco. Alimentar en banco aislado y **desconectar
actuadores/cargas de salidas**; no realizar upload mientras controlan
una máquina.

## Archivos versionados

```text
tools/alpha13/sketches/g8_p4_slave/g8_p4_slave.ino
tools/alpha13/sketches/g8_p4_master/g8_p4_master.ino
tools/alpha13/gates/a13_g8_p4_dual_plc_physical.py
tools/alpha13/gates/run_a13_g8_p4_dual_plc_physical.bat
tools/alpha13/gates/a13_g8_p4_contract_selftest.py
.github/workflows/alpha13-g8-p4-dual-plc-gate.yml
```

El flujo habitual será `git pull --ff-only` + el BAT. Este ejecutor
realiza preflight, dos builds normales del package (sin overlays),
comprobación de lib precompilada RTU y objeto RS485 fuente, enumeración
COM4/COM14 y prueba de origen USB CH340, y **pausa antes de upload**
para exigir la frase explícita de autorización mostrada en pantalla.
Después carga **primero Slave COM14, luego Master COM4**, abre ambas
consolas con DTR=RTS=False y valida los marcadores de rol y token.
No acciona relés desde los sketches de banco.

**Precaución:** los dos uploads sustituyen los programas existentes
en las placas. **No existe restauración automática de los anteriores
sketches**. Quien ejecute deberá confirmar que son placas
de pruebas y que el firmware anterior puede sobrescribirse.

Si el preflight o una compilación falla, no hace upload.
Si el upload del Slave falla, no carga Master.
Si falla tras un upload, el manifiesto lo registra y se detiene;
no intenta restaurar firmware de máquina ni simular un PASS.

## Contrato físico del gate

El PC envía una orden de inicio con token aleatorio por USB
únicamente al Master después de ver ambas señales READY. El Master
hace **12 solicitudes FC03 de 4 holding registers al Slave ID2**.
Mapa fijo de prueba:

```text
HR0=0x1234
HR1=0x2345
HR2=0x3456
HR3=0x4567
```

Cada request FC03 de cuatro registros tiene 8 bytes. Cada response
satisfactoria tiene 13 bytes. Por tanto, en ausencia de ruido y
tramas ajenas, y con la ruta RX no-bulk predeterminada:

```text
MODBUS_READS_SUCCESSFUL=12
MASTER_RX_BYTES_EXPECTED=12 * 13 = 156
SLAVE_RX_BYTES_EXPECTED=12 * 8 = 96
MASTER_FAIL=0
MASTER_CRC=0
MASTER_TIMEOUTS=0
SLAVE_CRC=0
MASTER_READY=1
SLAVE_READY=1
```

Requiere además contadores `rxFrames`, `txFrames` y
`requestsOk` suficientes. Un contador RX cero, duplicado o fuera
de la cuenta exacta conduce a REVIEW_STOP, no se tolera como PASS.
Las dos señales de UART READY no implican por sí mismas
comunicación física exitosa.

Logs por placa, compilaciones, uploads y manifest
`tools/alpha13/results/g8_p4_*/MANIFEST.json`.

### Condiciones de STOP

- Puerto incorrecto, no-CH340 o intercambiado, permiso de overwrite
  denegado, cableado no confirmado, FQBN/core/archive incorrecto.
- Build, upload, READY o token fallido; timeout de respuesta,
  valores de Holding incorrectos o contador CRC/bytes erróneo.
- Congelamiento post-upload/reinicio observado. No atribuirlo
  automáticamente al firmware ni ocultarlo como éxito.

La prueba se limita a lecturas FC03 sin comandos de escritura
de registros/DO; no habilita bulk RX, queued TX ni OpenPLC.

```text
G8_P4_DUAL_PLC_PLAN=READY_FOR_CI
G8_P4_MASTER=COM4
G8_P4_SLAVE=COM14
G8_P4_BAUD=115200_8N1
G8_P4_ID=2
G8_P4_RTU_FUNCTION=FC03
G8_P4_PHYSICAL=NOT_EXECUTED
G8_P4_UPLOAD_PERMISSION=REQUESTED_INTERACTIVELY_BEFORE_FLASH
G8_PHYSICAL_PASS=NOT_CLAIMED
G8_FUNCTIONAL_STATUS=IN_PROGRESS
```
