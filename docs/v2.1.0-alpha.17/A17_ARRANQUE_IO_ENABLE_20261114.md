# A17 — Arranque de E/S y protección de EN_IO (14/11/2026)

Rama: `v2.1.0-alpha.17/JWPLC-Sim/feature/arranque-io-enable` → `v2.1.0-alpha.17/JWPLC-Sim/main`.
Pedido del propietario (12/11, decisión del 14/11): corregirlo dentro del cierre de alpha.17.

```text
A17_IO_ENABLE_FAULT_REPORT=PASS        (JWPLC Sim, falla simulada del TCA)
A17_IO_ENABLE_RETRY=PASS               (JWPLC Sim)
A17_IO_ENABLE_SERIAL1_GUARD=PASS       (JWPLC Sim)
A17_IO_ENABLE_CORE_A=PASS              (core.a regenerado, ABI solo agrega)
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
