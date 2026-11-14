# A17 — Arranque de E/S y protección de EN_IO (14/11/2026)

Rama: `v2.1.0-alpha.17/JWPLC-Sim/feature/arranque-io-enable` → `v2.1.0-alpha.17/JWPLC-Sim/main`.
Pedido del propietario (12/11, decisión del 14/11): corregirlo dentro del cierre de alpha.17.

```text
A17_IO_ENABLE_FAULT_REPORT=PASS        (JWPLC Sim, falla simulada del TCA)
A17_IO_ENABLE_RETRY=PASS               (JWPLC Sim)
A17_IO_ENABLE_SERIAL1_GUARD=PASS       (JWPLC Sim)
A17_IO_ENABLE_CORE_A=PASS              (core.a regenerado, ABI solo agrega)
A17_IO_RULES_COMPILE=PASS              (ESP32: 5 sketches prohibidos fallan, 14 ejemplos compilan)
A17_IO_ENABLE_PHYSICAL=PENDING         (placa real: requiere taller)
```

## 1. Problema

Un estudiante reportó que los relés no respondían. Era un error de su código, pero la revisión (alpha.16) dejó dos
puntos débiles `[visto]`:

1. `initPeripherals()` (`cores/jwcontrol/peripherals_init.cpp`) pone EN_IO (GPIO27, `pins_is.h`) en 1 solo
   después de configurar el TCA6424A. Si `jwplcI2C_begin()` o el TCA fallaban hacía `return` sin aviso: EN_IO quedaba
   en 0 y los relés muertos para siempre.
2. Los pines por defecto de la UART1 del core son `RX1=26` (buzzer) y `TX1=27` (EN_IO). `Serial1.begin(baud)` sin
   pines tomaba EN_IO.

Que EN_IO quede en 0 mientras el expansor no esté configurado es correcto (salidas en estado seguro); lo que faltaba
era el aviso y la recuperación.

## 2. Cambios

| Archivo | Cambio |
|---|---|
| `cores/jwcontrol/peripherals_init.cpp/.h` | `jwplcStartExpander()` (TCA + bancos + EN_IO=1); si falla, `ets_printf` con el motivo por UART0 (sale aunque no se llame a `Serial.begin`) y `JWPLC_IO_FAULT_*`. `jwplcPeripheralsRetryIO()` y `jwplcSystemIOFault()`. |
| `cores/jwcontrol/main.cpp` | `jwplcSystemTask` reintenta el expansor cada `JWPLC_IO_RETRY_PERIOD_MS` (1000 ms) mientras `initialized` sea falso. Solo fallas del TCA: si falló el I2C tampoco arrancaron RTC/FRAM/SD/TFT y no se rehace ese arranque con el sketch corriendo. |
| `cores/jwcontrol/HardwareSerial.cpp` | JWPLC Basic: `Serial1.begin()` sin pines se rechaza con aviso; `begin()` y `setPins()` rechazan EN_IO en cualquier UART. |
| `cores/jwcontrol/jwplc_peripherals.cpp` | `pinMode(27, …)` y `digitalWrite(27, …)` se ignoran con un aviso (una vez). |
| `JWPLC_GlobalPeripherals` (`JWPLC_RuntimeView.h`) | `JWPLC_IO.fault()` → `JWPLC_IO_FAULT_NONE / _I2C / _TCA_INIT / _TCA_CONFIG`. |
| `precompiled/core/JWPLCBASIC/core.a` | Regenerado. SHA-256 `2d40cf05…ffe284c2`, 3 049 534 B. |

Tras recuperarse, el expansor arranca con las salidas en 0: un sketch que escribe una salida solo en `setup()` la
tiene que volver a escribir (el HAL de OpenPLC y `JWPLC_Runtime` escriben en cada ciclo).

Fuera de alcance: `analogRead(A17)` / `touchRead(T7)` siguen sin protección (no pasan por los wrappers JWPLC) y el
aviso en la TFT necesitaría tocar `JWPLC_Display` (`.a` ya regenerado en el cierre).

## 3. Evidencia

- `[ejecutado]` `core.a` desde fuente (arduino-cli, `jwplc_local:esp32:jwplcbasic`, arduino-esp32 3.3.8): antes del
  cambio el `core.a` versionado y el generado desde fuente tienen los mismos símbolos y `peripherals_init.cpp.o`
  idéntico (`movi.n a10, 27` → EN_IO=GPIO27 confirmado en el binario publicado). Después: solo se agregan
  `jwplcSystemIOFault` y `jwplcPeripheralsRetryIO`. Sin advertencias con `-Wall -Wextra` en los archivos tocados.
