# Alpha5 - validación del bridge GPIO para ESP32 genérico

Fecha: 2026-08-23

## Objetivo

Validar que el target genérico `ESP32 Board` del package JWPLC pueda resolver de forma explícita la ABI GPIO mínima usada por archives generados bajo el entorno `JWPLC_BASIC`, sin incorporar la semántica de pines virtuales del JWPLC.

La ABI mínima está formada por:

```txt
jwplc_pinMode
jwplc_digitalWrite
jwplc_digitalRead
```

Para el target genérico estas funciones delegan exclusivamente al GPIO nativo del core ESP32. El bridge vive en `cores/esp32`, por lo que no entra en `JWPLC Basic` (`jwcontrol_p2`) ni en `JWPLC Basic Core` (`jwcontrol`).

## Commits de implementación y gate

```txt
d2b63cb fix(build): añadir bridge GPIO para ESP32 generico
db93b8d test(build): añadir gate de enlace para bridge GPIO generico
```

Sketch de validación:

```txt
tools/build-speed-benchmark/sketches/07_generic_gpio_bridge/07_generic_gpio_bridge.ino
```

El sketch fuerza referencias a los tres símbolos pero no ejecuta operaciones GPIO, por lo que el gate valida exclusivamente disponibilidad ABI y enlace.

## Resultado

```txt
PASS
```

Configuración observada en el log:

```txt
FQBN: jwplc_local:esp32:esp32
Board: esp32
Core: esp32
Package: 2.1.0-dev
```

Arduino reutilizó correctamente su core cacheado:

```txt
Using precompiled core: ...\cores\jwplc_local_esp32_esp32_...\core.a
```

El enlace final completó sin referencias indefinidas a `jwplc_pinMode`, `jwplc_digitalWrite` ni `jwplc_digitalRead`, y se generaron ELF, BIN y merged BIN.

Memoria observada:

```txt
Sketch: 270820 bytes (20%) de 1310720 bytes
RAM global: 22068 bytes (6%) de 327680 bytes
RAM libre estimada: 305612 bytes
```

Log local preservado por el operador:

```txt
tools/build-speed-benchmark/results/manual-logs/20260823_130602_07_generic_gpio_bridge.log
```

## Decisión

El bridge GPIO genérico queda habilitado como mecanismo de compatibilidad para evaluar la recuperación incremental de precompilados compartidos.

Esto no convierte cualquier dependencia `jwplc_*` en aceptable. Sólo pueden considerarse bridge-compatible los tres símbolos GPIO anteriores. Cualquier otro símbolo `jwplc_*` externo en un archive compartido debe continuar siendo bloqueante.

La recuperación de precompilados se hará de a una librería, repitiendo gates de `ESP32 Board`, `JWPLC Basic` y `JWPLC Basic Core` antes de avanzar.
