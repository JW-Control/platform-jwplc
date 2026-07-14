# Retentivos v1 — fase 1 en RAM

## Objetivo

Introducir estado retentivo sin escribir todavía la región retentiva de FRAM.

La primera fase valida tres piezas por separado:

```text
marcación persistible del bloque
→ validación y codec
→ captura/restauración del estado en RAM
```

Solo después de cerrar esta fase se implementará el almacenamiento transaccional dentro de la región retentiva del mapa v1.

## Alcance v1

El único estado retentivo inicial es:

```text
SET/RESET.value
```

No son retentivos:

- `TON.value`;
- `TON.timing`;
- `TON.startedAtMs`;
- entradas y salidas físicas;
- `NOT`, `AND` y `OR`;
- estados intermedios no marcados.

Un temporizador acumulativo retentivo deberá ser un tipo de bloque separado en una fase futura. `TON` conserva su semántica actual.

## Flag de bloque

El registro binario v1 ya reservaba el byte relativo `0x01`. Se adopta:

```cpp
JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE = 0x01
```

Construcción compatible:

```cpp
// Forma histórica: no retentivo.
{LogicBlockType::SetReset, setSource, resetSource, 0, 0}

// Forma ampliada: retentivo.
{LogicBlockType::SetReset,
 setSource,
 resetSource,
 0,
 0,
 JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE}
```

`LogicBlockDefinition` continúa ocupando exactamente 12 bytes. El sexto argumento es opcional y los cinco argumentos históricos conservan el mismo orden.

## Reglas de validación

El programa se rechaza cuando:

- contiene bits de flags desconocidos;
- aplica `RETENTIVE` a un tipo distinto de `SET/RESET`.

Error público:

```text
INVALID_BLOCK_FLAGS
```

El codec aplica las mismas reglas al serializar y deserializar.

## Bitmap de estado

El snapshot usa un bit por índice de bloque:

```text
bytes = ceil(blockCount / 8)
```

Esto mantiene estable la correspondencia entre definición y estado sin crear IDs adicionales por bloque.

Solo se exportan e importan bits correspondientes a `SET/RESET` retentivos. Los bits de bloques no retentivos se ignoran incluso si están activos en el buffer recibido.

Para 100 bloques:

```text
bitmap máximo = 13 bytes
```

Para 400 bloques:

```text
bitmap máximo = 50 bytes
```

## API RAM

```cpp
runtime.retentiveStateBytes();
runtime.retentiveBlockCount();
runtime.exportRetentiveState(buffer, capacity);
runtime.importRetentiveState(buffer, length);
runtime.clearRetentiveStates();
```

Estas funciones no leen ni escriben FRAM.

`stop()` conserva por ahora su comportamiento histórico y limpia todos los estados temporales en RAM. La persistencia real restaurará el snapshot después de cargar el programa y antes de un `start()` explícito.

## Compatibilidad binaria

El registro sigue midiendo 12 bytes:

| Offset | Tamaño | Campo |
|---:|---:|---|
| `0x00` | 1 | tipo |
| `0x01` | 1 | flags del bloque |
| `0x02` | 2 | recurso |
| `0x04` | 2 | fuente A |
| `0x06` | 2 | fuente B |
| `0x08` | 4 | parámetro |

Las imágenes anteriores contienen `flags = 0` y mantienen su comportamiento no retentivo.

## Prueba preparada

Ejemplo:

```text
JWPLC_LogicRuntime_Retentive_State_RAM
```

La prueba:

1. comprueba que `LogicBlockDefinition` sigue midiendo 12 bytes;
2. confirma compatibilidad del inicializador histórico;
3. valida un programa con un `SET/RESET` normal y otro retentivo;
4. importa un bitmap con bits adicionales y confirma que solo cambia el bloque permitido;
5. exporta el estado y comprueba que solo aparece el bit retentivo;
6. valida limpieza y restauración del snapshot;
7. serializa y deserializa el flag mediante el codec v1;
8. rechaza flags desconocidos;
9. rechaza `TON` retentivo;
10. confirma que el snapshot puede reimportarse después de `stop()`.

Resultado esperado:

```text
Resultado: 34 PASS, 0 FAIL
RETENTIVOS EN RAM: PASS
```

La prueba inicializa E/S porque usa la fachada completa del runtime, pero no contiene bloques `DigitalOutput`, no usa la FRAM y mantiene Q0 apagadas.

## Siguiente fase

Después del PASS se definirá el registro retentivo transaccional dentro de:

```text
FRAM 8 KiB:  0x1440..0x1A3F, 1536 bytes
FRAM 32 KiB: 0x6040..0x703F, 4096 bytes
```

La persistencia deberá incluir como mínimo:

- firma y versión;
- secuencia;
- identidad del programa;
- generación;
- cantidad de bloques;
- bitmap;
- CRC32;
- dos copias alternas para tolerar cortes.

No se aplicará un snapshot a una identidad o generación de programa diferente.