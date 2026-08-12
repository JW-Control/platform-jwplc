# JWPLC Alpha4 — Cierre de gates físicos de comunicación

Fecha de validación: 2026-08-11

Branch:

```text
v2.1.0-alpha.4/feature/build-speed-cache
```

HEAD validado:

```text
5af40ff2e38819c7f00617d2433057200310bf13
```

## 1. Alcance

Este documento registra las validaciones físicas de comunicación realizadas
durante el cierre de `v2.1.0-alpha.4`.

Se validaron:

- Ethernet W5500;
- coexistencia SPI Ethernet + TFT + FRAM + microSD;
- DNS/TCP/HTTP real;
- RS-485 físico bidireccional;
- Modbus RTU físico como master y slave;
- FC03 Read Holding Registers;
- FC06 Write Single Register.

Las pruebas se realizaron con el package local:

```text
jwplc_local:esp32:jwplcbasic
```

No se retiró ningún periférico del autoload normal para conseguir estos
resultados.

---

# 2. Ethernet W5500

## 2.1 E1 — W5500 + Link + DHCP

Ejemplo usado:

```text
JWPLC_Ethernet/examples/Ethernet_Auto_DHCP_Status
```

Resultado:

```text
Enabled: yes
Attempted: yes
Ready: yes
HW: present
Link: UP
Status: OK
IP: 192.168.0.31
```

El estado permaneció estable durante toda la captura.

Resultado:

```text
ALPHA4_ETHERNET_E1=PASS
```

Evidencia externa:

```text
JWPLC_ALPHA4_ETH_E1_DHCP_20260811_085543.log
SHA256:
9E654BE954A7FF4440F7D7837B87DD70FB62C517BB9C99FBF4434581D0DF0829
```

---

## 2.2 E2A — coexistencia SPI

Ejemplo usado:

```text
JWPLC_Ethernet/examples/Ethernet_SPI_Coexistence
```

Con Ethernet enlazado y DHCP activo se verificó simultáneamente:

- W5500 operativo;
- TFT operativo;
- FRAM operativa;
- microSD operativa.

Estado repetido:

```text
ETH: OK
Link: UP
IP: 192.168.0.31
SD: OK
FRAM: OK
```

Se realizaron escrituras periódicas a microSD:

```text
SD log OK #1
...
SD log OK #8
```

No se observaron:

```text
SPI lock timeout
```

La TFT fue verificada visualmente y permaneció operativa durante la prueba.

Resultado:

```text
ALPHA4_ETHERNET_SPI_COEXISTENCE=PASS
```

Evidencia externa:

```text
JWPLC_ALPHA4_ETH_E2A_SPI_COEXISTENCE_20260811_085912.log
SHA256:
A8C31CC3F8498A9AC090CEC6F29398AE8F37BBF9CF070E4E9164A172AB7C1587
```

---

## 2.3 E3B — DNS + TCP + HTTP real

Ejemplo usado:

```text
JWPLC_Ethernet/examples/Ethernet_Continuous_Stress_TFT
```

Resultado:

```text
STARTUP_QUALIFIED_COUNT=1
HTTP_OK_COUNT=88
HTTP_ERROR_COUNT=0
OBSERVED_EVENT_COUNT=0
```

Las transacciones verificaron:

- W5500 presente;
- LINK ON;
- DHCP/IP válida;
- DNS real;
- TCP;
- recepción del primer byte;
- HTTP 200;
- contenido esperado.

Ejemplos:

```text
[ETH-STRESS][OK] #1 DNS LIVE ... HTTP 200
[ETH-STRESS][OK] #21 DNS LIVE ... HTTP 200
[ETH-STRESS][OK] #41 DNS LIVE ... HTTP 200
[ETH-STRESS][OK] #61 DNS LIVE ... HTTP 200
[ETH-STRESS][OK] #81 DNS LIVE ... HTTP 200
```

Resultado:

```text
ALPHA4_ETHERNET_REAL_COMMUNICATION=PASS
```

Evidencia externa:

```text
JWPLC_ALPHA4_ETH_E3B_RETRY_20260811_091538.log
SHA256:
E9F161AC23F244EF3EDE48DEDC896DB562904728ACFBE141AB9B4E80C416BB70
```

---

# 3. Patch de inicialización Ethernet

Durante el gate físico previo se identificó que el backend Ethernet heredado
retenía el mutex SPI durante una espera genérica de:

```text
delay(560)
```

El JWPLC Basic ya realiza un reset explícito del W5500 antes de entrar al
backend:

```text
RESET LOW  10 ms
RESET HIGH
espera      80 ms
```

Se eliminó únicamente la espera genérica de 560 ms del backend JWPLC.

No se modificaron:

- APIs públicas;
- autoload;
- timeout de FRAM;
- timeout de SD;
- mutex SPI global;
- periféricos disponibles.

Los gates E1, E2A y E3B demostraron físicamente que el cambio no rompe:

