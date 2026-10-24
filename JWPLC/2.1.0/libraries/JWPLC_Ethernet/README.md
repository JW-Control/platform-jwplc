# JWPLC_Ethernet

`JWPLC_Ethernet` permite usar el puerto Ethernet W5500 integrado del
**JWPLC Basic**.

La plataforma se encarga de inicializar y mantener el hardware de red. Tu
sketch normalmente sólo decide si usará DHCP o una IP fija y luego trabaja con
`EthernetClient`, `EthernetServer` o `EthernetUDP`.

## ¿Para qué sirve?

Con Ethernet puedes:

- conectar el JWPLC a una red local;
- obtener una IP automáticamente por DHCP;
- usar una IP estática;
- crear clientes TCP;
- crear servidores TCP;
- enviar y recibir UDP;
- usar protocolos construidos sobre Ethernet, como Modbus TCP.

Si tu objetivo es Modbus TCP, usa `JWPLC_ModbusTCP`. Esta librería es la base
de red.

## Qué hace automáticamente el JWPLC

En un sketch normal no tienes que:

- escribir `#include <JWPLC_Ethernet.h>`: el package ya la carga;
- llamar `Ethernet.begin()`;
- configurar el pin CS del W5500;
- reiniciar manualmente el chip;
- administrar el bus SPI compartido;
- llamar continuamente a `JWPLC_Ethernet.service()`.

El runtime mantiene Ethernet en segundo plano.

## Inicio rápido

Este sketch solicita una dirección por DHCP y la imprime cuando esté lista.

```cpp
bool ipMostrada = false;

void setup()
{
    Serial.begin(115200);

    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (JWPLC_Ethernet.isReady() &&
        !ipMostrada)
    {
        ipMostrada = true;

        Serial.print("IP: ");
        Serial.println(
            JWPLC_Ethernet.localIP());
    }
}
```

Conecta el cable Ethernet a una red con DHCP y abre el Monitor Serie.

## Conceptos básicos

### Dirección IP

Una IP identifica un equipo dentro de una red.

Ejemplo:

```text
192.168.1.50
```

### DHCP

DHCP permite que el router o servidor de red asigne automáticamente:

- IP;
- gateway;
- máscara;
- DNS.

En JWPLC:

```cpp
JWPLC_Ethernet.useDHCP();
```

### IP estática

Puedes fijar una dirección manualmente:

```cpp
JWPLC_Ethernet.setStaticIP(
    IPAddress(192, 168, 1, 50),
    IPAddress(192, 168, 1, 1),
    IPAddress(192, 168, 1, 1),
    IPAddress(255, 255, 255, 0));
```

### TCP

TCP crea una conexión entre dos equipos.

Normalmente tendrás:

- un **Server**, que escucha en un puerto;
- un **Client**, que se conecta al Server.

### UDP

UDP envía datagramas sin crear una conexión permanente.

Es simple y rápido, pero no garantiza que cada datagrama llegue.

### Puerto

Un puerto identifica un servicio dentro de una IP.

Ejemplo:

```text
IP    = 192.168.1.50
Port  = 5000
```

## Ejemplo 1 — Básico: DHCP y estado de red

```cpp
uint32_t ultimoReporte = 0;

void setup()
{
    Serial.begin(115200);

    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (millis() - ultimoReporte < 1000)
    {
        return;
    }

    ultimoReporte = millis();

    Serial.print("Estado: ");
    Serial.print(
        JWPLC_Ethernet.statusString());

    if (JWPLC_Ethernet.isReady())
    {
        Serial.print(" | IP: ");
        Serial.print(
            JWPLC_Ethernet.localIP());
    }

    Serial.println();
}
```

Estados como “esperando link” o “DHCP en progreso” pueden aparecer durante el
arranque.

## Ejemplo 2 — Intermedio: IP estática

Cambia estos valores según tu red.

