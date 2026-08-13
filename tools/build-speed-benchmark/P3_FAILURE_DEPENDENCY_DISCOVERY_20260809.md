# P3 - fallo de dependency discovery y correccion

Fecha: 2026-08-09

## Sintoma

Al activar `precompiled=full` para `JWPLC_Display`, el cold build fallo al compilar `01_empty` con:

`JWPLC_Display_Auto.h: fatal error: JWPLC_GlobalPeripherals.h: No such file or directory`

El log mostro antes:

`Skipping dependencies detection for precompiled library JWPLC_Display`

## Causa

D1 mantenia `JWPLC_Display_Auto.h` liviano durante discovery y confiaba en que la libreria `JWPLC_Display` terminara llevando al builder hacia el resto de dependencias.

Con P3, Arduino deja de inspeccionar los fuentes de `JWPLC_Display` porque existe un archive compatible `precompiled=full`. Por tanto, dependencias que antes aparecian durante el preprocesado de `JWPLC_Display.cpp` ya no se descubren automaticamente.

## Correccion experimental

Se mantiene `JWPLC_Display_Auto.h` como punto de entrada, pero se agregan marcadores vacios para resolver librerias sin expandir headers pesados:

- `JWPLC_GlobalPeripherals_Auto.h`
- `JWPLC_Bundled_ST77xx_Marker.h`
- `JWPLC_Bundled_GFX_Marker.h`
- `JWPLC_Bundled_BusIO_Marker.h`
- `JWPLC_Bundled_Ethernet_Marker.h`
- `JWPLC_Bundled_SD_Marker.h`

Durante discovery los marcadores solo identifican la libreria correspondiente. Durante compilacion normal `JWPLC_GlobalPeripherals_Auto.h` incluye la API completa de `JWPLC_GlobalPeripherals`.

## Beneficio secundario esperado

Los marcadores `JWPLC_Bundled_*` existen solamente dentro de las copias incluidas por el package JWPLC. Esto permite que el builder seleccione esas copias de forma determinista antes de resolver headers genericos que tambien pueden existir en el sketchbook del usuario.

Esto ataca la observacion previa donde Adafruit y Ethernet se resolvian desde `Documents/Arduino/libraries` en lugar de las copias bundled.

## Estado

Pendiente repetir P3 y verificar:

- cold build exitoso;
- `JWPLC_Display fuente=0`;
- core P2 con `stub=1` y cero fuentes `jwcontrol`;
- reduccion de pasadas `g++ -E`;
- mismo tamano y payload funcional;
- rutas bundled seleccionadas para ST77xx/GFX/BusIO/Ethernet/SD.