- detección W5500;
- link;
- DHCP;
- coexistencia SPI;
- DNS;
- TCP;
- HTTP.

Conclusión:

```text
ETHERNET_560MS_PATCH_PHYSICAL_VALIDATION=PASS
```

---

# 4. RS-485 físico

## 4.1 Configuración

Se utilizaron dos JWPLC Basic:

```text
DUT       : COM3
Responder : COM4
```

Configuración:

```text
Baud   : 115200
Formato: SERIAL_8N1
RX     : GPIO16
TX     : GPIO17
```

Hardware:

```text
MAX13487
auto-direccionamiento
sin DE/RE por software
```

Cableado:

```text
A/P  <-> A/P
B/N  <-> B/N
COM  <-> COM
```

---

## 4.2 Metodología

No se utilizó un simple loopback local.

El DUT transmitió una trama conocida al segundo JWPLC.

El segundo JWPLC:

1. recibió la trama por RS-485;
2. verificó la trama;
3. transformó cabecera, secuencia y payload;
4. transmitió una respuesta nueva.

Para obtener un PASS la comunicación debía recorrer físicamente:

```text
COM3 UART TX
    ->
MAX13487 COM3
    ->
bus RS-485
    ->
MAX13487 COM4
    ->
UART RX COM4
    ->
procesamiento
    ->
UART TX COM4
    ->
MAX13487 COM4
    ->
bus RS-485
    ->
MAX13487 COM3
    ->
UART RX COM3
```

Esto evita aceptar como válida una posible copia local accidental del TX.

---

## 4.3 Resultado

Se ejecutaron 100 round-trips:

```text
TOTAL=100
OK=100
TIMEOUT=0
MISMATCH=0
LAST_TX_MS=11231
LAST_RX_MS=11232
ALPHA4_RS485_PHYSICAL_GATE=PASS
```

Resultado:

```text
ALPHA4_RS485_PHYSICAL_GATE=PASS
```

Evidencia externa:

```text
JWPLC_ALPHA4_RS485_PHYSICAL_GATE_20260811_093523.log
SHA256:
75F73F1027CF1D40773EACC66903507BF61B23D4744FEFAEDFB038BBC8BC217C
```

---

# 5. Modbus RTU físico

## 5.1 Configuración

Se reutilizaron los dos JWPLC Basic:

```text
Master / DUT : COM3
Slave        : COM4
```

Configuración Modbus:

```text
Slave ID : 1
Master ID: 247
Baud     : 19200
Formato  : SERIAL_8E1
```

Funciones verificadas:

```text
FC03 - Read Holding Registers
FC06 - Write Single Register
```

---

## 5.2 Metodología

En cada ciclo el master:

1. generó un valor nuevo;
2. escribió `HR1` mediante FC06;
3. verificó la respuesta FC06;
4. leyó `HR0..HR3` mediante FC03;
5. verificó que `HR1` coincidiera exactamente con el valor escrito;
6. verificó dos firmas conocidas:

```text
HR2 = 0xA55A
HR3 = 0x5AA5
```

`HR0` se mantuvo como heartbeat dinámico del slave.

---

## 5.3 Resultado

Se realizaron 50 ciclos completos:

```text
TOTAL=50
FC06_WRITE_OK=50
FC03_READ_OK=50
VERIFY_OK=50
WRITE_FAIL=0
READ_FAIL=0
VERIFY_FAIL=0
MASTER_TIMEOUTS=0
LAST_TX_MS=7276
LAST_RX_MS=7288
ALPHA4_MODBUS_RTU_PHYSICAL_GATE=PASS
```

Resultado:

```text
ALPHA4_MODBUS_RTU_PHYSICAL_GATE=PASS
```

Evidencia externa:

```text
JWPLC_ALPHA4_MODBUS_RTU_PHYSICAL_GATE_20260811_094121.log
SHA256:
D5C4F64C785F7A2807948A7DBA6FEFD04430B0C9DA13C97833E3713996C0D1B6
```

---

# 6. Conclusión

Los gates físicos de comunicación requeridos para Alpha4 quedan cerrados:

| Gate | Resultado |
|---|---|
| W5500 detectado | PASS |
| Ethernet Link | PASS |
| DHCP | PASS |
| Ethernet + TFT + FRAM + SD | PASS |
| DNS real | PASS |
| TCP real | PASS |
| HTTP 200 real | PASS |
| RS-485 TX/RX físico | PASS |
| RS-485 100 round-trips | PASS |
| Modbus RTU FC03 | PASS |
| Modbus RTU FC06 | PASS |
| Modbus RTU 50 ciclos verificados | PASS |

Resultado global:

```text
ALPHA4_COMMUNICATION_PHYSICAL_GATES=PASS
```

## 7. Pendientes fuera de este documento

Este cierre no define ni modifica:

- OpenPLC;
- OTA;
- FlashFreq final;
- bootloader definitivo;
- P9.

Esos puntos deben conservar su estado independiente dentro del cierre de
Alpha4.