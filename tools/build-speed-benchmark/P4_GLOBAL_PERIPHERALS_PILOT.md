# P4 - piloto GlobalPeripherals precompilado por perfil

## Objetivo

Medir si precompilar `JWPLC_GlobalPeripherals.cpp` reduce de forma relevante el cold build restante despues de D1 + P1 + P2 + P3.

En el P3 validado se observaron varias pasadas de discovery sobre `JWPLC_GlobalPeripherals.cpp`, por lo que es el siguiente candidato interno de alto interes.

## Restriccion de perfil

`JWPLC_GlobalPeripherals.cpp` usa macros de hardware que difieren entre JWPLC Basic y JWPLC Basic Core, entre ellas:

- `JWPLC_HAS_FRAM`
- `JWPLC_HAS_SD`
- `JWPLC_HAS_ETHERNET`
- `JWPLC_FRAM_SIZE_BYTES`

Por tanto, un archive generado para Basic no se debe reutilizar automaticamente en Basic Core.

P4 se ejecuta exclusivamente sobre Basic. El script retira el archive de GlobalPeripherals al finalizar incluso cuando la prueba pasa.

## Discovery liviano

Cuando una libreria usa `precompiled=full`, Arduino puede omitir la inspeccion de dependencias de sus fuentes. Para conservar el autoload sin volver a expandir headers pesados se agregan headers marcador vacios a las librerias JWPLC implicadas y a SPI/Wire/SD.

No se agrega marcador para `Ethernet` porque existen dos backends homonimos en el entorno actual:

- la libreria clasica Arduino `Ethernet.h` usada por `JWPLC_Ethernet`;
- el backend `ETH.h` de Espressif incluido con Arduino-ESP32 3.3.8.

`JWPLC_Ethernet` se selecciona mediante su propio marcador y su fuente sigue siendo responsable de descubrir el backend real usado actualmente.

## Archive y ancla

Durante el piloto se genera:

`JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/src/esp32/libJWPLC_GlobalPeripherals.a`

El archive contiene el objeto Basic ya producido por el P2 validado.

`library.properties` usa `ldflags=-Wl,--undefined=JWPLC_RTC` como ancla unica del objeto de GlobalPeripherals. Al ser una sola unidad de traduccion, su extraccion incorpora tambien los providers fuertes RTC/FRAM/SD/botonera que sustituyen los fallbacks weak del core.

## Seguridad del piloto

`Run-JWPLCP4GlobalPeripheralsPilot.ps1`:

1. reutiliza los objetos P2 existentes;
2. restaura temporalmente el archive P3 de Display;
3. genera temporalmente el archive P4 de GlobalPeripherals Basic;
4. ejecuta un cold build limpio;
5. comprueba seleccion de librerias, objetos externos y members enlazados;
6. elimina los archives P3/P4 al terminar, incluso si hay fallo.

Con los archives eliminados, `precompiled=full` vuelve automaticamente al fuente y Basic/Core quedan en un estado seguro.

## Criterio para invertir en una solucion final por perfil

Si P4 reduce de forma significativa el cold build sin alterar el grafo externo, se implementara un mecanismo final que permita almacenar un archive GlobalPeripherals diferente para cada perfil de placa.

Si la mejora es pequena, se mantendra el fuente y se priorizaran otros cuellos de discovery antes de aumentar la complejidad del package.
