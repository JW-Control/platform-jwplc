# JW_SD

`JW_SD` es una librería para microSD basada en la librería nativa `SD`, pensada para trabajar de forma más segura en proyectos Arduino/ESP32 con **bus SPI compartido**.

Fue creada para el ecosistema **JWPLC**, pero también puede usarse en proyectos Arduino/ESP32 normales.

En `JWPLC Basic`, la microSD comparte SPI con:

- TFT ST7789.
- Ethernet W5500.
- FRAM SPI.
- microSD.

Por eso `JW_SD` agrega una capa de protección para operaciones comunes de archivo y ayuda a evitar accesos simultáneos al bus.

---

## Objetivo

Permitir un uso limpio:

```cpp
auto file = sd.open("/log.txt", FILE_APPEND);

if (file)
{
    file.println("Hola JWPLC");
    file.close();
}
```

sin que el usuario tenga que recordar manualmente `lock()` / `unlock()` en operaciones comunes de archivo.

---

## Cuándo usar `JW_SD`

Usa `JW_SD` cuando:

- tu proyecto comparte el bus SPI con otros periféricos;
- necesitas leer o escribir archivos desde Arduino;
- quieres una API sencilla encima de `SD`;
- quieres proteger operaciones comunes usando callbacks de bloqueo;
- trabajas con logs, recetas, configuraciones o archivos para HMI.

En proyectos muy simples con una sola microSD en SPI, también podrías usar directamente `SD`, pero en JWPLC Basic se recomienda `JW_SD`.

---

## Clases principales

| Clase | Uso |
|---|---|
| `JW_SD` | Objeto principal para inicializar y manejar la microSD. |
| `JWPLCFile` | Wrapper de archivo protegido. |
| `File` | Acceso nativo opcional mediante `openNative()`. |

---

## Instalación

### Como librería externa

1. Copia la librería en tu carpeta de librerías de Arduino.
2. Reinicia Arduino IDE.
3. Incluye:

```cpp
#include <JW_SD.h>
```

### Dentro del package JWPLC

En `JWPLC Basic`, normalmente se usa mediante el objeto global:

```cpp
JWPLC_SD
```

En ese caso, el package puede exponer la microSD ya integrada al runtime y al bus SPI compartido. No hace falta `#include <JW_SD.h>`: el package ya la carga.

---

## Uso básico standalone

```cpp
#include <Arduino.h>
#include <JW_SD.h>

JW_SD sd;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    if (!sd.begin(JWPLC_SD_CS))   // GPIO32 en el JWPLC Basic
    {
        Serial.println("No se pudo iniciar la SD");
        return;
    }

    auto file = sd.open("/log.txt", FILE_APPEND);

    if (file)
    {
        file.println("Hola desde JW_SD");
        file.close();
    }
}

void loop()
{
}
```

---

## Uso con tipo explícito

```cpp
JWPLCFile file = sd.open("/log.txt", FILE_APPEND);
```

## Uso con `auto`

```cpp
auto file = sd.open("/log.txt", FILE_APPEND);
```

Ambas formas son equivalentes.

---

## Ejemplo: escribir un log

```cpp
#include <Arduino.h>
#include <JW_SD.h>

JW_SD sd;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    if (!sd.begin(JWPLC_SD_CS))   // GPIO32 en el JWPLC Basic
    {
        Serial.println("SD no disponible");
        return;
    }

    auto file = sd.open("/events.csv", FILE_APPEND);

    if (!file)
    {
        Serial.println("No se pudo abrir /events.csv");
        return;
    }

    file.println("time_ms,event,value");
    file.print(millis());
    file.print(",boot,");
    file.println(1);
    file.close();

    Serial.println("Log escrito");
}

void loop()
{
}
```

---

## Ejemplo: leer un archivo

```cpp
#include <Arduino.h>
#include <JW_SD.h>

JW_SD sd;

void setup()
{
    Serial.begin(115200);
    delay(1000);

    if (!sd.begin(JWPLC_SD_CS))   // GPIO32 en el JWPLC Basic
    {
        Serial.println("SD no disponible");
        return;
    }

    auto file = sd.open("/config.txt", FILE_READ);

    if (!file)
    {
        Serial.println("No existe /config.txt");
        return;
    }

    while (file.available())
    {
        Serial.write(file.read());
    }

    file.close();
}

void loop()
{
}
```

---

## Ejemplo: listar archivos

