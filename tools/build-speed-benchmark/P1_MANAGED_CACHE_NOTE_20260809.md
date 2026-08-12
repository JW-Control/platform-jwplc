# P1 - nota sobre managed_cold contaminado

Fecha: 2026-08-09

## Resultado observado

En el run `20260809_141529`, el benchmark reporto:

```text
managed_cold           14.058 s | 1 compilacion
managed_warm_nochange  14.324 s | 1 compilacion
managed_warm_touch     14.687 s | 1 compilacion
```

El valor `managed_cold=14.058 s` NO representa un cold build real.

## Evidencia

El log `Basic_01_empty_managed_cold.log` reutilizo 97 objetos desde:

```text
%LOCALAPPDATA%\Temp\arduino\sketches\<hash>\...
```

mediante mensajes `Using previously compiled file`.

El benchmark vaciaba `ARDUINO_BUILD_CACHE_PATH`, pero no eliminaba el build temporal administrado por Arduino CLI. Por tanto, una ejecucion anterior podia contaminar la fase etiquetada como `managed_cold`.

## Datos validos del mismo run

Los warm builds SI son validos como ciclo incremental:

```text
managed_warm_nochange  14.324 s
managed_warm_touch     14.687 s
```

Los cinco pilotos P1 fueron detectados como librerias precompiladas:

- JW_RTC
- JW_FRAM
- JW_SD
- JW_MatrixButtons
- JWPLC_ModbusRTU

D1 tambien se mantiene activo: el warm build conserva solo 3 pasadas `g++ -E`.

## Cold P1 controlado

El generador P1 hizo una comparacion limpia con `--build-path` y `--clean` en el mismo run:

```text
Fuente D1 sin .a: 123.362 s | 102 compilaciones
P1 con 5 .a:     105.940 s |  97 compilaciones
```

Reduccion controlada de P1:

```text
17.422 s
14.1 %
```

## Accion

Se agrega `Clear-JWPLCArduinoTempBuild.ps1` para eliminar de forma selectiva solo los builds temporales asociados al sketch de benchmark antes de repetir una medicion cold administrada.

Antes de cerrar Alpha4, la tabla final debe distinguir explicitamente:

- cold limpio;
- warm sin cambios;
- warm despues de editar;
- upload full;
- upload app-only.
