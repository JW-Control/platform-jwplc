# Alpha12 — JWPLC Backplane: bus RS-485 configurable y multi-slot

## Base

```text
platform-jwplc
  rama:  v2.1.0-alpha.12/feature/openplc-engineering-closure
  base:  main @ aa0bfed1 (Alpha12 publicada, 2026-10-05)
  base anterior: release/v2.1.x @ 165f94fb (alpha11); rebase el 2026-10-06, ver §11

openplc-editor
  rama:  develop/alpha12-backplane-closure
  base:  integration/jwplc-alpha7-alpha12-upstream @ db3d05506
  STruCpp: v0.5.13 (sin cambios)
```

```text
VPP_SOURCE_BRANCH=v2.1.0-alpha.12/feature/openplc-engineering-closure
VPP_VERSION_BEFORE=2.1.0-alpha.19
VPP_VERSION_AFTER=2.1.0-alpha.20
VPP_PACKAGE_ID=com.jwcontrol.jwplc-basic
VPP_KEY_ID=jwcontrol-2026
VPP_SHA256=ad4a5411923c85b31c68f26391ccb0c5c9d2a6508c49420cf2dab434888d94bd  (build local, no reproducible byte a byte: Compress-Archive)
```

## 1. Modelo

```text
JWPLC Backplane (pantalla del VPP)
├── RS-485 Backplane            -> persistencia `backplane_rtu`
│   ├── baud_rate      "9600" | "19200" | "38400" | "57600" | "115200"   (default "115200")
│   └── serial_format  "8N1" | "8E1" | "8O1"                             (default "8N1")
│
├── Slot 1  JWPLC Basic v2.0.0 (fijo, I/O local)
├── Slot 2  JWPLC Basic Remote I/O  -> Slave ID
├── ...
└── Slot 8  JWPLC Basic Remote I/O  -> Slave ID
```

- El bus pertenece al controlador; cada slot solo guarda su Slave ID.
- No se reutiliza `Device > Modbus`: esa pantalla configura el servidor Modbus/debugger del propio JWPLC.
- Se persisten valores semánticos (`"8E1"`), nunca constantes del core Arduino.

## 2. Cadena de datos (única fuente de verdad)

```text
UI (form backplane_rtu)
 -> vendorScreenData.backplane_rtu
 -> guardar / reabrir proyecto
 -> generate-vpp-config (walker genérico del Editor)
 -> vpp_config.h:
      #define VPP_BACKPLANE_RTU_BAUD_RATE "38400"
      #define VPP_BACKPLANE_RTU_SERIAL_FORMAT "8E1"
 -> hal/jwplcbasic.cpp (constexpr -> JWPLC_MODBUS_BAUD / JWPLC_MODBUS_CONFIG)
 -> JWPLC_ModbusRTU.begin(247, baud, config) sobre Serial2
```

El formulario muestra los defaults pero solo guarda lo que el usuario edita. Por eso el HAL usa el mismo default:

| Estado del proyecto | Resultado en el HAL |
|---|---|
| Proyecto Alpha9, sin `backplane_rtu` | 115200 / 8N1 (igual que antes) |
| Solo se editó el baud | baud elegido / 8N1 |
| Valor fuera de la lista (proyecto importado/editado a mano) | **Error de compilación** (`static_assert`), nunca un fallback silencioso |

## 3. HAL multi-slot

- Hasta **7 módulos Remote I/O** (slots 2..8); el slot 1 es el controlador.
- Round-robin cooperativo, no bloqueante, desde `hardwareService()` y los hooks de scan existentes.
- Por slot en línea: `FC15` (salidas, snapshot) → `FC01` (feedback) → `FC02` (entradas) → siguiente slot.
- Un slot solo se carga si su Slave ID está en 1..247, no se repite y el allocator dejó sus 8 DI + 8 DO resolubles.

### Pérdida de comunicación (comportamiento industrial)

| Condición | Comportamiento |
|---|---|
| 1–2 fallos consecutivos (timeout/CRC/excepción) | Entradas conservan el último valor válido |
| 3 fallos consecutivos | Slot **fuera de línea**: sus `%IX` pasan a 0 (estado seguro); feedback inválido |
| Slot fuera de línea | Solo 1 sondeo `FC02` por vuelta (no multiplica timeouts del resto del bus) |
| Respuesta válida | Vuelve a línea automáticamente; el siguiente ciclo reescribe `FC15` |

Pendiente fuera de Alpha12: exponer el estado en línea/fuera de línea por slot al programa IEC (Alpha17 Diagnostics). El estado seguro de las **salidas** del slave ante pérdida del Master es responsabilidad del firmware del slave.

### Tiempo de ciclo del bus