```cpp
const IPAddress IP_LOCAL(
    192, 168, 1, 50);

const IPAddress DNS(
    192, 168, 1, 1);

const IPAddress GATEWAY(
    192, 168, 1, 1);

const IPAddress MASCARA(
    255, 255, 255, 0);

void setup()
{
    Serial.begin(115200);

    JWPLC_Ethernet.setStaticIP(
        IP_LOCAL,
        DNS,
        GATEWAY,
        MASCARA);
}

void loop()
{
    static bool mostrado = false;

    if (JWPLC_Ethernet.isReady() &&
        !mostrado)
    {
        mostrado = true;

        Serial.print("IP: ");
        Serial.println(
            JWPLC_Ethernet.localIP());

        Serial.print("Gateway: ");
        Serial.println(
            JWPLC_Ethernet.gatewayIP());
    }
}
```

La configuración debe hacerse en `setup()`, antes de que el runtime termine
de levantar Ethernet.

## Ejemplo 3 — Aplicación real: servidor TCP de comandos

Este Server escucha en el puerto 5000.

Comandos:

- `1` -> enciende `Q0_0`;
- `0` -> apaga `Q0_0`;
- `?` -> responde el estado.

```cpp
EthernetServer server(5000);

bool serverIniciado = false;
bool salida = false;

void setup()
{
    pinMode(Q0_0, OUTPUT);
    digitalWrite(Q0_0, LOW);

    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (!JWPLC_Ethernet.isReady())
    {
        return;
    }

    if (!serverIniciado)
    {
        server.begin();
        serverIniciado = true;
    }

    EthernetClient client =
        server.available();

    if (!client)
    {
        return;
    }

    while (client.available() > 0)
    {
        const int comando =
            client.read();

        if (comando == '1')
        {
            salida = true;
            digitalWrite(Q0_0, HIGH);
            client.println("ON");
        }
        else if (comando == '0')
        {
            salida = false;
            digitalWrite(Q0_0, LOW);
            client.println("OFF");
        }
        else if (comando == '?')
        {
            client.println(
                salida ? "ON" : "OFF");
        }
    }
}
```

## Ejemplo 4 — Avanzado de usuario: telemetría UDP

Este ejemplo envía el estado de las entradas una vez por segundo.

```cpp
EthernetUDP udp;

const IPAddress DESTINO(
    192, 168, 1, 100);

const uint16_t PUERTO_LOCAL = 5001;
const uint16_t PUERTO_DESTINO = 5000;

bool udpIniciado = false;
uint32_t ultimoEnvio = 0;

void setup()
{
    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (!JWPLC_Ethernet.isReady())
    {
        return;
    }

    if (!udpIniciado)
    {
        udpIniciado =
            udp.begin(PUERTO_LOCAL);

        if (!udpIniciado)
        {
            return;
        }
    }

    if (millis() - ultimoEnvio >= 1000)
    {
        ultimoEnvio = millis();

        udp.beginPacket(
            DESTINO,
            PUERTO_DESTINO);

        udp.print("IN=");
        udp.println(
            JWPLC_readInputs(),
            HEX);

        udp.endPacket();
    }
}
```

## API de usuario

### Elegir configuración de red — Básico

| Función | Qué hace | Nivel |
|---|---|---|
| `useDHCP()` | Selecciona configuración automática | Básico |
| `setStaticIP(local,dns,gateway,subnet)` | Define una IP fija | Intermedio |

Recomendación: configura DHCP o IP estática en `setup()`.

### Consultar el estado — Básico

| Función | Retorno / uso |
|---|---|
| `isReady()` | `true` cuando la red está utilizable |
| `isBusy()` | El runtime está trabajando en la configuración |
| `hardwarePresent()` | W5500 detectado |
| `linkUp()` | Cable/link Ethernet activo |
| `statusString()` | Estado legible |
| `diagnosticCode()` | Código corto de diagnóstico |

### Consultar parámetros de red — Básico / Intermedio

