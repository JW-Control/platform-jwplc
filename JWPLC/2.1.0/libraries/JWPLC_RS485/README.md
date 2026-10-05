# JWPLC_RS485

`JWPLC_RS485` te permite usar el puerto RS-485 integrado del **JWPLC Basic**
como si fuera un puerto serie de Arduino.

Puedes enviar y recibir bytes, texto o tramas propias sin controlar manualmente
la dirección del transceptor.

## ¿Para qué sirve?

RS-485 es una interfaz física muy usada en automatización industrial.

Esta librería es útil cuando quieres:

- comunicar el JWPLC con otro equipo por RS-485;
- crear un protocolo propio;
- enviar comandos o telemetría;
- probar físicamente el puerto RS-485;
- usar una base de transporte para protocolos como Modbus RTU.

Si lo que necesitas es **Modbus RTU**, normalmente debes usar
`JWPLC_ModbusRTU` en lugar de construir las tramas manualmente.

## Qué hace automáticamente el JWPLC

El JWPLC Basic actual usa un transceptor con autodirección.

Por eso no tienes que:

- controlar señales DE/RE;
- decidir cuándo cambiar entre transmisión y recepción;
- conocer los pines RX/TX internos;
- crear otra instancia de `HardwareSerial`.

Sólo debes elegir velocidad y formato serial.

## Inicio rápido

Este ejemplo envía un mensaje cada segundo.

```cpp
#include <JWPLC_RS485.h>

uint32_t contador = 0;
uint32_t ultimoEnvio = 0;

void setup()
{
    Serial.begin(115200);

    if (!JWPLC_RS485.begin(
            115200,
            SERIAL_8N1))
    {
        Serial.println(
            JWPLC_RS485.lastErrorString());
    }
}

void loop()
{
    const uint32_t ahora = millis();

    if (ahora - ultimoEnvio >= 1000)
    {
        ultimoEnvio = ahora;
        contador++;

        JWPLC_RS485.print("Mensaje #");
        JWPLC_RS485.println(contador);
    }
}
```

## Conceptos básicos

### Baudrate

Es la velocidad de comunicación.

Ejemplos comunes:

```text
9600
19200
38400
57600
115200
```

Los dos equipos conectados deben utilizar la misma velocidad.

### Formato serial

`SERIAL_8N1` significa:

```text
8 bits de datos
N = sin paridad
1 bit de stop
```

Otros formatos disponibles por Arduino incluyen, por ejemplo:

```cpp
SERIAL_8E1
SERIAL_8O1
SERIAL_8N2
```

Ambos extremos del bus deben usar el mismo formato.

### Leer y escribir

`JWPLC_RS485` hereda de `Stream` y `Print`.

Eso permite usar funciones familiares:

```cpp
JWPLC_RS485.available();
JWPLC_RS485.read();

JWPLC_RS485.print("Hola");
JWPLC_RS485.println(123);
JWPLC_RS485.write(data, length);
```

## Ejemplo 1 — Básico: enviar texto

```cpp
#include <JWPLC_RS485.h>

uint32_t ultimoEnvio = 0;

void setup()
{
    Serial.begin(115200);

    if (!JWPLC_RS485.begin(
            115200,
            SERIAL_8N1))
    {
        Serial.println("No se pudo iniciar RS-485");
    }
}

void loop()
{
    if (millis() - ultimoEnvio >= 1000)
    {
        ultimoEnvio = millis();

        JWPLC_RS485.println(
            "Hola desde JWPLC");
    }
}
```

No usamos `delay(1000)`, por lo que el resto del programa puede seguir
ejecutándose.

## Ejemplo 2 — Intermedio: recibir y responder

Este ejemplo devuelve por RS-485 cada byte recibido.

