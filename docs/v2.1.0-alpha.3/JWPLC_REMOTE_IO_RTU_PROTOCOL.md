# JWPLC Remote I/O RTU Protocol v1

## Nota de consolidación

Documento consolidado para **JWPLC Basic v2.1.0-alpha.3**.

Ruta actual en el repositorio:

```txt
docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_PROTOCOL.md
```

Este protocolo forma parte del diseño de **JWPLC Remote I/O sobre Modbus RTU / RS-485**. No debe interpretarse como integración automática en el runtime estable `v2.0.0`.

---

## Estado

Documento de diseño para la PoC de **JWPLC Remote I/O sobre Modbus RTU / RS-485**.

Este documento define el punto de partida para que uno o más **JWPLC Basic** funcionen como módulos remotos de entradas/salidas conectados por RS-485 a un **JWPLC Basic Master**.

## Objetivo

Diseñar un flujo estable para:

- usar JWPLC Basic como **Remote I/O Slave**;
- conectar varios slaves al Master por RS-485;
- detectar slaves desde OpenPLC Editor usando al Master como puente;
- asignar IDs Modbus RTU de forma segura;
- guardar la configuración persistente en FRAM;
- mantener la integración como opcional, sin romper el uso normal Arduino del JWPLC Basic.

## Arquitectura general

```txt
PC / OpenPLC Editor
        |
        | USB / Serial0
        v
JWPLC Basic Master
        |
        | RS-485 / Serial2
        v
JWPLC Basic Remote I/O Slave(s)
```

### Comunicación PC ↔ Master

La PC/OpenPLC Editor se comunica con el JWPLC Basic Master por USB/Serial0.

El editor solicita acciones como:

- escanear bus RTU;
- listar slaves encontrados;
- asignar un slave a un slot;
- cambiar ID;
- verificar configuración;
- consultar estado de commissioning.

### Comunicación Master ↔ Slaves

El Master se comunica con los slaves por RS-485 usando Serial2.

En JWPLC Basic:

```txt
RS-485 = Serial2
RX2    = IO16
TX2    = IO17
Driver = MAX13487 auto-dirección
```

## Decisiones cerradas

- El camino principal de Remote I/O será **Modbus RTU por RS-485**.
- Ethernet/Modbus TCP queda como alternativa futura o modo avanzado.
- El Master se comunica con el editor por Serial0.
- El Master se comunica con los slaves por Serial2.
- El editor no necesita adaptador USB-RS485 externo para el flujo principal.
- El Master actuará como puente y coordinador del bus.
- El firmware base de los slaves será precargado.
- Cada slave tendrá un UID derivado de la **MAC base del ESP32**.
- La MAC también se guardará en FRAM para futura integración TCP.
- El ID Modbus operativo se guardará en FRAM.
- La detección/asignación inicial de varios slaves no usará broadcast Modbus estándar.
- La detección/asignación inicial usará un modo propio: **JWPLC RTU Commissioning**.
- No se usarán registros altos tipo 1000 para identificación/configuración.
- La operación normal usará registros bajos dentro de `0..255`.
- No se usarán Function Codes custom `0x41..0x45` porque OpenPLC los usa para debug.

## Por qué no usar registros 1000

En el runtime Modbus actual de OpenPLC para Baremetal, la estructura Modbus usa tamaños internos tipo `uint8_t` para holding registers, coils, input registers e input status.

Por eso, para evitar choques o cambios innecesarios del runtime, el protocolo JWPLC Remote I/O RTU v1 se mantendrá dentro del rango bajo:

```txt
0..255
```

La zona de I/O real usará offsets bajos (`0..7`) y la zona de identificación/configuración usará rangos altos pero dentro de 255.

## Roles

### JWPLC Basic Master

Responsabilidades:

- recibir comandos del editor por Serial0;
- detener temporalmente el polling Modbus normal durante commissioning;
- enviar comandos de commissioning por Serial2;
- escanear slaves conectados al bus RS-485;
- detectar MAC/UID de slaves;
- asignar IDs Modbus por UID;
- verificar slaves usando Modbus RTU estándar;
- ejecutar posteriormente el programa OpenPLC/Ladder con Remote I/O.

### JWPLC Basic Remote I/O Slave

Responsabilidades:

- leer entradas físicas `I0_0..I0_7`;
- escribir salidas físicas `Q0_0..Q0_7`;
- exponer I/O por Modbus RTU;
- responder a commissioning por UID/MAC;
- guardar configuración persistente en FRAM;
- permitir cambio de ID cuando sea seguro;
- exponer identificación por registros Modbus estándar.

## Configuración persistente en FRAM

La configuración mínima del slave se guardará en FRAM.

Estructura sugerida:

```cpp
struct JwplcRemoteIoConfigV1 {
  uint32_t magic;
  uint16_t version;
  uint8_t configured;
  uint8_t slaveId;
  uint32_t baudrate;
  uint8_t parity;
  uint8_t stopBits;
  uint8_t mac[6];
  uint32_t uidCrc;
  uint16_t configCrc;
};
```

