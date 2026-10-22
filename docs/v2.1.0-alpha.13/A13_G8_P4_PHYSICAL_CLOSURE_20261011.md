# Alpha13 G8-P4 — Cierre de validación física de dos JWPLC Basic

**Fecha local de ejecución:** 2026-10-11.  
**Gate:** `G8-P4=PHYSICAL_PASS_2JWPLC_12FC03_RX_EXACT`.  
**Resultado funcional de G8:** `CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS`.  
**Alpha13:** `IN_PROGRESS`; sin PR, merge, tag ni release.

## Procedencia de la evidencia

El operador ejecutó en Windows el BAT versionado
`tools/alpha13/gates/run_a13_g8_p4_dual_plc_physical.bat`,
después de sincronizar la rama
`v2.1.0-alpha.13/feature/cleanup-robustness`
a `e5d7f2ce0d780a421cd5dec9c14929fe4119b713`.

Evidencia de consola recibida:

```text
RUN_ID=g8_p4_20261011_223555_543f69c4
PORTS_VISIBLE=COM4,COM1,COM14
MASTER_PORT=COM4
SLAVE_PORT=COM14
MASTER_USB=CH340 VID=6790 PID=29987
SLAVE_USB=CH340 VID=6790 PID=29987
PREFLIGHT=PASS
COMPILE_SLAVE_EXIT=0
COMPILE_MASTER_EXIT=0
RTU_ARCHIVE_LINK_SLAVE=PASS_EXACT_L_PATH_L_FLAG_SHA
RTU_ARCHIVE_LINK_MASTER=PASS_EXACT_L_PATH_L_FLAG_SHA
PRODUCT_BUILD_SLAVE=PASS_PRECOMPILED_RTU_RS485_SOURCE
PRODUCT_BUILD_MASTER=PASS_PRECOMPILED_RTU_RS485_SOURCE
BUILD=PASS
FLASH_AUTHORIZATION=PASS_EXPLICIT_OPERATOR_INPUT
UPLOAD_SLAVE_COM14_EXIT=0
UPLOAD_MASTER_COM4_EXIT=0
DUAL_READY=PASS_MASTER_COM4_SLAVE_COM14
MASTER_STARTED=PASS_TOKEN_MATCH
MASTER_OK=12
MASTER_FAIL=0
MASTER_RXBYTES=156
MASTER_RXFRAMES=12
MASTER_TXFRAMES=12
MASTER_TIMEOUTS=0
MASTER_CRC=0
MASTER_READY=1
SLAVE_REQUESTS=12
SLAVE_RXBYTES=96
SLAVE_RXFRAMES=12
SLAVE_TXFRAMES=12
SLAVE_CRC=0
SLAVE_READY=1
PHYSICAL=PASS
MASTER_RX_BYTES=PASS_156
SLAVE_RX_BYTES=PASS_96
MODBUS_FC03=PASS_12_OF_12
FINAL_AUDIT=PASS
STATUS=PHYSICAL_PASS_2JWPLC_12FC03_RX_EXACT
GIT_OR_PRODUCT_CHANGE=NONE
FIRMWARE_RESTORE=NOT_AUTOMATED
```

Directorio de resultados reportado en el Windows del operador:
`tools/alpha13/results/g8_p4_20261011_223555_543f69c4/`.
Los archivos `MANIFEST.json` y `SERIAL_MASTER.log` /
`SERIAL_SLAVE.log` están previstos en dicho directorio local;
**no afirmar que se hayan subido a GitHub**, ni inventar una
verificación remota de su contenido. La salida de consola es
la evidencia recibida para el cierre.

## Interpretación verificable

| Criterio | Exigencia | Medición | Estado |
|---|---|---|---|
| UART real Master y Slave | `READY=1` ambos | 1 / 1 | PASS |
| Tráfico RTU FC03 | 12 lecturas válidas, sin fallos | 12 / 12, FAIL=0 | PASS |
| Master `rxBytes` no-bulk | 12 × 13 bytes | 156 | PASS |
| Slave `rxBytes` no-bulk | 12 × 8 bytes | 96 | PASS |
| Master frames RX/TX | 12 / 12 | 12 / 12 | PASS |
| Slave frames RX/TX | 12 / 12 | 12 / 12 | PASS |
| Solicitudes atendidas Slave | 12 | 12 | PASS |
| CRC Master/Slave | 0 / 0 | 0 / 0 | PASS |
| Timeout Master | 0 | 0 | PASS |
| Paquete productivo normal | 2 compilaciones, archive correcto | PASS ambas | PASS |
| Consentimiento y upload | confirmación explícita y 2 uploads | PASS ambas | PASS |
| Auditoría final | `FINAL_AUDIT_PASS` | PASS | PASS |

Se validó la corrección de doble conteo en Slave y contador
faltante en Master **con hardware real, en el modo RX no-bulk**
y tramas FC03 de longitud conocida. Las dos instancias
de `JWPLC_RS485.begin()` finalizaron correctamente;
`HardwareSerial::operator bool()` comprueba driver instalado
en la ruta positiva. Las ramas de error UART se cubrieron
mediante los tests C++ nativos previos (20 casos), no con
inyección destructiva de fallo UART en las dos placas.

Archive producto RTU precompilado verificado anteriormente:
`f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416`.
Commit conjunto de dos fuentes y archive:
[`fb71d65eb2c842b3f46e6949760e693f3b73d95e`](https://github.com/JW-Control/platform-jwplc/commit/fb71d65eb2c842b3f46e6949760e693f3b73d95e).
CI nativo tras commit:
[run 38191980973](https://github.com/JW-Control/platform-jwplc/actions/runs/38191980973).
CI de contrato gate físico Windows/Linux:
[run 38194119661](https://github.com/JW-Control/platform-jwplc/actions/runs/38194119661).

## Límites y pendientes separados

- Tráfico real medido: **12 intercambios FC03 115200 8N1**,
  Slave ID2, holding registers 0..3 y RX no-bulk.
  No afirmar que se hayan probado físicamente otros FC,
  modos bulk, velocidades, cables largos, EMC o estrés prolongado.
- Falla de instalación de UART y `begin/end/begin` cubiertos
  por fixtures nativos; **no se provocó una falla física de UART**.
- Las dos placas conservan los firmwares de diagnóstico cargados;
  **el gate no restauró el firmware previo**. Antes de retornarlas
  a servicio se requerirá una restauración/reprogramación
  explícita, separada y verificada.
- `F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING`.
  Este pase confirma comunicación y ejecución durante la ventana
  del gate; no prueba ausencia de bloqueos intermitentes posteriores.
- No se modificó código productivo ni se hizo nuevo commit de
  fuentes durante P4; se versiona únicamente el cierre documental.
- No hay autorización para PR, merge o publicación de Alpha13
  por el cierre de este gate.

```text
G8_P0=CLOSED_PASS
G8_P1P2=CLOSED_PASS_CANDIDATE_AND_ARCHIVE_PARITY
G8_P3=CLOSED_PASS_ADOPTION_AND_GIT_COMMIT
G8_P4=CLOSED_PASS_2JWPLC_FC03_REAL
G8_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
A13-010=CLOSED_PRODUCT_COMMITTED_AND_PHYSICAL_PASS
A13-011=CLOSED_PRODUCT_COMMITTED_POSITIVE_PHYSICAL_AND_NATIVE_NEGATIVE_PASS
ALPHA13=IN_PROGRESS
NEXT=RECONCILE_ALPHA13_REMAINING_GATES_AND_CHECKLIST
NO_NEW_PRODUCT_CHANGE=YES
RELEASE_MERGE_PR_PUBLICATION=NO
```