```cpp
JWPLC_Ethernet.localIP();
JWPLC_Ethernet.subnetMask();
JWPLC_Ethernet.gatewayIP();
JWPLC_Ethernet.dnsServerIP();
```

### Diagnóstico — Intermedio

```cpp
JWPLC_Ethernet.lastError();
JWPLC_Ethernet.lastErrorString();
JWPLC_Ethernet.runtimeState();
JWPLC_Ethernet.printStatus(Serial);
```

`printStatus()` es la forma más sencilla de obtener un resumen durante
commissioning.

### EthernetClient — Básico / Intermedio

Crear:

```cpp
EthernetClient client;
```

Conectar:

```cpp
client.connect(
    IPAddress(192, 168, 1, 100),
    5000);
```

Funciones principales:

| Función | Uso |
|---|---|
| `connect(ip,port)` | Conectar a un Server |
| `connect(host,port)` | Conectar usando nombre |
| `connected()` | Consultar si sigue conectado |
| `available()` | Bytes recibidos disponibles |
| `read()` / `read(buffer,size)` | Leer |
| `write()` | Enviar bytes |
| `print()` / `println()` | Enviar texto |
| `flush()` | Esperar TX pendiente |
| `stop()` | Cerrar la conexión |
| `remoteIP()` | IP remota |
| `remotePort()` | Puerto remoto |
| `localPort()` | Puerto local |

Ejemplo típico:

```cpp
if (client.connect(serverIP, 5000))
{
    client.println("Hola");
}
```

### EthernetServer — Intermedio

Crear:

```cpp
EthernetServer server(5000);
```

Funciones principales:

| Función | Uso |
|---|---|
| `begin()` | Empieza a escuchar |
| `available()` | Obtiene un Client con datos disponibles |
| `accept()` | Acepta un Client |
| `write()` / `print()` / `println()` | Envía a Clients conectados |

### EthernetUDP — Intermedio

Crear:

```cpp
EthernetUDP udp;
```

Recibir:

```cpp
udp.begin(5000);

int tamano = udp.parsePacket();

if (tamano > 0)
{
    uint8_t buffer[64];

    int n = udp.read(
        buffer,
        sizeof(buffer));
}
```

Enviar:

```cpp
udp.beginPacket(destino, 5000);
udp.print("Hola");
udp.endPacket();
```

Funciones principales:

| Función | Uso |
|---|---|
| `begin(port)` | Abre un puerto local |
| `beginMulticast(ip,port)` | Escucha multicast |
| `stop()` | Cierra el socket UDP |
| `beginPacket(ip,port)` | Inicia un datagrama |
| `write()` / `print()` | Agrega datos |
| `endPacket()` | Envía el datagrama |
| `parsePacket()` | Detecta un datagrama recibido |
| `available()` | Bytes aún disponibles |
| `read()` | Lee los datos |
| `peek()` | Mira el siguiente byte |
| `flush()` | Descarta/termina lectura pendiente según la API Ethernet |
| `remoteIP()` | IP del emisor |
| `remotePort()` | Puerto del emisor |
| `localPort()` | Puerto local |

## Errores comunes

### Llamar `Ethernet.begin()` en un sketch JWPLC normal

No es necesario. Usa:

```cpp
JWPLC_Ethernet.useDHCP();
```

o:

```cpp
JWPLC_Ethernet.setStaticIP(...);
```

### Esperar la IP con un `while` bloqueante

Evita congelar `setup()` hasta que haya red.

Mejor consulta:

```cpp
if (JWPLC_Ethernet.isReady())
{
    // Red lista
}
```

desde `loop()`.

### Crear un Server antes de saber que la red está lista

Para un Server TCP genérico es más claro esperar:

```cpp
JWPLC_Ethernet.isReady()
```

antes de llamar `server.begin()`.

### Confundir TCP con UDP

TCP crea una conexión. UDP envía datagramas independientes.

### Usar `delay()` para mantener conexiones