Valores sugeridos:

```txt
magic      = 0x4A575254  // "JWRT"
version    = 0x0001
configured = 0 o 1
slaveId    = 1..247
baudrate   = 115200 inicialmente
parity     = 0 = none
stopBits   = 1
mac[6]     = MAC base del ESP32
uidCrc     = CRC32 o hash corto de MAC
configCrc  = CRC16 de la estructura
```

Reglas:

- Si `magic`, `version` o `configCrc` no son válidos, el equipo entra como `unconfigured`.
- Si el equipo está `unconfigured`, no debe responder en operación Modbus normal con un ID compartido cuando haya varios equipos en el bus.
- Un equipo `unconfigured` sí puede responder al modo `JWPLC RTU Commissioning`.
- La MAC se obtiene del ESP32 y se guarda en FRAM para trazabilidad y futura integración TCP.
- El ID Modbus se guarda en FRAM cuando queda asignado de forma segura.

## Mapa Modbus RTU operativo

El mapa operativo se mantiene simple.

### Discrete Inputs - FC2

```txt
Offset 0..7 = I0_0..I0_7
```

Uso:

- lectura de entradas digitales reales del slave;
- el Master/OpenPLC las mapeará como entradas remotas.

### Coils - FC1 / FC5 / FC15

```txt
Offset 0..7 = Q0_0..Q0_7
```

Uso:

- FC1 lee feedback/estado de salidas;
- FC5 escribe una salida individual;
- FC15 escribe varias salidas.

### Input Registers - FC4

Zona de identificación solo lectura.

```txt
240 = Magic 1              0x4A57  // "JW"
241 = Magic 2              0x504C  // "PL"
242 = Protocol version     0x0001
243 = Product code         0x0001  // JWPLC Basic Remote I/O
244 = Firmware version     major/minor
245 = I/O count            DI en byte alto, DO en byte bajo
246 = Capabilities         bits
247 = Status + Slave ID    status en byte alto, slaveId en byte bajo
248 = MAC bytes 0..1
249 = MAC bytes 2..3
250 = MAC bytes 4..5
251 = UID CRC high
252 = UID CRC low
253 = Config CRC
254 = Reserved
255 = Reserved
```

### Holding Registers - FC3 / FC6 / FC16

Zona de configuración cuando el ID actual es conocido y único.

```txt
224 = Unlock key           0xA55A
225 = New slave ID         1..247
226 = Baudrate code
227 = Command
228 = Command parameter
229 = Last command status
230 = Reserved
231 = Reserved
232 = Reserved
233 = Reserved
234 = Reserved
235 = Reserved
236 = Reserved
237 = Reserved
238 = Reserved
239 = Reserved
```

Comandos sugeridos en Holding Register `227`:

```txt
1 = Save new slave ID
2 = Save baudrate
3 = Factory reset config
4 = Reboot
```

Reglas para cambio por Modbus normal:

1. El ID actual debe ser conocido y único.
2. El Master escribe `0xA55A` en el registro `224`.
3. El Master escribe el nuevo ID en el registro `225`.
4. El Master escribe comando `1` en el registro `227`.
5. El slave valida rango `1..247`.
6. El slave guarda en FRAM.
7. El slave reporta resultado en `229`.
8. El cambio se aplica tras reinicio o tras una ventana segura sin polling.

## Estados del slave

```txt
0 = unconfigured
1 = configured
2 = commissioning
3 = duplicate-id suspected
4 = error
```

## Capabilities

Registro `246`, bits sugeridos:

```txt
bit 0 = Remote I/O digital soportado
bit 1 = Cambio de ID por Modbus soportado
bit 2 = Commissioning por UID soportado
bit 3 = TCP futuro soportado
bit 4 = FRAM disponible
bit 5 = Display/botonera disponible
bit 6 = Reservado
bit 7 = Reservado
```

## JWPLC RTU Commissioning

El commissioning es un protocolo propio sobre RS-485, coordinado por el Master.

No usa broadcast Modbus estándar para evitar que múltiples slaves sin configurar respondan al mismo tiempo con el mismo ID.

### Flujo general

```txt
1. Editor pide SCAN al Master por Serial0.
2. Master pausa polling Modbus normal.
3. Master entra en modo commissioning.
4. Master envía JWPLC_SCAN por Serial2.
5. Cada slave calcula una ventana de respuesta usando MAC/UID + nonce.
6. Cada slave responde en su ventana.
7. Master recopila respuestas.
8. Si detecta colisiones o faltan respuestas, repite con otro nonce y/o más ventanas.
9. Master devuelve lista al editor por Serial0.
```

### Anti-colisión

Cada slave calcula:

```txt
window = CRC16(MAC + nonce) % windowCount
delay  = baseDelay + window * slotTime + jitter
```

Valores iniciales sugeridos:

```txt
windowCount = 32
slotTime    = 8 ms
jitter      = 0..2 ms
```

Si hay colisión o sospecha de colisión, el Master puede repetir usando:

```txt
windowCount = 64
nuevo nonce
```

