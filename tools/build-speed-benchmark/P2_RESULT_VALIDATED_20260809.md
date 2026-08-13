# P2 - resultado validado de core precompilado

Fecha: 2026-08-09
Rama: `v2.1.0-alpha.4/feature/build-speed-cache`
Target validado: `jwplc_local:esp32:jwplcbasic`
Sketch: `01_empty`

## Resultado

Verificacion final reportada:

- Tiempo P2 limpio: 104.223 s.
- Compilaciones totales: 34.
- Compilaciones del core fuente `jwcontrol`: 0.
- Compilaciones del stub P2: 1.
- Pasadas de preprocesamiento `g++ -E`: 51.
- Tamano de aplicacion igual respecto al build fuente: si.
- SHA raw igual: no, por campos de identidad/hash del ELF/imagen.
- Payload fuera de esos campos: identico byte a byte.

El intento inmediatamente anterior ya habia mostrado:

- Build fuente: 113.026 s, 97 compilaciones.
- Build P2: 104.370 s, 34 compilaciones.
- Reduccion estructural: 63 compilaciones menos, correspondientes al core.

## Conclusion P2

P2 queda VALIDADO para JWPLC Basic como prueba de concepto de core precompilado por perfil.

La sustitucion del core funciona y conserva el payload funcional de la aplicacion. Sin embargo, quitar 63 compilaciones solo redujo alrededor de 8-9 s el cold build en esta PC. Por tanto, el cuello dominante restante no es la compilacion del core.

El siguiente frente debe centrarse en dependency/library discovery y preprocesamiento. El build P2 todavia ejecuta 51 pasadas `g++ -E`.

## Hallazgo para el siguiente experimento

En el log P2 final, 17 de las 51 pasadas `g++ -E` corresponden a `JWPLC_Display.cpp`. El resto se reparte principalmente entre fuentes de Adafruit ST77xx/GFX/BusIO, SD/FS, Ethernet y otras librerias del autoload.

Esto indica que D1 aligero correctamente el discovery del sketch, pero el discovery interno de la libreria Display sigue expandiendo su arbol de dependencias de forma iterativa.

## Estado

- D1: aprobado.
- P1: mecanismo de librerias `.a` aprobado; ganancia limitada por cantidad de unidades cubiertas.
- P2: aprobado para Basic.
- Validacion de P2 para Basic Core: pendiente antes de cierre de Alpha4.
- Integracion definitiva de archives precompilados: pendiente de definir despues de completar los experimentos de rendimiento y compatibilidad.