No es necesario. Procesa red periódicamente dentro del flujo normal de
`loop()`.

### Envolver `EthernetClient` con `jwplcSPI_acquire()`

Es tentador «proteger» una consulta TCP o DNS tomando el bus SPI antes de
empezar:

```cpp
// Incorrecto: connect() y DNS fallan siempre
if (jwplcSPI_acquire(1000))
{
    cliente.connect("example.com", 80);
    jwplcSPI_release();
}
```

No hace falta y además rompe la conexión. `EthernetClient`, `EthernetUDP` y
`DNSClient` ya toman el bus en **cada** operación. El mutex SPI del JWPLC no
es recursivo: si el sketch ya lo tiene, la librería no puede tomarlo y la
operación termina en error (por ejemplo, «FALLO CONNECT/DNS»).

Lo correcto es llamar directamente:

```cpp
if (cliente.connect("example.com", 80))
{
    cliente.print("GET / HTTP/1.0\r\n\r\n");
}
```

Entre una operación y la siguiente la TFT, la FRAM y la microSD pueden seguir
usando el bus. `jwplcSPI_acquire()` sólo tiene sentido para leer varios datos
de estado seguidos (hardware, enlace, IP) sin que otro periférico se intercale,
como hace el ejemplo `Ethernet_Continuous_Stress_TFT`.

## API avanzada

### Timeouts y retransmisiones del runtime

```cpp
JWPLC_Ethernet.setTimeouts(
    5000,
    1000);

JWPLC_Ethernet.setRetransmissionCount(3);
```

Úsalos sólo si tu red requiere ajustes específicos.

### Estado detallado

```cpp
JWPLCEthernetRuntimeState estado =
    JWPLC_Ethernet.runtimeState();

JWPLCEthernetError error =
    JWPLC_Ethernet.lastError();
```

Los enums exactos están definidos en `JWPLC_Ethernet.h`.


## Clase avanzada Alpha13 — Comunicación TCP sin bloquear la espera

> **Si estás comenzando:** con los ejemplos anteriores y los métodos
> `connect()`, `read()`, `write()` y `stop()` puedes crear tus primeras
> aplicaciones. Esta clase es **opcional**: explica cómo trabajar en varios
> pasos cuando tu máquina no puede dedicar todo el `loop()` a esperar red.

### Paso 1. ¿Qué significa «cooperativo»?

Imagina que quieres conectar el JWPLC a un servidor mientras sigues leyendo
botones y atendiendo la lógica de una máquina. En vez de esperar en un
`while` hasta conectar, puedes **iniciar** la conexión y **consultar**
su resultado desde vueltas sucesivas de `loop()`.

En las operaciones `begin...Async()` y `poll...Async()`, el resultado es:

| Valor | Significado | Qué debes hacer |
|---|---|---|
| `1` | Operación completada | Continuar |
| `0` | Sigue pendiente, incluso si otro periférico está usando SPI | Consultar de nuevo más adelante |
| `-1` | Error, timeout o cancelación según el método | Revisar el estado y decidir cómo recuperar |

El autoload de JWPLC mantiene sus servicios normales. **Cooperativo no
significa tiempo real estricto:** otros métodos clásicos del sketch pueden
seguir bloqueando, y el código de usuario debe llamar periódicamente al
motor que haya iniciado.

### Paso 2. Ejemplo 5 — Conectarse sin esperar en un bucle

El servidor debe estar encendido y escuchando en el puerto elegido.
Sustituye la IP por la de tu servidor y abre el Monitor Serie a 115200.

