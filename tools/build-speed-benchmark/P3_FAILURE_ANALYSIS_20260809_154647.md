# P3 - analisis de fallo 2026-08-09 15:46:47

## Resultado observado

Cold P2 + P3:

- 104.788 s
- 31 compilaciones
- JWPLC_Display fuente: 0
- core stub P2: 1
- 51 pasadas g++ -E
- 0 pasadas sobre JWPLC_Display.cpp
- app P2 referencia: 406016 bytes
- app P3 fallida: 465168 bytes

El validador rechazo P3 correctamente porque el tamano y el payload cambiaron.

## Causa 1: archive Display no extraido

El map de P3 contiene la carga de `libJWPLC_Display.a`, pero ningun miembro del archive fue extraido. En particular no aparecen los equivalentes enlazados de:

- JWPLC_Display.cpp.o
- JWPLC_IdleScreen.cpp.o

El core JWPLC contiene implementaciones weak de callbacks del display. Al convertir JWPLC_Display en un archive estatico, esas definiciones weak ya satisfacen las referencias existentes y el linker no tiene una referencia fuerte que obligue a extraer el objeto de la libreria precompilada.

Decision P3: declarar en `JWPLC_Display/library.properties`:

`ldflags=-Wl,--undefined=JWPLC_Display`

El simbolo global `JWPLC_Display` pertenece a JWPLC_Display.cpp.o. Forzarlo como undefined obliga al linker a extraer ese miembro del archive; las referencias internas deben arrastrar despues JWPLC_IdleScreen.cpp.o. No se usa `--whole-archive` para evitar incluir secciones innecesarias.

## Causa 2: marcador Ethernet incorrecto

Los marcadores bundled experimentales forzaron la libreria `Ethernet` v3.3.8 incluida con el core ESP32. Esa libreria contiene ETH.cpp/Network/esp_netif y no es la libreria Arduino Ethernet 2.0.2 utilizada por JWPLC_Ethernet para W5500.

El mismo build termino resolviendo tambien Ethernet 2.0.2 desde el sketchbook, por lo que coexistieron dos librerias homonimas y el firmware crecio aproximadamente 59 KiB.

Decision: retirar todos los marcadores bundled experimentales de P3. Durante discovery, `JWPLC_Display_Auto.h` incluye `Adafruit_ST7789.h` mediante la resolucion normal de Arduino. `JWPLC_GlobalPeripherals_Auto.h` permanece como marcador liviano de la libreria JWPLC propia.

La reproducibilidad de dependencias externas se tratara como una tarea separada; no se resolvera forzando marcadores con nombres que puedan colisionar con librerias del core.

## Criterio para la siguiente corrida

P3 solo se aprueba si:

- compila correctamente;
- JWPLC_Display fuente = 0;
- core stub = 1;
- el map demuestra extraccion del archive Display;
- no aparece simultaneamente Ethernet 3.3.8 y Ethernet 2.0.2;
- app conserva tamano y payload equivalente a P2;
- se registra la reduccion real de pasadas de preprocesamiento y tiempo.
