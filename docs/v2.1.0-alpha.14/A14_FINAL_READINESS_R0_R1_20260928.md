# Alpha14 — Readiness final R0 y gate Arduino IDE R1

Fecha: `2026-09-28`

Rama:

```text
v2.1.0-alpha.14/feature/modbus-tcp
```

## R0 — resultado

```text
A14_FINAL_READINESS_R0=PASS
PRODUCT_FAILURE=NO
HARDWARE_FAILURE=NO
```

HEAD validado por el usuario:

```text
5919645077671afac244a07e6eb2fcabf3dd46ff
```

### Git / repository

```text
TRACKED_DIRTY_COUNT=0
STAGED_COUNT=0
UNTRACKED_COUNT=65
UNTRACKED_PRODUCT_COUNT=0
GIT_DIFF_CHECK=PASS
CONFLICT_MARKER_MATCHES=0
LOCAL_AHEAD_OF_REMOTE=0
LOCAL_BEHIND_REMOTE=0
RELEASE_BASE_IS_ANCESTOR=True
A14_R0_GIT_STATE=PASS
```

Los 65 untracked locales no estaban dentro de:

```text
JWPLC/2.1.0/
tools/modbus-tcp-benchmark/firmware/
```

por lo que no contaminaban package ni firmware de qualification.

### Artifacts finales

```text
core.a
SHA256=4BFF8C8241DA2E8BD0E1BBA99835ADDF91B9085A05C4DFBD05C339B824794566

libJW_SD.a
SHA256=E75BDE36481BF621DB37300ADEA7CF0D4A89B73ECE442A218135BD3E8E8AA5C1

libJWPLC_Display.a
SHA256=52B9BC617FACB77705161B4F07E6D45571043E4473934EFE19A1F5444BB5D986

libJWPLC_TFT.a
SHA256=5D860A131811DD9A7EB6FA55F5674B1D78B0DE7DFAF8748CE18A60CEED2D3738

libJWPLC_ModbusRTU.a
SHA256=486BE38AE088B94898E516FFBC125855F22C2EC5EE8A6FA9E10F35D7CAC3A3BE
```

```text
A14_R0_ARTIFACT_INVARIANTS=PASS
A14_R0_ETHERNET_NB3_PRODUCT_HASHES=PASS
A14_R0_W5500_SPI_HZ=26000000
```

### Autoservicio Modbus TCP

El source tiene cuatro llamadas textuales porque H3E.0B mantiene dos ramas
mutuamente excluyentes por cada punto lógico de servicio:

```text
PRE-LOOP:
  profiler enabled  -> callback
  profiler disabled -> callback

POST-LOOP:
  profiler enabled  -> callback
  profiler disabled -> callback
```

Resultado:

```text
CORE_AUTOSERVICE_TEXT_CALL_COUNT=4
CORE_AUTOSERVICE_PRE_LOOP_BRANCH_CALL_COUNT=2
CORE_AUTOSERVICE_POST_LOOP_BRANCH_CALL_COUNT=2
CORE_AUTOSERVICE_LOGICAL_SERVICE_POINTS=2
MASTER_MANUAL_TCP_TASK_CALL_COUNT=0
A14_R0_MODBUS_TCP_AUTOSERVICE_CONTRACT=PASS
```

### Arduino CLI

CLI observada:

```text
arduino-cli 1.0.2
Commit 33dfa8e8
2024-07-02
```

Compiles:

| Sketch | Resultado | Tiempo observado |
|---|---|---:|
| Modbus TCP Server público | PASS | 100.625 s |
| Modbus TCP Client público | PASS | 100.172 s |
| Full-runtime Master | PASS | 101.453 s |
| RTU Slave | PASS | 90.516 s |

Los cuatro generaron 4 binarios.

Selección:

```text
JWPLC_ModbusTCP repo path = PASS
JWPLC_ModbusRTU repo path = PASS
```

Full-runtime Master y RTU Slave:

```text
JWPLC_Display selected/precompiled = PASS
JWPLC_TFT selected/precompiled = PASS
JWPLC_ModbusRTU selected/precompiled = PASS

Display source object count = 0
TFT source object count = 0
ModbusRTU source object count = 0

external TFT_eSPI selected = False
external TFT_eSPI object count = 0
```

