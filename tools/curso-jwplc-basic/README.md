# Pruebas del Curso JWPLC Basic

Una prueba `*.jwtest.json` de JWPLC Sim por cada lección de
`JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/examples/Curso_JWPLC_Basic`.
Cada una aplica los estímulos que pide la lección (pulsadores, sensores) y
comprueba los relés y el Monitor Serie en los instantes indicados.

El JWPLC simulado corre `setup()` unos 710 ms después del arranque; los
estímulos y comprobaciones empiezan después de ese instante.

## Correr las pruebas

Desde un clon de `JW-Control/jwplc-sim`, contra esta copia del package:

```powershell
node cli/jwsim-cli.mjs test <ruta a platform-jwplc>/tools/curso-jwplc-basic --package <ruta a platform-jwplc>
```

## Compilar para la placa

```powershell
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic <carpeta de la lección>
```