```cpp
#include <JWPLC_RS485.h>

void setup()
{
    Serial.begin(115200);

    JWPLC_RS485.begin(
        115200,
        SERIAL_8N1);
}

void loop()
{
    while (JWPLC_RS485.available() > 0)
    {
        int dato =
            JWPLC_RS485.read();

        if (dato >= 0)
        {
            Serial.write(
                (uint8_t)dato);

            JWPLC_RS485.write(
                (uint8_t)dato);
        }
    }
}
```

## Ejemplo 3 — Aplicación real: comandos simples

Supongamos que otro equipo envía un carácter:

- `'1'`: encender `Q0_0`;
- `'0'`: apagar `Q0_0`;
- `'?'`: consultar estado.

```cpp
#include <JWPLC_RS485.h>

bool salida = false;

void setup()
{
    pinMode(Q0_0, OUTPUT);

    digitalWrite(Q0_0, LOW);

    JWPLC_RS485.begin(
        115200,
        SERIAL_8N1);
}

void loop()
{
    if (JWPLC_RS485.available() <= 0)
    {
        return;
    }

    const int dato =
        JWPLC_RS485.read();

    if (dato == '1')
    {
        salida = true;
        digitalWrite(Q0_0, HIGH);
        JWPLC_RS485.println("OK ON");
    }
    else if (dato == '0')
    {
        salida = false;
        digitalWrite(Q0_0, LOW);
        JWPLC_RS485.println("OK OFF");
    }
    else if (dato == '?')
    {
        JWPLC_RS485.println(
            salida ? "ON" : "OFF");
    }
}
```

En un proyecto industrial real conviene definir una trama más robusta o usar
un protocolo estándar como Modbus RTU.

## Ejemplo 4 — Avanzado de usuario: leer varios bytes juntos

Si esperas muchos datos puedes leerlos en bloque:

```cpp
#include <JWPLC_RS485.h>

uint8_t buffer[64];

void setup()
{
    Serial.begin(115200);

    JWPLC_RS485.begin(
        115200,
        SERIAL_8N1);
}

void loop()
{
    const size_t recibidos =
        JWPLC_RS485.readAvailable(
            buffer,
            sizeof(buffer));

    if (recibidos > 0)
    {
        Serial.print("Bytes recibidos: ");
        Serial.println(recibidos);
    }
}
```

`readAvailable()` lee hasta el tamaño máximo indicado, pero no espera a que
lleguen datos futuros.

## API de usuario

### Iniciar y detener — Básico

| Función | Qué hace | Nivel |
|---|---|---|
| `begin()` | Inicia con los valores por defecto de la librería | Básico |
| `begin(baud)` | Inicia con una velocidad y formato por defecto | Básico |
| `begin(baud, config)` | Inicia con velocidad y formato explícitos | Básico |
| `end()` | Detiene el puerto RS-485 | Intermedio |

Recomendado:

```cpp
JWPLC_RS485.begin(
    115200,
    SERIAL_8N1);
```

### Comprobar estado — Básico / Intermedio

| Función | Retorno | Uso |
|---|---|---|
| `isReady()` | `bool` | Saber si el puerto fue iniciado correctamente |
| `isEnabled()` | `bool` | Saber si el board dispone de RS-485 |
| `baudRate()` | `uint32_t` | Baud solicitado |
| `effectiveBaudRate()` | `uint32_t` | Baud efectivo reportado por la UART |
| `config()` | `uint32_t` | Configuración serial activa |
| `configString()` | `const char*` | Configuración en texto legible |

### Recibir datos — Básico

| Función | Qué hace |
|---|---|
| `available()` | Cantidad de bytes disponibles |
| `read()` | Lee un byte |
| `peek()` | Mira el próximo byte sin retirarlo |
| `readAvailable(buffer, maxSize)` | Lee varios bytes disponibles |

Ejemplo:

```cpp
if (JWPLC_RS485.available() > 0)
{
    int dato = JWPLC_RS485.read();
}
```

### Enviar datos — Básico

Por herencia de `Print` puedes usar:

```cpp
JWPLC_RS485.print("Valor=");
JWPLC_RS485.println(25);
```

También:

```cpp
JWPLC_RS485.write(
    buffer,
    length);
```

`write()` espera a que la transmisión termine físicamente antes de regresar.

### Esperar fin de transmisión — Intermedio

```cpp
JWPLC_RS485.flush();
```

Normalmente no necesitas llamarlo después de `write()`, porque `write()`
ya conserva comportamiento bloqueante.

Puede ser útil cuando has usado una transmisión encolada.

### Actividad — Intermedio

| Función | Qué indica |
|---|---|
| `lastActivityMs()` | Última actividad RX o TX |
| `lastRxActivityMs()` | Última recepción |
| `lastTxActivityMs()` | Última transmisión |
| `hasRecentActivity(windowMs)` | Si hubo actividad dentro de una ventana |

Ejemplo:

```cpp
if (JWPLC_RS485.hasRecentActivity(2000))
{
    Serial.println("BUS activo recientemente");
}
```

### Diagnóstico — Intermedio

| Función | Uso |
|---|---|
| `lastError()` | Código del último error |
| `lastErrorString()` | Error en texto |
| `statusString()` | Estado general |
| `printStatus(out)` | Imprime un resumen completo |

Ejemplo:

```cpp
JWPLC_RS485.printStatus(Serial);
```

## Errores comunes

### Configurar velocidades diferentes en cada equipo

Ambos extremos deben coincidir en baudrate y formato.

### Intentar controlar DE/RE manualmente

No es necesario en JWPLC Basic v2.

### Reconfigurar `Serial2` directamente

Evita algo como:

```cpp
Serial2.begin(...);
```

si `JWPLC_RS485` o `JWPLC_ModbusRTU` está usando el puerto.

### Usar RS-485 manual y Modbus RTU al mismo tiempo

`JWPLC_ModbusRTU` utiliza este mismo transporte. No deben existir dos partes
del sketch reconfigurando el mismo puerto.

### Esperar mensajes con `delay()`

Prefiere comprobar `available()` dentro de `loop()` y dejar que el resto
de la aplicación continúe.

## API avanzada

### Transmisión encolada

```cpp
size_t escritos =
    JWPLC_RS485.writeQueued(
        buffer,
        length);
```

`writeQueued()` intenta encolar datos y regresar sin esperar el final físico
de la UART cuando el hardware lo soporta.

Puedes consultar:

```cpp
JWPLC_RS485.queuedWriteSupported();
JWPLC_RS485.txBufferSize();
JWPLC_RS485.autoDirection();
```

Si utilizas esta ruta y necesitas saber que ya terminó la transmisión:

```cpp
JWPLC_RS485.flush();
```

Para una aplicación normal `write()` es más simple.

### Diagnóstico del hardware serie

Existen:

```cpp
JWPLC_RS485.rxPin();
JWPLC_RS485.txPin();
JWPLC_RS485.apbClockForced();
JWPLC_RS485.clockSourceString();
```

Son útiles para diagnóstico avanzado, no para configurar el hardware del
JWPLC Basic.

### Acceso al Stream

```cpp
Stream &puerto =
    JWPLC_RS485.stream();
```

Puede servir cuando una función propia recibe un `Stream&`.

## Compatibilidad

También existe:

```cpp
HardwareSerial &uart =
    JWPLC_RS485.serial();
```

Es una salida de compatibilidad/diagnóstico.

Para código nuevo **no se recomienda** reconfigurar directamente esa UART.
Usa los métodos de `JWPLC_RS485`.

Los callbacks C:

```text
jwplcRs485PreTransmitCallback()
jwplcRs485PostTransmitCallback()
jwplcRs485ActivityCallback()
```

son hooks de integración del package y no forman parte del camino normal de
usuario.

## Versión

Documentado para:

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_RS485 1.0.1
```
