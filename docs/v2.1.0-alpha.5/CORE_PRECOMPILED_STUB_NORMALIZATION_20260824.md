# Alpha5 — normalización del stub de core precompilado

Fecha: 2026-08-24

## Decisión

`cores/jwcontrol` permanece como el único core fuente y canónico del JWPLC.

El antiguo directorio:

`cores/jwcontrol_p2`

se renombra a:

`cores/jwcontrol_precompiled_stub`

y su translation unit mínima pasa a llamarse:

`precompiled_core_stub.c`

Este directorio no contiene una segunda implementación del core. Su única
función es satisfacer la fase `build.core` de Arduino cuando JWPLC Basic
enlaza el core funcional desde:

`precompiled/core/JWPLCBASIC/core.a`

Ese archive debe generarse siempre desde `cores/jwcontrol`.

## Gates de compatibilidad

### Generic ESP32

- FQBN: `jwplc_local:esp32:esp32`
- Core reportado por Arduino: `esp32`
- Sketch: 266756 bytes
- Resultado: PASS

### JWPLC Basic

- FQBN: `jwplc_local:esp32:jwplcbasic`
- Core reportado por Arduino: `jwcontrol_precompiled_stub`
- `precompiled/core/JWPLCBASIC/core.a`: enlazado
- Sketch: 394313 bytes
- Resultado: PASS

### JWPLC Basic Core

- FQBN: `jwplc_local:esp32:jwplcbasiccore`
- Core reportado por Arduino: `jwcontrol`
- El core fuente se compila directamente.
- No enlaza el `core.a` de JWPLC Basic.
- Sketch: 339736 bytes
- Resultado: PASS

## Falso negativo del primer verificador

El primer gate automático terminó globalmente en `FAIL` aunque los tres
builds habían pasado.

La causa fue inferir el core utilizado mediante un regex sobre todo el log
verbose.

Generic ESP32 recibe un include auxiliar:

`cores/jwcontrol/peripherals/include`

Ese `-I` no significa que Generic compile o enlace el core `jwcontrol`.

Regla reafirmada:

- no inferir translation units ni selección real de core a partir de rutas `-I`;
- usar `Using core ...`, fuentes efectivamente compiladas, `compile_commands.json`
  o mapa de enlace según corresponda.

Después de reanalizar los tres logs existentes sin recompilar:

`GENERIC_CORE_GATE=PASS`

`BASIC_PRECOMPILED_CORE_GATE=PASS`

`BASIC_SOURCE_CORE_GATE=PASS`

`ALPHA5_CORE_STUB_NORMALIZATION=PASS`

## Alcance

Este cambio es arquitectónico/nomenclatural.

No cambia:

- APIs Arduino/JWPLC;
- periféricos integrados;
- mutex SPI;
- timeouts;
- Ethernet;
- Display;
- comportamiento del firmware.

Los documentos históricos Alpha4 pueden conservar el nombre `P2` /
`jwcontrol_p2` cuando describen fielmente el estado de aquella etapa.