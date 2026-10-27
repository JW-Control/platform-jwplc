# Referencia rápida JWPLC Basic (package 2.1.x)

Funciones que se usan en la plantilla y dónde ver un ejemplo completo **sin internet**
(Arduino IDE → Archivo → Ejemplos → *librería* → *ejemplo*). Los objetos `JWPLC_*` ya existen:
el package los crea e inicia, no hay que declararlos.

## Hardware

```text
E/S digitales   8 entradas I0_0..I0_7 · 8 salidas a relé Q0_0..Q0_7 (expansor TCA6424A)
Botonera        BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_ESC, BTN_OK, BTN_DOWN
Pantalla        TFT 320x170 (pantalla IDLE del package o pantallas del HMI Designer)
Memoria         FRAM (no volátil, sin desgaste) y microSD
Reloj           RTC con batería
Comunicaciones  RS485 (Modbus RTU) y Ethernet W5500 (Modbus TCP)
Buzzer          integrado, no bloqueante
```

## Entradas y salidas · ejemplo `JWPLC_GlobalPeripherals/01.DigitalIO_Basic`

| Función | Qué hace |
|---|---|
| `uint8_t JWPLC_readInputs()` | Las 8 entradas en un byte (bit 0 = I0_0) |
| `void JWPLC_writeOutputs(uint8_t b)` | Escribe las 8 salidas (bit 0 = Q0_0) |
| `uint8_t JWPLC_readOutputs()` | Estado actual de las salidas |
| `digitalRead(I0_3)` / `digitalWrite(Q0_2, HIGH)` | Acceso de a una (más lento que por bloque) |
| `JWPLC_IO.input(i)` / `JWPLC_IO.output(i)` | Último estado leído por el runtime, sin acceso extra |
| `bitRead(v, n)` / `bitWrite(v, n, x)` | Leer o escribir un bit de un byte |

## Botonera · `JWPLC_GlobalPeripherals/02.Buttons_Basic`

| Función | Qué hace |
|---|---|
| `JWPLC_Buttons.pressed(BTN_OK)` | `true` una vez por pulsación |
| `JWPLC_Buttons.released(BTN_OK)` | `true` una vez al soltar |
| `JWPLC_Buttons.isDown(BTN_OK)` | Tecla mantenida ahora |
| `JWPLC_Buttons.clearPendingInput()` | Olvida pulsaciones anteriores (al arrancar) |

No llamar `JWPLC_Buttons.update()`: el package escanea la botonera.

## Pantalla IDLE · `JWPLC_Display/01.Display_IDLE_Status`

| Función | Qué hace |
|---|---|
| `JWPLC_Display.setRunLed(bool)` | Indicador RUN |
| `JWPLC_Display.setErrCode("EMG")` | Código ERR (1 a 4 caracteres A-Z/0-9; `""` = sin error) |
| `JWPLC_Display.setIdleWakeMode(IDLE_WAKE_DISABLED)` | Las teclas no abren la pantalla de usuario |
| `JWPLC_Display.setBusLedAuto(true)` / `setEthLedAuto(true)` | BUS y ETH en diagnóstico automático |

Pantallas propias: usar el **JWPLC HMI Designer** (ejemplos `JWPLC_Display/05.HMI_Plantilla_Caldera`,
`06.HMI_Botonera_Interactiva`, `07.HMI_Iconos_Planta`). Redibujar sólo lo que cambia.

## Reloj · `JWPLC_GlobalPeripherals/03.RTC_Basic`

| Función | Qué hace |
|---|---|
| `JWPLC_Time.valid()` | La hora es válida |
| `JWPLC_Time.hour()`, `.minute()`, `.second()`, `.day()`, `.month()`, `.year()` | Fecha y hora (sin acceso extra al bus) |
| `JWPLC_RTC.readTemperatureC(t)` | Temperatura interna del RTC |

## FRAM · `JWPLC_GlobalPeripherals/04.FRAM_Basic`

