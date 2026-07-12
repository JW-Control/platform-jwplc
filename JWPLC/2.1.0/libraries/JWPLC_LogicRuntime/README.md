# JWPLC_LogicRuntime

Motor lógico por bloques para **JWPLC Basic**.

## Estado

```text
PoC 1 / motor lógico fijo en RAM
```

La librería se integra sobre la placa existente `JWPLC Basic`. No crea una variante física nueva y no reemplaza el uso normal de sketches Arduino.

## Alcance actual

- Ciclo de vida `begin()`, `loadProgram()`, `start()`, `tick()` y `stop()`.
- Perfil automático para FRAM de 8 KiB.
- Límite inicial de 100 bloques.
- Perfil futuro de 32 KiB con límite provisional de 400 bloques.
- Programa representado mediante bloques ordenados y referencias a bloques anteriores.
- Validación de fuentes, recursos y salidas duplicadas.
- Ejecución determinista desde RAM.
- Lectura de `I0_0..I0_7`.
- Escritura segura de `Q0_0..Q0_7` al final de cada scan.
- Salidas apagadas al iniciar, detenerse o detectar un fallo.
- Bloques iniciales:
  - entrada digital;
  - salida digital;
  - `NOT`;
  - `AND`;
  - `OR`;
  - `SET/RESET` con prioridad de reset;
  - temporizador `TON`.

## Ejemplo predeterminado

`JWPLC_LogicRuntime_Default.ino` ejecuta:

```text
I0_0 AND NOT I0_1 -> TON 2 s -> Q0_0
```

Comportamiento esperado:

- `Q0_0` permanece apagada si `I0_0` está inactiva.
- `Q0_0` permanece apagada si `I0_1` está activa.
- Si `I0_0` permanece activa e `I0_1` inactiva durante dos segundos, `Q0_0` se activa.
- Al perder la condición, el `TON` y `Q0_0` se desactivan.
- Las demás salidas permanecen apagadas.

## Fuera del PoC 1

- Persistencia en FRAM.
- Slots A/B.
- Retentivos persistentes.
- Editor frontal.
- TFT de monitorización.
- microSD.
- OTA.
- Integración obligatoria con OpenPLC.

## Estructura actual

```text
JWPLC_LogicRuntime/
├── src/
│   ├── JWPLC_LogicRuntime.h
│   ├── JWPLC_LogicRuntime.cpp
│   ├── runtime/
│   │   ├── LogicBlock.h
│   │   ├── LogicProgram.h
│   │   ├── LogicValidator.h/.cpp
│   │   └── LogicEngine.h/.cpp
│   ├── storage/
│   │   └── LogicStorageProfile.h
│   └── io/
│       └── JWPLCLogicIO.h/.cpp
└── examples/
    └── JWPLC_LogicRuntime_Default/
```

## Decisiones vigentes

- El programa activo se ejecuta desde RAM.
- El orden actual del arreglo es el orden de ejecución.
- Cada bloque solo puede referenciar bloques anteriores.
- La validación rechaza lazos o referencias hacia adelante.
- Una salida física solo puede ser asignada por un bloque de salida.
- El tamaño serializado será validado además de `maxBlocks` cuando se agregue FRAM.
