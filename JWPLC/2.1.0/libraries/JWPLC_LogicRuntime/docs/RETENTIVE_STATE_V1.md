# Retentivos v1

## Alcance

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

## Fase 1 — estado en RAM

### Flag de bloque

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

### Reglas de validación

El programa se rechaza cuando:

- contiene bits de flags desconocidos;
- aplica `RETENTIVE` a un tipo distinto de `SET/RESET`.

Error público:

```text
INVALID_BLOCK_FLAGS
```

El codec aplica las mismas reglas al serializar y deserializar.

### Bitmap de estado

El snapshot usa un bit por índice de bloque:

```text
bytes = ceil(blockCount / 8)
```

Solo se exportan e importan bits correspondientes a `SET/RESET` retentivos. Los bits de bloques no retentivos se ignoran incluso si están activos en el buffer recibido.

```text
100 bloques: bitmap máximo de 13 bytes
400 bloques: bitmap máximo de 50 bytes
```

### API RAM

```cpp
runtime.retentiveStateBytes();
runtime.retentiveBlockCount();
runtime.exportRetentiveState(buffer, capacity);
runtime.importRetentiveState(buffer, length);
runtime.clearRetentiveStates();
```

Estas funciones no leen ni escriben FRAM.

`stop()` conserva su comportamiento histórico y limpia todos los estados temporales. La persistencia real restaurará el snapshot después de cargar el programa y antes de un `start()` explícito.

### Compatibilidad binaria

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

### Validación física

Ejemplo:

```text
JWPLC_LogicRuntime_Retentive_State_RAM
```

Resultado:

```text
Resultado: 34 PASS, 0 FAIL
RETENTIVOS EN RAM: PASS
```

Compilación:

```text
Flash:       424845 bytes / 3145728 bytes (13 %)
RAM global:   32612 bytes / 327680 bytes (9 %)
RAM restante: 295068 bytes
```

Se confirmó:

1. `LogicBlockDefinition` permanece en 12 bytes;
2. los inicializadores históricos dejan `flags = 0`;
3. solo `SET/RESET` admite `RETENTIVE`;
4. el validador y el codec rechazan flags inválidos;
5. el bitmap ignora bloques no retentivos y `TON`;
6. exportación, limpieza e importación recuperan el estado esperado;
7. el flag sobrevive serialización y deserialización;
8. un snapshot puede reimportarse después de `stop()`.

La prueba inicializó E/S, no incluyó bloques `DigitalOutput`, no utilizó FRAM y mantuvo Q0 apagadas.

## Fase 2 — store retentivo A/B simulado

Antes de escribir la FRAM física se valida un gestor transaccional sobre `LogicMemoryStorage` con inyección de cortes.

Regiones físicas reservadas:

```text
FRAM 8 KiB:  0x1440..0x1A3F, 1536 bytes
FRAM 32 KiB: 0x6040..0x703F, 4096 bytes
```

Cada región se divide en dos registros iguales:

```text
8 KiB:  Copia 0 = 768 bytes,  Copia 1 = 768 bytes
32 KiB: Copia 0 = 2048 bytes, Copia 1 = 2048 bytes
```

### Registro retentivo v1

Cada copia usa una cabecera de 64 bytes seguida del bitmap:

| Offset | Tamaño | Campo |
|---:|---:|---|
| `0x00` | 4 | Magic `JWRT` |
| `0x04` | 2 | Versión de formato |
| `0x06` | 2 | Tamaño de cabecera |
| `0x08` | 4 | Secuencia |
| `0x0C` | 4 | Program ID |
| `0x10` | 4 | Generación |
| `0x14` | 2 | Cantidad de bloques |
| `0x16` | 2 | Bytes de bitmap |
| `0x18` | 4 | CRC32 del bitmap |
| `0x1C` | 4 | CRC32 de cabecera |
| `0x20` | 4 | Flags reservados |
| `0x24` | 28 | Reserva futura |
| `0x40` | variable | Bitmap |

El CRC de cabecera se calcula con el campo `0x1C` igual a cero.

### Commit transaccional

La escritura de una nueva copia sigue este orden:

1. invalidar el magic de la copia destino;
2. escribir el bitmap;
3. escribir los bytes `0x04..0x3F` de la cabecera;
4. escribir `JWRT` al final como commit.

La copia anterior permanece válida durante todo el proceso. Un reinicio antes del último paso debe recuperar el snapshot anterior.

### Identidad estricta

Un snapshot solo se carga cuando coinciden exactamente:

```text
Program ID
generación
cantidad de bloques
bytes esperados del bitmap
```

No se aplicará estado perteneciente a otro programa o generación.

### Prueba preparada

Ejemplo:

```text
JWPLC_LogicRuntime_Retentive_Store_AB
```

La prueba:

- no inicializa E/S;
- no accede a la FRAM física;
- guarda y recarga dos snapshots alternos;
- rechaza identidad, generación y cantidad de bloques distintas;
- corrompe la copia más reciente y recupera la anterior;
- prueba todos los cortes parciales de una actualización;
- confirma la nueva copia con el presupuesto exacto de escritura.

Resultado esperado:

```text
Resultado: 38 PASS, 0 FAIL
STORE RETENTIVO A/B SIMULADO: PASS
```

Después del PASS se conectará el gestor a la región retentiva de la FRAM mediante una prueba física reversible.