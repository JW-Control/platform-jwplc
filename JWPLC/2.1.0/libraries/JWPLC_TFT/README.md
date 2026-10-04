# JWPLC_TFT

Backend gráfico propio del ecosistema JWPLC.

## Arquitectura

`JWPLC_TFT` expone una API pública independiente del motor gráfico.

Durante el cierre Alpha12 se compila **source-first** para que las fuentes actuales sean autoritativas. Antes de publicar Alpha12 se regenerará y recalificará el archive precompilado correspondiente.

El usuario no necesita instalar ni configurar TFT_eSPI.

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

Fuentes autoritativas:

```text
src/JWPLC_TFT.cpp
src/tft_setup.h
```

Archive destinado al package final:

```text
src/esp32/libJWPLC_TFT.a
```

El backend incorpora TFT_eSPI 2.5.43 detrás de la API `JWPLC_TFT`.
`library.properties` no declara TFT_eSPI como dependencia de usuario.

Estado durante el cierre Alpha12:

```text
SOURCE_FIRST=YES
PRECOMPILED_FINAL_ALPHA12=PENDING_REGEN
```

No se debe reutilizar un archive histórico como si representara las fuentes actuales.

## JWPLC Basic v3

El target planificado usa tambien ST7789 con panel 240x320. Ese perfil no se
activa en 2.1.x hasta fijar y calificar board target, pinout, offsets,
orientacion y configuracion fisica final.

## Relacion con JWPLC_Display

`JWPLC_TFT` es la capa de hardware/renderer. La HMI declarativa, paginas,
modo IDLE, dirty cache y contrato del JWPLC HMI Designer pertenecen a
`JWPLC_Display`.

La migración de `JWPLC_Display` a este backend ya fue completada durante H3E y validada bajo full runtime.

Alpha13 será el ciclo dedicado a continuar la evolución funcional de TFT/Display sobre esta arquitectura, sin volver a exponer TFT_eSPI como dependencia pública.

## Licencias de terceros

El backend precompilado incorpora TFT_eSPI 2.5.43. Los avisos originales se
conservan en:

```text
licenses/TFT_eSPI-2.5.43-license.txt
```

## Estado Alpha12

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_TFT 0.1.0
JWPLC_DISPLAY_BACKEND_MIGRATION=COMPLETE
FULL_RUNTIME_TFT=PASS_PHYSICAL
TFT_ESPI_USER_DEPENDENCY=NO
PRECOMPILED_ALPHA12=PENDING_REGEN
NEXT_DISPLAY_ALPHA=13
```

La API raw histórica basada en tipo explícito `Adafruit_ST7789&` no forma parte
del backend actual. El patrón recomendado es obtener `JWPLC_TFTClass&` mediante
`JWPLC_Display.tft()` o usar directamente las APIs HMI de alto nivel.
