# Direccion P5 - 2026-08-09

## Decision

Se invierte el orden preliminar indicado al cierre de P3 determinista.

Orden vigente:

1. **P5A: backend Ethernet W5x00 2.0.2 precompilado.**
2. **P5B: evaluar precompilacion Adafruit bundled solo despues de resolver su layout.**

## Motivo

`JWPLC_Ethernet_W5x00_Backend` ya utiliza layout Arduino 1.5 con fuentes bajo `src/`, por lo que admite de forma natural `precompiled=full` y un archive en `src/{build.mcu}` con fallback a fuentes cuando el archive no existe.

Las tres Adafruit bundled conservan el layout plano upstream con fuentes en la raiz de la libreria. La especificacion Arduino ubica los binarios precompilados en `src/{build.mcu}`. Agregar un arbol `src/` a una libreria plana requiere validar primero el cambio de layout y el fallback de fuentes; no se hara dentro de P5A por estabilidad.

## Alcance P5A

- Reutilizar los 8 objetos Ethernet del P3 determinista validado.
- Generar `src/esp32/libJWPLC_Ethernet_W5x00_Backend.a`.
- Ejecutar un cold limpio con P1 + P2 + P3 + P5A.
- Confirmar cero compilaciones fuente del backend Ethernet.
- Comparar seleccion de librerias, objetos externos, mapa de enlace y tamano de aplicacion contra P3 determinista.
- Si falla, retirar el archive y volver automaticamente al fallback fuente.

P4 permanece cerrado/rechazado y no se mezcla con P5.
