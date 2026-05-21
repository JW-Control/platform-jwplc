# JW_RTC

`JW_RTC` es la librería del ecosistema **JW Control** para manejar RTC de la familia **DS3232M / DS3232** con una API limpia para Arduino/JWPLC.

En el package **JWPLC Basic v2.0.0**, esta librería se usa para integrar el RTC físico del PLC al runtime del sistema.

---

## Alcance actual

La versión actual está pensada principalmente para el package JWPLC y usa internamente:

```txt
jwplc_i2c_bridge
```

Esto permite compartir el bus I2C del core JWPLC sin que el usuario tenga que inicializar manualmente `Wire`.

En una versión futura se puede agregar un backend adicional basado en `Wire` para proyectos Arduino/ESP32 genéricos fuera del package JWPLC.

---

## Características principales

`JW_RTC` permite:

- leer fecha y hora;
- escribir fecha y hora;
- verificar si la hora es válida;
- detectar pérdida de energía mediante `OSF`;
- convertir a/desde timestamp Unix;
- leer temperatura interna del RTC;
- leer/escribir aging offset;
- configurar salida square-wave;
- habilitar/deshabilitar salida de 32 kHz;
- configurar Alarm 1 y Alarm 2;
- usar SRAM/NVRAM respaldada por batería;
- usar aliases amigables como `JWRTCDateTime`.

---

## Inclusión

En uso standalone:

```cpp
#include <JW_RTC.h>
```

En `JWPLC Basic`, normalmente se usa mediante el objeto global:

```cpp
JWPLC_RTC
```

---

## Ejemplo mínimo

```cpp
#include <JW_RTC.h>

JW_RTC rtc;

void setup()
{
    Serial.begin(115200);
    delay(300);

    if (!rtc.begin())
    {
        Serial.print("RTC begin failed: ");
        Serial.println(JW_RTC::errorToString(rtc.lastError()));
        return;
    }

    Serial.println("RTC ready");
}

void loop()
{
    JWRTCDateTime dt;

    if (rtc.read(dt))
    {
        Serial.print(dt.year);
        Serial.print('/');
        Serial.print(dt.month);
        Serial.print('/');
        Serial.print(dt.day);
        Serial.print(' ');
        Serial.print(dt.hour);
        Serial.print(':');
        Serial.print(dt.minute);
        Serial.print(':');
        Serial.println(dt.second);
    }

    delay(1000);
}
```

---

## Aliases amigables

La librería expone aliases para evitar nombres largos:

```cpp
JWRTCDateTime
JWRTCAlarm1Config
JWRTCAlarm2Config
JWRTCError
JWRTCSquareWaveMode
JWRTCAlarm1Mode
JWRTCAlarm2Mode
```

Ejemplo:

```cpp
JWRTCDateTime dt;
dt.year = 2026;
dt.month = 5;
dt.day = 4;
dt.hour = 12;
dt.minute = 30;
dt.second = 0;
dt.dayOfWeek = 0; // opcional, se puede autocalcular al escribir
```

---

## Estructura de fecha/hora

```cpp
struct DateTime
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t dayOfWeek; // 1=domingo ... 7=sábado
    bool valid;
};
```

Notas:

- Rango de año recomendado: `2000..2099`.
- Si `dayOfWeek = 0` al escribir, la librería puede calcularlo automáticamente.
- `valid` normalmente se llena al leer desde el RTC.

---

## Inicialización

### Constructor básico

```cpp
JW_RTC rtc;
```

### `begin()`

Inicializa el RTC usando el bridge I2C del ecosistema JWPLC:

```cpp
if (!rtc.begin())
{
    Serial.println(JW_RTC::errorToString(rtc.lastError()));
}
```

### `beginWithPins()`

Útil si se quiere inicializar el bridge con pines explícitos:

```cpp
rtc.beginWithPins(21, 22, 400000UL);
```

### `setClock()`

Cambia la frecuencia del bus I2C después del inicio:

```cpp
rtc.setClock(100000UL);
rtc.setClock(400000UL);
```

---

## Presencia y validez

### `isPresent()`

```cpp
if (rtc.isPresent())
{
    Serial.println("RTC detectado");
}
```

### `isTimeValid()`

```cpp
if (rtc.isTimeValid())
{
    Serial.println("Hora válida");
}
else
{
    Serial.println("RTC perdió energía o la hora no es válida");
}
```

### `lostPower()`

Helper equivalente a “la hora no es válida”:

```cpp
if (rtc.lostPower())
{
    Serial.println("RTC perdió energía");
}
```

### `clearOscillatorStopFlag()`

Limpia el flag `OSF` luego de setear o validar la hora:

```cpp
rtc.clearOscillatorStopFlag();
```

---

## Leer fecha y hora

### `read()`

```cpp
JWRTCDateTime dt;

if (rtc.read(dt))
{
    Serial.println(dt.year);
}
```

### `now()`

```cpp
JWRTCDateTime dt = rtc.now();
```

---

## Escribir fecha y hora

```cpp
JWRTCDateTime dt;
dt.year = 2026;
dt.month = 5;
dt.day = 4;
dt.hour = 12;
dt.minute = 30;
dt.second = 0;
dt.dayOfWeek = 0;

if (!rtc.write(dt))
{
    Serial.println(JW_RTC::errorToString(rtc.lastError()));
}
```

