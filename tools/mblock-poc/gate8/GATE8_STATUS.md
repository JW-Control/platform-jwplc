# JWPLC Basic - mBlock - Gate 8 (core publicado 2.1.0-alpha.14 e integración HMI Designer)

Fecha: 2026-10-24
Rama: `experiment/mblock-jwplc`

## Estado

~~~text
GATE8A_PUBLISHED_A14_CORE=PASS        [ejecutado] jwplc:esp32 2.1.0-alpha.14 instalada por el propietario
GATE8B_BRIDGE_ON_PUBLISHED_CORE=PASS  [ejecutado] fase 1 y programa A de la fase 4 compilan
GATE8C_HMI_DESIGNER_INTEGRATION=IN_PROGRESS
~~~

## Gate 8A/8B - Core publicado

La alpha.14 está publicada: tag `v2.1.0-alpha.14` (`f3f47aee`, PR #157), ZIP del
release y entrada en `JWPLC/package_jwplc_index_dev.json` (no en el índice
principal `package_jwplc_index.json`, que es el que tenía configurado el Arduino
CLI de la PC; Arduino IDE 2 sí usa el índice dev). El propietario la instaló
(`jwplc:esp32 2.1.0-alpha.14`, reemplaza la alpha.13): `JWPLC_Display` 1.1.0,
`JWPLC_GlobalPeripherals` 1.1.0 con `JWPLC_Buzzer`.

El bridge vuelve al core publicado con `install-package-from-ref.cjs --reset`,
que ahora escribe `bridge.json` con `fqbn=jwplc:esp32:jwplcbasic` y un sketchbook
vacío propio (`%USERPROFILE%\.jwplc\mblock-bridge\sketchbook-empty`), para que
ninguna librería del sketchbook del usuario tape las del package.

~~~text
programa                 copia de la rama (jwplc_a14)   core publicado (jwplc)
fase 1                   418 244 B                      450 020 B   (98 s en frío)
fase 4 A                 438 217 B                      468 725 B   (66 s)
~~~

Causa de la diferencia [ejecutado]: los `.a` de Display/TFT y el `core.a`
precompilado son idénticos entre la rama (`eddee62e`) y el tag; el resto del
diff son ejemplos y finales de línea. Lo que cambia es la toolchain: una
plataforma en el sketchbook (`hardware/jwplc_a14`) no tiene herramientas propias
instaladas y arduino-cli resolvió `esp32-libs` con la de `esp32:esp32`
(**3.3.11**), mientras que el core publicado usa la que declara el package,
`jwplc/esp32-libs/`**3.3.8** (`compile_commands.json` del build).

Consecuencia: los Gates 4 a 7 se compilaron contra las librerías IDF 3.3.11. Las
pruebas en placa de las fases 1, 2 y 3 (Gates 4E, 5E, 6F) pasaron con esa base;
desde aquí todo compila con la toolchain oficial del package. Si se vuelve a
usar `install-package-from-ref.cjs` con un ref no publicado, tener en cuenta
este efecto.