```cpp
EthernetClient cliente;

const IPAddress SERVIDOR(192, 168, 1, 100);
const uint16_t PUERTO = 5000;

bool conexionIniciada = false;
bool pruebaTerminada = false;
uint32_t inicioMs = 0;

void finalizar(int resultado)
{
    pruebaTerminada = true;

    if (resultado == 1)
    {
        Serial.println("Conectado");
        cliente.println("Hola desde JWPLC");
        cliente.stop(); // Método clásico: simplifica el ejemplo.
    }
    else
    {
        Serial.println("Conexion no completada");
        cliente.cancelConnectAsync();
    }
}

void setup()
{
    Serial.begin(115200);
    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (pruebaTerminada ||
        !JWPLC_Ethernet.isReady())
    {
        return;
    }

    if (!conexionIniciada)
    {
        conexionIniciada = true;
        inicioMs = millis();

        int estado = cliente.beginConnectAsync(
            SERVIDOR,
            PUERTO);

        if (estado != 0)
        {
            finalizar(estado);
        }

        return;
    }

    if (millis() - inicioMs >= 5000)
    {
        finalizar(-1);
        return;
    }

    int estado = cliente.pollConnectAsync();

    if (estado != 0)
    {
        finalizar(estado);
    }
}
```

**¿Qué aprendiste?** `beginConnectAsync()` se ejecuta una sola vez;
`pollConnectAsync()` se consulta después. En cada vuelta el sketch
puede salir y dejar avanzar otras tareas. El envío `println()` y el
cierre `stop()` del ejemplo siguen siendo llamadas clásicas; no
debes considerarlos automáticamente asíncronos.

### Paso 3. Tabla de métodos TCP cooperativos — Avanzado

Estas funciones están declaradas en **`EthernetClient`**.
No son métodos de `JWPLC_Ethernet`, ni es necesario llamarlas para
habilitar la Ethernet integrada.

| Acción | Empezar | Consultar avance | Saber si sigue activa | Cancelar |
|---|---|---|---|---|
| Conectar | `beginConnectAsync(ip, puerto)` | `pollConnectAsync()` | `connectAsyncInProgress()` | `cancelConnectAsync()` |
| Cerrar TCP | `beginStopAsync()` | `pollStopAsync()` | `stopAsyncInProgress()` | `cancelStopAsync()` |
| Esperar TX | `beginFlushAsync()` | `pollFlushAsync()` | `flushAsyncInProgress()` | `cancelFlushAsync()` |
| Enviar bytes | `beginWriteAsync(buffer, cantidad)` | `pollWriteAsync()` | `writeAsyncInProgress()` | `cancelWriteAsync()` |

Para un primer proyecto usa preferentemente `connect()`, `read()`,
`write()` y `stop()`, que son más fáciles de manejar aunque puedan
esperar hasta su timeout.

**Cuidado con la escritura cooperativa:** el arreglo que pasas a
`beginWriteAsync()` debe existir y conservar exactamente sus datos
hasta que la operación termine o sea cancelada. No pases un buffer local
que desaparezca al salir de la función. No lances una segunda operación
sobre el mismo cliente sin gestionar la primera.

`cancelStopAsync()` puede forzar el cierre del socket; no es una
«pausa» reversible. Si el resultado de un poll es `0`, no concluyas
que el cable Ethernet se desconectó: el bus SPI puede estar ocupado.

### Paso 4. Lectura TCP de alta velocidad — Integración avanzada

Hay dos métodos públicos adicionales en `EthernetClient`:

| Método | Función |
|---|---|
| `jwplcReadTcpFastDeferred(buffer, maxBytes)` | Extrae bytes y aplaza la confirmación del puntero RX al W5500 |
| `jwplcCommitRxFast()` | Confirma que el hardware puede liberar la memoria RX correspondiente |

Se usan **en pareja**. Si una lectura diferida consume datos y
nunca se confirma, la memoria de recepción puede quedar retenida. El
`commit` devuelve `bool`; un resultado `false` no demuestra que el
hardware haya registrado el avance. El integrador debe gestionar
reintento/recuperación según su máquina de estados. Una lectura con
retorno `0` tampoco distingue por sí sola «sin datos» de «SPI ocupado».

**Para comenzar, no necesitas estos métodos.** Usa
`available()` y `read()`; el camino de lectura normal administra
el avance de recepción por ti.