Cada transacción puede tardar hasta `JWPLC_MODBUS_TIMEOUT_MS = 250 ms` si falla. Con N slots en línea el período de refresco por slot crece aproximadamente de forma lineal con N. Hay que medirlo en banco con el probe (`-DJWPLC_ALPHA7_RTU_TIMING_DIAGNOSTICS=1`) para 1, 2 y 3 slots y a 115200 y 38400.

## 4. Validación del Slave ID en el Editor

El codificador de bytes del Editor enmascara a 8 bits: un Slave ID 258 se habría emitido como 2. Alpha12 agrega:

- En la UI, un error junto al campo si está fuera de 1..247 o si se repite en otro slot.
- En la compilación, `validateModuleConfigValues()` **detiene el build** con un mensaje por slot si hay valores fuera de rango, no enteros o duplicados.
- Un Slave ID sin valor guardado toma el número de slot, igual que lo muestra la UI (`defaultFromSlot`).
- En el HAL, como última defensa, se descartan los duplicados y los valores fuera de rango.

## 5. Compatibilidad con el flujo Arduino

- Solo cambian `vpp/hal/jwplcbasic.cpp`, `vpp/screens/backplane.json` y `vpp/manifest.json`, archivos que solo se usan en builds de OpenPLC.
- No se tocan `platform.txt`, el core, el autoload ni las librerías `JWPLC_RS485` / `JWPLC_ModbusRTU`.
- Si no hay slots remotos, el Master RTU no se inicia y Serial2 queda libre.
- El probe de timing Alpha7.18 ahora está **desactivado por defecto**, así que Serial0 queda libre en producción.

## 6. Verificación ejecutada

Compilación real del HAL con `arduino-cli` + `jwplc:esp32:jwplcbasic` (core 2.1.0-alpha.11). El `vpp_config.h` se generó con el generador real del Editor.

| Caso | Resultado |
|---|---|
| Proyecto Alpha9 (3 slots, sin `backplane_rtu`) | PASS |
| Sin Backplane (`vpp_config.h` vacío) | PASS |
| Solo baud `9600` | PASS |
| `38400` / `8E1`, 3 slots | PASS |
| `38400` / `8E1`, probe de timing activado | PASS |
| Baud `250000` | FAIL esperado: `static_assert` baudrate |
| Formato `7N2` | FAIL esperado: `static_assert` formato |

Firma: `verifyPackageSignature` del Editor → `valid=true`. Con `hal/jwplcbasic.cpp` alterado → `Tampered file detected`.

Editor: `tsc` limpio y 79 suites / 2202 tests PASS (store, compile, vpp, iec-address, compiler, firmware, components).

## 7. Gates pendientes (hardware / E2E)

```text
BACKPLANE_RTU_BAUDRATE_UI=PENDING (verificar render en el Editor)
BACKPLANE_RTU_SERIAL_FORMAT_UI=PENDING
BACKPLANE_RTU_CONFIG_PERSISTENCE=PENDING (save -> cerrar -> reabrir)
BACKPLANE_RTU_CONFIG_HAL=PASS (compilación)
BACKPLANE_RTU_DEFAULT_COMPATIBILITY=PASS (compilación) / PENDING (hardware)
SLAVE_ID_SAVE/REOPEN/RECOMPILE=PENDING
RTU_DEFAULT_115200_8N1=PASS_PHYSICAL (2026-09-23, 1 esclavo: sketch JWPLC_RemoteIO_Slave_RTU y luego dispositivo JWPLC BASIC Remote IO desde OpenPLC)
RTU_ALTERNATE_PROFILE=PENDING_PHYSICAL (p. ej. 38400/8E1)
REMOTE_IO_MULTISLOT=PENDING_PHYSICAL (>= 2 slaves)
REMOTE_IO_OFFLINE_SAFE_STATE=PENDING_PHYSICAL (desconectar un slave -> %IX a 0, resto del bus sigue)
REMOTE_IO_MULTIBIT=PENDING_PHYSICAL (8 patrones)
```

## 8. Failsafe de salidas y tiempo entre escrituras (resuelto en VPP 2.1.0-alpha.21)

El sketch `JWPLC_RemoteIO_Slave_RTU` apagaba las salidas tras 100 ms sin FC05/FC15. Con varios slots, a baud bajo o con un slot fuera de línea, las salidas de los esclavos sanos podían parpadear.

| Lado | Cambio |
|---|---|
| Maestro (HAL) | Un fallo corta el ciclo del slot actual (no encadena FC01/FC02 contra un módulo que no responde). Los slots fuera de línea se sondean de a uno y como máximo cada `JWPLC_REMOTE_OFFLINE_PROBE_INTERVAL_MS = 1000`. Si todos están fuera de línea, el bus queda en reposo entre sondeos. |
| Esclavo (sketch) | `OUTPUT_FAILSAFE_MS = 1000`. |