### Respuesta esperada del Master al editor

Formato textual inicial por Serial0:

```txt
JWPLC_RTU_DEVICE;uid=AABBCCDDEEFF;mac=AA:BB:CC:DD:EE:FF;id=unassigned;model=JWPLC_BASIC_REMOTE_IO;di=8;do=8;fw=1.0
JWPLC_RTU_DEVICE;uid=AABBCCDDEE01;mac=AA:BB:CC:DD:EE:01;id=3;model=JWPLC_BASIC_REMOTE_IO;di=8;do=8;fw=1.0
```

## Asignación de ID por UID

El editor selecciona un equipo detectado y asigna un slot/ID.

Ejemplo:

```txt
Slot 2 -> UID AABBCCDDEEFF -> Slave ID 2
Slot 3 -> UID AABBCCDDEE01 -> Slave ID 3
```

El editor manda al Master por Serial0:

```txt
JWPLC_ASSIGN_UID uid=AABBCCDDEEFF id=2
```

El Master transmite por RS-485 un comando de commissioning con:

```txt
UID destino
nuevo slaveId
nonce
checksum
```

Solo el slave cuyo UID coincide guarda el ID en FRAM.

Después, el Master verifica con Modbus RTU estándar:

```txt
Leer FC4 registros 240..255 al ID nuevo.
Confirmar Magic, MAC, UID y Product Code.
```

## Cambio de ID cuando hay varios slaves conectados

### Caso 1: IDs únicos conocidos

Si todos los slaves tienen IDs únicos, el cambio puede hacerse por Modbus normal:

```txt
FC6 224 = 0xA55A
FC6 225 = nuevo ID
FC6 227 = 1
```

### Caso 2: IDs duplicados, desconocidos o equipos sin configurar

Si hay riesgo de colisión, el cambio debe hacerse por commissioning usando UID:

```txt
JWPLC_ASSIGN_UID uid=<UID_DESTINO> id=<NUEVO_ID>
```

Solo el equipo con ese UID aplica el cambio.

## Precarga de firmware base

Cada JWPLC Basic Slave recibirá primero un firmware base:

```txt
JWPLC_RemoteIO_Slave_RTU
```

Ese firmware:

- obtiene MAC base del ESP32;
- genera UID;
- inicializa FRAM;
- valida configuración persistente;
- entra como configurado o sin configurar;
- responde a commissioning;
- opera como Modbus RTU Slave cuando tiene ID válido.

## PoC por etapas

### PoC 1 - I/O RTU mínimo

Objetivo:

- validar I/O físico real por RS-485.

Incluye:

- firmware slave con ID fijo por define;
- FC2 lee `I0_0..I0_7`;
- FC15 escribe `Q0_0..Q0_7`;
- Master prueba lectura/escritura por RTU;
- sin FRAM;
- sin commissioning.

### PoC 2 - Configuración persistente en FRAM

Objetivo:

- validar configuración persistente.

Incluye:

- MAC/UID guardado en FRAM;
- slaveId guardado en FRAM;
- lectura de identificación por FC4;
- cambio de ID si el ID actual es único;
- validación de CRC/config.

### PoC 3 - Commissioning por UID

Objetivo:

- detectar y configurar varios slaves conectados al mismo bus.

Incluye:

- Master recibe `SCAN` por Serial0;
- Master escanea por Serial2;
- slaves responden por ventana anti-colisión;
- Master devuelve lista al editor;
- asignación de ID por UID;
- verificación por Modbus RTU estándar.

### PoC 4 - Integración en OpenPLC Editor / Backplane

Objetivo:

- configurar Remote I/O RTU desde el editor.

Incluye:

- botón `Scan JWPLC RTU Devices`;
- lista de slaves detectados;
- asignación de UID/ID a slots;
- generación de configuración Remote Device RTU;
- documentación de uso.

## Comandos Serial0 Master ↔ Editor

Formato inicial sugerido, textual y fácil de depurar:

```txt
JWPLC_SCAN_RTU baud=115200
JWPLC_ASSIGN_UID uid=AABBCCDDEEFF id=2
JWPLC_VERIFY_RTU id=2
JWPLC_LIST_RTU
```

Respuestas sugeridas:

```txt
OK
ERR code=<codigo> msg=<mensaje>
JWPLC_RTU_DEVICE;uid=...;mac=...;id=...;model=...;di=8;do=8;fw=1.0
JWPLC_RTU_SCAN_DONE count=4
```

## Pendientes

- Definir formato binario exacto de frames de commissioning.
- Definir CRC final para commissioning.
- Definir códigos de error.
- Definir si el primer scanner será CLI/Serial Monitor o UI dentro del editor.
- Definir si el Master puente vive como firmware auxiliar o dentro del runtime OpenPLC.
- Validar tiempos reales con 4 slaves conectados.
- Validar recuperación ante IDs duplicados.
- Validar almacenamiento FRAM en ciclos repetidos.
- Definir cómo se mostrará el UID/MAC en el Backplane.
- Definir cómo se integrará TCP futuro sin romper RTU.
