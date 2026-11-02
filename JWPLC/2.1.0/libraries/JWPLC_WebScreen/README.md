# JWPLC_WebScreen

La pantalla del JWPLC Basic **en el navegador**, en vivo y con botonera, por Ethernet. Sirve para
demostraciones en un proyector y para soporte remoto. Versión **0.1.0** (package 2.1.0-alpha.16).

```cpp
#include <JWPLC_WebScreen.h>

void setup()
{
    JWPLC_WebScreen.begin(); // puerto 80; escucha en cuanto Ethernet tenga IP
}

void loop()
{
}
```

Abrir `http://<IP del equipo>/` en Chrome o Edge (la IP sale en la pantalla de reposo o en *Red* del Runtime).
La página repite la TFT tal cual; sus teclas, o las flechas, Enter y Esc del teclado, funcionan como la botonera
física, que sigue funcionando. En la página, `T` oculta las teclas y `F` pasa a pantalla completa.

| Llamada | Para qué |
|---|---|
| `begin(port = 80)` | arranca el servidor |
| `url()` | dirección para el navegador, p. ej. `"http://192.168.1.50/"` (vacía sin IP) |
| `viewerConnected()` | hay un navegador mirando (pidió la pantalla en los últimos 3 s) |
| `allowKeys(bool)` / `keysAllowed()` | permitir o no que la página pulse teclas (sí por defecto) |
| `isRunning()` / `port()` | estado del servidor |

- **Memoria:** 54 KB de RAM para la copia de la pantalla, reservados al arrancar la TFT.
- **Red:** solo viajan los tramos de fila que cambiaron; con la pantalla quieta casi no hay tráfico.
- **Seguridad:** sin contraseña; usarla solo en redes de confianza. Solo Ethernet (WiFi, más adelante).

Ejemplos: `01.Pantalla_Web` (cualquier sketch) y `02.Runtime_En_Vivo` (Runtime con IP fija `192.168.10.10`
para cable directo a la laptop). JWPLC Sim muestra la misma página en `127.0.0.1:1080`.

Se distribuye en código fuente (sin `.a`). Detalle y mediciones: `docs/v2.1.0-alpha.16/A16_PANTALLA_WEB_20261101.md`.