| Función | Qué hace |
|---|---|
| `JWPLC_FRAM.size()` | Bytes disponibles (0 = no hay FRAM) |
| `JWPLC_FRAM.writeBlock(dir, estructura, version)` | Guarda una struct con versión y verificación |
| `JWPLC_FRAM.readBlock(dir, estructura, version)` | `false` si está vacía, dañada o de otra versión |
| `JWPLC_FRAM.writeCString(dir, "texto")` / `readCString(dir, buf, tam)` | Texto corto |

## microSD · `JWPLC_GlobalPeripherals/05.microSD_Basic`

`JWPLC_SD.isReady()`, `JWPLC_SD.open("/log.txt", FILE_APPEND)` → `JWPLCFile` (`print`, `read`, `close`).
Nunca leer la SD desde funciones de dibujo de la pantalla.

## Buzzer · `JWPLC_GlobalPeripherals/07.Buzzer_Basic`

`JWPLC_Buzzer.beep()`, `JWPLC_Buzzer.play(notas, cantidad, bucle)`, `JWPLC_Buzzer.stop()`,
`JWPLC_Buzzer.setVolume(0..255)`.

## Modbus RTU (RS485) · `JWPLC_ModbusRTU/01…03`

| Función | Qué hace |
|---|---|
| `JWPLC_ModbusRTU.setHoldingRegisters(arreglo, n)` | Publica registros (esclavo) |
| `JWPLC_ModbusRTU.begin(id, baudios, SERIAL_8N1)` | Inicia; `false` → ver `lastErrorString()` |
| `JWPLC_ModbusRTU.task()` | Atender el bus: **en cada ciclo** |
| `requestReadHoldingRegisters(...)`, `masterDone()`, `masterSucceeded()` | Maestro (ver ejemplo 02) |

## Modbus TCP (Ethernet) · `JWPLC_ModbusTCP/01.ModbusTCP_Server`

`#include <JWPLC_ModbusTCP.h>` (la plantilla lo hace si `USAR_MODBUS_TCP` = 1),
`JWPLC_ModbusTCP.setHoldingRegisters(arreglo, n)`, `JWPLC_ModbusTCP.beginServer(unitId, 502)`,
`JWPLC_ModbusTCP.serverReady()`. El package la atiende en segundo plano.

## Ethernet · `JWPLC_Ethernet/01.Ethernet_DHCP_Basic` y `02.Ethernet_StaticIP_Basic`

`JWPLC_Ethernet.useDHCP()` o `JWPLC_Ethernet.setStaticIP(ip, dns, gateway, mascara)` en `setup()`;
`JWPLC_Ethernet.isReady()`, `JWPLC_Ethernet.localIP()`. No llamar `Ethernet.begin()`.

## Reglas del JWPLC Basic

1. Sin `delay()` en el ciclo: usar `TON`/`TOF` de `K_Utilidades.h` o `millis()`.
2. Pantalla, Ethernet, SD y FRAM comparten el bus SPI: el package lo coordina; no acceder a esos
   chips por fuera de sus objetos `JWPLC_*`.
3. La pantalla sólo redibuja lo que cambió: enviar valores cuando cambian, no en cada ciclo.
4. El equipo debe funcionar igual sin el cable USB (Serial es sólo diagnóstico).

## Si algo no funciona

| Síntoma | Revisar |
|---|---|
| No compila: «JWPLC_… no declarado» | Placa elegida: **Herramientas → Placa → JWPLC Basic** |
| Las salidas no responden | `F_Salidas` escribe `imagenSalidas`; ¿la lógica pone la variable en `true`? ¿emergencia activa? |
| Alarma EMG al encender | Entradas NC (paro, emergencia, térmico) deben estar cerradas; ver `MAPA_IO.md` |
| Modbus RTU sin respuesta | ID, baudios y formato en `A_Config.h`; A/B del RS485; `task()` en cada ciclo |
| Modbus TCP sin respuesta | `USAR_MODBUS_TCP` = 1, cable, IP en el monitor serie, puerto 502 |
| Ciclo lento (`max=` alto en Serial) | Una pestaña está esperando: buscar `delay()` o bucles `while` |
