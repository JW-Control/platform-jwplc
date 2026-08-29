# JWPLC_LogicRuntime

Motor lógico experimental del package **JWPLC ESP32** para ejecutar lógica determinista sobre **JWPLC Basic** sin reemplazar el flujo normal de Arduino.

La librería contiene actualmente dos fronteras distintas que conviene no mezclar:

- **runtime v1**: motor con integración a I/O, almacenamiento y persistencia;
- **motor v2**: contrato experimental RAM-only utilizado por el editor FBD actual.

No se debe asumir OpenPLC integrado. Este runtime es una implementación propia del ecosistema JWPLC.

## Runtime v1

Header principal:

```cpp
#include <JWPLC_LogicRuntime.h>
```

El runtime v1 mantiene el recorrido validado de programa lógico persistente:

```text
FRAM
→ clasificación de arranque
→ carga activa o fallback
→ copia profunda a RAM
→ ejecución del motor
→ parada segura
→ rollback/restauración cuando corresponde
```

Capacidades validadas del camino v1:

- ciclo de vida `begin()`, `loadProgram()`, `start()`, `tick()` y `stop()`;
- ejecución desde RAM;
- perfil de FRAM de 8 KiB;
- codec binario versionado con CRC32;
- almacenamiento transaccional A/B;
- fallback ante imagen activa corrupta;
- formato/guardado/carga/rollback explícitos;
- detección de FRAM sin formato sin destruirla automáticamente;
- lectura de entradas desde el snapshot lógico JWPLC;
- escritura agrupada de salidas;
- apagado seguro de salidas al iniciar, detener o entrar en fallo;
- bloques digitales y temporizados ya validados por esta línea de desarrollo.

La capacidad compilada y la capacidad física de almacenamiento se tratan como conceptos separados. La configuración actual del runtime v1 se mantiene orientada a un perfil inicial de hasta 100 bloques en el build validado.

## Motor v2 explícito

Header recomendado para código nuevo del motor experimental:

```cpp
#include <JWPLC_LogicRuntime_V2.h>
```

Nombres públicos recomendados:

```cpp
JWPLCLogicV2::Engine
JWPLCLogicV2::Program
JWPLCLogicV2::BlockRecord
JWPLCLogicV2::InputLink
JWPLCLogicV2::BlockType
```

Los nombres históricos `LogicV2EnginePrototype`, `LogicV2Program` y relacionados se conservan por compatibilidad durante la estabilización.

El contrato actual está documentado en:

```text
docs/LOGIC_RUNTIME_V2_CONTRACT.md
```

Estado del contrato:

```text
CONTRACT_MAJOR = 1
CONTRACT_MINOR = 0
RECORD_SCHEMA_VERSION = 1
RAM-ONLY
SIN FRAM v2
SIN SALIDAS FÍSICAS v2
```

Por tanto, el motor v2 **no sustituye todavía** al runtime v1 persistente.

## Modelo v2

Un bloque v2 usa `LogicV2BlockRecord` y sus entradas se describen mediante `LogicV2InputLink`.

Regla topológica principal:

> Una entrada sólo puede apuntar a un bloque anterior o a una fuente especial válida.

Fuentes especiales:

```text
HI
LO
OPEN
```

Esto evita ciclos y permite evaluar el programa en un recorrido determinista de índice ascendente.

La semántica de entradas pertenece al motor, no a la UI:

```cpp
engine.inputValue(blockIndex, inputIndex);
```

La negación por pin también forma parte del contrato del enlace.

## Tipos del motor v2

El contrato actual contempla como ejecutables:

- entrada digital;
- salida digital lógica;
- `HI` / `LO`;
- `NOT`;
- `AND` de 2 a 8 entradas;
- `OR` de 2 a 8 entradas;
- `NAND` de 2 a 8 entradas;
- `NOR` de 2 a 8 entradas;
- `XOR` de 2 a 8 entradas;
- `SET/RESET` con prioridad de RESET;
- `TON`.

Que el motor pueda ejecutar un tipo **no significa automáticamente** que el asistente gráfico actual permita crearlo. La UI mantiene su propia lista de capacidades habilitadas.

## TON v2

`TON` usa:

- entrada `0`: trigger;
- `parameter`: tiempo efectivo en milisegundos;
- bits de `resource`: base de presentación de UI.

Bases de presentación actuales:

```text
0 = segundos : centésimas
1 = minutos : segundos
2 = horas : minutos
3 = reservado
```

La base de presentación no cambia la unidad interna del motor.

## Estado del engine v2

```text
EMPTY
READY
RUNNING
STOPPED
FAULT
```

`loadProgram()` valida y copia profundamente el programa. `start()` y `stop()` reinician el estado temporal que corresponde sin introducir persistencia automática.

## Edición transaccional

La UI v2 utiliza una sesión de edición en RAM:

```text
copiar programa del motor
→ editar borrador
→ validar imagen completa
→ detener motor si estaba RUNNING
→ cargar copia válida
→ reiniciar cuando corresponde
```

Las operaciones estructurales ya implementadas en esta línea incluyen append y eliminación controlada con validación de consumidores, compactación y rollback del borrador ante fallo.

La edición v2 no debe escribir FRAM ni accionar salidas físicas desde el callback gráfico.

## Separación con la UI

`JWPLC_LogicRuntime` no debe depender de TFT o botonera. La integración gráfica pertenece a:

```text
JWPLC_LogicRuntime_UI
```

El runtime puede utilizarse sin incluir la UI.

## Seguridad y determinismo

Principios mantenidos por el desarrollo:

- referencias topológicas verificables;
- validación antes de cargar/aplicar;
- copia profunda del programa;
- salida segura en el runtime v1 ante stop/fault;
- ninguna escritura persistente implícita por abrir un editor;
- ninguna operación larga de FRAM/SD/Ethernet dentro de callbacks TFT.

El runtime no se declara hard real-time. En el hardware actual las entradas dependen además del periodo de actualización del snapshot lógico del core.

## Documentación técnica relevante

```text
docs/LOGIC_RUNTIME_V2_CONTRACT.md
docs/V2_ENGINE_RAM_RESULTS.md
docs/VARIABLE_INPUTS_RAM_RESULTS.md
docs/RUNTIME_V2_TON_RAM_PHYSICAL_RESULTS.md
ARCHITECTURE.md
PORTABILITY.md
```

Estos documentos registran el detalle de experimentos y gates. El README resume únicamente el contrato vigente.

## Pendientes del motor v2

Antes de declararlo sustituto estable del runtime v1 quedan, entre otros:

- persistencia/codec v2;
- política de retentividad v2;
- integración explícita de salidas físicas;
- completar los tipos deseados en la UI;
- estabilizar nombres públicos y retirar gradualmente alias `Prototype` sin romper compatibilidad;
- mantener pruebas de evaluación/validación y regresión.

## Estado

```text
JWPLC ESP32 2.1.0-alpha.6
JWPLC_LogicRuntime 0.2.0
runtime v1: línea persistente validada
motor v2: contrato experimental RAM-only
```

El desarrollo del motor lógico es independiente del trabajo de OpenPLC. No documentar como integrado aquello que aún pertenece a prototipos o contratos experimentales.