- `[ejecutado]` compilación ESP32 en modo normal (stub + `core.a`): `00.Plantilla_Proyecto_JWPLC`, `01.DigitalIO_Basic`,
  `06.IO_Estado_Escaneo` y una sonda con `Serial1`/`pinMode(27)`: compilan sin advertencias.
- `[ejecutado]` JWPLC Sim (`main` d69fd30, package en esta rama): `tests/examples/JWPLC_GlobalPeripherals` 19/19,
  `parity` 0 diferencias.
- `[ejecutado]` JWPLC Sim con el modelo TCA6424A parcheado en local (NACK en los 6 primeros START, sin commitear):

  ```text
  706 ms  [JWPLC] E/S NO DISPONIBLES: el TCA6424A no responde en I2C. EN_IO (GPIO27) queda en 0 ...
  707 ms  setup ready=0 fault=2
  1712 ms [JWPLC] E/S recuperadas: TCA6424A configurado y EN_IO (GPIO27) en 1.
  1712 ms DO Q0_0 = 1
  2000 ms ready=1 fault=0
  ```

- `[ejecutado]` sonda `Serial1.begin(9600)` / `Serial1.begin(9600, SERIAL_8N1, 4, 27)` / `pinMode(27)`: los tres avisos
  salen por UART0 y `Q0_0` sigue activándose.
- `[no verificado]` en placa: arranque normal, OpenPLC (`hardwareInit()` no cambia) y benchmark de compilación.

## 4. Restricciones de programación (pedido del propietario, 14/11)

Revisión `[visto]` de lo que hacía el core antes de este cambio:

| Llamada | Antes | Ahora |
|---|---|---|
| `Serial1` (cualquier uso) | compilaba; `begin()` sin pines tomaba GPIO27 | **error de compilación** (`static_assert` en `HardwareSerial.h`) |
| `digitalWrite(I0_x, v)` | escribía el registro de salida del banco 0 del TCA; como el banco está configurado como entrada no salía ningún pulso, pero el valor quedaba latente | **error de compilación**; con pin variable se ignora con aviso |
| `pinMode(I0_x, OUTPUT)` | **cambiaba la dirección del pin del TCA a salida**: el expansor empujaba la línea de la entrada (y con el valor latente de un `digitalWrite` previo) | **error de compilación**; con pin variable se ignora con aviso |
| `pinMode(Q0_x, INPUT)` | ponía el pin del TCA como entrada: el relé quedaba sin mando | **error de compilación**; con pin variable se ignora con aviso |
| `pinMode/digitalWrite(27, …)` | aviso en tiempo de ejecución (sección 2) | **error de compilación** |
| `pinMode(I0_x, INPUT)`, `pinMode(Q0_x, OUTPUT)` | escribía la dirección (redundante) | no hace nada: la dirección la fija el core al arrancar |
| `digitalRead(Q0_x)` | devuelve el estado comandado de la salida (shadow) | sin cambios: es lo que en ladder es un contacto de una bobina `%QX` |

Cómo: los macros `pinMode`/`digitalWrite` de `Arduino.h` agregan `static_assert` con `__builtin_constant_p`, que el
compilador resuelve también con `-O0` (JWPLC Sim) y que vale `false` para un pin en variable sin evaluar el resto. El
core crea `Serial1` con `JWPLC_CORE_ALLOW_SERIAL1`. `HardwareSerial mi(1)` sigue siendo posible y la protección de la
sección 2 lo cubre. Los ejemplos genéricos del ESP32 que usan `Serial1` (WiFi, Serial) ya no compilan en esta placa.

Evidencia `[ejecutado]` (ESP32, arduino-esp32 3.3.8):

```text
NegSerial1  Serial1.begin(9600)       -> error: JWPLC Basic: Serial1 no esta disponible ...
NegWriteI0  digitalWrite(I0_0, HIGH)  -> error: I0_x es una entrada; digitalWrite() solo acepta salidas Q0_x
NegModeI0   pinMode(I0_3, OUTPUT)     -> error: I0_x es una entrada; pinMode(I0_x, OUTPUT) no se permite
NegModeQ0   pinMode(Q0_2, INPUT)      -> error: Q0_x es una salida; pinMode(Q0_x, INPUT) no se permite
NegEnIo     digitalWrite(27, LOW)     -> error: GPIO27 es EN_IO (habilita los reles) y lo maneja el core
PosVarios   pinMode(Q0_0/I0_0) correctos, digitalRead(Q0_1), digitalWrite(I0_X[i]) en bucle, GPIO25/26 -> compila
```

Compilan sin advertencias: `JWPLC_GlobalPeripherals` 00–07, `03.Display_HMI_Pages`, `Expansion_Esclavo_Basic`,
`ModbusTCP` 01–02 y `RS485` 01–02.
