# Alpha5 - Validacion fisica de compatibilidad

Fecha: 2026-08-23
Rama: `v2.1.0-alpha.5/feature/esp32-precompiled-compatibility`
Target: `jwplc_local:esp32:jwplcbasic`

## Objetivo

Cerrar los gates fisicos pendientes tras retirar archives ESP32 acoplados al core JWPLC y devolver a compilacion desde fuente `JW_MatrixButtons`, `JW_SD`, `SD`, `Adafruit_BusIO`, `Adafruit_GFX_Library`, `JWPLC_Display` y `JWPLC_Ethernet_W5x00_Backend`.

Las pruebas se realizan con el autoload normal activo y sin retirar perifericos del package.

## 1. Botonera fisica JWPLC Basic

Sketch:

```txt
tools/build-speed-benchmark/sketches/05_buttons_physical/05_buttons_physical.ino
```

Resultado:

```txt
PASS
```

Arranque observado:

```txt
JWPLC Alpha5 - gate fisico botonera
Botonera ready: YES
Presionar una vez cada boton: LEFT, UP, RIGHT, ESC, OK, DOWN.
JWPLC_Display inicializado
```

Eventos fisicos recibidos durante la prueba:

```txt
[PRESS] LEFT
[PRESS] UP
[PRESS] DOWN
[PRESS] ESC
[PRESS] OK
[PRESS] RIGHT
```

Se observaron correctamente los seis IDs fisicos esperados: `LEFT`, `UP`, `RIGHT`, `ESC`, `OK` y `DOWN`.

La aparicion posterior de `JWPLC_Display inicializado` confirma que la prueba se ejecuto con el autoload normal activo y no impidio la lectura de la botonera.

Conclusion: la vuelta de `JW_MatrixButtons` a compilacion desde fuente conserva el comportamiento fisico de la botonera en JWPLC Basic.

## 2. microSD fisica integrada

Sketch:

```txt
tools/build-speed-benchmark/sketches/06_sd_physical/06_sd_physical.ino
```

Resultado:

```txt
PASS
```

Salida observada:

```txt
JWPLC Alpha5 - gate fisico microSD
SD enabled: YES
SD ready: YES
Card present: YES
Leido: JWPLC Alpha5 SD source PASS
[PASS] Escritura y lectura microSD correctas
JWPLC_Display inicializado
```

La prueba confirma en hardware real:

- capability SD habilitada en JWPLC Basic;
- inicializacion correcta del objeto global `JWPLC_SD`;
- deteccion de tarjeta presente;
- apertura y escritura de archivo;
- cierre y reapertura;
- lectura correcta del contenido escrito;
- convivencia con el autoload de Display y el bus SPI compartido.

Conclusion: retirar los archives precompilados de `JW_SD` y `SD` y volverlos a compilacion desde fuente no rompe el uso fisico de microSD en JWPLC Basic.

## Resultado global de hardware

```txt
Botonera: PASS
microSD:  PASS
```

Con estas pruebas quedan cerrados los dos gates fisicos que faltaban para la correccion de compatibilidad de precompilados.

El siguiente paso ya no es una correccion funcional: corresponde ejecutar una auditoria global final y un benchmark fresco de tiempos de compilacion con el estado compatible actual, porque los tiempos Alpha4 ya no representan exactamente la configuracion vigente.
