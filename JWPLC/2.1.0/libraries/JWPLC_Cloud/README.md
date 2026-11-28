# JWPLC_Cloud

Manda variables y alarmas del JWPLC Basic a **JW Cloud**, por cable o WiFi, sin detener el `loop()`.
El cliente las ve en la app de JW Cloud (valores, tendencia, historial de alarmas) y cada alarma
nueva le llega como aviso al celular.

```cpp
#include <JWPLC_Cloud.h>

void setup()
{
    JWPLC_Cloud.addNumber("p", "Presión", "bar", 2, 0, 6);
    JWPLC_Cloud.addBool("b1", "Bomba 1");
    JWPLC_Cloud.addAlarm("E01", "Cisterna vacía");
    JWPLC_Cloud.begin("token-del-equipo");
}

void loop()
{
    JWPLC_Cloud.set("p", presion);
    JWPLC_Cloud.set("b1", digitalRead(Q0_0));
    JWPLC_Cloud.alarm("E01", !digitalRead(I0_0));
}
```

## Cómo funciona

- `set()` y `alarm()` solo guardan el valor. Una tarea aparte toma una muestra cada `setInterval()`
  (10 s por defecto) y la manda por HTTPS (`JWPLC_TLS`) a `ota.jw-control.com`.
- Sale por la red activa de `JWPLC_Red`: cable por defecto; con `JWPLC_WiFi` y `JWPLC_Red.useAuto()`,
  cable o WiFi sin cambiar el sketch.
- Sin red guarda hasta 120 muestras y hasta 32 cambios de alarma, y los manda al volver con su
  hora real (cada dato viaja con su antigüedad: el JWPLC no necesita reloj).
- `alarm()` se puede llamar en cada vuelta: solo cuenta el cambio. Al arrancar se manda el estado
  de todas las alarmas, y JW Cloud cierra las que quedaron abiertas de antes.
- `status()` / `statusString()`: `CONECTANDO`, `EN LINEA`, `SIN RED`, `TOKEN NO VALIDO`,
  `ERROR DEL SERVIDOR` o el error TLS (`CERTIFICADO NO VALIDO`...). `pending()` y `sentCount()`.

## Alta del equipo

JW Control da de alta el equipo en el panel de JW Cloud con el correo de la cuenta del cliente y
entrega el **token** (se muestra una sola vez). Va en `begin(token)`.

## Límites

16 variables (número o ON/OFF) y 16 alarmas. id: letras, números o `_`, hasta 15. Nombre hasta 40
bytes, unidad hasta 12, texto de alarma hasta 60. Memoria: unos 17 KB de RAM más los ~40 KB del
saludo TLS mientras envía.

## Probar sin Internet (JW Cloud local)

En el repo de JW Cloud: `node scripts/cloud-dev.mjs 8788 --plc --push`. En el sketch:
`setServer("<IP de la PC>", 8788, false)` y el token que muestra cloud-dev. En JWPLC Sim la PC es
`192.168.250.1`. HTTP sin TLS solo dentro de la red local.
