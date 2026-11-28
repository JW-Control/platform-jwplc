# JWPLC_Web

Panel web del JWPLC Basic, con usuarios y contraseña. Con `JWPLC_Web.begin()` y `publish()` el equipo sirve un
panel para el celular o la PC que se arma solo con las variables del programa: no hay que escribir HTML.

Reemplaza a `JWPLC_WebScreen` (alpha.16 a alpha.18): la pantalla del equipo en vivo con la botonera ahora es la
pestaña **Pantalla** del panel y pide sesión.

```cpp
#include <JWPLC_Web.h>

float presion = 0;
float consigna = 3.5;

void setup()
{
    JWPLC_Web.begin("Bombeo Edificio A");
    JWPLC_Web.publish("presion", presion).label("Presión de red").unit("bar").range(0, 10).trend();
    JWPLC_Web.publish("consigna", consigna).label("Consigna").unit("bar").range(1, 6).writable(WEB_OPERADOR);
    JWPLC_Web.addAlarm("baja", "Presión baja", WEB_ALARMA_ALTA);
}

void loop()
{
    JWPLC_Web.setAlarm("baja", presion < 1.0);
}
```

## Qué trae el panel

| Pestaña | Qué hace | Rol mínimo |
|---|---|---|
| Inicio | Variables publicadas en vivo (1 s), agrupadas con `group()`; «Cambiar valor» si la variable es `writable()` | Visor (mirar), el de `writable()` (cambiar) |
| Alarmas | Activas, reconocidas y su historial (24 eventos) | Visor (mirar), Operador (reconocer) |
| Tendencias | Hasta 4 variables con `trend()`, 120 puntos cada una, descarga CSV | Visor |
| Pantalla | La TFT en vivo y la botonera | Visor (mirar), Operador (teclas) |
| Equipo | IP, memoria, sesiones; usuarios (admin); auditoría de cambios (técnico o más) | Visor |

## Primer ingreso y seguridad

- **Sin clave de fábrica.** Mientras el equipo no tiene usuarios, el primer ingreso pide un código de 6 dígitos
  que cambia cada 60 s y crea el usuario administrador. El código se ve en la TFT (Runtime: *MENU › Red ›
  Conectar celular*; en un sketch propio, `JWPLC_Web.pairingCode()` o el QR de `pairingURL()`) y en el monitor
  serie. Quien lo lee tiene el equipo delante: eso prueba que puede reclamarlo.
- **La contraseña no viaja por la red.** El navegador deriva una clave con PBKDF2-HMAC-SHA256 (2000 vueltas, sal
  de 16 bytes) y en cada ingreso firma con HMAC-SHA256 un reto de un solo uso que da el equipo. El equipo guarda
  solo la sal y la clave derivada (NVS), nunca la contraseña.
- **Sesiones:** cookie `HttpOnly` y `SameSite=Strict`, 30 min sin uso, hasta 4 abiertas. Todo POST exige la
  cabecera `X-JW` (contra CSRF).
- **Bloqueo:** 5 intentos fallidos bloquean el ingreso 5 minutos. A un usuario que no existe se le contesta igual
  que a uno que existe (no se puede adivinar quién hay).
- **Roles:** `WEB_VISOR` < `WEB_OPERADOR` < `WEB_TECNICO` < `WEB_ADMIN`. Hasta 6 usuarios.
- **Cabeceras:** CSP sin recursos externos, `X-Frame-Options: DENY`, `nosniff`, `no-referrer`.
- **Auditoría:** quién cambió qué variable (valor anterior y nuevo), ingresos, ingresos fallidos y cambios de
  usuarios (últimos 16).
- **Recuperación:** `JWPLC_Web.resetUsers()` borra usuarios y sesiones (por ejemplo, manteniendo una tecla al
  encender); el próximo ingreso vuelve a pedir el código.

Límite honesto: el panel va por HTTP (sin TLS) dentro de la red de la planta. No abrir puertos del router hacia el
equipo: para verlo desde afuera usar JW Cloud o una VPN.

## API

| Llamada | Qué hace |
|---|---|
| `JWPLC_Web.begin(nombre, puerto = 80)` | Arranca el panel; escucha en cuanto hay red |
| `JWPLC_Web.publish(id, variable)` | Publica un `bool`, `int`, `long`, `unsigned long` o `float` global |
| `.label(texto)` `.unit(texto)` `.group(texto)` | Nombre, unidad y grupo en el panel |
| `.range(min, max)` `.decimals(n)` | Rango (barra y validación al escribir) y decimales |
| `.writable(rol)` | Rol mínimo para cambiarla (por defecto nadie: solo lectura) |
| `.trend()` | La guarda en la tendencia |
| `JWPLC_Web.addAlarm(id, texto, prioridad)` | Declara una alarma (`WEB_ALARMA_BAJA/MEDIA/ALTA`) |
| `JWPLC_Web.setAlarm(id, activa)` | La enciende o apaga desde el loop (barato si no cambia) |
| `JWPLC_Web.alarmActive(id)` `activeAlarms()` | Estado de alarmas |
| `JWPLC_Web.trendEvery(segundos)` | Paso de la tendencia (por defecto 5 s: 10 min) |
| `JWPLC_Web.pairingCode()` `pairingURL()` | Código de primer ingreso y URL para un QR (vacíos si ya hay usuarios) |
| `JWPLC_Web.hasUsers()` `resetUsers()` | Usuarios |
| `JWPLC_Web.url()` | `http://IP/` (vacía sin red) |
| `JWPLC_Web.allowKeys(bool)` | Permite o no la botonera desde la web |
| `JWPLC_Web.viewerConnected()` `sessions()` | Alguien mirando la pantalla; sesiones abiertas |

Límites: 32 variables, 16 alarmas, 6 usuarios, 4 variables en tendencia. Ids con letras, dígitos, `_` y `-`.

## Red

Atiende por cable, por WiFi o por los dos según `JWPLC_Red` (`useEthernet`, `useWiFi`, `useAuto`). Para WiFi,
incluir también `<JWPLC_WiFi.h>` en el sketch.

## Memoria (JWPLC Basic v2, ESP32 de 4 MB sin PSRAM)

- Página: ~11 KB comprimida dentro del firmware (no usa sistema de archivos).
- RAM: ~13 KB fijos (variables, alarmas, tendencia, sesiones y búferes HTTP) + 54 KB de la copia de la pantalla + 7 KB de pila
  de la tarea web. Si no hay memoria para la copia, el panel funciona sin la pestaña Pantalla.
- Sin actualización del firmware por la web en la v2 (la partición no tiene segundo espacio de OTA). En la v3
  (ESP32-S3 con 16 MB) se podrá sumar OTA firmado, más historia y HTTPS.

## Ejemplos

| Ejemplo | Qué muestra |
|---|---|
| `01.Panel_Basico` | Un contador, un límite editable y una alarma |
| `02.Bombeo_Presion_Constante` | Bombeo de dos bombas con consigna, alarmas, tendencia y QR de primer ingreso en la TFT |
| `03.Pantalla_Web` | Solo la pantalla en vivo (antes `JWPLC_WebScreen` 01) |
| `04.Runtime_En_Vivo` | Runtime de panel con IP fija y el panel web (antes `JWPLC_WebScreen` 02) |

## Desarrollo

La página está en `extras/panel.html`. Después de cambiarla, regenerar el header embebido:

```bash
node extras/generar_pagina.mjs   # escribe src/JWPLC_Web_Page.h
```
