# JWPLC_TFT

Backend grafico propio del ecosistema JWPLC.

## Arquitectura

`JWPLC_TFT` expone una API publica independiente del motor grafico. El
backend calificado para JWPLC Basic v2 se distribuye precompilado dentro del
package, por lo que el usuario no necesita instalar ni configurar TFT_eSPI.

La API publica no expone tipos de TFT_eSPI ni Adafruit.

## Hardware activo â€” JWPLC Basic v2

- controlador: ST7789;
- panel fisico: 170x320;
- geometria logica landscape: 320x170;
- rotation: 1;
- orden de color: BGR;
- inversion: ON;
- SPI: MODE0;
- frecuencia TFT: 80 MHz;
- bus compartido protegido mediante el mutex SPI del JWPLC.

## API

El objeto global es:

```cpp
JWPLC_TFT
```

La API incluye primitivas de dibujo, texto, geometria y batching. El batching
`beginBatch()/endBatch()` permite que `JWPLC_Display` agrupe un dirty pass
completo bajo una sola transaccion del backend.

## Backend

Para ESP32 el package usa:

```text
src/esp32/libJWPLC_TFT.a
```

El archive contiene el wrapper `JWPLC_TFT` y el backend TFT_eSPI 2.5.43
calificado. `library.properties` no declara TFT_eSPI como dependencia de
usuario.

Los sources `JWPLC_TFT.cpp` y `tft_setup.h` permanecen versionados para
mantenimiento y regeneracion del archive. Los builds normales del package
deben seleccionar `precompiled=full` y no compilar esos sources.

## JWPLC Basic v3

El target planificado usa tambien ST7789 con panel 240x320. Ese perfil no se
activa en 2.1.x hasta fijar y calificar board target, pinout, offsets,
orientacion y configuracion fisica final.

## Relacion con JWPLC_Display

`JWPLC_TFT` es la capa de hardware/renderer. La HMI declarativa, paginas,
modo IDLE, dirty cache y contrato del JWPLC HMI Designer pertenecen a
`JWPLC_Display`.

La migracion de `JWPLC_Display` a este backend se realiza en H3E.3.

## Licencias de terceros

El backend precompilado incorpora TFT_eSPI 2.5.43. Los avisos originales se
conservan en:

```text
licenses/TFT_eSPI-2.5.43-license.txt
```