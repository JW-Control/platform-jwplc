# v2.1.0-alpha.19 — Panel web con usuarios, red por cable o WiFi y una placa por package

> **Borrador de cierre (28/11).** Se completa cuando entren las features abiertas de la línea: PR #255
> (`JWPLC_WiFi` y `JWPLC_Red`) y PR #257 (`JWPLC_Web`). Hasta entonces, lo marcado *(pendiente)* no está en
> `v2.1.0-alpha.19/JWPLC-Sim/main`.

**Canal:** PreRelease de desarrollo (índice dev). No sustituye a `v2.0.0` estable.
**Origen:** línea `JWPLC-Sim`, rama `v2.1.0-alpha.19/JWPLC-Sim/main`. Se publica sobre la 2.1.0-alpha.18.

## Antes de actualizar: cambia el FQBN de ESP32 Board

**Quien usa la placa «ESP32 Board» (ESP32 genérico) en Arduino IDE o en un script tiene que cambiar de package.**

| | Hasta alpha.18 | Desde alpha.19 |
|---|---|---|
| Package en el gestor de placas | «JW Control ESP32 Boards» | **«ESP Board»** (packager `jwesp`) |
| FQBN de ESP32 Board | `jwplc:esp32:esp32` | **`jwesp:esp32:esp32`** |
| Package del JWPLC Basic | «JW Control ESP32 Boards» | **«JWPLC»** (mismo packager `jwplc`, mismo FQBN `jwplc:esp32:jwplcbasic`) |

Qué hacer:

1. En **Herramientas › Placa › Gestor de placas**, actualizar **JWPLC** a 2.1.0-alpha.19 (el JWPLC Basic no
   cambia de FQBN).
2. Si se usa un ESP32 genérico: instalar también **ESP Board** 2.1.0-alpha.19 y volver a elegir
   **Herramientas › Placa › ESP Board › ESP32 Board**. El IDE no la cambia solo.
3. En scripts (`arduino-cli compile --fqbn …`), cambiar `jwplc:esp32:esp32` por `jwesp:esp32:esp32`.

`jwplc:esp32:esp32` sigue compilando (placa oculta, «ESP32 Board (compatibilidad)») para no romper los scripts
que lo fijan, como Laundry (`jwplc:esp32:esp32:PartitionScheme=no_fs`). No sale en el menú y se retirará en una
alpha futura. Con esa placa oculta, `JW_RTC` del package no enlaza (ya pasaba en alpha.18): usar la `JW_RTC` de
`JW-Control/JW-Libraries` en la carpeta `libraries` del usuario. Detalle en
[A19_LIBRERIAS_POR_PLACA_20261128.md](A19_LIBRERIAS_POR_PLACA_20261128.md) §3 y §5.

## Qué trae

- **Una placa por package** (#253, #256):
  - **JWPLC** (`jwplc`): solo el JWPLC Basic. **Archivo › Ejemplos** muestra solo las librerías `JW_` y
    `JWPLC_`; las de arduino-esp32 siguen instaladas y compilan (`#include <WiFi.h>` compila en el Basic).
  - **ESP Board** (`jwesp`): ESP32 Board con los ejemplos originales de arduino-esp32 3.3.8 y sin librerías JW.
    Usa las mismas herramientas de `jwplc`: no descarga el compilador otra vez.
  - **JWPLC Basic Core** queda oculta (decisión del propietario, 28/11: solo se vende y se mantiene el Basic). El
    FQBN sigue compilando.
- *(pendiente, PR #255)* **`JWPLC_WiFi` y `JWPLC_Red` 0.1.0:** red por cable o por WiFi con la misma API,
  punto de acceso con QR.
- *(pendiente, PR #257)* **`JWPLC_Web` 0.1.0:** panel web con usuarios y contraseña (reemplaza a
  `JWPLC_WebScreen`). Variables publicadas, consignas con rol y rango, alarmas con reconocimiento, tendencias con
  CSV y la pantalla en vivo. Sin clave de fábrica: el primer ingreso pide el código de 6 dígitos del equipo.
  **`JWPLC_WebScreen` sale del package:** los sketches que la usen pasan a `JWPLC_Web` (la pantalla en vivo es su
  pestaña **Pantalla**).

## Instalación

1. Package: en Arduino IDE, con el índice dev (ver README), actualizar **JWPLC** a 2.1.0-alpha.19. Opcional:
   **ESP Board** para un ESP32 genérico (ver arriba).
2. OpenPLC Editor: *(se completa al cierre: versión de la JWPLC Edition que acompaña a esta alpha)*.
3. JWPLC Sim 0.7.0 trae el curso C++ completo hasta la Unidad 9, incluida la Unidad 8 (Ethernet, Modbus TCP y
   panel web), y la Unidad 5 del curso IEC (Comunicaciones).

## Verificación

*(Se completa al cierre en `ALPHA19_STATUS.md` §3.)* Hechas hasta ahora:

- #253 y #256: pruebas de instalación y compilación con arduino-cli 1.3.1 en la nube
  ([A19_LIBRERIAS_POR_PLACA_20261128.md](A19_LIBRERIAS_POR_PLACA_20261128.md) §4) y `SIM_REGRESSION` en verde.
- Pendiente: Arduino IDE 2.3.10 en la PC del propietario con el índice de la PreRelease (menú Ejemplos con las
  dos placas y actualizar desde alpha.18 con ESP32 Board elegida).

## Limitaciones conocidas

- El IDE no migra solo la placa elegida: quien tenía ESP32 Board de `jwplc` debe elegirla otra vez en **ESP Board**.
- `JW_RTC` del package no enlaza con `jwplc:esp32:esp32` (ver arriba).
- Sólo Windows x64 para los instaladores.

## Versiones que forman esta Release

| Componente | Versión | Commit |
|---|---|---|
| Package `jwplc:esp32` («JWPLC») | 2.1.0-alpha.19 | `release/v2.1.x` al publicar |
| Package `jwesp:esp32` («ESP Board») | 2.1.0-alpha.19 | armado al publicar desde el mismo árbol (`jwplc_esp32_generico.py`) |
| HMI Designer | *(al cierre)* | |
| VPP JWPLC Basic | *(al cierre)* | |
| OpenPLC Editor – JWPLC Edition | *(al cierre)* | |
| JWPLC Sim (gate `SIM_REGRESSION`) | 0.7.0 | jwplc-sim `feature/alpha19` en la corrida del PR de cierre |
