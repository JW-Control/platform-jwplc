# Contrato del motor lógico JWPLC v2

## Estado

```text
CONTRATO 1.0 CANDIDATO
RAM-ONLY / SIN FRAM / SIN SALIDAS FÍSICAS
```

Este documento formaliza el motor utilizado por `JWPLC_LogicRuntime_UI` para el editor FBD actual. No reemplaza todavía al runtime v1 estable ni modifica el codec o almacenamiento existente.

## Encabezados públicos

### Runtime v1 estable

```cpp
#include <JWPLC_LogicRuntime.h>
```

Responsabilidades actuales:

- `JWPLC_LogicRuntime`;
- `LogicEngine` y `LogicProgram` históricos;
- almacenamiento, codec y FRAM;
- retentividad v1;
- integración con I/O del JWPLC.

### Motor v2 explícito

```cpp
#include <JWPLC_LogicRuntime_V2.h>
```

Nombres recomendados para código nuevo:

```cpp
JWPLCLogicV2::Engine
JWPLCLogicV2::Program
JWPLCLogicV2::BlockRecord
JWPLCLogicV2::InputLink
JWPLCLogicV2::BlockType
```

Los nombres `LogicV2EnginePrototype`, `LogicV2Program` y relacionados se conservan por compatibilidad mientras se estabiliza la API.

## Versión del contrato

```text
CONTRACT_MAJOR       = 1
CONTRACT_MINOR       = 0
RECORD_SCHEMA_VERSION = 1
```

Cambios que alteren tamaño, significado o codificación de bloques/enlaces deben incrementar la versión de esquema antes de habilitar persistencia.

## Formato de programa

### Bloque

```text
LogicV2BlockRecord = 12 bytes
```

| Campo | Uso |
|---|---|
| `type` | Tipo funcional del bloque. |
| `flags` | Reservado; actualmente debe ser cero. |
| `firstInput` | Índice inicial dentro del arreglo de enlaces. |
| `inputCount` | Cantidad de entradas lógicas del bloque. |
| `reserved` | Debe ser cero. |
| `resource` | Recurso físico o metadato específico del tipo. |
| `parameter` | Parámetro numérico principal. TON lo interpreta en milisegundos. |

### Enlace

```text
LogicV2InputLink = 2 bytes
```

El bit superior almacena negación por pin. Los 15 bits restantes contienen:

- índice de un bloque fuente anterior;
- `HI`;
- `LO`;
- `OPEN`.

## Orden topológico

Un bloque únicamente puede consumir:

- bloques con índice menor;
- `HI`;
- `LO`;
- `OPEN`, cuando el tipo lo permite.

No se admiten ciclos ni referencias hacia bloques posteriores. Esta regla permite evaluación determinista en un único recorrido de `0` a `blockCount - 1`.

## Semántica única de entradas

La resolución de una entrada pertenece al motor v2. La UI no debe duplicar estas reglas.

```cpp
engine.inputValue(blockIndex, inputIndex)
```

Reglas:

- `HI` produce `true`;
- `LO` produce `false`;
- `OPEN` usa el valor neutral del bloque consumidor;
- `AND` y `NAND`: neutral `true`;
- `OR`, `NOR` y `XOR`: neutral `false`;
- la negación se aplica después de resolver la fuente.

## Tipos soportados por el motor 1.0

| Tipo | Estado del motor | Estado actual de UI |
|---|---|---|
| Entrada digital | Ejecutable | Disponible |
| Salida digital lógica | Ejecutable | Disponible |
| HI / LO | Ejecutables como tipos y fuentes especiales | Fuentes especiales |
| NOT | Ejecutable | Disponible |
| AND 2..8 | Ejecutable | AND2 disponible |
| OR 2..8 | Ejecutable | Pendiente de asistente |
| NAND 2..8 | Ejecutable | Pendiente de asistente |
| NOR 2..8 | Ejecutable | Pendiente de asistente |
| XOR 2..8 | Ejecutable | Pendiente de asistente |
| SET/RESET | Ejecutable; RESET prioritario | Pendiente de asistente |
| TON | Ejecutable | Disponible |