Peor caso entre escrituras FC15 a un módulo sano: ciclo de los slots en línea + 1 timeout (≈ 420 + 250 ms con 7 slots a 9600), por debajo de 1000 ms. Un esclavo desconectado se detecta en 3 vueltas y se reconecta en ≤ 1 s tras volver.

```text
VPP_VERSION=2.1.0-alpha.21
VPP_SHA256=83fdd4b22d5193d31904959aceb72a86089996b836b4b2315671bfa7a3abfd20
```

## 9. Diferido fuera de Alpha12

- Commissioning del Slave ID por el bus (registros 224–240).
- Estado del slot visible desde IEC (Alpha17).

## 10. Esclavo Remote I/O programable desde OpenPLC (VPP 2.1.0-alpha.22)

Decisión del responsable (2026-09-23): se adelanta a Alpha12 para no depender del Arduino IDE en campo.

Nuevo dispositivo **`JWPLC BASIC Remote IO [2.0.0]`** en el mismo VPP.

> El nombre no lleva `/`: el editor usa el nombre del dispositivo como carpeta de build, y una `/` generaba `buildJWPLC BASIC Remote IO [2.0.0]` (corregido en alpha.23).
>
> Desde VPP 2.1.0-alpha.24 el dispositivo declara `capabilities.iecProgramOptional: true`, y el editor de la rama Alpha12 compila el esclavo **con `main` vacío**: completa en memoria una variable y un cuerpo ST no-op, sin modificar el proyecto. Con un editor sin ese cambio, `xml2st` exige al menos una variable y un rung en `main`.
>
> Usa un **proyecto separado** para cada esclavo. Si cambias la placa del proyecto del maestro, el Backplane no se pierde (se guarda por placa), pero el `main` del maestro usa alias del Backplane que no existen en el esclavo.

| Aspecto | Contrato |
|---|---|
| HAL | `hal/jwplcbasic-remoteio-slave.cpp`, con la misma lógica que el sketch validado `JWPLC_RemoteIO_Slave_RTU` (FC02 entradas, FC01 feedback, FC05/FC15 salidas) |
| Pantalla | `Remote I/O Slave` (persistencia `remote_io_slave`): Slave ID 1..247, Baudrate, Formato serie, Failsafe 1000/2000/5000 ms |
| Defaults | ID 2, 115200/8N1, 1000 ms (iguales al sketch) |
| Validación | Slave ID no entero o fuera de rango, baud, formato o failsafe no soportado → **error de compilación** |
| I/O | `pinMapping=false`: el programa IEC del esclavo no controla la I/O física; las salidas pertenecen al maestro |
| Failsafe | Mínimo 1000 ms (ver §8); sin escritura del maestro → Q0_0..Q0_7 en LOW |
| Carga | USB desde OpenPLC, como cualquier proyecto |

Verificación: HAL compilado con `vpp_config.h` del editor. Por defecto, ID 3 y 7/38400/8E1/2000 → PASS. ID 0, ID 300, ID 2.5 y failsafe 100 → FAIL esperado. Firma `valid=true`.

```text
VPP_VERSION=2.1.0-alpha.24
VPP_SHA256=2670e80771133aa41b3494698fdabf647698f7b9ac5780e8446376a2597da0fd
REMOTE_IO_SLAVE_FROM_OPENPLC=PASS_PHYSICAL (2026-09-23, 1 esclavo ID 2, 115200/8N1, main vacío, VPP alpha.24)
```

## 11. Rebase sobre la Alpha12 publicada y adaptación (2026-10-06)

