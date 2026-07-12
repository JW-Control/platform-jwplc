# JWPLC_LogicRuntime

Motor lógico por bloques para **JWPLC Basic**.

## Estado

```text
PoC 3 / motor en RAM, E/S optimizadas y codec binario versionado
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
- Lectura de `I0_0..I0_7` desde el snapshot lógico del core JWPLC.
- Escritura conjunta de `Q0_0..Q0_7` mediante una única operación por banco.
- Omisión de escrituras físicas cuando el bitmap Q0 no cambia.
- Salidas apagadas al iniciar, detenerse o detectar un fallo.
- Estadísticas de scan:
  - último tiempo;
  - mínimo;
  - promedio;
  - máximo;
  - cantidad acumulada de scans;
  - escrituras físicas Q0.
- Codec binario portable:
  - little-endian explícito;
  - cabecera de 64 bytes;
  - registros de bloque de 12 bytes;
  - CRC32 de cabecera;
  - CRC32 de payload;
  - metadatos de ID y generación;
  - nombre corto de programa.
- Bloques iniciales:
  - entrada digital;
  - salida digital;
  - `NOT`;
  - `AND`;
  - `OR`;
  - `SET/RESET` con prioridad de reset;
  - temporizador `TON`.

## Validación física completada

Programa probado:

```text
I0_0 AND NOT I0_1 -> TON 2 s -> Q0_0
```

Se verificó:

- encendido retardado de `Q0_0`;
- apagado inmediato al perder la condición;
- reinicio correcto del `TON`;
- permanencia de las salidas no utilizadas en estado apagado.

## Autopruebas del validador

Resultado validado:

```text
10 PASS, 0 FAIL
VALIDACION COMPLETA: PASS
```

Consumo informado para `JWPLC_LogicRuntime_Validation`:

```text
Flash: 418001 bytes
RAM global: 27916 bytes
```

## Rendimiento medido

Primera implementación con E/S individuales:

```text
mínimo:    7231 us
promedio:  7680 us aproximadamente
máximo:   43217 us
```

Implementación por snapshot/banco después de calentamiento:

```text
mínimo:     380 us
promedio:   414 us aproximadamente
máximo:    3889 us
```

Resultado aproximado:

```text
Reducción del promedio: 94.6 %
Mejora: 18.6 veces
```

El PoC 2.2 añade una optimización adicional: Q0 solo se escribe físicamente cuando cambia el bitmap. El ejemplo reporta `escrituras Q0` para verificarlo.

Las entradas siguen consumiendo el snapshot lógico que el core actualiza por defecto cada 20 ms. Los picos de latencia de FreeRTOS y periféricos concurrentes deben seguir considerándose; este PoC no declara hard real-time.

## Formato binario PoC 3

```text
Tamaño = 64 + 12 × bloques
```

| Bloques | Imagen |
|---:|---:|
| 6 | 136 B |
| 100 | 1264 B |
| 400 | 4864 B |

Con el perfil inicial:

```text
Slot por programa: 2560 bytes
Imagen de 100 bloques: 1264 bytes
```

El formato no usa volcados de estructuras C++, evitando depender de padding, alineamiento o versión del compilador.

Documentación detallada:

- `docs/LOGIC_PROGRAM_IMAGE_FORMAT_V1.md`
- `docs/POC_VALIDATION_RESULTS.md`

## Ejemplos

### `JWPLC_LogicRuntime_Default`

Ejecuta la lógica física y reporta:

```text
scan us [last/min/avg/max]
scans
escrituras Q0
```

### `JWPLC_LogicRuntime_Validation`

Prueba sin conmutar salidas:

- programa válido;
- puntero nulo;
- programa vacío;
- límite excedido;
- tipo inválido;
- fuentes incorrectas;
- recursos fuera de rango;
- salida duplicada.

### `JWPLC_LogicRuntime_Codec`

Prueba sin conmutar salidas:

- serialización y deserialización;
- conservación de bloques y metadatos;
- validación de la imagen reconstruida;
- CRC de cabecera y payload;
- truncamiento;
- buffer insuficiente;
- nombre demasiado largo;
- capacidad del slot inicial para 100 bloques.

## Fuera del PoC 3

- Escritura real en FRAM.
- Slots A/B transaccionales.
- Retentivos persistentes.
- Activación y rollback de programa.
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
│   │   ├── LogicStorageProfile.h
│   │   ├── LogicProgramImage.h
│   │   └── LogicProgramCodec.h/.cpp
│   └── io/
│       └── JWPLCLogicIO.h/.cpp
├── docs/
│   ├── LOGIC_PROGRAM_IMAGE_FORMAT_V1.md
│   └── POC_VALIDATION_RESULTS.md
└── examples/
    ├── JWPLC_LogicRuntime_Default/
    ├── JWPLC_LogicRuntime_Validation/
    └── JWPLC_LogicRuntime_Codec/
```

## Decisiones vigentes

- El programa activo se ejecuta desde RAM.
- El orden del arreglo es el orden de ejecución.
- Cada bloque solo puede referenciar bloques anteriores.
- La validación rechaza lazos o referencias hacia adelante.
- Una salida física solo puede ser asignada por un bloque de salida.
- El límite se valida por cantidad de bloques y por tamaño serializado.
- La FRAM de 8 KiB permite iniciar con 100 bloques.
- La futura FRAM de 32 KiB solo ampliará perfiles y límites.
- El proyecto editable completo no pertenece al bytecode ejecutable.