---

## Setear desde fecha/hora de compilación

```cpp
JWRTCDateTime dt;

if (JW_RTC::fromBuildTime(__DATE__, __TIME__, dt))
{
    rtc.write(dt);
    rtc.clearOscillatorStopFlag();
}
```

Esto es útil para cargar una hora inicial al momento de subir el sketch.

---

## Timestamp Unix

### Leer Unix time

```cpp
uint32_t ts = 0;

if (rtc.readUnix(ts))
{
    Serial.println(ts);
}
```

### Escribir Unix time

```cpp
rtc.writeUnix(1776542400UL);
```

### Convertir manualmente

```cpp
uint32_t ts = JW_RTC::toUnix(dt);
JW_RTC::fromUnix(ts, dt);
```

---

## Temperatura

El DS3232M / DS3232 permite leer temperatura interna.

```cpp
float tempC = 0.0f;

if (rtc.readTemperatureC(tempC))
{
    Serial.print("Temp C: ");
    Serial.println(tempC, 2);
}
```

También puede leerse en centésimas de grado:

```cpp
int16_t tempCenti = 0;

if (rtc.readTemperatureCentiC(tempCenti))
{
    Serial.println(tempCenti); // 3725 = 37.25 °C
}
```

---

## Aging offset

```cpp
int8_t offset = 0;

rtc.getAgingOffset(offset);
rtc.setAgingOffset(-2);
```

---

## Square-wave y 32 kHz

### Square-wave

```cpp
rtc.setSquareWave(JW_RTC::SquareWaveMode::Hz1);
```

Modos típicos:

```txt
Off
Hz1
Hz1024
Hz4096
Hz8192
```

### Salida 32 kHz

```cpp
rtc.set32kHzOutput(true);

bool enabled = false;
rtc.get32kHzOutput(enabled);
```

---

## Alarmas

### Alarm 1

```cpp
JWRTCAlarm1Config a1;
a1.mode = JW_RTC::Alarm1Mode::MatchDateHoursMinutesSeconds;
a1.second = 0;
a1.minute = 30;
a1.hour = 8;
a1.day = 1;
a1.dayOfWeek = false;

rtc.setAlarm1(a1);
```

### Alarm 2

```cpp
JWRTCAlarm2Config a2;
a2.mode = JW_RTC::Alarm2Mode::MatchHoursMinutes;
a2.minute = 45;
a2.hour = 18;
a2.day = 1;
a2.dayOfWeek = false;

rtc.setAlarm2(a2);
```

### Flags de alarma

```cpp
bool fired = false;

rtc.getAlarm1Flag(fired);
rtc.clearAlarm1Flag();

rtc.getAlarm2Flag(fired);
rtc.clearAlarm2Flag();
```

---

## NVRAM respaldada por batería

El DS3232M / DS3232 ofrece SRAM respaldada por batería.

Ejemplo con bytes:

```cpp
uint8_t data[4] = {1, 2, 3, 4};

rtc.nvramWrite(0, data, sizeof(data));

uint8_t restored[4];
rtc.nvramRead(0, restored, sizeof(restored));
```

Ejemplo con estructura:

```cpp
struct ConfigData
{
    uint16_t magic;
    uint32_t counter;
};

ConfigData cfg = {0x55AA, 123};

rtc.nvramWriteObject(0, cfg);

ConfigData restored;
rtc.nvramReadObject(0, restored);
```

---

## Manejo de errores

### `lastError()`

```cpp
JWRTCError err = rtc.lastError();
```

### `errorToString()`

```cpp
Serial.println(JW_RTC::errorToString(rtc.lastError()));
```

Errores típicos:

```txt
Ok
DeviceNotFound
BusInitFailed
ReadFailed
WriteFailed
InvalidArgument
OutOfRange
NotReady
```

---

## Uso típico dentro de JWPLC Basic

En `JWPLC Basic`, el RTC está integrado al runtime.

Uso conceptual:

```cpp
auto now = JWPLC_RTC.now();
```

Aplicaciones:

- fecha/hora en pantalla;
- logs en microSD;
- timestamp de eventos;
- control de procesos;
- diagnóstico;
- registro de alarmas.

---

## Validación recomendada para alpha31

Para alpha31 se recomienda validar:

- RTC detectado.
- `now()` / `read()`.
- seteo manual de hora.
- seteo desde `__DATE__` / `__TIME__`.
- detección de hora inválida / `lostPower()`.
- lectura de temperatura.
- comportamiento dentro de `JWPLC Basic`.
- ausencia de bloqueo si el RTC no responde.

---

## Nota de backend actual

La versión actual usa:

```txt
jwplc_i2c_bridge
```

Esto la hace ideal para el package **JWPLC Basic** actual.

Futuro posible:

```txt
backend Wire
```

para soportar placas Arduino/ESP32 genéricas fuera del entorno JWPLC.

---

## Estado

Documentación propuesta en español para revisión de:

```txt
JWPLC Basic v2.0.0-alpha.31
JW_RTC
```
