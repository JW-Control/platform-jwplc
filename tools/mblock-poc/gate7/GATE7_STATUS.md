# JWPLC Basic - mBlock - Gate 7 (fase 4: RS485, Ethernet y Modbus)

Fecha: 2026-10-24
Rama: `experiment/mblock-jwplc`
Package: 2.1.0-alpha.14 (`origin/v2.1.0-alpha.14/HmiDesigner/main` @ `eddee62e`) vía `jwplc_a14:esp32:jwplcbasic`.
Extensión: `jwplc_basic` 0.6.0. Bridge: 0.3.0.

## Estado

~~~text
GATE7A_LIBRARY_PHASE4=PASS              [ejecutado] headers _RS485, _ModbusRTU, _Ethernet, _ModbusTCP
GATE7B_PALETTE_PHASE4=PASS              [ejecutado] RS485 | Ethernet | Modbus visibles
GATE7C_CODEGEN_PHASE4=PASS              [ejecutado] 3 programas de prueba (A, B, C)
GATE7D_COMPILE_A14_FROM_MBLOCK=PASS     [ejecutado] A, B, C y regresión de la fase 1
GATE7E_UPLOAD_AND_PHYSICAL=PENDING      [no verificado] sólo compilación por indicación del propietario
~~~

## Bloques (categoría avanzada, mismo lenguaje sencillo)

| Categoría | Bloque | Código | Base en el package |
|---|---|---|---|
| RS485 | iniciar RS485 a [9600..115200] baudios | `JWPLC_RS485.begin(baud, SERIAL_8N1)` | `JWPLC_RS485` 1.1.0 (MAX13487E, dirección automática) |
| RS485 | enviar línea [x] por RS485 | `JWPLC_Line485.send(x)` | `println` |
| RS485 | cuando llegue una línea por RS485 (evento) | `if (JWPLC_Line485.available()) {...}` | lectura hasta `\n`, máx. 128 caracteres |
| RS485 | última línea recibida por RS485 | `JWPLC_Line485.line()` | |
| Ethernet | usar Ethernet con IP automática (DHCP) | `JWPLC_EthernetEasy.useDHCP()` | `JWPLC_Ethernet.useDHCP()` |
| Ethernet | usar Ethernet con IP [ip] y puerta de enlace [gw] | `useStaticIP(ip, gw)` | `setStaticIP` (máscara /24, DNS = puerta de enlace) |
| Ethernet | ¿Ethernet conectada? / IP de Ethernet | `ready()` / `ip()` | `isReady()` / `localIP()` |
| Modbus | poner registro [n] a [v] / valor del registro [n] | `JWPLC_Registers.set/get` | 16 registros, registro 1 = 40001 |
| Modbus | iniciar Modbus RTU esclavo con ID [id] a [baudios] | `JWPLC_ModbusEasy.beginSlave` + `JWPLC_ModbusRTU.task()` en `loop()` | `setHoldingRegisters` + `begin` |
| Modbus | iniciar Modbus RTU maestro a [baudios] | `beginMaster` (ID local 247, como los ejemplos) + `task()` | |
| Modbus | leer registro [n] del esclavo [id] | `readRegister(id, n)` (-1 si falla) | `readHoldingRegistersSync` (FC03, 1 s) |
| Modbus | escribir [v] en el registro [n] del esclavo [id] | `writeRegister(id, n, v)` | `writeSingleRegisterSync` (FC06, 1 s) |
| Modbus | iniciar servidor Modbus TCP | `JWPLC_ModbusTcpEasy.beginServer()` | `beginServer(1, 502)`; el core lo atiende solo |

Decisiones:

- **Registros compartidos:** los niños ven «registros 1..16»; el esclavo RTU y
  el servidor TCP publican el mismo arreglo como Holding Registers
  40001..40016, así un SCADA o un maestro puede leer y escribir lo mismo por
  RS485 o por Ethernet. Los valores se guardan como 0..65535 (redondeados).
- **Un header por función** (`_RS485`, `_ModbusRTU`, `_Ethernet`,
  `_ModbusTCP`): un programa sólo con RS485 no enlaza Modbus, y uno sólo con
  Ethernet no enlaza el servidor Modbus TCP (que se engancha al core con un
  hook cooperativo al enlazarse).
- **Ethernet:** los bloques de configuración deben ir en «cuando JWPLC Basic
  arranca» (`setup()`), que termina antes de que el runtime empiece la red,
  como indica `01.Ethernet_DHCP_Basic`.
- **Maestro RTU:** usa la API `...Sync` del package (bloqueante, máx. 1 s por
  petición, cediendo la CPU). Sin esclavo, cada lectura tarda 1 s y devuelve -1.
- **RS485 y Modbus RTU comparten el puerto:** no usar texto por RS485 y Modbus
  RTU en el mismo programa (documentado en los headers).

## Programas de prueba (`test/cdp-program-phase4.js`)

- **A:** Ethernet DHCP + servidor Modbus TCP + RS485 de texto. Cada 1 s: registro 1
  = segundos, IP en la línea 1, registro 2 en la línea 3, `t=...` por RS485; al
  llegar una línea por RS485 se muestra en la línea 2.
- **B:** Ethernet con IP fija 192.168.0.50 + Modbus RTU maestro: cada 1 s lee el
  registro 1 del esclavo 2 (línea 2) y escribe un contador en su registro 2.
- **C:** Modbus RTU esclavo ID 2: registro 1 = segundos; si el registro 2 es
  impar, Q0.0 encendida.

Código generado: `results/20261024_gate7/GATE7_PHASE4_{A,B,C}_GENERATED.ino`.

## Compilación

[ejecutado] Desde el renderer de mBlock, con los 6 headers enviados por la ruta del driver:

~~~text
A (RS485 + Ethernet + Modbus TCP)   438 217 B flash   95 s (primera vez)
B (Ethernet + Modbus RTU maestro)   431 837 B flash   93 s
C (Modbus RTU esclavo)              428 169 B flash   41 s
fase 1 (regresión)                  418 244 B flash   (igual que antes)
~~~

Log: `results/20261024_gate7/GATE7_COMPILE_A14.log`.

## Prueba física pendiente (Gate 7E)

1. **Modbus RTU:** programa C en una JWPLC y B en otra (RS485 A/B cruzados,
   115200 8N1). En B la línea 2 muestra los segundos del esclavo; en C, la línea 1
   «R2 = n» sube y Q0.0 conmuta cada segundo. Con una sola placa: C contra un
   maestro en la PC (adaptador USB-RS485 y Modbus Poll/QModMaster).
2. **Modbus TCP + RS485:** programa A con cable Ethernet. La línea 1 muestra la
   IP; desde la PC, leer 40001 (segundos) y escribir 40002 (se ve en la línea 3)
   con Unit ID 1 y puerto 502. Con un adaptador USB-RS485: se reciben las líneas
   `t=...` y una línea enviada desde la PC aparece en la línea 2.
3. **IP fija:** programa B muestra «IP 192.168.0.50» y responde a ping.

## Siguiente

- Integración con el HMI Designer (pantalla diseñada, campos por nombre).
- Bloques de Modbus para bits (coils / entradas discretas), por ejemplo
  publicar I0/Q0 directamente.