### Errores comunes en las funciones avanzadas

- **Reiniciar la conexión en cada vuelta:** llama una vez a
  `beginConnectAsync()` y luego usa `pollConnectAsync()`.
- **Esperar con `while (pollConnectAsync() == 0)`:** anula la ventaja
  del comportamiento cooperativo y puede retrasar tu lógica de aplicación.
- **Tratar `0` como fallo:** significa pendiente, posiblemente por SPI.
- **Modificar o destruir un buffer mientras se transmite:** el motor
  puede necesitarlo en las siguientes llamadas.
- **Olvidar `jwplcCommitRxFast()`:** puede retener memoria RX.

**Cobertura:** estos contratos existen en el código Alpha13, pero no se
afirma que cada combinación de cancelación, contención SPI y cierre
diferido haya superado ensayos físicos independientes. Ver el dictamen
G10-H para los límites de las pruebas.

## Mejoras de estabilidad en Alpha13

El puerto W5500 mantiene el manejo automático del JWPLC Basic. En esta
versión mejoramos la recuperación al conectar o desconectar el cable,
las transferencias TCP y UDP y la convivencia con los otros dispositivos SPI.

- Una conexión TCP que se cierra o falla no debe dejar un socket retenido
  indefinidamente. El runtime administra su liberación y reutilización.
- Las escrituras TCP grandes respetan la cantidad realmente aceptada; las
  lecturas no intentan recibir más bytes de los permitidos por el transporte.
- UDP admite datagramas que requieren envíos parciales y recupera operaciones
  incompletas sin depender de un `delay()` en tu sketch.
- El pin CS del W5500 lo fija el package mediante `JWPLC_ETH_CS` y la
  corrección H0-F1 de `SS_PIN_DEFAULT`. **No configures pines internos**
  ni uses GPIO6–11 para Ethernet. El sketch no debe corregir el variant.
- El perfil `JWPLC Basic Core` deshabilita Ethernet desde el core: la
  política del package impide al servicio acceder a W5500/CS en ese perfil.
  Los binarios compartidos solo se aprueban tras verificar ambos FQBN.

Tu código habitual con `useDHCP()`, `isReady()`,
`EthernetClient`, `EthernetServer` y `EthernetUDP` se mantiene.

**Alcance comprobado:** respuestas reales Modbus TCP, eco UDP y
desconexión/reconexión Ethernet en dos JWPLC Basic. Eso no garantiza la
ausencia de cualquier fallo de DNS, renovación de DHCP o saturación de sockets.

## Cambios en 2.1.0-alpha.14

La librería y su `libJWPLC_Ethernet.a` no cambian respecto de Alpha13. Se
corrigieron dos ejemplos:

- `Ethernet_HTTP_TFT_Diagnostics` y `Ethernet_Continuous_Stress_TFT` ya no
  envuelven DNS, `connect()`, envío y recepción con `jwplcSPI_acquire()`.
  Con ese bloqueo externo las consultas fallaban siempre (ver
  «Envolver `EthernetClient` con `jwplcSPI_acquire()`» en Errores comunes).

## Compatibilidad

Se conservan rutas Arduino Ethernet históricas como:

```cpp
Ethernet.begin(...);
Ethernet.maintain();
JWPLC_Ethernet.begin();
JWPLC_Ethernet.maintain();
```

Para código nuevo en JWPLC Basic se recomienda el runtime administrado:

```cpp
JWPLC_Ethernet.useDHCP();
```

o:

```cpp
JWPLC_Ethernet.setStaticIP(...);
```

Métodos como `configure()`, `setResetPin()`, `probeHardware()`,
`csPin()`, `resetPin()` y los hooks de profiling/test son de
bring-up/diagnóstico del package y no deben utilizarse para reconfigurar el
hardware del producto desde un sketch normal.

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.14
JWPLC_Ethernet 1.0.0
```
