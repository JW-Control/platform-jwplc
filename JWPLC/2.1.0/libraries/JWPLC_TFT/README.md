# JWPLC_TFT

Backend grafico propio del ecosistema JWPLC.

## Estado H3E.2A

Esta primera fundacion:

- expone el objeto global `JWPLC_TFT`;
- no expone tipos `TFT_eSPI` ni Adafruit en la API publica;
- soporta el hardware actual JWPLC Basic v2:
  - ST7789;
  - panel fisico 170x320;
  - landscape logico 320x170;
  - rotation 1;
  - BGR;
  - inversion ON;
  - SPI MODE0;
  - 80 MHz;
- usa el mutex SPI global del JWPLC;
- permite batching mediante `beginBatch()/endBatch()`;
- cubre las primitivas que hoy necesita `JWPLC_Display`.

El backend H3E.2A usa temporalmente TFT_eSPI 2.5.43 como dependencia privada.

## Pendientes deliberados

H3E.2B debe resolver el empaquetado interno/pinneado del backend para no
depender de configuracion manual del usuario ni de `User_Setup.h`.

JWPLC Basic v3 usara tambien ST7789, con panel 240x320. Ese perfil queda
reservado, pero no se habilita en 2.1.x hasta fijar y calificar el target de
board, pinout y configuracion fisica final.

La HMI declarativa, paginas, modo IDLE y el contrato del HMI Designer siguen
perteneciendo a `JWPLC_Display`; H3E.3 migrara esa capa para dibujar sobre
`JWPLC_TFT`.