```cpp
#include <Arduino.h>
#include <JW_SD.h>

JW_SD sd;

void listDir(const char *dirname)
{
    File root = sd.openNative(dirname, FILE_READ);

    if (!root)
    {
        Serial.println("No se pudo abrir directorio");
        return;
    }

    if (!root.isDirectory())
    {
        Serial.println("No es directorio");
        root.close();
        return;
    }

    File file = root.openNextFile();

    while (file)
    {
        Serial.print(file.isDirectory() ? "[DIR] " : "[FILE] ");
        Serial.print(file.name());

        if (!file.isDirectory())
        {
            Serial.print(" | ");
            Serial.print(file.size());
            Serial.print(" bytes");
        }

        Serial.println();

        file.close();
        file = root.openNextFile();
    }

    root.close();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    if (!sd.begin(JWPLC_SD_CS))   // GPIO32 en el JWPLC Basic
    {
        Serial.println("SD no disponible");
        return;
    }

    listDir("/");
}

void loop()
{
}
```

> Nota: para operaciones avanzadas como recorrer directorios, `openNative()` puede ser práctico, pero sus operaciones posteriores ya no quedan protegidas automáticamente por `JWPLCFile`.

---

## Bus SPI compartido

Para proyectos con varios periféricos SPI, se pueden configurar callbacks:

```cpp
sd.setBusLockCallbacks(lockCallback, unlockCallback, userData, 100);
```

Cada operación de `JWPLCFile` intenta bloquear el bus antes de ejecutar la operación sobre el `File` nativo.

Patrón recomendado:

1. Abrir archivo con `open()`.
2. Escribir/leer.
3. Cerrar archivo.
4. Evitar mantener archivos abiertos más tiempo del necesario.

---

## `open()` vs `openNative()`

### `open()`

```cpp
auto file = sd.open("/log.txt", FILE_APPEND);
```

Devuelve un `JWPLCFile`.

Ventajas:

- operaciones comunes protegidas;
- mejor para bus SPI compartido;
- recomendado para uso normal.

### `openNative()`

```cpp
File file = sd.openNative("/log.txt", FILE_READ);
```

Devuelve un `File` nativo de Arduino.

Ventajas:

- compatibilidad con APIs avanzadas;
- útil para directorios y funciones no envueltas por `JWPLCFile`.

Advertencia:

- las operaciones posteriores como `println()`, `read()` o `close()` ya no quedan protegidas automáticamente por `JW_SD`.

---

## DataLog buffered — Alpha12

Alpha12 añade un logger de alto nivel diseñado para no escribir cada registro
directamente sobre la microSD desde el camino caliente de la aplicación.

Clase:

```cpp
JWPLCDataLog
```

Ejemplo:

```cpp
JWPLCDataLog logProceso;

void setup()
{
    logProceso.begin(
        JWPLC_SD,
        "/proceso.csv",
        4096,   // buffer RAM
        1024,   // commit threshold
        1000);  // commit timeout ms

    logProceso.writeLine("time_ms,temp,state");
}

void loop()
{
    char line[64];

    snprintf(
        line,
        sizeof(line),
        "%lu,%.1f,%u",
        millis(),
        42.5f,
        1u);

    logProceso.writeLine(line);

    // En JWPLC Basic normal no es obligatorio llamar service()
    // continuamente: el runtime atiende el manager de DataLogs.
}
```

El buffer se reserva al ejecutar `begin()`. Cada objeto representa un archivo
independiente.

APIs principales:

```cpp
begin(...)
write(...)
writeLine(...)
service()
commit()
close()

isActive()
path()
bufferSize()
pendingBytes()
freeBytes()
commitThreshold()
commitTimeout()

acceptedWrites()
acceptedBytes()
committedBytes()
commitCount()
failedCommits()

lastError()
lastErrorString()
status()
```

El manager de `JW_SD` admite actualmente:

```text
MAX_DATALOGS=4
```

Diagnóstico global:

```cpp
JWPLC_SD.serviceDataLogs();
JWPLC_SD.activeDataLogs();
```

### Autoservicio en JWPLC Basic

El core expone un callback de DataLog y el `jwplcSystemTask` llama
periódicamente al manager.

Por tanto:

```text
DATALOG_BUFFERED_API=PRODUCT
DATALOG_SYSTEM_AUTOSERVICE=YES
```

`service()` sigue disponible para uso explícito/standalone.

