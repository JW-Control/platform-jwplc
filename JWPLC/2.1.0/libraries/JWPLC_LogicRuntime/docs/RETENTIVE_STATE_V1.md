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

El registro binario v1 usa el byte relativo `0x01` para flags:

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

`LogicBlockDefinition` continúa ocupando 12 bytes. Los inicializadores históricos conservan los mismos cinco argumentos y dejan `flags = 0`.

### Reglas

El programa se rechaza cuando:

- contiene bits de flags desconocidos;
- aplica `RETENTIVE` a un tipo distinto de `SET/RESET`.

Error público:

```text
INVALID_BLOCK_FLAGS
```

El codec aplica las mismas reglas al serializar y deserializar.

### Bitmap y API RAM

El snapshot usa un bit por índice de bloque:

```text
bytes = ceil(blockCount / 8)
100 bloques: 13 bytes
400 bloques: 50 bytes
```

Solo se exportan e importan bits correspondientes a `SET/RESET` retentivos.

```cpp
runtime.retentiveStateBytes();
runtime.retentiveBlockCount();
runtime.exportRetentiveState(buffer, capacity);
runtime.importRetentiveState(buffer, length);
runtime.clearRetentiveStates();
```

Estas funciones no leen ni escriben FRAM. `stop()` mantiene su comportamiento histórico y limpia los estados temporales.

### Validación física

```text
Ejemplo: JWPLC_LogicRuntime_Retentive_State_RAM
Resultado: 34 PASS, 0 FAIL
RETENTIVOS EN RAM: PASS

Flash:       424845 bytes / 3145728 bytes (13 %)
RAM global:   32612 bytes / 327680 bytes (9 %)
RAM restante: 295068 bytes
```

La prueba inicializó E/S, no incluyó bloques `DigitalOutput`, no utilizó FRAM y mantuvo Q0 apagadas.

## Fase 2 — store retentivo A/B simulado

### Regiones

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

La copia anterior permanece válida durante toda la actualización.

### Identidad estricta

Un snapshot solo se carga cuando coinciden exactamente:

```text
Program ID
generación
cantidad de bloques
bytes esperados del bitmap
```

No se aplica estado perteneciente a otro programa o generación.

### Validación simulada

```text
Ejemplo: JWPLC_LogicRuntime_Retentive_Store_AB
Resultado: 38 PASS, 0 FAIL
STORE RETENTIVO A/B SIMULADO: PASS

Flash:       423713 bytes / 3145728 bytes (13 %)
RAM global:   32524 bytes / 327680 bytes (9 %)
RAM restante: 295156 bytes

Bytes escritos por snapshot completo: 66
Puntos de corte probados: 66
```

Se confirmó:

- guardado alterno A/B;
- selección por secuencia;
- carga estricta por identidad;
- recuperación de A al corromper B;
- conservación de A en todos los cortes parciales;
- confirmación de B únicamente con el presupuesto completo de 66 bytes.

La prueba no inicializó E/S ni accedió a la FRAM física.

## Fase 3 — prueba física reversible de la región retentiva

Ejemplo preparado:

```text
JWPLC_LogicRuntime_Retentive_Store_FRAM
```

La prueba modifica temporalmente solo:

```text
0x1440..0x1A3F
```

Protecciones:

- respaldo de los 1536 bytes en NVS;
- CRC32 del respaldo;
- marca de restauración pendiente;
- restauración automática después de un reinicio inesperado;
- verificación byte por byte;
- guardas de 16 bytes antes y después de la región;
- sin inicialización de E/S.

Flujo:

1. respaldar la región original;
2. limpiar temporalmente la región;
3. guardar snapshot A en copia 0;
4. reabrir y cargar A;
5. guardar snapshot B en copia 1;
6. reabrir y cargar B;
7. rechazar identidades distintas;
8. corromper el CRC de B;
9. reabrir y recuperar A;
10. comprobar que las regiones adyacentes no cambiaron;
11. restaurar exactamente los 1536 bytes originales.

Comando requerido:

```text
RETENTIVE
```

Resultado esperado:

```text
Resultado: 44 PASS, 0 FAIL
STORE RETENTIVO EN FRAM: PASS
```

Después del PASS se conectará el store retentivo a la fachada de alto nivel y al ciclo `cargar programa → restaurar snapshot → start()`.