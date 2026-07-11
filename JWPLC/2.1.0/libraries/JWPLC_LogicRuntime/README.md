# JWPLC_LogicRuntime

Motor lógico por bloques para **JWPLC Basic**.

## Estado

```text
PoC 0 / estructura inicial
```

La librería se integra sobre la placa existente `JWPLC Basic`. No crea una variante física nueva y no reemplaza el uso normal de sketches Arduino.

## Objetivo inicial

- Ejecutar un programa lógico determinista desde RAM.
- Mantener el runtime separado del core `jwcontrol`.
- Empezar con la FRAM actual de 8 KiB.
- Usar un perfil inicial de hasta 100 bloques.
- Escalar posteriormente a una FRAM de 32 KiB sin cambiar el motor.

## Fuera del PoC 0

- Persistencia en FRAM.
- Slots A/B.
- Editor frontal.
- TFT de monitorización.
- microSD.
- OTA.
- Integración obligatoria con OpenPLC.

## Estructura prevista

```text
JWPLC_LogicRuntime/
├── src/
│   ├── JWPLC_LogicRuntime.h
│   ├── JWPLC_LogicRuntime.cpp
│   ├── runtime/
│   ├── blocks/
│   ├── storage/
│   ├── io/
│   └── diagnostics/
└── examples/
    └── JWPLC_LogicRuntime_Default/
```

Las carpetas se irán materializando por etapas para evitar introducir código vacío o APIs prematuras.