La rama se rebasó de `release/v2.1.x @ 165f94fb` (alpha11) a **`main @ aa0bfed1`**, la Alpha12 publicada (#100 "cerrar comunicaciones, runtime y autocontención", sincronizada a `main` en #103/#104). El árbol de `main` es idéntico al de `release/v2.1.x @ 20f1b660` (`TREE_PARITY=True`, ver `ALPHA12_RELEASE_MAIN_TOPOLOGY_20261005.md`).

```text
BASE_BEFORE=165f94fb (release/v2.1.x, alpha11)
BASE_AFTER=aa0bfed1 (main, Alpha12 publicada)
CONFLICTS=0 (ningún archivo tocado por ambos lados)
ALPHA12_CODE_MODIFIED=0 (core y librerías intactos; del ejemplo JWPLC_RemoteIO_Slave_RTU solo el failsafe previo)
BACKUP=backup/openplc-engineering-closure-alpha11-0da901ae (local)
```

### Qué cambió en Alpha12 que afecta al Backplane

- `JWPLC_ModbusRTU`: selector de motor `SYNC`/`ASYNC` (`ASYNC` por defecto). La API unificada `read...()/write...()` bloquea o no según el motor; `request...()` queda como API explícita de compatibilidad. Timing interno en microsegundos. TX encolado, RX por bloques y despacho temprano del Slave quedan desactivados por defecto ("qualification").
- `JWPLC_RS485`: dirección del bus por hardware (`JWPLC_RS485_AUTO_DIRECTION=1`), buffer TX de 512 B y reloj APB forzado en ESP32.
- El core no usa `Serial2` ni `JWPLC_ModbusRTU` por su cuenta: el bus del Backplane sigue siendo exclusivo del HAL.

### Adaptación del HAL maestro (VPP 2.1.0-alpha.25)

| Cambio | Motivo |
|---|---|
| `motor(ASYNC)` explícito tras `begin()`; si falla, `end()` y el Master no se habilita | El scan PLC nunca debe esperar al bus. No depender del default del package |
| `requestWriteMultipleCoils` / `requestReadCoils` / `requestReadDiscreteInputs` → `writeMultipleCoils` / `readCoils` / `readDiscreteInputs` | API recomendada por Alpha12 para código nuevo. En `ASYNC` llama a la misma `request...()`: el comportamiento en el bus no cambia |
| `#error` si falta `jwplc_modbus_motor.h` | Con un package anterior a Alpha12 la compilación se detiene con un mensaje claro ("Actualizar desde el Board Manager") en vez de un error críptico |

Se mantiene sin cambios, a propósito:

- **Frame gap de 2 ms.** El Master cierra cada respuesta por longitud esperada (el gap solo se usa en excepciones), y el ESP32 entrega las tramas cortas del Remote I/O (≤ 10 bytes) en una sola ráfaga. Es el valor validado en banco. La librería indica cambiarlo solo si un equipo lo requiere.
- **TX encolado, RX por bloques y despacho temprano:** siguen en *qualification* en Alpha12; no se activan en un producto.
- **HAL esclavo y sketch `JWPLC_RemoteIO_Slave_RTU`:** usan `setCoils` / `setDiscreteInputs` / `task()` / `hasCoilWrite()` / `lastCoilWriteMs()`, que Alpha12 conserva y recomienda para fail-safe. Del sketch, la rama solo conserva el cambio previo del failsafe a 1000 ms (§5.2/§8); la adaptación a Alpha12 no lo toca.

```text
VPP_VERSION=2.1.0-alpha.25
VPP_KEY_ID=jwcontrol-2026
VPP_SHA256=11082e859cb0deaaf810d161c12d82cee02d480dd71de40d917ee6065dffe7ca
VPP_FILE=jwplc-basic-openplc-2.1.0-alpha.25.jwcontrol-signed.vpp
REQUIRES=jwplc:esp32 2.1.0-alpha.12 o superior
```

### Verificación

`arduino-cli` del editor con `jwplc:esp32 2.1.0-alpha.12` instalado desde el índice dev (contenido igual al árbol del repo, salvo fin de línea), sobre los builds reales del banco (`PruebasBlackplane/build`, generados por el editor):

| Caso | Resultado |
|---|---|
| Maestro: slot 2 Remote I/O, 115200/8N1 por defecto | PASS |
| Maestro: `38400` / `8E1` | PASS |
| Maestro: `38400` / `8E1` con probe de timing (`-DJWPLC_ALPHA7_RTU_TIMING_DIAGNOSTICS=1`) | PASS |
| Maestro: baud `250000` | FAIL esperado: `static_assert` baudrate |
| Maestro con la librería `JWPLC_ModbusRTU` de alpha11 | FAIL esperado: `#error` "requiere ... 2.1.0-alpha.12" |
| Esclavo OpenPLC: `main` vacío, valores por defecto | PASS |
| Esclavo OpenPLC: ID 3, `38400` / `8E1`, failsafe `"1000"` | PASS |
| Sketch Arduino `JWPLC_RemoteIO_Slave_RTU` | PASS |
| Firma del `.vpp` con `verifyPackageSignature` del editor | `valid=true` |
| `hal/jwplcbasic.cpp` alterado después de firmar | Rechazado (`Tampered file detected`) |

```text
ALPHA12_CORE_COMPILE=PASS (maestro, esclavo OpenPLC y sketch esclavo)
ALPHA12_CORE_PHYSICAL=PASS (2026-10-06, maestro <-> esclavo con core Alpha12 y HAL alpha.24)
VPP_ALPHA25_PHYSICAL=PENDING (importar alpha.25 y repetir maestro <-> esclavo)
```