## TON

- Entrada `0`: `Trg`.
- `parameter`: tiempo efectivo en milisegundos.
- `resource` bits `1..0`: base de presentación preferida de la UI.
- `scan(..., nowMs)` es obligatorio cuando existe al menos un TON.
- al perder `Trg`, salida y tiempo transcurrido vuelven a cero;
- al alcanzar `parameter`, la salida queda activa mientras `Trg` continúe activo.

Base de presentación actual:

```text
0 = s  (segundos : centésimas)
1 = m  (minutos : segundos)
2 = h  (horas : minutos)
3 = reservado
```

La base no modifica la ejecución interna; solo conserva la intención de edición y visualización.

## Estado del motor

```text
EMPTY -> READY -> RUNNING
              -> STOPPED
cualquier error de carga/evaluación -> FAULT
```

- `loadProgram()` valida y realiza copia profunda.
- `start()` limpia estado temporal y comienza ejecución.
- `stop()` limpia estado temporal, conservando la imagen cargada.
- una edición aplicada recarga el programa y reinicia estados temporales.

## Edición transaccional

`RuntimeUIV2EditSession`:

1. copia bloques y enlaces del motor;
2. modifica únicamente el borrador;
3. valida la imagen completa;
4. detiene el motor si estaba corriendo;
5. carga la copia validada;
6. reinicia el motor cuando corresponde.

No escribe FRAM ni conmuta salidas físicas.

## Eliminación de bloques

La operación estructural disponible es:

```cpp
removeBlock(blockIndex)
```

Contrato inicial:

- no permite borrar el último bloque;
- no permite borrar un bloque con consumidores;
- no elimina en cascada;
- compacta bloques y enlaces;
- corrige `firstInput`;
- decrementa referencias hacia bloques posteriores;
- restaura el borrador completo si la validación final falla.

La UI debe mostrar el número de consumidores antes de confirmar. Una eliminación en cascada podrá añadirse más adelante como una acción separada y explícita.

## Frontera con runtime v1

El motor v2 no debe utilizar directamente:

- `LogicBlockDefinition` v1;
- `LogicEngine` v1;
- codec v1;
- FRAM v1;
- retentividad v1;
- escritura física de Q.

`JWPLC_LogicRuntime.h` continúa incluyendo tipos v2 por compatibilidad histórica, pero todo código nuevo del editor debe incluir `JWPLC_LogicRuntime_V2.h` de forma explícita.

## Frontera de UI

La única clase activa que debe instanciarse es:

```cpp
RuntimeUIFBDMap
```

Las clases `RuntimeUIFBDMapV4` a `RuntimeUIFBDMapV14` son revisiones internas heredadas y no forman parte del contrato público. La fachada activa fija la implementación validada y aplica la política de refresco TFT.

## Política TFT

| Contexto | Periodo |
|---|---:|
| Mapa FBD normal | 100 ms |
| Detalle normal | 100 ms |
| Nodo `+` | 40 ms |
| Asistente de creación | 40 ms |
| Editor de fuente/entrada | 40 ms |
| Editor TON | 40 ms |

Las cachés regionales siguen siendo obligatorias: una llamada de refresh no implica que deban enviarse píxeles al TFT.

## Pendientes antes de declarar el motor v2 estable

- pruebas unitarias/host de validación y evaluación;
- eliminación desde UI;
- habilitar en UI todos los tipos ya ejecutables;
- definir TOF y TP con estado temporal formal;
- definir persistencia/codec v2;
- decidir retentividad;
- integrar salidas físicas de forma explícita;
- reemplazar gradualmente los nombres `Prototype` sin romper compatibilidad;
- aplanar las revisiones históricas de UI cuando la candidata funcional quede cerrada.