Final:

```text
A14_R0_MODBUS_TCP_SERVER_CLI=PASS
A14_R0_MODBUS_TCP_CLIENT_CLI=PASS
A14_R0_FULL_RUNTIME_MASTER_CLI=PASS
A14_R0_RTU_SLAVE_CLI=PASS
A14_R0_REPOSITORY_MUTATION=NO
A14_R0_RESULT=PASS
A14_FINAL_READINESS_R0=PASS
```

## Incidencia R0 — F068

Primer R0:

```text
CORE_AUTOSERVICE_HOOK_CALL_COUNT=4
expected=2
A14_FINAL_READINESS_R0=FAIL
```

Causa:

- el gate contó literales globales;
- el source tenía dos ramas mutuamente excluyentes alrededor de cada punto;
- existían 4 llamadas textuales pero sólo 2 puntos lógicos ejecutables por ciclo.

Clasificación:

```text
F068=R0_AUTOSERVICE_LITERAL_COUNT_FALSE_NEGATIVE
HARNESS_FAILURE=YES
PRODUCT_FAILURE=NO
HARDWARE_FAILURE=NO
```

Prevención:

- validar estructura semántica del bloque;
- separar pre-loop y post-loop;
- no inferir frecuencia de ejecución únicamente por conteo textual.

## R1 — Arduino IDE + upload físico final

R1 valida únicamente lo que R0/CLI no puede demostrar:

```text
ARDUINO_IDE_REAL_COMPILE
ARDUINO_IDE_REAL_UPLOAD
NORMAL_PACKAGE_AUTOLOAD
POST_UPLOAD_PHYSICAL_RUNTIME
```

Se reutiliza el gate físico integral histórico:

```text
tools/build-speed-benchmark/sketches/
06_alpha4_local_physical_gate/
06_alpha4_local_physical_gate.ino
```

No se renombra ni duplica porque ya es el gate de autoload normal reutilizado
en cierres posteriores.

### Cobertura R1

```text
Display / TFT
RTC
FRAM
microSD
botonera
8 entradas digitales
8 salidas / relés
```

Ethernet y RS-485/Modbus no se repiten dentro de este sketch porque Alpha14 ya
dispone de evidencia física posterior y más fuerte:

```text
H3E.5 full runtime = PASS
TCP + RTU simultaneous = PASS
Ethernet NB3 hashes unchanged = PASS
R0 public Modbus TCP examples compile = PASS
R0 full-runtime Master/Slave compile = PASS
```

### Procedimiento R1

En Arduino IDE:

```text
Board: JWPLC Basic
Port: COM14
Sketch:
tools/build-speed-benchmark/sketches/
06_alpha4_local_physical_gate/
06_alpha4_local_physical_gate.ino
```

1. Verify/Compile.
2. Confirmar compilación completada sin error.
3. Upload a COM14.
4. Abrir Monitor Serie a 115200 baud.
5. Completar las etapas interactivas del sketch:
   - 6 botones;
   - activar/desactivar 8 DI;
   - observar 8 relés;
   - responder Y si la secuencia física de DO fue correcta;
   - confirmar TFT y responder Y.

### Criterio final

Debe producir:

```text
ALPHA4_DISPLAY_READY=PASS
ALPHA4_RTC=PASS
ALPHA4_FRAM=PASS
ALPHA4_SD=PASS
ALPHA4_BUTTONS=PASS
ALPHA4_INPUTS=PASS
ALPHA4_OUTPUTS=PASS
ALPHA4_DISPLAY_VISUAL=PASS
ALPHA4_LOCAL_PHYSICAL_GATE=PASS
```

En Alpha14 esos markers históricos se reinterpretan como evidencia reutilizada:

```text
A14_R1_ARDUINO_IDE_COMPILE=PASS
A14_R1_ARDUINO_IDE_UPLOAD=PASS
A14_R1_NORMAL_AUTOLOAD_PHYSICAL=PASS
A14_FINAL_ARDUINO_IDE_GATE=PASS
```

No declarar los últimos cuatro markers hasta observar la compilación/upload de
Arduino IDE y el resultado físico final.
