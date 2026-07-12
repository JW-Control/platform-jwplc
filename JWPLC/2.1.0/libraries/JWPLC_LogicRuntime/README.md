# JWPLC_LogicRuntime

Motor lógico por bloques para **JWPLC Basic**.

## Estado

```text
PoC 2 / motor en RAM, E/S físicas, métricas y autopruebas de validación
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
- Estadísticas de scan:
  - último tiempo;
  - mínimo;
  - promedio;
  - máximo;
  - cantidad acumulada de scans.
- Bloques iniciales:
  - entrada digital;
  - salida digital;
  - `NOT`;
  - `AND`;
  - `OR`;
  - `SET/RESET` con prioridad de reset;
  - temporizador `TON`.

## Validación física completada

El PoC 1 fue compilado, cargado y validado sobre JWPLC Basic con el programa:

```text
I0_0 AND NOT I0_1 -> TON 2 s -> Q0_0
```

Resultado informado:

```text
PASS
```

Se verificó el encendido retardado de `Q0_0`, el apagado inmediato al perder la condición y la permanencia de las salidas no utilizadas en estado apagado.

## Ejemplo predeterminado

`JWPLC_LogicRuntime_Default.ino` ejecuta la lógica física anterior y reporta cada segundo:

```text
scan us [last/min/avg/max]
```

Comportamiento esperado:

- `Q0_0` permanece apagada si `I0_0` está inactiva.
- `Q0_0` permanece apagada si `I0_1` está activa.
- Si `I0_0` permanece activa e `I0_1` inactiva durante dos segundos, `Q0_0` se activa.
- Al perder la condición, el `TON` y `Q0_0` se desactivan.
- Las demás salidas permanecen apagadas.

## Ejemplo de autopruebas

`JWPLC_LogicRuntime_Validation.ino` prueba sin inicializar E/S:

- programa válido;
- puntero nulo;
- programa vacío;
- límite de bloques excedido;
- tipo de bloque inválido;
- fuente ausente;
- fuente fuera de rango;
- referencia no anterior;
- recurso fuera de rango;
- salida digital duplicada.

Resultado esperado:

```text
10 PASS, 0 FAIL
VALIDACION COMPLETA: PASS
```

## Fuera del PoC 2

- Persistencia en FRAM.
- Slots A/B.
- Retentivos persistentes.
- Formato serializado definitivo.
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
    ├── JWPLC_LogicRuntime_Default/
    └── JWPLC_LogicRuntime_Validation/
```

## Decisiones vigentes

- El programa activo se ejecuta desde RAM.
- El orden actual del arreglo es el orden de ejecución.
- Cada bloque solo puede referenciar bloques anteriores.
- La validación rechaza lazos o referencias hacia adelante.
- Una salida física solo puede ser asignada por un bloque de salida.
- El tamaño serializado será validado además de `maxBlocks` cuando se agregue FRAM.
- La FRAM de 8 KiB permite avanzar con el mismo motor usando un perfil de capacidad menor.