### Reglas de uso

- `write()/writeLine()` aceptan datos en RAM antes del commit físico;
- el commit ocurre por threshold/timeout o llamada explícita;
- revisar `failedCommits()` / `lastError()` en aplicaciones críticas;
- `close(true)` intenta confirmar datos pendientes;
- no asumir que un dato aceptado en RAM ya fue persistido físicamente.

La regresión full-runtime Alpha12 validó DataLog con:

```text
SD_DATALOG_FAILED_COMMITS=0
```

durante las campañas de coexistencia cerradas.

---

## Recomendaciones para microSD

Para JWPLC Basic se recomienda:

- Formato **FAT32**.
- Para tarjetas grandes, usar partición **MBR**.
- Evitar exFAT si se quiere máxima compatibilidad Arduino.
- Cerrar archivos después de escribir.
- No retirar la microSD durante escritura.
- Evitar operaciones SD pesadas dentro de callbacks gráficos.
- Cachear datos en `loop()` y dibujar en pantalla solo variables simples.

---

## Problemas comunes

### La SD no inicia

Revisar:

- Pin CS correcto.
- Alimentación estable.
- Tarjeta formateada en FAT32.
- Partición MBR en tarjetas grandes.
- Cableado SPI.
- Que otro periférico SPI no esté dejando el bus bloqueado.

### Archivo no abre

Revisar:

- Ruta con `/` inicial.
- Modo correcto: `FILE_READ`, `FILE_WRITE`, `FILE_APPEND`.
- Que la tarjeta esté montada.
- Que el archivo exista si se abre solo lectura.

### Datos corruptos o archivo incompleto

Posibles causas:

- Retiro de microSD durante escritura.
- Falta de `close()`.
- Reset del equipo durante escritura.
- Acceso simultáneo al bus SPI.
- Escrituras demasiado frecuentes sin control.

### Conflictos con TFT/Ethernet/FRAM

En JWPLC Basic, evita consultar SD desde callbacks de dibujo.

Recomendado:

```cpp
// En loop()
actualizarEstadoDesdeSD();

// En callback gráfico
dibujarVariablesCacheadas();
```

No recomendado:

```cpp
// Dentro de callback gráfico
auto file = JWPLC_SD.open("/log.txt", FILE_READ);
```

---

## Correcciones de mantenimiento en Alpha13

Se corrigió un carácter de control NUL accidental en la fuente de SD y
se volvió a generar `libJW_SD.a` desde el código corregido. Se conserva
la API `JWPLC_SD` y el DataLog buffered de Alpha12.

Se comprobó en un JWPLC Basic real que la microSD puede montarse y
abrir su carpeta raíz en modo **sólo lectura**, con RTC y botonera activos.
Esa prueba no incluye escritura o extracción de la tarjeta en caliente.

**Recomendación:** cierra los archivos después de escribir y no realices
operaciones SD dentro de callbacks gráficos.

## Pines en el JWPLC Basic (2.1.0-alpha.14)

La microSD comparte el bus SPI con la TFT, el W5500 y la FRAM; cada chip se
elige con su propio pin CS. En vez de escribir el número (`32`), los ejemplos
y este README usan la constante que define el core del JWPLC:

```cpp
JW_SD sd(JWPLC_SD_CS);   // o sd.begin(JWPLC_SD_CS)
```

Así el sketch sigue siendo correcto aunque cambie la placa, y se evita
confundir el CS de la microSD con el de otro periférico del bus.

En un sketch normal del JWPLC Basic no necesitas crear un `JW_SD` propio:
usa el objeto global `JWPLC_SD`, que el runtime ya inicializa. Los ejemplos
standalone de esta librería crean su propia instancia para poder usarse
también fuera del package.

La librería y su `libJW_SD.a` no cambian en esta versión.

## Estado Alpha12 (funcionalidad conservada en Alpha13 y 2.1.0-alpha.14)

```text
JWPLC ESP32 v2.1.0-alpha.14 (funcionalidad de v2.1.0-alpha.12)
JW_SD 1.0.2
SPI_SHARED_BUS_PROTECTION=YES
DATALOG_BUFFERED_API=YES
DATALOG_SYSTEM_AUTOSERVICE=YES
FULL_RUNTIME_DATALOG=PASS_PHYSICAL
```

La documentación futura de otros ciclos debe mantenerse fuera de este README
de cierre cuando todavía no corresponda al package publicado.
